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
//            so its slope becomes variance instead and widens the glint and blurs the reflection, the way
//            unresolved waves look on a real sea (Bruneton et al. 2010, "seamless transitions from
//            geometry to BRDF"; same idea as Toksvig / LEAN mapping).
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
    float2 uv1 = input.uv * normalScale + g_NormalScroll.xy * time;
    float2 uv2 = input.uv * normalScale * detailScale + g_NormalScroll.zw * time;
    // rgb [0~1] -> normal vector [-1~1] 범위로 만들기 위해 * 2.0 - 1.0
    float3 n1 = g_NormalMap.Sample(g_NormalSampler, uv1).xyz * 2.0 - 1.0;
    float3 n2 = g_NormalMapB.Sample(g_NormalSampler, uv2).xyz * 2.0 - 1.0;
    // Whiteout blend: add the slopes (xy), multiply the z. Plain n1 + n2 halves the detail of each layer.
    float3 blendedNormalTS = float3((n1.xy + n2.xy) * normalStrength, n1.z * n2.z);
    blendedNormalTS = normalize(blendedNormalTS);

    // Plane TBN: tangent = world X, bitangent = world Z, normal = vertex normal (+ far-field wave slopes).
    FarWaves farWaves = EvaluateFarWaves(input.restPosLocal);
    float farWaveNormals = g_SurfaceParams.w; // 0 = off: no far normals, no roughness (renders as before)
    float slopeVariance = farWaves.slopeVariance * farWaveNormals;
    float3 baseNormalWS = normalize(input.normalWS + farWaveNormals * mul(farWaves.normalDelta, (float3x3)g_World));
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
    // Rough (unresolved) water reflects a blurred sky: the reflected ray spreads by about twice the
    // slope deviation. The hardware mip is taken outside the branch (it needs derivatives).
    float envAutoLod = EnvAutoLod(envLookupDirWS);
    if (slopeVariance > 0.0)
    {
        reflectedSceneColor = SampleEnvBlurred(envLookupDirWS, 2.0 * sqrt(slopeVariance), envAutoLod);
    }

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

    // Linear HDR radiance. Values above 1 (glint, bright sky) are kept; the tonemap pass compresses
    // the whole frame, sky included, with one curve.
    return float4(finalColor, 1.0);
}

