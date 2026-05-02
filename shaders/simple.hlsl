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
};

TextureCube  g_Skybox        : register(t0);
SamplerState g_Sampler       : register(s0);
Texture2D    g_NormalMap     : register(t1);
SamplerState g_NormalSampler : register(s1);

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
    // Two scrolling normal map samples blended in tangent space.
    float2 uv1 = input.uv * g_NormalScale + g_NormalScroll.xy * g_Time;
    float2 uv2 = input.uv * g_NormalScale + g_NormalScroll.zw * g_Time;
    float3 n1 = g_NormalMap.Sample(g_NormalSampler, uv1).xyz * 2.0f - 1.0f;
    float3 n2 = g_NormalMap.Sample(g_NormalSampler, uv2).xyz * 2.0f - 1.0f;
    float3 nTan = normalize(n1 + n2);

    // Plane TBN: tangent = world X, bitangent = world Z, normal = vertex normal.
    float3 N0 = normalize(input.normalWS);
    float3 T  = normalize(mul(float3(1, 0, 0), (float3x3)g_World));
    float3 B  = normalize(mul(float3(0, 0, 1), (float3x3)g_World));
    float3 N  = normalize(nTan.x * T + nTan.y * B + nTan.z * N0);

    float3 V = normalize(g_CameraPositionWS.xyz - input.worldPos);
    float  NdotV = saturate(dot(N, V));

    float3 L = normalize(-g_LightDirection.xyz);
    float  NdotL = saturate(dot(N, L));

    float3 waterCol = lerp(g_DeepColor.rgb, g_ShallowColor.rgb, NdotV);
    float3 litWater = waterCol * g_LightColor.rgb * g_LightColor.a * NdotL;

    float  fresnel = pow(1.0f - NdotV, g_FresnelPower);

    float3 R = reflect(-V, N);
    float3 envColor = g_Skybox.Sample(g_Sampler, R).rgb;

    float3 lit = lerp(litWater, envColor, saturate(fresnel * g_ReflectionStrength));
    return float4(lit * g_TintColor.rgb, 1.0f);
}
