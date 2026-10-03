#ifndef WS_CUBEMAP_HLSLI
#define WS_CUBEMAP_HLSLI

// Reserves t0/s0. Do not redeclare these registers in any shader that includes this file.
TextureCube  g_Skybox  : register(t0);
SamplerState g_Sampler : register(s0);

float3 SampleEnv(float3 R)
{
    return g_Skybox.Sample(g_Sampler, R).rgb;
}

// Mip the hardware would pick for R (uses screen derivatives: call outside data-dependent branches).
float EnvAutoLod(float3 R)
{
    return g_Skybox.CalculateLevelOfDetail(g_Sampler, R);
}

// Sky blurred over a cone of about blurAngle radians: pick the mip whose texel covers that angle
// (one face spans pi/2 over width texels), never sharper than the hardware mip.
float3 SampleEnvBlurred(float3 R, float blurAngle, float autoLod)
{
    uint width, height, mipCount;
    g_Skybox.GetDimensions(0, width, height, mipCount);
    float texelAngle = 1.5707963 / width;
    float lod = max(autoLod, log2(max(blurAngle / texelAngle, 1.0)));
    return g_Skybox.SampleLevel(g_Sampler, R, min(lod, mipCount - 1.0)).rgb;
}

#endif // WS_CUBEMAP_HLSLI
