// Contact foam masks (foam-masks) and the caustic pattern (caustics), baked once into assets/textures, not run while
// the app draws.
// Every pattern is built on lattices that wrap around the texture (cell index modulo the cell count, noise
// periods that are whole numbers of cells), so the right edge continues into the left one and the texture
// repeats without a seam. Channels: R = foam (1 = foam forms here first), G = bubbles, B = breakup noise
// (0.5 = neutral). Same seed, same image: integer hashing only, no time, no randomness from the GPU.

cbuffer FoamBakeCB : register(b0)
{
    uint4 g_Bake; // x = mask (1 cells, 2 bubbles, 3 cells + bubbles + breakup, 4 streaks), y = size (px), z = seed
};

RWTexture2D<unorm float4> g_Output : register(u0);

// PCG-style integer hash -> [0, 1).
uint HashU(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float Hash(uint2 cell, uint salt)
{
    return HashU(cell.x * 73856093u ^ cell.y * 19349663u ^ (g_Bake.z + salt) * 83492791u) / 4294967296.0;
}

float2 Hash2(uint2 cell, uint salt)
{
    return float2(Hash(cell, salt), Hash(cell, salt + 101u));
}

int2 WrapCell(int2 cell, int2 cells)
{
    return (cell % cells + cells) % cells;
}

struct Worley
{
    float f1;      // distance to the nearest feature point (cell units)
    float f2;      // to the second nearest
    float radius;  // the nearest point's own random value (bubble size)
    float present; // the nearest point's random value (does this cell have a bubble)
};

// Worley (cellular) noise on a cells.x by cells.y lattice that wraps: one jittered point per cell.
Worley PeriodicWorley(float2 uv, int2 cells, uint salt)
{
    float2 p = uv * float2(cells);
    int2 base = int2(floor(p));
    Worley result;
    result.f1 = 10.0;
    result.f2 = 10.0;
    result.radius = 0.0;
    result.present = 0.0;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            int2 cell = base + int2(x, y);
            uint2 wrapped = uint2(WrapCell(cell, cells));
            float2 feature = float2(cell) + 0.1 + 0.8 * Hash2(wrapped, salt);
            float d = length(p - feature);
            if (d < result.f1)
            {
                result.f2 = result.f1;
                result.f1 = d;
                result.radius = Hash(wrapped, salt + 7u);
                result.present = Hash(wrapped, salt + 13u);
            }
            else if (d < result.f2)
            {
                result.f2 = d;
            }
        }
    }
    return result;
}

// Foam network: bright along the cell borders (where the two nearest points are almost equally near).
float CellEdges(float2 uv, int2 cells, float width, uint salt)
{
    Worley w = PeriodicWorley(uv, cells, salt);
    return 1.0 - smoothstep(0.0, width, w.f2 - w.f1);
}

// Bubbles: a round bubble in some of the cells, random size, a bright rim and a fainter inside.
float Bubbles(float2 uv, int2 cells, float minRadius, float maxRadius, float fill, uint salt)
{
    Worley w = PeriodicWorley(uv, cells, salt);
    if (w.present > fill) { return 0.0; }
    float r = lerp(minRadius, maxRadius, w.radius);
    float disc = 1.0 - smoothstep(r - 0.04, r, w.f1);
    float rim = smoothstep(r - 0.12, r - 0.03, w.f1);
    return disc * lerp(0.45, 1.0, rim);
}

