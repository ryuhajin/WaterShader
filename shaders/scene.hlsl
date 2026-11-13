#include "Color.hlsli"
#include "DepthBands.hlsli"
#include "Waves.hlsli"
#include "Shadow.hlsli"

// Opaque objects of the test scenes (water-lab). Patterns follow world position (1 unit = 1 m), so a
// scaled box keeps 1 m checker cells and the ruler bands read as heights.

cbuffer SceneCB : register(b0)
{
    row_major float4x4 g_World;
    row_major float4x4 g_WorldInvTranspose;
    row_major float4x4 g_View;
    row_major float4x4 g_Projection;
    float4 g_LightDirection; // xyz = direction light travels (away from the sun)
    float4 g_LightColor;     // linear rgb, a = intensity
    float4 g_AmbientColor;   // linear rgb, a = intensity
    float4 g_BaseColor;      // linear albedo
    float4 g_MaterialParams; // x = pattern (0 solid, 1 checker, 2 ruler, 3 pebbles), y = debug mode (0 render, 1 water depth bands, 2 water depth numeric, 3 "not measured" under the water numeric views)
    WaveParams g_WaterWaves[WAVE_COUNT]; // the water surface (wave-occlusion), for the answer key
    float4 g_WaterCamera;    // xyz = camera position (the waves fade with distance)
    float4 g_WaterTimeArea;  // x = time, y = water patch half size, z = wave edge fade width
    float4 g_UnderwaterLight; // downwelling: xyz = the water's extinction (1/m), w = cos of the sun's angle under the
                              // water (Snell, from vertical; 0 = off: lit as in air)
    ShadowInputs g_Shadow;   // sun-shadow: the sun shadow map
    float4 g_Caustics;       // caustics: x = strength (0 = off), y = 1 / pattern size (1/m), z = pattern time (s x flow speed),
                             // w = fade-out depth (m)
    float4 g_Caustics2;      // x = mip bias (softness), y = 1 - 1/n (how much a tilted surface bends the light),
                             // zw = xz offset per meter of depth back toward the sun (0 = straight)
    float4 g_Caustics3;      // following the big waves (1 = physical, 0 = off): x = sway, y = light bands
};

Texture2D<float> g_ShadowMap : register(t0);               // sun-shadow: the sun's depth (orthographic)
SamplerComparisonState g_ShadowSampler : register(s0);     // LESS_EQUAL, outside the map = lit
Texture2D g_CausticMap : register(t1);                     // caustics: baked tileable pattern (R), with mips
SamplerState g_WrapSampler : register(s1);                 // repeat

// Water depth of a surface point: the wave surface right above it minus its height (y = 0 when the waves are off).
float WaterDepthBelowSurface(float3 p)
{
    float sideError;
    float surfaceY = WaterSurfaceHeightAt(g_WaterWaves, p.xz, g_WaterCamera.xyz, g_WaterTimeArea.x,
                                          float4(g_WaterTimeArea.yz, 0.0, 0.0), sideError);
    return surfaceY - p.y;
}

// How deep a point sits under the water (m), or 0 when it is dry: outside the water patch, or above the water.
// Only looked up when something under the water needs it (downwelling, caustics).
float UnderwaterDepth(float3 p)
{
    float halfSize = g_WaterTimeArea.y;
    if (halfSize <= 0.0 || max(abs(p.x), abs(p.z)) > halfSize)
    {
        return 0.0;
    }
    float highestWave = 0.0;
    [unroll]
    for (int i = 0; i < WAVE_COUNT; ++i)
    {
        highestWave += max(g_WaterWaves[i].amplitude, 0.0);
    }
    if (p.y > highestWave)
    {
        return 0.0; // above every crest: dry, no need for the height lookup
    }
    return max(WaterDepthBelowSurface(p), 0.0);
}

// Downwelling: the light that reaches a point under the water has crossed water on its way down, and loses the same
// share per meter as the view path does on its way up (exp(-extinction x path)). The sun's path is the depth over
// the cosine of its angle under the water (bent toward vertical at the surface, Snell); the sky light comes from
// every direction and is taken straight down. Dry (depth 0) or off: 1, lit as in air.
void UnderwaterLightFade(float depth, out float3 sunFade, out float3 skyFade)
{
    sunFade = 1.0;
    skyFade = 1.0;
    if (g_UnderwaterLight.w > 0.0 && depth > 0.0)
    {
        sunFade = exp(-g_UnderwaterLight.xyz * (depth / g_UnderwaterLight.w));
        skyFade = exp(-g_UnderwaterLight.xyz * depth);
    }
}

