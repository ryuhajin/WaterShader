#include "Common.hlsli"
#include "Waves.hlsli"

// Gerstner waves (AccumulateGerstnerWave in Waves.hlsli, shared with the pixel shaders).
float3 GerstnerDisplace(float3 localPos, out float3 normalOut)
{
    float3 worldPos = mul(float4(localPos, 1.0), g_World).xyz;
    float viewDistance = length(worldPos.xz - g_CameraPositionWS.xz);
#if SCENE_INPUTS
    // Test scenes: the waves fade out at the edge of the finite water patch (wave-occlusion).
    float edgeFade = WaterEdgeFade(worldPos.xz, g_WaterArea);
#else
    float edgeFade = 1.0;
#endif

    float3 offset = 0.0;
    float3 normalSum = float3(0.0, 1.0, 0.0);

    [unroll]
    for (int i = 0; i < WAVE_COUNT; ++i)
    {
        AccumulateGerstnerWave(g_Waves[i], localPos.xz, viewDistance, g_WaterParams.x, edgeFade, offset, normalSum);
    }

    normalOut = normalize(normalSum);
    return localPos + offset;
}

PSInput VSMain(VSInput input)
{
    PSInput output;

    float3 waveNormal;
    float3 displacedLocalPos = GerstnerDisplace(input.position, waveNormal);
    float4 worldPos = mul(float4(displacedLocalPos, 1.0), g_World);

    output.position = mul(mul(worldPos, g_View), g_Projection);
    output.normalWS = normalize(mul(waveNormal, (float3x3)g_World));
    output.worldPos = worldPos.xyz;
    output.uv = input.uv;
    output.restPosLocal = input.position;
    return output;
}
