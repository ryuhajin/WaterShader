#include "Color.hlsli"

// Final pass: linear HDR scene -> exposure -> tone curve -> sRGB-encoded back buffer.
// The encode is done here instead of through an *_SRGB render target view because ImGui draws
// onto the same back buffer with sRGB colors and must not be encoded a second time.

cbuffer TonemapCB : register(b0)
{
    float g_Exposure;  // linear multiplier (2^EV)
    uint  g_Operator;  // 0 = none (clamp), 1 = Reinhard, 2 = ACES per channel, 3 = ACES hue-preserving
    float2 g_Padding;
};

Texture2D<float4> g_HdrScene : register(t0);

struct VSOutput
{
    float4 position : SV_POSITION;
};

// One triangle covering the screen: ids 0,1,2 -> (-1,1), (3,1), (-1,-3).
VSOutput VSMain(uint id : SV_VertexID)
{
    VSOutput output;
    float2 uv = float2((id << 1) & 2, id & 2);
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float3 TonemapReinhard(float3 x)
{
    return x / (1.0 + x);
}

// Krzysztof Narkowicz's fit of the ACES filmic curve.
float3 TonemapAces(float3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Hue-preserving ACES with white highlights.
//  - Per-channel ACES pushes bright saturated colors toward white: it bleaches skies that were already
//    tone-mapped once (LDR photos) - measured sunset sky saturation 0.33 -> 0.21.
//  - Luminance-only ACES (curve on luminance, RGB scaled by the ratio) keeps the sky (0.29) but leaves
//    a 20x-bright glint flat orange with clipped channels instead of burning to white.
//  So: luminance mapping for in-range colors, blended into per-channel for real HDR values.
float3 TonemapAcesHuePreserving(float3 x)
{
    float luminance = dot(x, float3(0.2126, 0.7152, 0.0722));
    float3 luminanceMapped = x * (TonemapAces(luminance.xxx).x / max(luminance, 1e-5));
    float3 channelMapped = TonemapAces(x);
    float highlight = smoothstep(1.0, 4.0, max(x.r, max(x.g, x.b)));
    return lerp(saturate(luminanceMapped), channelMapped, highlight);
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    // Same size as the back buffer, so read the texel directly (no sampler / filtering).
    float3 color = g_HdrScene.Load(int3(input.position.xy, 0)).rgb * g_Exposure;

    if (g_Operator == 1)      { color = TonemapReinhard(color); }
    else if (g_Operator == 2) { color = TonemapAces(color); }
    else if (g_Operator == 3) { color = TonemapAcesHuePreserving(color); }

    return float4(LinearToSrgb(color), 1.0); // LinearToSrgb clamps to [0,1] (operator 0 = clamp)
}
