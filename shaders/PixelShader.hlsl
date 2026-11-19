#include "Common.hlsli"
#include "Lighting.hlsli"
#include "Cubemap.hlsli"
#include "Color.hlsli"
#include "Waves.hlsli"

// Water-only resources. Cubemap.hlsli already reserves t0/s0; do not reuse here.
Texture2D    g_NormalMap     : register(t1); // layer A
Texture2D    g_NormalMapB    : register(t2); // layer B (may be the same texture as A)
SamplerState g_NormalSampler : register(s1);

#if SCENE_INPUTS
// Test scenes (scene-color-depth): the scene as it was before the water was drawn. Read with Load, one texel
// per pixel, so pass-through returns exactly the same value (a filtered sample would blend neighbors).
Texture2D        g_SceneColor : register(t3); // HDR color copy
Texture2D<float> g_SceneDepth : register(t4); // depth buffer, 0 (near) .. 1 (far), not linear in distance
Texture2D        g_FoamMap    : register(t5); // contact foam noise (R), the cloud texture of the old foam-mask branch
Texture2D        g_SceneNormal : register(t6); // world normal x 0.5 + 0.5 written by the opaque pass, a = 0: none
Texture2D<float> g_ShadowMap  : register(t7); // sun-shadow: the sun's depth (orthographic, 0 at the sun end .. 1)
SamplerComparisonState g_ShadowSampler : register(s2); // LESS_EQUAL, outside the map = lit
#include "DepthBands.hlsli"
#endif

float4 DebugOut(float3 value)
{
    return float4(SrgbToLinear(saturate(value)), 1.0);
}

#if SCENE_INPUTS
// World position of the surface the depth buffer holds at this pixel (the same math as the receiver in PSMain).
float3 SceneWorldPosition(int2 pixel, float depth, float2 size)
{
    float2 uv = (float2(pixel) + 0.5) / size;
    float4 positionH = mul(float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0), g_InvViewProjection);
    return positionH.xyz / positionH.w;
}

// Normal of the surface behind this pixel from the depth buffer alone (contact-foam; no normal buffer yet):
// the positions of a horizontal and a vertical neighbor span the surface, and their cross product is the normal.
// Of the two neighbors on each axis the one whose depth is closer to this pixel's is used, so a neighbor on
// another object across a silhouette is skipped when the other side is still on this surface (Turanszki 2019).
// At the frame edge the missing neighbor is mirrored. The sign is set to face the camera.
float3 SceneNormalFromDepth(int2 pixel, float depth, float3 center, int2 size)
{
    int2 left  = int2(pixel.x > 0 ? pixel.x - 1 : pixel.x + 1, pixel.y);
    int2 right = int2(pixel.x < size.x - 1 ? pixel.x + 1 : pixel.x - 1, pixel.y);
    int2 up    = int2(pixel.x, pixel.y > 0 ? pixel.y - 1 : pixel.y + 1);
    int2 down  = int2(pixel.x, pixel.y < size.y - 1 ? pixel.y + 1 : pixel.y - 1);
    float depthLeft  = g_SceneDepth.Load(int3(left, 0));
    float depthRight = g_SceneDepth.Load(int3(right, 0));
    float depthUp    = g_SceneDepth.Load(int3(up, 0));
    float depthDown  = g_SceneDepth.Load(int3(down, 0));
    bool useRight = abs(depthRight - depth) < abs(depthLeft - depth);
    bool useDown  = abs(depthDown - depth) < abs(depthUp - depth);
    float2 sizeF = float2(size);
    float3 alongX = useRight ? SceneWorldPosition(right, depthRight, sizeF) - center
                             : center - SceneWorldPosition(left, depthLeft, sizeF);
    float3 alongY = useDown  ? SceneWorldPosition(down, depthDown, sizeF) - center
                             : center - SceneWorldPosition(up, depthUp, sizeF);
    float3 normal = normalize(cross(alongY, alongX));
    return dot(normal, g_CameraPositionWS.xyz - center) < 0.0 ? -normal : normal;
}

// The normal the opaque pass stored for this pixel (normal-gbuffer). Interpolated vertex normals: a rounded
// pillar stays round, and edges and thin objects are as right as anywhere else. Zero where nothing wrote one.
float3 SceneNormalStored(int2 pixel)
{
    float4 stored = g_SceneNormal.Load(int3(pixel, 0));
    return stored.a > 0.5 ? normalize(stored.xyz * 2.0 - 1.0) : float3(0.0, 0.0, 0.0);
}

