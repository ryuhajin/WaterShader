#pragma once

#include <DirectXMath.h>

#include <cmath>

// Colors are picked in sRGB (ImGui color pickers, preset file) because that is what people see.
// Lighting needs linear values, so they are converted once when they go into a constant buffer.
inline float SrgbToLinear(float c)
{
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// rgb converted, w (intensity / alpha) left as-is.
inline DirectX::XMFLOAT4 SrgbToLinear(const DirectX::XMFLOAT4& c)
{
    return {SrgbToLinear(c.x), SrgbToLinear(c.y), SrgbToLinear(c.z), c.w};
}

inline float LinearToSrgb(float c)
{
    c = c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

// A measured linear light (e.g. E/pi from an HDR sky) as "sRGB picker color + intensity":
// the color is normalized by its largest channel, the intensity carries the magnitude.
inline void SplitLinearLight(const DirectX::XMFLOAT3& linear, DirectX::XMFLOAT3& srgbColor, float& intensity)
{
    intensity = linear.x > linear.y ? (linear.x > linear.z ? linear.x : linear.z) : (linear.y > linear.z ? linear.y : linear.z);
    if (intensity <= 0.0f)
    {
        srgbColor = {1.0f, 1.0f, 1.0f};
        intensity = 0.0f;
        return;
    }
    srgbColor = {LinearToSrgb(linear.x / intensity), LinearToSrgb(linear.y / intensity), LinearToSrgb(linear.z / intensity)};
}
