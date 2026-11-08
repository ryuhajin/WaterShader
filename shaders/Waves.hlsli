#ifndef WS_WAVES_HLSLI
#define WS_WAVES_HLSLI

#define WAVE_COUNT 4

struct WaveParams
{
    float2 direction;
    float  amplitude;
    float  wavelength;
    float  speed;
    float  steepness;  // Gerstner Q (0 = plain sine)
    float2 padding;
};

// Distance LOD of the mesh waves: the ocean grid gets coarser with distance, so each wave fades out
// before the vertex spacing is too wide to sample it (short waves fade first). The vertex shader
// scales the wave by this; the pixel shader carries the remaining (1 - fade) as lighting only.
float WaveVertexFade(float wavelength, float viewDistance)
{
    return 1.0 - smoothstep(wavelength * 8.0, wavelength * 14.0, viewDistance);
}

// Gerstner wave: the vertex moves in a circle instead of only up/down.
// steepness(Q) = 0 is the old sine wave; larger Q pulls vertices toward the crest, so crests get
// sharp and troughs get wide/flat like real water.
//   P.xz += Q·A·D·cos(θ),  P.y += A·sin(θ),  θ = k(D·xz) − ωt,  k = 2π/λ, ω = k·speed
// Normal (GPU Gems 1, ch.1): N = (−Σ D.x·kA·cos θ, 1 − Σ Q·kA·sin θ, −Σ D.y·kA·cos θ)
// Shared by the water vertex shader and the pixel shaders that need the water height somewhere else
// (wave-occlusion), so both always see the same surface. extraFade scales the wave like the distance fade
// (1 = none; the test scenes fade the waves out at the edge of the water patch).
void AccumulateGerstnerWave(
    WaveParams wave,
    float2 positionXZ, float viewDistance, float time, float extraFade,
    inout float3 offset, inout float3 normalSum)
{
    if (wave.amplitude <= 0.0f || wave.wavelength <= 0.0f) return;

    // Distance LOD (Waves.hlsli): past it the mesh is flat and PixelShader adds the slopes back.
    float fade = WaveVertexFade(wave.wavelength, viewDistance) * extraFade;
    if (fade <= 0.0) return;
    wave.amplitude *= fade;
    float waveNumber = 6.2831853 / wave.wavelength;
    float phaseSpeed = waveNumber * wave.speed;
    float phase = dot(wave.direction, positionXZ) * waveNumber - phaseSpeed * time;
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

// Waves die out over the last area.y meters of a square water patch of half size area.x (0 = no patch), so the
// sideways motion never pulls the patch edge inward and opens a gap (wave-occlusion).
float WaterEdgeFade(float2 positionXZ, float4 area)
{
    if (area.x <= 0.0) return 1.0;
    float inside = area.x - max(abs(positionXZ.x), abs(positionXZ.y));
    return saturate(inside / max(area.y, 1e-3));
}

// Where the water surface point that rests at restXZ moves to (offset from (restXZ.x, 0, restXZ.y)), with the
// same fades as the vertex shader. The test scenes draw the water with the identity world matrix at y = 0.
float3 WaterSurfaceOffset(WaveParams waves[WAVE_COUNT], float2 restXZ, float3 cameraPos, float time, float4 area)
{
    float viewDistance = length(restXZ - cameraPos.xz);
    float edgeFade = WaterEdgeFade(restXZ, area);
    float3 offset = 0.0;
    float3 normalSum = float3(0.0, 1.0, 0.0);
    [unroll]
    for (int i = 0; i < WAVE_COUNT; ++i)
    {
        AccumulateGerstnerWave(waves[i], restXZ, viewDistance, time, edgeFade, offset, normalSum);
    }
    return offset;
}

// Height of the water surface right above world point xz. Gerstner moves points sideways as well as up, so the
// rest point x0 whose wave carries it onto xz is found first: x0 = xz - sideways(x0), repeated (each round
// shrinks the error by the summed steepness, at most ~0.4 here: 5 rounds leave well under 1 mm of a ~8 cm sideways
// motion). sideError = how far the found point still lands from xz (m).
float WaterSurfaceHeightAt(WaveParams waves[WAVE_COUNT], float2 xz, float3 cameraPos, float time, float4 area,
                           out float sideError)
{
    float2 rest = xz;
    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        rest = xz - WaterSurfaceOffset(waves, rest, cameraPos, time, area).xz;
    }
    float3 offset = WaterSurfaceOffset(waves, rest, cameraPos, time, area);
    sideError = length(rest + offset.xz - xz);
    return offset.y;
}

#endif // WS_WAVES_HLSLI
