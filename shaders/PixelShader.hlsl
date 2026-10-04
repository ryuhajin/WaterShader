#include "Common.hlsli"
#include "Lighting.hlsli"
#include "Cubemap.hlsli"
#include "Color.hlsli"
#include "Waves.hlsli"

// Water-only resources. Cubemap.hlsli already reserves t0/s0; do not reuse here.
Texture2D    g_NormalMap     : register(t1); // layer A
Texture2D    g_NormalMapB    : register(t2); // layer B (may be the same texture as A)
SamplerState g_NormalSampler : register(s1);

float4 DebugOut(float3 value)
{
    return float4(SrgbToLinear(saturate(value)), 1.0);
}

// Far-field waves, in three bands per wave (debug mode 6 shows them as R / G / B):
//   mesh   - near: the vertex shader displaces the mesh and supplies the normal (weight = vertex fade).
//   pixel  - the mesh LOD dropped the wave but a pixel still resolves it: its slope is added here per
//            pixel, so lit crests keep following the wind (weight = (1 - vertex fade) * pixel fade).
//   rough  - too small for a pixel: drawing it as crests only aliases (moire, lattice of crossing waves),
//            so its slope becomes variance instead and widens the glint (energy-normalized), the way
//            unresolved waves look on a real sea (Bruneton et al. 2010, "seamless transitions from
//            geometry to BRDF"; same idea as Toksvig / LEAN mapping). The sky reflection is not
//            blurred by it: blurring through the cube mips erased the shore and sky reflections at
//            the horizon (wave-far-normals NOTES, step4).
// The pixel band ends early (16 -> 8 px per wavelength): at 4 -> 2 px the four crossing sine stripes read
// as a regular lattice and the glint turns them into moire.
static const float kPixelFadeStart = 1.0 / 16.0; // footprint (wavelengths per pixel) where the fade starts
static const float kPixelFadeEnd   = 1.0 / 8.0;

struct FarWaves
{
    float3 normalDelta;   // local space, added to the vertex normal. Same terms as AccumulateGerstnerWave
                          // with fade = w: q·kA = steepness·w / WAVE_COUNT
    float slopeVariance;  // σ² of the unresolved slopes: a sine of slope amplitude kA has mean square (kA)²/2
    float3 bandWeights;   // average over the waves: x = mesh, y = pixel, z = rough
};

FarWaves EvaluateFarWaves(float3 restPosLocal)
{
    float3 restPosWS = mul(float4(restPosLocal, 1.0), g_World).xyz;
    float viewDistance = length(restPosWS.xz - g_CameraPositionWS.xz);
    // Screen-space derivatives outside the loop (not inside data-dependent branches).
    float2 dx = ddx(restPosLocal.xz);
    float2 dy = ddy(restPosLocal.xz);

    FarWaves result;
    result.normalDelta = 0.0;
    result.slopeVariance = 0.0;
    result.bandWeights = 0.0;
    float activeWaves = 0.0;

    [unroll]
    for (int i = 0; i < WAVE_COUNT; ++i)
    {
        WaveParams wave = g_Waves[i];
        if (wave.amplitude <= 0.0f || wave.wavelength <= 0.0f) continue;

        float vertexFade = WaveVertexFade(wave.wavelength, viewDistance);
        // How many wavelengths one pixel spans along the travel direction: crests running toward the
        // camera stay resolvable much farther than crests running across the view.
        float footprint = max(abs(dot(dx, wave.direction)), abs(dot(dy, wave.direction))) / wave.wavelength;
        float pixelFade = 1.0 - smoothstep(kPixelFadeStart, kPixelFadeEnd, footprint);
        float w = (1.0 - vertexFade) * pixelFade;
        float rough = (1.0 - vertexFade) * (1.0 - pixelFade);
#if OCEAN_DETAIL
        // "Far wave crests" < 1: past the mesh fade, two crossing sines read as a regular lattice (rows of
        // dashes head-on, radial streaks at an angle). Hand that share of the pixel band to the roughness.
        float keepCrests = g_DetailParams2.x;
        rough += w * (1.0 - keepCrests);
        w *= keepCrests;
#endif

        float waveNumber = 6.2831853 / wave.wavelength;
        float phase = dot(wave.direction, restPosLocal.xz) * waveNumber - waveNumber * wave.speed * g_WaterParams.x;
        float slopeAmplitude = waveNumber * wave.amplitude;
        result.normalDelta.xz -= wave.direction * slopeAmplitude * w * cos(phase);
        result.normalDelta.y  -= wave.steepness * w / WAVE_COUNT * sin(phase);
        result.slopeVariance += rough * slopeAmplitude * slopeAmplitude * 0.5;

        result.bandWeights += float3(vertexFade, w, rough);
        activeWaves += 1.0;
    }

    result.bandWeights /= max(activeWaves, 1.0);
    return result;
}