// Value noise on a lattice of period cells (wraps), smooth (quintic) interpolation.
float PeriodicValueNoise(float2 uv, int period, uint salt)
{
    float2 p = uv * period;
    int2 i = int2(floor(p));
    float2 f = p - floor(p);
    float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    int2 cells = int2(period, period);
    float a = Hash(uint2(WrapCell(i, cells)), salt);
    float b = Hash(uint2(WrapCell(i + int2(1, 0), cells)), salt);
    float c = Hash(uint2(WrapCell(i + int2(0, 1), cells)), salt);
    float d = Hash(uint2(WrapCell(i + int2(1, 1), cells)), salt);
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// Low-frequency breakup: 4 octaves, every period a whole number of cells, stretched back to about 0..1.
float Breakup(float2 uv, uint salt)
{
    float sum = 0.0;
    float amplitude = 0.5;
    int period = 3;
    [unroll]
    for (int octave = 0; octave < 4; ++octave)
    {
        sum += amplitude * PeriodicValueNoise(uv, period, salt + octave * 31u);
        amplitude *= 0.5;
        period *= 2;
    }
    return saturate((sum / 0.9375 - 0.5) * 1.8 + 0.5);
}

// Caustics (caustics step): where the wave lenses gather the sun, bright thin curved lines; between them dimmer cells.
// Cell borders of a Worley lattice, bent by a periodic domain warp (real caustic lines are curves, not straight
// polygon edges). Line = exp(-(F2 - F1) / width): brightest exactly on the border, falling off fast, plus a weak wide
// glow so the cells next to a line are a little lit.
float CausticLines(float2 uv, int cells, float width, uint salt)
{
    float2 warp = float2(PeriodicValueNoise(uv, 4, salt + 40u), PeriodicValueNoise(uv, 4, salt + 41u)) - 0.5;
    Worley w = PeriodicWorley(uv + warp * (0.8 / float(cells)), int2(cells, cells), salt);
    float border = w.f2 - w.f1;
    return exp(-border / width) + 0.18 * exp(-border / (width * 6.0));
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint size = g_Bake.y;
    if (id.x >= size || id.y >= size) { return; }
    float2 uv = (float2(id.xy) + 0.5) / float(size);

    float foam = 0.0;
    float bubbles = 0.0;
    float breakup = 0.5;
    switch (g_Bake.x)
    {
    case 1: // Cells: a foam network at two scales
        foam = saturate(0.7 * CellEdges(uv, int2(7, 7), 0.16, 1u) + 0.45 * CellEdges(uv, int2(17, 17), 0.12, 2u));
        break;
    case 2: // Bubbles: big bubbles in R, small ones in G
        foam = Bubbles(uv, int2(6, 6), 0.18, 0.42, 0.75, 3u);
        bubbles = max(Bubbles(uv, int2(19, 19), 0.15, 0.40, 0.6, 4u), Bubbles(uv, int2(37, 37), 0.18, 0.40, 0.5, 5u));
        break;
    case 3: // Cells + bubbles + breakup (the recommended default)
        foam = saturate(0.7 * CellEdges(uv, int2(7, 7), 0.16, 1u) + 0.45 * CellEdges(uv, int2(17, 17), 0.12, 2u));
        bubbles = max(Bubbles(uv, int2(19, 19), 0.15, 0.40, 0.6, 4u), Bubbles(uv, int2(37, 37), 0.18, 0.40, 0.5, 5u));
        breakup = Breakup(uv, 6u);
        break;
    case 4: // Streaks: cells stretched along v (5 cells down, 16 across), small bubbles, breakup
        foam = saturate(0.75 * CellEdges(uv, int2(16, 5), 0.14, 7u) + 0.4 * CellEdges(uv, int2(31, 9), 0.12, 8u));
        bubbles = Bubbles(uv, int2(37, 37), 0.18, 0.40, 0.5, 5u);
        breakup = Breakup(uv, 9u);
        break;
    case 5: // Caustics: a large and a finer network (gray: the same value in R, G, B)
    {
        float c = saturate(0.72 * CausticLines(uv, 6, 0.06, 12u) + 0.3 * CausticLines(uv, 11, 0.035, 13u));
        g_Output[id.xy] = float4(c, c, c, 1.0);
        return;
    }
    default:
        break;
    }
    g_Output[id.xy] = float4(foam, bubbles, breakup, 1.0);
}
