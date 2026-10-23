#include "Color.hlsli"

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
    float4 g_MaterialParams; // x = pattern (0 solid, 1 checker, 2 ruler, 3 pebbles), y = debug mode (0 render, 1 depth)
};

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

// Depth bands for the debug view, picked so the Water Lab pads (0.2 / 1 / 3 / 6 m) and the 8 m floor
// each land in their own color.
float3 DepthBandColor(float depth)
{
    if (depth < 0.5) return float3(0.30, 0.95, 1.00); // ~0.2 m
    if (depth < 2.0) return float3(0.35, 0.90, 0.35); // ~1 m
    if (depth < 4.5) return float3(1.00, 0.90, 0.25); // ~3 m
    if (depth < 7.0) return float3(1.00, 0.55, 0.20); // ~6 m
    return float3(0.75, 0.40, 0.95);                  // deeper (lab floor 8 m)
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
    float3 lighting = g_AmbientColor.rgb * g_AmbientColor.a + g_LightColor.rgb * g_LightColor.a * diffuse;

    if (debugMode == 1)
    {
        // Depth below the water surface (y = 0). Values are display colors (the tonemap pass is skipped
        // for debug views), so they go out through SrgbToLinear like the water debug views.
        float depth = -p.y;
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
        // Water line at y = 0, ~2 px wide.
        float waterLine = 1.0 - saturate(abs(p.y) / max(depthWidth * 1.5, 1e-5));
        color = lerp(color, float3(1.0, 0.1, 0.1), waterLine);
        return float4(SrgbToLinear(saturate(color)), 1.0);
    }

    return float4(albedo * lighting, 1.0);
}