// Phong-style exponent widened by a slope variance: 1/n' = 1/n + σ² (a lobe of width ~1/n convolved
// with the extra spread, approximately). σ² = 0 returns n bit-exactly: the GPU divides through an
// approximate reciprocal, so n / 1 is not always n, and the near field must not change.
float WidenExponent(float exponent, float slopeVariance)
{
    return slopeVariance > 0.0 ? exponent / (1.0 + exponent * slopeVariance) : exponent;
}

// "Align to wind": turn a normal map on the water by angle a (cos, sin; world XZ, 0 = +X, 90 = +Z).
// Sampling at R(-a)·P puts what was at P0 at R(a)·P0. Both water meshes run u = +X, v = +Z, so the uv
// is already world-aligned (X, Z). (1, 0) leaves the uv unchanged.
float2 RotateRippleUv(float2 uv, float2 rotation)
{
    return float2(rotation.x * uv.x + rotation.y * uv.y, -rotation.y * uv.x + rotation.x * uv.y);
}

// The sampled slopes are in the turned map's frame: rotate them by +a back into the plane's X / Z.
float2 RotateRippleSlope(float2 slope, float2 rotation)
{
    return float2(rotation.x * slope.x - rotation.y * slope.y, rotation.y * slope.x + rotation.x * slope.y);
}

// ---- Ocean detail (ocean-hero) ----
// Compiled only into the OCEAN_DETAIL permutation, which ColorShader binds while a detail value is above 0.
// The plain program stays the exact code the older presets were made with (bit-exact captures).
#if OCEAN_DETAIL

// How much of a normal map's ripples its mips have averaged away at this pixel: still all there up to mip 1,
// gone by mip 5 (16 texels per pixel). (Toksvig's |n| estimate does not work here: the mips of these maps
// are renormalized, so their normals never get shorter.)
float RippleLostFraction(Texture2D map, float2 uv)
{
    return smoothstep(1.0, 5.0, map.CalculateLevelOfDetail(g_NormalSampler, uv));
}

