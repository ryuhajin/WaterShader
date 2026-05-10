#ifndef WS_COMMON_HLSLI
#define WS_COMMON_HLSLI

struct WaveParams
{
    float2 direction;
    float  amplitude;
    float  wavelength;
    float  speed;
    float3 padding;
};

cbuffer PerFrameCB : register(b0)
{
    row_major float4x4 g_World;
    row_major float4x4 g_View;
    row_major float4x4 g_Projection;

    float4 g_LightDirection;   // xyz = direction, w unused
    float4 g_LightColor;       // rgb = color, a = intensity
    float4 g_AmbientColor;     // rgb = color, a = intensity
    float4 g_CameraPositionWS;

    float4 g_FacingColor;      // high viewFacingAmount
    float4 g_GrazingColor;     // low viewFacingAmount
    float4 g_NormalScroll;     // xy = uv1 scroll, zw = uv2 scroll

    float4 g_WaterParams;      // x=time, y=reflectionStrength, z=fresnelPower, w=normalScale
    float4 g_SpecularParams;   // x=strength, y=sharpness, z/w unused

    WaveParams g_Waves[2];
    float4 g_DebugParams;      // x = debug mode: 0 render, 1 normal map, 2 world N, 3 UV, 4 front/back face
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
    float2 uv       : TEXCOORD1;
};

#endif // WS_COMMON_HLSLI


