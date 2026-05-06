#include "Common.hlsli"
#include "Lighting.hlsli"
#include "Cubemap.hlsli"

// Water-only resources. Cubemap.hlsli already reserves t0/s0; do not reuse here.
Texture2D    g_NormalMap     : register(t1);
SamplerState g_NormalSampler : register(s1);

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
    float  NdotL = Lambert(N, L);

    float3 waterCol = lerp(g_DeepColor.rgb, g_ShallowColor.rgb, NdotV);
    float3 litWater = waterCol * g_LightColor.rgb * g_LightColor.a * NdotL;

    float  fresnel  = FresnelSchlick(NdotV, g_FresnelPower);

    float3 R        = reflect(-V, N);
    float3 envColor = SampleEnv(R);

    float3 lit = lerp(litWater, envColor, saturate(fresnel * g_ReflectionStrength));
    return float4(lit * g_TintColor.rgb, 1.0f);
}
