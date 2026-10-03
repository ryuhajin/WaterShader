#include "Common.hlsli"
#include "Waves.hlsli"

// Gerstner wave: the vertex moves in a circle instead of only up/down.
// steepness(Q) = 0 is the old sine wave; larger Q pulls vertices toward the crest, so crests get
// sharp and troughs get wide/flat like real water.
//   P.xz += Q·A·D·cos(θ),  P.y += A·sin(θ),  θ = k(D·xz) − ωt,  k = 2π/λ, ω = k·speed
// Normal (GPU Gems 1, ch.1): N = (−Σ D.x·kA·cos θ, 1 − Σ Q·kA·sin θ, −Σ D.y·kA·cos θ)
void AccumulateGerstnerWave(
    WaveParams wave,
    float2 positionXZ, float viewDistance,
    inout float3 offset, inout float3 normalSum)
{
    if (wave.amplitude <= 0.0f || wave.wavelength <= 0.0f) return;

    // Distance LOD (Waves.hlsli): past it the mesh is flat and PixelShader adds the slopes back.
    float fade = WaveVertexFade(wave.wavelength, viewDistance);
    if (fade <= 0.0) return;
    wave.amplitude *= fade;
    float waveNumber = 6.2831853 / wave.wavelength;
    float phaseSpeed = waveNumber * wave.speed;
    float phase = dot(wave.direction, positionXZ) * waveNumber - phaseSpeed * g_WaterParams.x;
    float sinPhase = sin(phase);
    float cosPhase = cos(phase);

    // Q/(k·A·count) keeps the sum of all waves from looping over itself even at steepness 1.
    float q = wave.steepness * fade / (waveNumber * wave.amplitude * WAVE_COUNT);
    float kA = waveNumber * wave.amplitude;

    offset.xz += q * wave.amplitude * wave.direction * cosPhase;
    offset.y  += wave.amplitude * sinPhase;

    normalSum.xz -= wave.direction * kA * cosPhase;
    normalSum.y  -= q * kA * sinPhase;
}

float3 GerstnerDisplace(float3 localPos, out float3 normalOut)
{
    float3 worldPos = mul(float4(localPos, 1.0), g_World).xyz;
    float viewDistance = length(worldPos.xz - g_CameraPositionWS.xz);

    float3 offset = 0.0;
    float3 normalSum = float3(0.0, 1.0, 0.0);

    [unroll]
    for (int i = 0; i < WAVE_COUNT; ++i)
    {
        AccumulateGerstnerWave(g_Waves[i], localPos.xz, viewDistance, offset, normalSum);
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