// How far a pixel lies inside the frame (pixels, negative outside).
float FrameInside(float2 pixel, float2 frameSize)
{
    return min(min(pixel.x, frameSize.x - pixel.x), min(pixel.y, frameSize.y - pixel.y));
}

// Refraction: how many times the bent ray's length is corrected from the surface found on screen (refraction).
#ifndef REFRACTION_STEPS
#define REFRACTION_STEPS 4
#endif

// Baked foam mask (foam-masks): repeats without a seam, so it is read as it is (wrap). Two layers, the second turned,
// a little larger and drifting across the first (flow = how far the first has drifted, in mask repeats), so the
// network keeps changing shape instead of sliding as one sheet. R = the brighter of the two networks, G = either
// layer's bubbles, B = the first layer's breakup (low frequency; averaging would flatten it).
float4 FoamMask(float2 uv, float flow)
{
    float4 a = g_FoamMap.Sample(g_NormalSampler, uv);
    float2 uvB = float2(uv.x * 0.8 - uv.y * 0.6, uv.x * 0.6 + uv.y * 0.8) * 0.77 + float2(0.31, 0.57) + float2(-0.6, 0.8) * (0.4 * flow);
    float4 b = g_FoamMap.Sample(g_NormalSampler, uvB);
    return float4(max(a.r, 0.85 * b.r), max(a.g, b.g), a.b, 1.0);
}

// Foam noise of the old cloud texture (contact-foam), 0..1. The texture does not tile (its edges do not match), so it
// is read mirrored: every other repeat is flipped, which hides the seam. A second read, turned and smaller, breaks
// the mirror symmetry.
float FoamNoise(float2 uv)
{
    float2 uvB = float2(uv.x * 0.8 - uv.y * 0.6, uv.x * 0.6 + uv.y * 0.8) * 1.7 + 0.37;
    float2 mirroredA = 1.0 - abs(frac(uv * 0.5) * 2.0 - 1.0);
    float2 mirroredB = 1.0 - abs(frac(uvB * 0.5) * 2.0 - 1.0);
    float noise = 0.6 * g_FoamMap.Sample(g_Sampler, mirroredA).r + 0.4 * g_FoamMap.Sample(g_Sampler, mirroredB).r;
    return saturate((noise - 0.5) * 1.6 + 0.5); // two reads average toward gray: give the contrast back
}
#endif

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
#if OCEAN_DETAIL
        // --render-size: measure the footprint in 720p pixels, so a high-resolution frame keeps the
        // 720p band split instead of carrying fine crests twice as far (which averages into grey).
        footprint *= g_DetailParams2.y > 0.0 ? g_DetailParams2.y : 1.0; // 0 = an older build: off
#endif
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
    return smoothstep(1.0, 5.0, map.CalculateLevelOfDetail(g_NormalSampler, uv) + g_DetailParams2.z);
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
    // The surface normal comes first: the test scenes bend their view of the water below with it (refraction).
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
#if OCEAN_DETAIL
    // --render-size: the mip a 720p frame would use (bias = log2 of the size ratio), same reason as above.
    float3 n1 = g_NormalMap.SampleBias(g_NormalSampler, uv1, g_DetailParams2.z).xyz * 2.0 - 1.0;
    float3 n2 = g_NormalMapB.SampleBias(g_NormalSampler, uv2, g_DetailParams2.z).xyz * 2.0 - 1.0;
#else
    float3 n1 = g_NormalMap.Sample(g_NormalSampler, uv1).xyz * 2.0 - 1.0;
    float3 n2 = g_NormalMapB.Sample(g_NormalSampler, uv2).xyz * 2.0 - 1.0;
#endif
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
#if SCENE_INPUTS
    // Rendering review: the ripple map's two in-plane axes follow the wave surface, so they lie in it (Gram-Schmidt:
    // remove the part of X along the wave normal, then the second axis = X' x N, which is +Z on flat water). Flat X / Z
    // on a tilted wave face pierce the surface and mix the ripple slopes with the wave's own tilt. Test scenes only:
    // the old ocean keeps its exact look.
    tangentWS = normalize(tangentWS - baseNormalWS * dot(baseNormalWS, tangentWS));
    bitangentWS = cross(tangentWS, baseNormalWS);
