#ifndef WS_COMMON_HLSLI
#define WS_COMMON_HLSLI

cbuffer PerFrameCB : register(b0)
{
    row_major float4x4 g_World;
    row_major float4x4 g_View;
    row_major float4x4 g_Projection;
    float4 g_LightDirection;
    float4 g_LightColor;
    float4 g_TintColor;
    float4 g_CameraPositionWS;
    float4 g_ShallowColor;
    float4 g_DeepColor;
    float4 g_NormalScroll;        // xy = scroll1, zw = scroll2
    float  g_Time;
    float  g_ReflectionStrength;
    float  g_FresnelPower;
    float  g_NormalScale;
    // Wave 0
    float2 g_Wave0_Dir;
    float  g_Wave0_Amp;
    float  g_Wave0_Wavelen;
    float  g_Wave0_Speed;
    float3 _wave0_pad;
    // Wave 1
    float2 g_Wave1_Dir;
    float  g_Wave1_Amp;
    float  g_Wave1_Wavelen;
    float  g_Wave1_Speed;
    float3 _wave1_pad;
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
