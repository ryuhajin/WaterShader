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
