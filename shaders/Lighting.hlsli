#ifndef WS_LIGHTING_HLSLI
#define WS_LIGHTING_HLSLI

float Lambert(float3 N, float3 L)
{
    return saturate(dot(N, L));
}

float FresnelSchlick(float NdotV, float power)
{
    return pow(1.0f - NdotV, power);
}

#endif // WS_LIGHTING_HLSLI
