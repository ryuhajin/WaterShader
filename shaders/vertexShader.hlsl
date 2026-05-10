#include "Common.hlsli"

// 파도 하나가 현재 정점 위치에 만드는 높이와 기울기를 계산해서 누적
void AccumulateSineWave(
    float2 waveDir, float amplitude, float wavelength, float speed,
    float2 positionXZ,
    inout float heightSum, inout float slopeX, inout float slopeZ)
{
    if (amplitude <= 0.0f || wavelength <= 0.0f) return;
    float waveNumber = 6.2831853 / wavelength;
    float phaseSpeed = waveNumber * speed;
    float phase = dot(waveDir, positionXZ) * waveNumber - phaseSpeed * g_WaterParams.x;
    float sinPhase = sin(phase);
    float cosPhase = cos(phase);
    heightSum += amplitude * sinPhase;
    slopeX += waveDir.x * waveNumber * amplitude * cosPhase;
    slopeZ += waveDir.y * waveNumber * amplitude * cosPhase;
}

float3 SineDisplace(float3 localPos, out float3 normalOut)
{
    float waveHeight = 0.0;
    float slopeX = 0.0;
    float slopeZ = 0.0;

    AccumulateSineWave(g_Waves[0].direction, g_Waves[0].amplitude, g_Waves[0].wavelength, g_Waves[0].speed, localPos.xz, waveHeight, slopeX, slopeZ);
    AccumulateSineWave(g_Waves[1].direction, g_Waves[1].amplitude, g_Waves[1].wavelength, g_Waves[1].speed, localPos.xz, waveHeight, slopeX, slopeZ);

    normalOut = normalize(float3(-slopeX, 1.0, -slopeZ));
    return float3(localPos.x, localPos.y + waveHeight, localPos.z);
}

PSInput VSMain(VSInput input)
{
    PSInput output;

    float3 waveNormal;
    float3 displacedLocalPos = SineDisplace(input.position, waveNormal);
    float4 worldPos = mul(float4(displacedLocalPos, 1.0), g_World);

    output.position = mul(mul(worldPos, g_View), g_Projection);
    output.normalWS = normalize(mul(waveNormal, (float3x3)g_World));
    output.worldPos = worldPos.xyz;
    output.uv = input.uv;
    return output;
}