#endif
    float3 finalNormalWS  = normalize(blendedNormalTS.x * tangentWS + blendedNormalTS.y * bitangentWS + blendedNormalTS.z * baseNormalWS);
#if SCENE_INPUTS
    int3 scenePixel = int3(input.position.xy, 0); // SV_POSITION.xy is the pixel center (x + 0.5)
    float3 sceneColor = g_SceneColor.Load(scenePixel).rgb;
    float sceneDepth = g_SceneDepth.Load(scenePixel);
    int sceneDebugMode = (int)g_DebugParams.x;
    int transmissionMode = (int)(g_SceneParams.x + 0.5);
    // 8 = raw depth as gray: almost everything sits near 1 (white), the value is not proportional to distance.
    if (sceneDebugMode == 8) { return DebugOut(sceneDepth.xxx); }
    // 9 = where the water is drawn: the scene tinted blue (HDR, tonemapped like a normal frame).
    if (sceneDebugMode == 9) { return float4(sceneColor * float3(0.35, 0.55, 1.0) + float3(0.0, 0.02, 0.08), 1.0); }
    // sun-shadow: how much sun reaches this surface point (1 = lit, 0 = a caster is between it and the sun). It only
    // turns off the mirrored sun (glint): the sun is not there to be reflected. The water color is NOT shadowed here: it
    // is scattered along the whole path under the surface, so one surface point would paint the shadow onto the surface
    // like a decal (a second copy of the floor shadow). The floor's own shadow, seen through the water, is the shadow.
    float surfaceShadow = SunShadow(g_ShadowMap, g_ShadowSampler, g_Shadow, input.worldPos, finalNormalWS,
                                    normalize(-g_LightDirection.xyz));
    // 29 = sun share at the water surface (white = sun, black = shadow).
    if (sceneDebugMode == 29) { return DebugOut(surfaceShadow.xxx); }

    // Receiver = the surface this pixel sees through the water (depth-reconstruct). The pixel and its depth are
    // where projection and view put the surface; the inverse matrix takes them back, and dividing by w undoes
    // the perspective divide. Depth 1 is the sky (cleared, or the skybox drawn at z = w): no surface, nothing to
    // reconstruct. The skybox's interpolated z / w can land a step or two below 1 in the 24-bit buffer, and those
    // pixels then reconstruct onto the far plane (rows of far-plane normals in debug 22), so everything past
    // ~900 m counts as sky (depth 0.99999; the test scenes reach ~50 m, the far plane is 1000 m).
    uint sceneWidth, sceneHeight;
    g_SceneDepth.GetDimensions(sceneWidth, sceneHeight);
    float2 sceneUv = (float2(scenePixel.xy) + 0.5) / float2(sceneWidth, sceneHeight);
    float4 receiverH = mul(float4(sceneUv.x * 2.0 - 1.0, 1.0 - sceneUv.y * 2.0, sceneDepth, 1.0), g_InvViewProjection);
    float3 receiverWS = receiverH.xyz / receiverH.w;
    bool hasReceiver = sceneDepth < 0.99999;
    float maxOpticalDistance = g_SceneParams.y;
    // Water depth: how far the surface lies below the water right above it (the answer key measures the same thing).
    // With waves (wave-occlusion) that water height is not this pixel's: it is looked up above the surface, through
    // the same wave function as the vertex shader. Without waves it is 0, the old flat water.
    // View path: how much water the line of sight crosses, water surface -> surface (no air part). A surface above
    // the water right over it (a step a trough has bared) is seen through air: no path.
    float receiverSideError = 0.0;
    float receiverWaterY = hasReceiver
        ? WaterSurfaceHeightAt(g_Waves, receiverWS.xz, g_CameraPositionWS.xyz, g_WaterParams.x, g_WaterArea, receiverSideError)
        : 0.0;
    float waterDepth = hasReceiver ? receiverWaterY - receiverWS.y : maxOpticalDistance;
    float viewPath = hasReceiver ? (waterDepth > 0.0 ? length(receiverWS - input.worldPos) : 0.0) : maxOpticalDistance;

    // Refraction (refraction): the water below is looked up at another pixel than straight behind, two ways.
    //  Normal offset (default; like Unreal's "Pixel Normal Offset", recommended there for large water): the surface
    //   behind is shifted sideways by how far the water surface tilts away from flat (in meters, then projected).
    //   Flat water does not move at all, ripples and waves wobble the view, so the landing stays close and rarely
    //   needs what is off screen. Tapered at the shore and faded where the landing nears the frame edge.
    //  Physical: the line of sight bent by Snell (water 1.33) and followed to the surface it reaches (projected
    //   to the screen and its length corrected REFRACTION_STEPS times). Flat water bends it too: deep floors look
    //   shallow and a pillar kinks at the waterline, but low views need the frame below the screen, so the bend
    //   is squeezed near the edge (kept as the comparison mode, see the refraction NOTES).
    // Either way only the frame drawn before the water is known: a landing on a surface above the water (a step or
    // pillar standing out of it: foreground leak), or off screen, is tried again at half and a quarter, then the
    // straight view is kept. Strength 0 = straight as before.
    bool physicalRefraction = g_RefractionParams.w > 0.5;
    float refractionStrength = g_RefractionParams.x;
    int refractionResult = 0;          // 0 straight, 1 refracted, 2 foreground leak, 3 off screen, 4 refracted at 1/2 or 1/4
    float2 refractionOffset = 0.0;     // pixels
    float refractionMiss = 0.0;        // m: how far the surface found lies from the bent ray (physical mode)
    [branch] if (refractionStrength > 0.0 && transmissionMode >= 2) // pass-through stays the frame without the water
    {
        float2 frameSize = float2(sceneWidth, sceneHeight);
        float2 pixelCenter = input.position.xy;
        float edgeMargin = max(g_RefractionParams.z, 1.0);
        float3 incident = normalize(input.worldPos - g_CameraPositionWS.xyz);
        float3 surfaceNormal = dot(finalNormalWS, incident) > 0.0 ? -finalNormalWS : finalNormalWS; // wave back faces
        float strength = refractionStrength;
        float2 normalOffset = 0.0;     // normal offset mode: the full offset in pixels
        float3 bent = incident;        // physical mode
        if (physicalRefraction)
        {
            bent = refract(incident, surfaceNormal, 1.0 / max(g_RefractionParams.y, 1.0));
            // Frame edge: a first guess of the landing (full bend, the straight path's length) and its distance to
            // the edge d1 (negative = outside). Inside the margin m (or the pixel's own distance d0, if smaller) the
            // landing distance is compressed as m * exp((d1 - m) / m): never reaching the edge, still ordered.
            float d0 = FrameInside(pixelCenter, frameSize);
            float4 guessClip = mul(mul(float4(input.worldPos + normalize(lerp(incident, bent, strength)) * viewPath, 1.0), g_View), g_Projection);
            float d1 = guessClip.w > 0.0 ? FrameInside(float2(guessClip.x / guessClip.w * 0.5 + 0.5, 0.5 - guessClip.y / guessClip.w * 0.5) * frameSize, frameSize)
                                         : -frameSize.x;
            float roomMargin = min(edgeMargin, d0);
            float softD1 = roomMargin * exp((d1 - roomMargin) / max(roomMargin, 1.0));
            strength *= d1 >= roomMargin ? 1.0 : saturate((d0 - softD1) / max(d0 - d1, 1e-3));
        }
        else
        {
            // The surface behind is shifted sideways in the world, then projected: (1 - 1/n) x tilt x depth for small
            // tilts (Snell; flat water = 0), against the tilt like the bent ray. In meters, so near water moves more
            // pixels than far water (a fixed pixel offset barely showed close to the camera, where one pebble is
            // tens of pixels). Stylized: the depth is a constant "wobble depth" instead of the real one, so shallow
            // water over rails wobbles as much as a deep basin (the real depth made it almost still). Only the shore
            // fade (real water depth) brings it to 0 where a surface meets the water. No surface behind: no offset.
            float shiftDepth = max(g_RefractionParams2.y, 0.0);
            float depthTaper = smoothstep(0.0, max(g_RefractionParams2.x, 1e-3), waterDepth);
            float3 shift = float3(-surfaceNormal.x, 0.0, -surfaceNormal.z)
                         * (1.0 - 1.0 / max(g_RefractionParams.y, 1.0)) * shiftDepth * depthTaper;
            float4 shiftedClip = mul(mul(float4(receiverWS + shift, 1.0), g_View), g_Projection);
            normalOffset = hasReceiver && shiftedClip.w > 0.0
                ? float2(shiftedClip.x / shiftedClip.w * 0.5 + 0.5, 0.5 - shiftedClip.y / shiftedClip.w * 0.5) * frameSize - pixelCenter
                : float2(0.0, 0.0);
            // Edge fade on the landing (EEVEE / Godot style): the offset shrinks as its landing enters the margin.
            normalOffset *= saturate(FrameInside(pixelCenter + normalOffset * strength, frameSize) / edgeMargin);
        }
        [loop]
        for (int attempt = 0; attempt < 3 && refractionResult != 1 && refractionResult != 4; ++attempt)
        {
            float scale = strength * exp2(-attempt);
            float3 rayDir = normalize(lerp(incident, bent, scale));
            float2 landedPixel = pixelCenter + normalOffset * scale;
            float landedDepth = sceneDepth;
            float3 landedWS = receiverWS;
            bool onScreen = all(landedPixel >= 0.0) && all(landedPixel < frameSize);
            if (physicalRefraction)
            {
                float rayLength = viewPath;
                [unroll]
                for (int step = 0; step < REFRACTION_STEPS; ++step)
                {
                    float4 clip = mul(mul(float4(input.worldPos + rayDir * rayLength, 1.0), g_View), g_Projection);
                    float2 ndc = clip.xy / clip.w;
                    landedPixel = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5) * frameSize;
                    onScreen = clip.w > 0.0 && all(landedPixel >= 0.0) && all(landedPixel < frameSize);
                    if (!onScreen) { break; }
                    landedDepth = g_SceneDepth.Load(int3(landedPixel, 0));
                    landedWS = SceneWorldPosition(int2(landedPixel), landedDepth, frameSize);
                    rayLength = landedDepth < 0.99999 ? length(landedWS - input.worldPos) : maxOpticalDistance;
                }
            }
            else if (onScreen)
            {
                landedDepth = g_SceneDepth.Load(int3(landedPixel, 0));
                landedWS = SceneWorldPosition(int2(landedPixel), landedDepth, frameSize);
            }
            bool landedSky = landedDepth >= 0.99999;
            float landedSideError = 0.0;
            float landedWaterY = onScreen && !landedSky
                ? WaterSurfaceHeightAt(g_Waves, landedWS.xz, g_CameraPositionWS.xyz, g_WaterParams.x, g_WaterArea, landedSideError)
                : 0.0;
            refractionResult = !onScreen ? 3 : !landedSky && landedWS.y >= landedWaterY ? 2 : attempt == 0 ? 1 : 4;
            if (refractionResult == 1 || refractionResult == 4)
            {
                refractionOffset = landedPixel - pixelCenter;
                refractionMiss = physicalRefraction && !landedSky ? length(cross(landedWS - input.worldPos, rayDir)) : 0.0;
                scenePixel = int3(landedPixel, 0);
                sceneColor = g_SceneColor.Load(scenePixel).rgb;
                sceneDepth = landedDepth;
                receiverWS = landedWS;
                hasReceiver = !landedSky;
                receiverWaterY = landedWaterY;
                receiverSideError = landedSideError;
                waterDepth = hasReceiver ? receiverWaterY - receiverWS.y : maxOpticalDistance;
                viewPath = hasReceiver ? length(receiverWS - input.worldPos) : maxOpticalDistance;
            }
        }
    }
    // 26 = refraction offset (hue = direction, brightness = pixels, 32 px = full), 27 = refraction result (green =
    // refracted, cyan = refracted at 1/2 or 1/4, red = foreground leak, blue = off screen, black = straight),
    // 28 = how far the surface found lies from the bent ray (white = 10 cm).
    if (sceneDebugMode == 26) { return DebugOut(float3(refractionOffset / 64.0 + 0.5, length(refractionOffset) / 32.0)); }
    if (sceneDebugMode == 27)
    {
        float3 resultColors[5] = { float3(0.0, 0.0, 0.0), float3(0.1, 0.8, 0.2), float3(0.9, 0.1, 0.1),
                                   float3(0.1, 0.3, 0.9), float3(0.1, 0.8, 0.8) };
        return DebugOut(resultColors[refractionResult]);
    }
    if (sceneDebugMode == 28) { return DebugOut((refractionMiss / 0.1).xxx); }
    // 25 = how far the height lookup's rest point still lands from the surface point (white = 1 mm).
    if (sceneDebugMode == 25) { return DebugOut((receiverSideError / 0.001).xxx); }
    // 10 / 11 = water depth / view path in the answer key's bands, 1 m contours, magenta = no surface.
    if (sceneDebugMode == 10 || sceneDebugMode == 11)
    {
        float meters = sceneDebugMode == 10 ? waterDepth : viewPath;
        float3 color = hasReceiver ? DepthBandColor(meters) : kNoSurfaceColor;
        color = lerp(color, float3(0.05, 0.05, 0.05), MeterContour(meters) * 0.8);
        return DebugOut(color);
    }
    // 12 / 13 = the same as numbers, for tools/compare_depth.ps1.
    if (sceneDebugMode == 12 || sceneDebugMode == 13)
    {
        float meters = sceneDebugMode == 12 ? waterDepth : viewPath;
        // A surface above the water right over it gets the answer key's "above water" marker.
        return DebugOut(!hasReceiver ? float3(0.0, 0.0, kMarkerNoSurface)
                        : sceneDebugMode == 12 && waterDepth <= 0.0 ? float3(0.0, 0.0, kMarkerAboveWater)
                        : EncodeMeters(meters, kMarkerMeasured));
    }
    // Transmittance: the share of the light left after the view path through the water, per channel
    // (rgb-attenuation). Each meter keeps the same fraction, so 2 m keeps the square of what 1 m keeps:
    // exp(-extinction x path). Extinction 0 gives exactly 1.
    float3 transmittance = exp(-g_Extinction.rgb * viewPath);
    // 14 = transmittance as color (white = all light gets through, black = none); compare tool input.
    if (sceneDebugMode == 14) { return DebugOut(transmittance); }

    // The water's own color (water-scatter): sunlight and skylight scattered back toward the eye by the water,
    // filling in what the extinction took: + color x incoming light x strength x (1 - T). Same form as aerial
    // perspective. Lit by the scene's lights, so a sunset preset darkens and warms the water too.
    float sunHeight = saturate(-g_LightDirection.y); // g_LightDirection is the direction light travels
    float3 incomingLight = g_AmbientColor.rgb * g_AmbientColor.a + g_LightColor.rgb * g_LightColor.a * sunHeight;
    // Ramp (art mode): shallow -> mid -> deep picked by water depth (the same bed keeps its color from any
    // angle) or by view path (deeper looking at a low angle, like the physical mode).
    float rampDistance = g_RampParams.z > 0.5 ? viewPath : waterDepth;
    float rampMidDepth = max(g_RampParams.x, 1e-3);
    float rampDeepDepth = max(g_RampParams.y, rampMidDepth + 1e-3);
    float3 rampColor = lerp(g_RampShallow.rgb, g_RampMid.rgb, smoothstep(0.0, rampMidDepth, rampDistance));
    rampColor = lerp(rampColor, g_RampDeep.rgb, smoothstep(rampMidDepth, rampDeepDepth, rampDistance));
    float3 waterColor = g_RampParams.w > 0.5 ? rampColor : g_ScatterColor.rgb;
    float3 inscatter = waterColor * incomingLight * g_ScatterColor.a * (1.0 - transmittance);
    // 15 = the water color part alone (HDR, tonemapped like a normal frame: attenuation + this = scattering).
    if (sceneDebugMode == 15) { return float4(inscatter, 1.0); }
    // 16 = the ramp color as picked (no light, no transmittance).
    if (sceneDebugMode == 16) { return DebugOut(LinearToSrgb(rampColor)); }

    // What comes up from under the water, per Transmission mode (0 = the old opaque water, below).
    // Pass-through: no water effect at all. Must match the frame with the water switched off.
    // Attenuation: the scene without the water, dimmed by what the water took on the way. Nothing is added
    // back (that is the water's own color), so very long paths and no surface go black.
    // Scattering: what the extinction took is filled with the water's own color (bed part + water part).
    float3 underwaterColor = transmissionMode == 1 ? sceneColor
                           : transmissionMode == 2 ? sceneColor * transmittance
                           : sceneColor * transmittance + inscatter;

    // Contact foam (contact-foam): a white band where a surface comes up through the water, so a pillar reads as
    // standing in the water even from above, where the reflection is only a few percent. The band follows the
    // water depth of the surface behind the pixel (1 right under the water, 0 at the band width, in meters, so
    // the width does not change with the camera). Water depth alone also catches flat shallows (rail heads,
    // a 0.2 m pad), so the slope filter keeps the band on steep surfaces, with the normal estimated from the
    // depth buffer. Not in pass-through, which has to stay the frame without the water. Surfaces more than 1 cm above
    // the water right over them (bared by a trough) get none; the waterline itself (depth ~0, either sign) keeps it.
    // contact-foam-polish: the distance the band is measured in. 0 = the water depth of the surface behind (vertical, the
    // same from every camera); 1 = how far the view travels in the water before it hits that surface (the reference's
    // scene depth - water depth: changes with the camera angle). Comparison only.
    float foamDepth = g_FoamParams3.w > 0.5 ? viewPath : waterDepth;
    float foamWidth = max(g_FoamParams.x, 1e-3);
    bool foamCandidate = transmissionMode >= 2 && g_FoamParams.w > 0.0 && hasReceiver && waterDepth > -0.01 && foamDepth < foamWidth;
    // Normal source (normal-gbuffer): estimated from the depth buffer, or the one the opaque pass stored next to the
    // color (one read instead of four depth reads, four reconstructions and a cross product).
    bool storedNormal = g_SceneParams.w > 0.5;
    float3 receiverNormal = float3(0.0, 0.0, 0.0);
    float steepness = 0.0;
    [branch] if (hasReceiver && (foamCandidate || (sceneDebugMode >= 21 && sceneDebugMode <= 24)))
    {
        receiverNormal = (storedNormal && sceneDebugMode != 22) || sceneDebugMode == 23 || sceneDebugMode == 24
            ? SceneNormalStored(scenePixel.xy)
            : SceneNormalFromDepth(scenePixel.xy, sceneDepth, receiverWS, int2(sceneWidth, sceneHeight));
        steepness = 1.0 - abs(receiverNormal.y);
    }
    // 21 = steepness of the surface behind the water (black = flat, white = wall), 22 = the normal from depth and
    // 23 = the stored normal as color, 24 = the angle between the two (black 0 -> white 30 degrees or more).
    if (sceneDebugMode == 21) { return DebugOut(steepness.xxx); }
    if (sceneDebugMode == 22 || sceneDebugMode == 23) { return DebugOut(hasReceiver ? receiverNormal * 0.5 + 0.5 : float3(0.0, 0.0, 0.0)); }
    if (sceneDebugMode == 24)
    {
        float3 fromDepth = SceneNormalFromDepth(scenePixel.xy, sceneDepth, receiverWS, int2(sceneWidth, sceneHeight));
        float angle = degrees(acos(clamp(dot(fromDepth, receiverNormal), -1.0, 1.0)));
        return DebugOut(hasReceiver ? (angle / 30.0).xxx : float3(0.0, 0.0, 0.0));
    }
    float foamAmount = 0.0;
    [branch] if (foamCandidate)
    {
        float closeness = 1.0 - saturate(foamDepth / foamWidth);
        // Edge bias (reference: falloff bias): pushes the ramp toward full strength, so the band fills in and stops hard
        // at the width (a thicker, more cartoon edge). 0 = the ramp as before.
        closeness = saturate(closeness + g_FoamParams3.x);
        float slopeMask = lerp(1.0, smoothstep(g_FoamParams2.y, g_FoamParams2.z, steepness), g_FoamParams2.x);
        // The noise drifts downwind on the water surface; its threshold sits inside [softness, 1 - softness],
        // so closeness 0 never shows foam and closeness 1 always does, whatever the noise.
        float2 drift = g_Waves[0].direction * (g_WaterParams.x * g_FoamParams2.w);
        // Rides the waves (reference: surface UVs tied to the vertices): the pattern is laid on the water where it rests,
        // before the Gerstner waves push it sideways, so it sways back and forth with them instead of standing still.
        float2 foamXZ = g_FoamParams3.z > 0.5 ? mul(float4(input.restPosLocal, 1.0), g_World).xz : input.worldPos.xz;
        float2 foamUv = (foamXZ - drift) / max(g_FoamParams.z, 1e-3);
        float softness = clamp(g_FoamParams.y, 1e-3, 0.5);
        float band = closeness * slopeMask;
        if (g_FoamMapParams.x < 0.5)
        {
            // The old cloud texture (contact-foam), read and used as before.
            float noise = FoamNoise(foamUv);
            float threshold = lerp(softness, 1.0 - softness, noise);
            foamAmount = saturate(smoothstep(threshold - softness, threshold + softness, band) * g_FoamParams.w);
        }
        else
        {
            // Baked seamless mask (foam-masks). R = foam network: where it is bright the threshold drops, so the foam
            // forms along the cell borders first and fills in toward the waterline. B = breakup: raises the
            // threshold in patches, so the band breaks into islands. G = loose bubbles in the band's outer part,
            // beyond the solid body.
            float4 mask = FoamMask(foamUv, g_WaterParams.x * g_FoamParams2.w / max(g_FoamParams.z, 1e-3));
            float threshold = lerp(softness, 1.0 - softness, saturate(1.0 - mask.r + (mask.b - 0.5) * g_FoamMapParams.z));
            float body = smoothstep(threshold - softness, threshold + softness, band);
            float bubbles = smoothstep(0.45, 0.7, mask.g) * saturate(band * 3.0) * g_FoamMapParams.y;
            foamAmount = saturate(max(body, bubbles) * g_FoamParams.w);
        }
    }
    // 20 = foam amount (white = foam).
    if (sceneDebugMode == 20) { return DebugOut(foamAmount.xxx); }
    // Leading edge (reference: alpha x leading): right at the contact line the water and its foam fade into the surface
    // they touch, over the first share of the band width. Softens the hard line where the water meets the object.
    float foamLead = 1.0;
    if (foamCandidate && g_FoamParams3.y > 0.0)
    {
        foamLead = saturate(foamDepth / (foamWidth * g_FoamParams3.y));
    }
    // 30 = leading edge (white = water as usual, black = faded into the surface behind).
    if (sceneDebugMode == 30) { return DebugOut(foamLead.xxx); }
    // Foam floats on the surface: it covers the reflection and the water below alike, lit like the water color.
    float3 foamColor = g_FoamColor.rgb * incomingLight;

    // Surface reflection off: the water below alone, as if the surface reflected nothing (foam still on top).
    bool surfaceReflection = g_SceneParams.z > 0.5;
    if (transmissionMode != 0 && !surfaceReflection)
    {
        float3 color = lerp(underwaterColor, foamColor, foamAmount);
        return float4(foamLead < 1.0 ? lerp(sceneColor, color, foamLead) : color, 1.0);
    }
