#ifndef WS_CUBEMAP_HLSLI
#define WS_CUBEMAP_HLSLI

// Reserves t0/s0. Do not redeclare these registers in any shader that includes this file.
TextureCube  g_Skybox  : register(t0);
SamplerState g_Sampler : register(s0);

float3 SampleEnv(float3 R)
{
    return g_Skybox.Sample(g_Sampler, R).rgb;
}

#endif // WS_CUBEMAP_HLSLI
