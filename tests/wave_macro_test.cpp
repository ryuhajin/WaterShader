// WaveMacroTest: checks the Simple wave generator (src/WaveMacro.h) without D3D or a window.
// Run: build/vs2022/<Config>/WaveMacroTest.exe  (exit code 0 = all passed)

#include "WaveMacro.h"

#include <cstdio>

namespace
{
int g_failures = 0;

void Check(bool ok, const char* what)
{
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

bool Close(float a, float b, float tolerance)
{
    return std::fabs(a - b) <= tolerance;
}

using Waves = ColorShader::Wave[ColorShader::kWaveCount];

void CheckSane(const WaveMacro& m, const char* name)
{
    Waves waves;
    GenerateWaves(m, waves);
    for (const ColorShader::Wave& w : waves)
    {
        const float len = std::sqrt(w.direction.x * w.direction.x + w.direction.y * w.direction.y);
        const bool finite = std::isfinite(w.amplitude) && std::isfinite(w.wavelength) && std::isfinite(w.speed) &&
            std::isfinite(w.steepness) && std::isfinite(w.direction.x) && std::isfinite(w.direction.y);
        if (!finite || !Close(len, 1.0f, 1e-4f) || w.steepness < 0.0f || w.steepness > 1.0f ||
            w.wavelength < 0.2f - 1e-4f || w.amplitude < 0.0f || w.speed < 0.0f)
        {
            Check(false, name);
            return;
        }
    }
    Check(WavesMatchMacro(m, waves), name);
}

void CheckRoundTrip(const WaveMacro& m, const char* name)
{
    Waves waves;
    GenerateWaves(m, waves);
    const WaveMacro e = EstimateWaveMacro(waves);
    const bool ok = Close(wave_macro::WrapDeg(e.windDeg - m.windDeg), 0.0f, 0.01f) && Close(e.spreadDeg, m.spreadDeg, 0.01f) &&
        Close(e.size, m.size, 1e-4f) && Close(e.height, m.height, 1e-5f) && Close(e.chop, m.chop, 1e-4f) &&
        (m.height == 0.0f || Close(e.speedScale, m.speedScale, 1e-4f));
    Check(ok, name);
}
} // namespace

int main()
{
    // 1. The default Simple values reproduce the original hand-tuned defaults.
    const ColorShader::WaterParams defaults;
    Check(WavesMatchMacro(WaveMacro{}, defaults.waves), "default WaveMacro matches ColorShader default waves");
    Check(EstimateWaveMacro(defaults.waves).spreadDeg > 59.0f, "estimated spread of the defaults is ~60");

    // 2. Generate -> Estimate gives the same Simple values back.
    CheckRoundTrip(WaveMacro{}, "round trip: defaults");
    CheckRoundTrip({ -135.0f, 15.0f, 4.0f, 0.12f, 0.3f, 2.0f }, "round trip: big slow-ish swell");
    CheckRoundTrip({ 170.0f, 90.0f, 0.8f, 0.0f, 0.0f, 0.5f }, "round trip: flat, minimum size");
    CheckRoundTrip({ 0.0f, 0.0f, 8.0f, 0.3f, 0.85f, 3.0f }, "round trip: everything at max");

    // 3. Slider extremes: no NaN, unit directions, steepness in [0, 1], wavelength >= 0.2.
    CheckSane({ 20.0f, 60.0f, 0.8f, 0.0f, 0.0f, 0.0f }, "sane: all minimums");
    CheckSane({ 180.0f, 90.0f, 8.0f, 0.3f, 0.85f, 3.0f }, "sane: all maximums");
    CheckSane({ -180.0f, 0.0f, 1.6f, 0.03f, 0.55f, 1.0f }, "sane: spread 0");

    // 4. Shipped presets that only scale the default heights (sunset 0.6x, tropical 0.75x) are not Custom.
    for (float scale : { 0.6f, 0.75f })
    {
        Waves waves;
        for (int i = 0; i < ColorShader::kWaveCount; ++i)
        {
            waves[i] = defaults.waves[i];
            waves[i].amplitude *= scale;
        }
        Check(WavesMatchMacro(EstimateWaveMacro(waves), waves), "height-scaled default waves are not Custom");
    }

    // 5. A per-wave edit is detected as Custom.
    {
        Waves waves;
        GenerateWaves(WaveMacro{}, waves);
        waves[2].wavelength *= 1.5f;
        Check(!WavesMatchMacro(WaveMacro{}, waves), "edited wavelength shows as Custom");
        GenerateWaves(WaveMacro{}, waves);
        waves[3].steepness += 0.1f;
        Check(!WavesMatchMacro(WaveMacro{}, waves), "edited steepness shows as Custom");
    }

    if (g_failures == 0)
    {
        std::printf("WaveMacroTest: all passed\n");
    }
    else
    {
        std::printf("WaveMacroTest: %d failed\n", g_failures);
    }
    return g_failures == 0 ? 0 : 1;
}