// Caustics: the rippled surface works as many small lenses. Where a ripple bulges it gathers the sunlight into a bright
// line on the floor, where it dips it spreads the light thinner. Light is moved, not made, so the pattern scales the
// sunlight around 1: x (pattern / its average). Not computed from the actual waves (a baked tileable pattern, bent and
// drifted in two scales so its shape keeps changing); its place on the floor is where the bent sun ray came through
// the surface, so it sticks to the floor while the camera moves.
// Depth: fades in over the first 0.1 m (the lenses have not gathered the light yet), then blurs and loses its contrast
// toward the fade-out depth (the networks of large and small ripples overlap and wash out). The sunlight itself fading
// with depth is the downwelling term, and the water color laid over it is the water pass: both separate.
// Slope (dh/dx, dh/dz) and curvature (d2h/dx2 + d2h/dz2) of the big (Gerstner) waves at xz, with the same distance
// and patch-edge fades as the water mesh. The height part of each wave only (A sin): the Gerstner sideways motion
// sharpens the crests but barely moves where the light gathers.
void WaveSlopeCurvature(float2 xz, out float2 slope, out float curvature)
{
    slope = 0.0;
    curvature = 0.0;
    float viewDistance = length(xz - g_WaterCamera.xz);
    float edgeFade = WaterEdgeFade(xz, float4(g_WaterTimeArea.yz, 0.0, 0.0));
    [unroll]
    for (int i = 0; i < WAVE_COUNT; ++i)
    {
        WaveParams wave = g_WaterWaves[i];
        if (wave.amplitude <= 0.0 || wave.wavelength <= 0.0)
        {
            continue;
        }
        float amplitude = wave.amplitude * WaveVertexFade(wave.wavelength, viewDistance) * edgeFade;
        float k = 6.2831853 / wave.wavelength;
        float phase = dot(wave.direction, xz) * k - k * wave.speed * g_WaterTimeArea.x;
        slope += wave.direction * (amplitude * k * cos(phase));
        curvature -= amplitude * k * k * sin(phase) * dot(wave.direction, wave.direction);
    }
}

// waveBand: the big waves' own light bands (1 = none), for the debug view.
float CausticFactor(float3 p, float depth, out float waveBand)
{
    waveBand = 1.0;
    if (g_Caustics.x <= 0.0 || depth <= 0.0)
    {
        return 1.0;
    }
    float2 surfaceXZ = p.xz + g_Caustics2.zw * depth; // back along the bent sun ray to the surface (0 = straight up)
    float fadeDepth = max(g_Caustics.w, 0.6);
    float contrast = smoothstep(0.0, 0.1, depth) * (1.0 - smoothstep(0.5, fadeDepth, depth));
    // Following the big waves, as a lens of focal power = curvature: a surface tilted by slope s sends the light
    // sideways by depth x (1 - 1/n) x s on its way down (Snell, small angles; the same shift as the normal-offset
    // refraction). So the light at this point came through the surface that much further back: the ripple network
    // sways with the passing waves. And a crest (curvature < 0) squeezes the light into a narrower strip, a trough
    // spreads it: brightness x (1 - lens x curvature), the linear part of 1 / (area ratio), which keeps the average
    // at 1 (the curvature of a wave averages to 0). Bands grow with depth until the light would cross (the focus).
    if (g_Caustics3.x > 0.0 || g_Caustics3.y > 0.0)
    {
        float2 slope;
        float curvature;
        WaveSlopeCurvature(surfaceXZ, slope, curvature);
        float lens = depth * g_Caustics2.y;
        surfaceXZ -= g_Caustics3.x * lens * slope;
        waveBand = max(1.0 - g_Caustics3.y * contrast * lens * curvature, 0.0);
    }
    float2 uv = surfaceXZ * g_Caustics.y;
    float t = g_Caustics.z;
    // Two scales of slow bending: the cells stretch, merge and split instead of only sliding.
    float2 warp = 0.09 * float2(sin(uv.y * 2.1 + t * 0.9) + sin(uv.x * 0.9 + uv.y * 0.7 - t * 0.5),
                                cos(uv.x * 1.9 - t * 0.8) + cos(uv.x * 0.6 - uv.y * 0.8 + t * 0.6))
                + 0.035 * float2(sin(uv.x * 1.1 + uv.y * 5.3 + t * 1.7), cos(uv.x * 4.7 + uv.y * 0.9 - t * 1.5));
    uv += warp + float2(0.031, 0.017) * t;
    float deep = saturate(depth / fadeDepth);
    float pattern = g_CausticMap.SampleBias(g_WrapSampler, uv, g_Caustics2.x + 2.5 * deep).r;
    float average = g_CausticMap.SampleLevel(g_WrapSampler, uv, 16.0).r; // the 1 x 1 mip: the whole pattern's mean
    return max((1.0 + g_Caustics.x * contrast * (pattern / max(average, 1e-3) - 1.0)) * waveBand, 0.0);
}

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normalWS : NORMAL;
    float3 worldPos : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.position, 1.0), g_World);
    output.position = mul(mul(worldPos, g_View), g_Projection);
    output.normalWS = normalize(mul(input.normal, (float3x3)g_WorldInvTranspose));
    output.worldPos = worldPos.xyz;
    return output;
}

