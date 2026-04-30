cbuffer PerFrameCB : register(b0)
{
    row_major float4x4 g_World;
    row_major float4x4 g_View;
    row_major float4x4 g_Projection;
    float4 g_LightDirection;
    float4 g_LightColor;
    float4 g_TintColor;
    float4 g_CameraPositionWS;
    float  g_Time;
    float  g_ReflectionStrength;
    float2 _padding;
};

TextureCube  g_Skybox  : register(t0);
SamplerState g_Sampler : register(s0);

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

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.position, 1.0f), g_World);
    output.position = mul(mul(worldPos, g_View), g_Projection);
    output.normalWS = normalize(mul(input.normal, (float3x3)g_World));
    output.worldPos = worldPos.xyz;
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.normalWS);
    float3 L = normalize(-g_LightDirection.xyz);
    float  NdotL = saturate(dot(N, L));
    float3 lambert = g_TintColor.rgb * g_LightColor.rgb * g_LightColor.a * NdotL;

    float3 V = normalize(g_CameraPositionWS.xyz - input.worldPos);
    float3 R = reflect(-V, N);
    float3 envColor = g_Skybox.Sample(g_Sampler, R).rgb;

    float3 lit = lambert + envColor * g_ReflectionStrength;
    return float4(lit, 1.0f);
}
