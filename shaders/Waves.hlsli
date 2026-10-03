#ifndef WS_WAVES_HLSLI
#define WS_WAVES_HLSLI

#define WAVE_COUNT 4

// Distance LOD of the mesh waves: the ocean grid gets coarser with distance, so each wave fades out
// before the vertex spacing is too wide to sample it (short waves fade first). The vertex shader
// scales the wave by this; the pixel shader carries the remaining (1 - fade) as lighting only.
float WaveVertexFade(float wavelength, float viewDistance)
{
    return 1.0 - smoothstep(wavelength * 8.0, wavelength * 14.0, viewDistance);
}

#endif // WS_WAVES_HLSLI
