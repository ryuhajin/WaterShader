#include "Color.hlsli"

// Final pass: linear HDR scene -> exposure -> tone curve -> sRGB-encoded back buffer.
// The encode is done here instead of through an *_SRGB render target view because ImGui draws
// onto the same back buffer with sRGB colors and must not be encoded a second time.

cbuffer TonemapCB : register(b0)
{
    float g_Exposure;  // linear multiplier (2^EV)
    uint  g_Operator;  // 0 = none (clamp), 1 = Reinhard, 2 = ACES (Narkowicz fit)
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

float4 PSMain(VSOutput input) : SV_TARGET
{
    // Same size as the back buffer, so read the texel directly (no sampler / filtering).
    float3 color = g_HdrScene.Load(int3(input.position.xy, 0)).rgb * g_Exposure;

    if (g_Operator == 1)      { color = TonemapReinhard(color); }
    else if (g_Operator == 2) { color = TonemapAces(color); }

    return float4(LinearToSrgb(color), 1.0); // LinearToSrgb clamps to [0,1] (operator 0 = clamp)
}