float Hash21(float2 p)
{
    p = frac(p * float2(233.34, 851.73));
    p += dot(p, p + 23.45);
    return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
    float2 cell = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    float a = Hash21(cell);
    float b = Hash21(cell + float2(1.0, 0.0));
    float c = Hash21(cell + float2(0.0, 1.0));
    float d = Hash21(cell + float2(1.0, 1.0));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// Wind gust patches ("cat's paws"): broad areas where the wind ruffles the surface more or less.
// Two octaves of value noise in world XZ, drifting downwind. Returns about -1..1.
float GustMask(float2 positionXZ, float time)
{
    float gustScale = max(g_DetailParams.z, 1.0);
    float2 wind = g_Waves[0].direction;
    float2 p = (positionXZ - wind * (time * g_Waves[0].speed * 1.5)) / gustScale;
    // The second octave is turned and offset so the two lattices never line up.
    float2 q = float2(p.x * 0.8 - p.y * 0.6, p.x * 0.6 + p.y * 0.8) * 2.03 + 17.1;
    float n = 0.65 * ValueNoise(p) + 0.35 * ValueNoise(q);
    return saturate((n - 0.5) * 1.6 + 0.5) * 2.0 - 1.0; // a little more contrast than the raw noise
}

#endif // OCEAN_DETAIL

// SV_IsFrontFace = rasterizer stage에서 결정되는 system value. 픽셀이 front face에 속하면 true.
float4 PSMain(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    // Two scrolling normal map samples blended in tangent space.
    float time = g_WaterParams.x;
    float reflectionStrength = g_WaterParams.y;
    float fresnelPower = g_WaterParams.z;
    float normalScale = g_WaterParams.w;
    float specularStrength = g_SpecularParams.x;
    float specularSharpness = g_SpecularParams.y;

    float fresnelF0 = g_SurfaceParams.x;
    float normalStrength = g_SurfaceParams.y;
    float detailScale = g_SurfaceParams.z;

    // Layer A = broad ripples, layer B = finer chop at a different tiling, so the repeat is harder to spot.
    // Each layer has its own normal map; with two different maps the B layer no longer repeats A's shapes.
    // "Align to wind" turns only the pattern. The scroll is added in plane uv before the turn, so the pattern
    // drifts along -scroll on the water as Flow direction shows (added after the turn, it drifted along
    // -R(a)·scroll, i.e. off by the alignment angle a).
    float2 uv1 = RotateRippleUv(input.uv * normalScale + g_NormalScroll.xy * time, g_NormalRotation.xy);
    float2 uv2 = RotateRippleUv(input.uv * normalScale * detailScale + g_NormalScroll.zw * time, g_NormalRotation.zw);
    // rgb [0~1] -> normal vector [-1~1] 범위로 만들기 위해 * 2.0 - 1.0
    float3 n1 = g_NormalMap.Sample(g_NormalSampler, uv1).xyz * 2.0 - 1.0;
    float3 n2 = g_NormalMapB.Sample(g_NormalSampler, uv2).xyz * 2.0 - 1.0;
    n1.xy = RotateRippleSlope(n1.xy, g_NormalRotation.xy);
    n2.xy = RotateRippleSlope(n2.xy, g_NormalRotation.zw);

    // Whiteout blend: add the slopes (xy), multiply the z. Plain n1 + n2 halves the detail of each layer.
    float3 blendedNormalTS = float3((n1.xy + n2.xy) * normalStrength, n1.z * n2.z);

#if OCEAN_DETAIL
    // Gust patches scale the ripple slopes (and with them the ripple roughness below).
    float gustStrength = g_DetailParams.y;
    float gust = 0.0;
    float gustFactor = 1.0;
    [branch] if (gustStrength > 0.0)
    {
        gust = GustMask(input.worldPos.xz, time);
        gustFactor = max(1.0 + gustStrength * gust, 0.0);
        blendedNormalTS.xy *= gustFactor;
    }

    // Ripple slopes the mips averaged away (far field) become glint width instead of vanishing
    // (each map's own slope variance, measured from the texture: g_FarParams.zw).
    // Slopes scale with the ripple strength, so their variance scales with its square.
    float rippleRoughness = g_DetailParams.x;
    float rippleVariance = 0.0;
    [branch] if (rippleRoughness > 0.0)
    {
        float rippleStrength = normalStrength * gustFactor;
        rippleVariance = (g_FarParams.z * RippleLostFraction(g_NormalMap, uv1) + g_FarParams.w * RippleLostFraction(g_NormalMapB, uv2))
            * rippleStrength * rippleStrength * rippleRoughness;
    }
#endif
    blendedNormalTS = normalize(blendedNormalTS);

    // Plane TBN: tangent = world X, bitangent = world Z, normal = vertex normal (+ far-field wave slopes).
    FarWaves farWaves = EvaluateFarWaves(input.restPosLocal);
    float farWaveNormals = g_SurfaceParams.w; // 0 = off: no far normals, no roughness (renders as before)
    float slopeVariance = farWaves.slopeVariance * farWaveNormals * g_FarParams.x; // Light window "Far spread"
#if OCEAN_DETAIL
    slopeVariance += rippleVariance;
#endif
    float3 baseNormalWS = normalize(input.normalWS + farWaveNormals * mul(farWaves.normalDelta, (float3x3)g_World));
#if OCEAN_DETAIL
    // Gusts also roughen / calm the waves themselves (half as much as the ripples): calm patches turn
    // into smoother sky mirrors, rough ones scatter it - what makes cat's paws visible from the air.
    [branch] if (gustStrength > 0.0)
    {
        float3 upWS = normalize(mul(float3(0, 1, 0), (float3x3)g_World));
        baseNormalWS = normalize(lerp(upWS, baseNormalWS, max(1.0 + 0.5 * gustStrength * gust, 0.0)));
    }
#endif
    float3 tangentWS  = normalize(mul(float3(1, 0, 0), (float3x3)g_World));
    float3 bitangentWS  = normalize(mul(float3(0, 0, 1), (float3x3)g_World));
    float3 finalNormalWS  = normalize(blendedNormalTS.x * tangentWS + blendedNormalTS.y * bitangentWS + blendedNormalTS.z * baseNormalWS);

    // Debug visualizations (g_DebugParams.x)
    // 0 = final render
    // 1 = normal map layer A
    // 2 = final world-space normal
    // 3 = mesh UV
    // 4 = front/back face
    // 6 = wave LOD (avg over waves): R = mesh waves, G = pixel-shader wave normals, B = roughness
    // Debug values are data, not light: DebugOut pre-decodes them so the final LinearToSrgb in the
    // tonemap pass (forced to "no tone curve" while debugging) gives back exactly these numbers.
    int debugMode = (int)g_DebugParams.x;
    if (debugMode == 1) { return DebugOut(g_NormalMap.Sample(g_NormalSampler, uv1).rgb); }
    if (debugMode == 2) { return DebugOut(finalNormalWS * 0.5 + 0.5); }
    if (debugMode == 3) { return DebugOut(float3(frac(input.uv), 0.0)); }
    if (debugMode == 4) { return DebugOut(isFrontFace ? float3(0.1, 0.9, 0.2) : float3(0.9, 0.1, 0.1)); }
    if (debugMode == 6) { return DebugOut(farWaves.bandWeights * float3(1.0, farWaveNormals, farWaveNormals)); }

    float3 viewDirWS = normalize(g_CameraPositionWS.xyz - input.worldPos);
    float  viewFacingAmount = saturate(dot(finalNormalWS, viewDirWS));

    float3 lightDirWS = normalize(-g_LightDirection.xyz);
    float  diffuseAmount = DiffuseFactor(finalNormalWS, lightDirWS);

    // view-angle 기반 water color 계산
    float3 waterBaseColor = lerp(g_GrazingColor.rgb, g_FacingColor.rgb, viewFacingAmount);
    float3 ambientLight = waterBaseColor * g_AmbientColor.rgb * g_AmbientColor.a; // ambientColor.a = 환경광 세기
    float3 diffuseLight = waterBaseColor * g_LightColor.rgb * g_LightColor.a * diffuseAmount;
    // Unresolved far waves widen the highlight; (n' + 2) / (n + 2) keeps its energy instead of its peak.
    float roughSpecularSharpness = WidenExponent(specularSharpness, slopeVariance);
    float specularEnergy = slopeVariance > 0.0 ? (roughSpecularSharpness + 2.0) / (specularSharpness + 2.0) : 1.0;
    float specular = SpecularFactor(finalNormalWS, lightDirWS, viewDirWS, roughSpecularSharpness)
        * specularEnergy * specularStrength * g_LightColor.a;
    float3 litWaterColor = ambientLight + diffuseLight + specular.xxx;

    float reflectionByViewAngle = ViewFresnelFactor(viewFacingAmount, fresnelPower, fresnelF0);

    float3 reflectionDirWS = reflect(-viewDirWS, finalNormalWS);
    // A steep ripple back-face can reflect below the horizon and pick up the ground of the skybox.
    // Mirror it back into the sky instead (the ground is not really visible in a water reflection).
    float3 envLookupDirWS = float3(reflectionDirWS.x, abs(reflectionDirWS.y), reflectionDirWS.z);
    float3 reflectedSceneColor = SampleEnv(envLookupDirWS);

    float reflectionAmount = saturate(reflectionByViewAngle * reflectionStrength);
    if (debugMode == 5) { return DebugOut(float3(diffuseAmount, specular, reflectionAmount)); }

    float3 finalColor = lerp(litWaterColor, reflectedSceneColor, reflectionAmount);

    // Sun glint = mirror reflection of the analytic sun (cut out of HDR skies, so it is not counted twice).
    // Energy-normalized Phong lobe: (n + 2) / (2 pi) * pow(R.L, n) integrates to 1 over the hemisphere,
    // so the reflected sun energy is F * E_sun whatever the lobe width. g_LightColor holds E_sun / pi
    // (Lambert convention), hence pi * (n + 2) / (2 pi) = (n + 2) / 2.
    // sunGlintIntensity is now a multiplier on that physical value (1 = physically based).
    // Unresolved far waves widen the lobe (and the normalization dims it to match): a broad glitter path.
    float sunGlintPower = WidenExponent(g_SpecularParams.z, slopeVariance);
    float sunGlintIntensity = g_SpecularParams.w;
    float lobe = SunGlintFactor(reflectionDirWS, lightDirWS, sunGlintPower) * (sunGlintPower + 2.0) * 0.5;
    finalColor += lobe * sunGlintIntensity * reflectionAmount * g_LightColor.rgb * g_LightColor.a;

#if OCEAN_DETAIL
    // Aerial perspective toward the horizon: far water (glint included) fades into the sky just above the
    // horizon in the same direction, so the ocean meets the sky without a hard reflective band.
    float hazeStrength = g_DetailParams.w;
    float hazeAmount = 0.0;
    [branch] if (hazeStrength > 0.0)
    {
        float distanceToPoint = length(input.worldPos - g_CameraPositionWS.xyz);
        hazeAmount = hazeStrength * (1.0 - exp(-distanceToPoint / max(g_FarParams.y, 1.0)));
        // Sky ~10 deg up in the same heading, from a coarse mip: right at the horizon these skies show
        // hills and trees, which every column picked up separately as radial streaks on the water.
        float2 horizontal = -viewDirWS.xz;
        horizontal *= rsqrt(max(dot(horizontal, horizontal), 1e-8));
        float3 hazeDirWS = normalize(float3(horizontal.x, 0.18, horizontal.y));
        float3 hazeColor = g_Skybox.SampleLevel(g_Sampler, hazeDirWS, 7.0).rgb;
        finalColor = lerp(finalColor, hazeColor, hazeAmount);
    }

    // 7 = ocean detail: R = ripple roughness (x20), G = gust mask, B = haze amount
    if (debugMode == 7) { return DebugOut(float3(rippleVariance * 20.0, gust * 0.5 + 0.5, hazeAmount)); }
#endif

    // Linear HDR radiance. Values above 1 (glint, bright sky) are kept; the tonemap pass compresses
    // the whole frame, sky included, with one curve.
    return float4(finalColor, 1.0);
}

