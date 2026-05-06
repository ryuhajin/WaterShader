#include "Common.hlsli"

void AccumulateSineWave(
    float2 dir, float amp, float wavelen, float speed,
    float2 xz,
    inout float h, inout float dx, inout float dz)
{
    if (amp <= 0.0f || wavelen <= 0.0f) return;
    float k     = 6.2831853f / wavelen;
    float omega = k * speed;
    float phase = dot(dir, xz) * k - omega * g_Time;
    float s = sin(phase);
    float c = cos(phase);
    h  += amp * s;
    dx += dir.x * k * amp * c;
    dz += dir.y * k * amp * c;
}

float3 SineDisplace(float3 p, out float3 normalOut)
{
    float h = 0.0f;
    float dx = 0.0f;
    float dz = 0.0f;

    AccumulateSineWave(g_Wave0_Dir, g_Wave0_Amp, g_Wave0_Wavelen, g_Wave0_Speed, p.xz, h, dx, dz);
    AccumulateSineWave(g_Wave1_Dir, g_Wave1_Amp, g_Wave1_Wavelen, g_Wave1_Speed, p.xz, h, dx, dz);

    normalOut = normalize(float3(-dx, 1.0f, -dz));
    return float3(p.x, p.y + h, p.z);
}

PSInput VSMain(VSInput input)
{
    PSInput output;
    float3 waveNormal;
    float3 displaced = SineDisplace(input.position, waveNormal);
    float4 worldPos = mul(float4(displaced, 1.0f), g_World);
    output.position = mul(mul(worldPos, g_View), g_Projection);
    output.normalWS = normalize(mul(waveNormal, (float3x3)g_World));
    output.worldPos = worldPos.xyz;
    output.uv = input.uv;
    return output;
}