// The two world axes lying in the surface (top faces use XZ, walls use ZY or XY).
float2 SurfacePlane(float3 p, float3 n)
{
    float3 a = abs(n);
    if (a.y >= a.x && a.y >= a.z) return p.xz;
    if (a.x >= a.z) return p.zy;
    return p.xy;
}

// 0 / 1 alternating cells of the given size.
float CheckerCell(float2 coord, float cellSize)
{
    float2 cell = floor(coord / cellSize);
    return frac((cell.x + cell.y) * 0.5) * 2.0;
}

// 1 m cells, each split into 25 cm cells that differ only slightly.
float CheckerPattern(float2 coord)
{
    return lerp(0.55, 1.0, CheckerCell(coord, 1.0)) * lerp(0.92, 1.0, CheckerCell(coord, 0.25));
}

// Height bands on walls: 50 cm bands alternating, a dark line at every whole meter.
float RulerPattern(float3 p, float3 n)
{
    if (abs(n.y) > 0.5) return CheckerPattern(p.xz);
    float band = lerp(0.62, 1.0, frac(floor(p.y / 0.5) * 0.5) * 2.0);
    float distanceToMeter = abs(frac(p.y + 0.5) - 0.5);
    float meterLine = 1.0 - smoothstep(0.012, 0.025, distanceToMeter);
    return band * lerp(1.0, 0.25, meterLine);
}

float2 Hash22(float2 p)
{
    float3 q = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    q += dot(q, q.yzx + 33.33);
    return frac((q.xx + q.yz) * q.zy);
}

