#pragma once

#include "ColorShader.h"

#include <algorithm>
#include <cmath>

// "Simple" wave controls: six artist-facing values that generate the four Gerstner waves.
// The per-slot ratios below were read back from the original hand-tuned defaults
// (ColorShader::WaterParams::waves), so the default WaveMacro reproduces them.
struct WaveMacro
{
    float windDeg    = 20.0f;  // travel direction of the main wave (0 = +X, 90 = +Z)
    float spreadDeg  = 60.0f;  // how far the smallest wave turns away from the wind
    float size       = 1.6f;   // crest-to-crest length of the main wave
    float height     = 0.03f;  // amplitude of the main wave
    float chop       = 0.55f;  // Gerstner steepness of the main wave; smaller waves get a bit more
    float speedScale = 1.0f;   // 1 = deep-water speed for the default size
};

namespace wave_macro
{
constexpr int kCount = ColorShader::kWaveCount;

constexpr float kLengthRatio[kCount]   = { 1.0f, 0.65625f, 0.3875f, 0.25625f };
constexpr float kHeightRatio[kCount]   = { 1.0f, 0.6f, 1.0f / 3.0f, 0.2f };
constexpr float kSpreadFactor[kCount]  = { 0.0f, -7.0f / 12.0f, 7.0f / 12.0f, -1.0f };
constexpr float kSteepnessStep[kCount] = { 0.0f, 0.05f, 0.10f, 0.15f };

// Deep water: phase speed grows with the square root of the wavelength.
constexpr float kReferenceSpeed  = 0.55f;
constexpr float kReferenceLength = 1.6f;

// Slider ranges. Size >= 0.8 keeps the smallest wave >= 0.2 (the per-wave slider minimum),
// chop <= 0.85 keeps the sharpest wave <= 1.
constexpr float kSizeMin = 0.8f, kSizeMax = 8.0f;
constexpr float kHeightMax = 0.3f;
constexpr float kSpreadMax = 90.0f;
constexpr float kChopMax = 0.85f;
constexpr float kSpeedMax = 3.0f;

constexpr float kDegToRad = 3.14159265f / 180.0f;

inline float WrapDeg(float deg)
{
    deg = std::fmod(deg + 180.0f, 360.0f);
    return (deg < 0.0f ? deg + 360.0f : deg) - 180.0f;
}

inline float DirectionDeg(const DirectX::XMFLOAT2& direction)
{
    return std::atan2(direction.y, direction.x) / kDegToRad;
}

inline float SpeedFor(float wavelength, float speedScale)
{
    return speedScale * kReferenceSpeed * std::sqrt((std::max)(wavelength, 0.0f) / kReferenceLength); // parenthesized: <windows.h> max() macro
}

inline bool Near(float a, float b, float relative, float absolute)
{
    return std::fabs(a - b) <= absolute + relative * (std::max)(std::fabs(a), std::fabs(b));
}
} // namespace wave_macro

inline void GenerateWaves(const WaveMacro& macro, ColorShader::Wave (&waves)[ColorShader::kWaveCount])
{
    using namespace wave_macro;
    for (int i = 0; i < kCount; ++i)
    {
        const float angle = (macro.windDeg + kSpreadFactor[i] * macro.spreadDeg) * kDegToRad;
        ColorShader::Wave& w = waves[i];
        w.direction  = { std::cos(angle), std::sin(angle) };
        w.wavelength = macro.size * kLengthRatio[i];
        w.amplitude  = macro.height * kHeightRatio[i];
        w.speed      = SpeedFor(w.wavelength, macro.speedScale);
        w.steepness  = std::clamp(macro.chop + kSteepnessStep[i], 0.0f, 1.0f);
    }
}

// Best guess of the Simple values behind a set of waves (presets saved before WaveMacro existed).
// Wave 1 gives everything except the spread, which comes from Wave 4's angle to the wind.
inline WaveMacro EstimateWaveMacro(const ColorShader::Wave (&waves)[ColorShader::kWaveCount])
{
    using namespace wave_macro;
    const ColorShader::Wave& main = waves[0];
    WaveMacro macro;
    macro.windDeg = DirectionDeg(main.direction);
    macro.spreadDeg = std::clamp(-WrapDeg(DirectionDeg(waves[kCount - 1].direction) - macro.windDeg), 0.0f, kSpreadMax);
    macro.size = std::clamp(main.wavelength, kSizeMin, kSizeMax);
    macro.height = std::clamp(main.amplitude, 0.0f, kHeightMax);
    macro.chop = std::clamp(main.steepness, 0.0f, kChopMax);
    const float unitSpeed = SpeedFor(main.wavelength, 1.0f);
    macro.speedScale = unitSpeed > 0.0f ? std::clamp(main.speed / unitSpeed, 0.0f, kSpeedMax) : 1.0f;
    return macro;
}

// True when the waves are (within slider-noise tolerance) what the Simple values generate.
// Anything else was edited per wave in Advanced and is shown as "Custom".
inline bool WavesMatchMacro(const WaveMacro& macro, const ColorShader::Wave (&waves)[ColorShader::kWaveCount])
{
    using namespace wave_macro;
    ColorShader::Wave expected[kCount];
    GenerateWaves(macro, expected);
    for (int i = 0; i < kCount; ++i)
    {
        const ColorShader::Wave& a = waves[i];
        const ColorShader::Wave& b = expected[i];
        if (!Near(a.amplitude, b.amplitude, 0.03f, 1e-5f))
        {
            return false;
        }
        if (a.amplitude <= 0.0f)
        {
            continue; // a flat wave has no visible direction, length or speed
        }
        if (!Near(a.wavelength, b.wavelength, 0.03f, 1e-4f) ||
            !Near(a.speed, b.speed, 0.03f, 1e-4f) ||
            !Near(a.steepness, b.steepness, 0.0f, 0.01f) ||
            std::fabs(WrapDeg(DirectionDeg(a.direction) - DirectionDeg(b.direction))) > 1.0f)
        {
            return false;
        }
    }
    return true;
}