#endif

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
#if SCENE_INPUTS
    lobe *= surfaceShadow; // sun-shadow: no mirrored sun where the sun does not reach the surface
#endif
    finalColor += lobe * sunGlintIntensity * reflectionAmount * g_LightColor.rgb * g_LightColor.a;

#if SCENE_INPUTS
    // surface-reflection: the water below takes the place of the painted water color (two tints, diffuse light
    // and the Blinn-Phong highlight are all left out, or the water's color and the sun would count twice).
    // Fresnel splits the light: R of it is the mirrored sky and sun, the rest went into the water.
    //   final = water below x (1 - R) + (sky + sun glint) x R
    if (transmissionMode != 0)
    {
        float3 reflectionPart = (reflectedSceneColor + lobe * sunGlintIntensity * g_LightColor.rgb * g_LightColor.a)
            * reflectionAmount;
        float3 underwaterPart = underwaterColor * (1.0 - reflectionAmount);
        // 17 = reflection amount R (black = see-through, white = mirror), 18 / 19 = the two parts (HDR,
        // tonemapped like a normal frame: reflection part + underwater part = final, before any haze).
        if (sceneDebugMode == 17) { return DebugOut(reflectionAmount.xxx); }
        if (sceneDebugMode == 18) { return float4(reflectionPart, 1.0); }
        if (sceneDebugMode == 19) { return float4(underwaterPart, 1.0); }
        finalColor = lerp(reflectionPart + underwaterPart, foamColor, foamAmount);
        if (foamLead < 1.0)
        {
            finalColor = lerp(sceneColor, finalColor, foamLead);
        }
    }
#endif

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