// Rounded stones from a Voronoi pattern: F1 = distance to the nearest stone center, F2 = to the second.
// The gap between stones is where F2 - F1 is small (https://iquilezles.org/articles/voronoilines/).
// Returns brightness; stoneTint varies per stone.
float PebblePattern(float2 coord, out float3 stoneTint)
{
    const float stoneSize = 0.12; // m
    float2 cellCoord = coord / stoneSize;
    float2 baseCell = floor(cellCoord);
    float f1 = 8.0;
    float f2 = 8.0;
    float2 nearestCell = baseCell;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 cell = baseCell + float2(x, y);
            float2 center = cell + 0.15 + 0.7 * Hash22(cell);
            float d = length(cellCoord - center);
            if (d < f1) { f2 = f1; f1 = d; nearestCell = cell; }
            else if (d < f2) { f2 = d; }
        }
    }
    float2 rnd = Hash22(nearestCell + 17.0);
    stoneTint = lerp(float3(0.85, 0.82, 0.78), float3(1.08, 1.05, 1.0), rnd.x);
    float gap = smoothstep(0.02, 0.12, f2 - f1);   // dark seams between stones
    float dome = 1.0 - 0.35 * saturate(f1 * 1.1); // stones a little brighter in the middle
    float stones = lerp(0.25, 1.0, gap) * dome * lerp(0.75, 1.1, rnd.y);
    // Far away a stone gets smaller than a pixel and the pattern shimmers: fade to its average instead.
    float stonesPerPixel = max(fwidth(cellCoord.x), fwidth(cellCoord.y));
    float farFade = smoothstep(0.25, 0.6, stonesPerPixel);
    stoneTint = lerp(stoneTint, float3(0.96, 0.93, 0.89), farFade);
    return lerp(stones, 0.66, farFade);
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 n = normalize(input.normalWS);
    float3 p = input.worldPos;
    int pattern = (int)g_MaterialParams.x;
    int debugMode = (int)g_MaterialParams.y;

    float3 albedo = g_BaseColor.rgb;
    if (pattern == 1)
    {
        albedo *= CheckerPattern(SurfacePlane(p, n));
    }
    else if (pattern == 2)
    {
        albedo *= RulerPattern(p, n);
    }
    else if (pattern == 3)
    {
        float3 tint;
        albedo *= PebblePattern(SurfacePlane(p, n), tint) * tint;
    }

    float3 lightDirWS = normalize(-g_LightDirection.xyz);
    float diffuse = saturate(dot(n, lightDirWS));
    // sun-shadow: the sun term alone is shadowed; the sky light (ambient) still reaches the shadow.
    float shadow = SunShadow(g_ShadowMap, g_ShadowSampler, g_Shadow, p, n, lightDirWS);
    // Under the water (depth looked up only when downwelling or caustics need it; 0 = dry).
    float depth = (g_UnderwaterLight.w > 0.0 || g_Caustics.x > 0.0 || debugMode == 6 || debugMode == 7) ? UnderwaterDepth(p) : 0.0;
    // Downwelling: under the water both lights arrive weakened by the water above (1 = off / dry).
    float3 sunFade;
    float3 skyFade;
    UnderwaterLightFade(depth, sunFade, skyFade);
    // Caustics: the sunlight rearranged by the ripples (1 = off / dry). Part of the sun term, so the shadow and the
    // downwelling fade apply to it too.
    float waveBand;
    float caustic = CausticFactor(p, depth, waveBand);
    float3 lighting = g_AmbientColor.rgb * g_AmbientColor.a * skyFade
                    + g_LightColor.rgb * g_LightColor.a * sunFade * (diffuse * shadow * caustic);

    // 6 = caustic factor: mid gray = 1 (no change), brighter = light gathered, darker = light spread thinner
    // (gray x factor, white from 2). Dry, shadowed or turned away from the sun: the plain gray.
    // 7 = the big waves' light bands alone, same scale.
    if (debugMode == 6 || debugMode == 7)
    {
        float factor = debugMode == 6 ? caustic : waveBand;
        float shown = dot(n, lightDirWS) > 0.0 ? lerp(1.0, factor, shadow) : 1.0;
        return float4(SrgbToLinear(saturate(0.5 * shown).xxx), 1.0);
    }

    // 5 = sun share (white = sun reaches the point, black = shadow). Faces turned away from the sun get no sun anyway
    // and are drawn mid gray, so only the shadow map's answer shows.
    if (debugMode == 5)
    {
        return float4(SrgbToLinear(dot(n, lightDirWS) > 0.0 ? shadow.xxx : float3(0.5, 0.5, 0.5)), 1.0);
    }

    // 3 = behind the water's numeric views: nothing measured here (only the water writes values).
    if (debugMode == 3)
    {
        return float4(SrgbToLinear(float3(0.0, 0.0, kMarkerAboveWater)), 1.0);
    }

    // 4 = world normal as color (normal x 0.5 + 0.5): the answer key for the normal the water reconstructs from
    // the depth buffer (contact-foam; water debug 22 uses the same colors).
    if (debugMode == 4)
    {
        return float4(SrgbToLinear(n * 0.5 + 0.5), 1.0);
    }

    // 2 = water depth as numbers (EncodeMeters): the answer key the depth-buffer reconstruction is compared to.
    if (debugMode == 2)
    {
        float depth = WaterDepthBelowSurface(p);
        float3 encoded = depth > 0.0 ? EncodeMeters(depth, kMarkerMeasured) : float3(0.0, 0.0, kMarkerAboveWater);
        return float4(SrgbToLinear(encoded), 1.0);
    }

    if (debugMode == 1)
    {
        // Depth below the water surface (y = 0). Values are display colors (the tonemap pass is skipped
        // for debug views), so they go out through SrgbToLinear like the water debug views.
        float depth = WaterDepthBelowSurface(p);
        float shade = 0.55 + 0.45 * diffuse; // keep the shapes readable
        float luminance = dot(albedo, float3(0.2126, 0.7152, 0.0722));
        float3 color = depth > 0.0
            ? DepthBandColor(depth) * shade * lerp(0.75, 1.0, saturate(luminance * 2.0))
            : float3(0.35, 0.35, 0.35) * shade;

        // 1 m contour lines on sloped / vertical faces (a flat face sits on one depth and would fill in).
        float depthWidth = fwidth(depth);
        if (depthWidth > 1e-5 && depth > 0.0)
        {
            float contour = 1.0 - saturate(abs(frac(depth + 0.5) - 0.5) / depthWidth);
            color = lerp(color, float3(0.05, 0.05, 0.05), contour * 0.8);
        }
        // Water line (the wave surface; y = 0 without waves), ~2 px wide.
        float waterLine = 1.0 - saturate(abs(depth) / max(depthWidth * 1.5, 1e-5));
        color = lerp(color, float3(1.0, 0.1, 0.1), waterLine);
        return float4(SrgbToLinear(saturate(color)), 1.0);
    }

    return float4(albedo * lighting, 1.0);
}

// Two targets (normal-gbuffer): the color, and the world normal for the water pass to read instead of estimating it
// from the depth buffer. Used only while the normal target is bound ("stored scene normal" source); otherwise
// PSMain draws the color alone.
struct PSOutput
{
    float4 color  : SV_Target0;
    float4 normal : SV_Target1; // world normal x 0.5 + 0.5, a = 1: a surface wrote it (cleared to 0)
};

PSOutput PSMainWithNormal(PSInput input)
{
    PSOutput output;
    output.color = PSMain(input);
    output.normal = float4(normalize(input.normalWS) * 0.5 + 0.5, 1.0);
    return output;
}
