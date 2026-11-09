#pragma once

#include <DirectXMath.h>

// sun-shadow: what a receiver (scene objects, water) needs to read the sun shadow map. Built once a frame by
// Graphics and copied into each shader's constant buffer; the same layout as ShadowInputs in Shadow.hlsli.
struct ShadowFrame
{
    DirectX::XMFLOAT4X4 lightViewProjection = {}; // world -> shadow map: x, y in -1..1, z 0 (sun end) .. 1
    DirectX::XMFLOAT4 params = {}; // x = on (0 / 1), y = filter (0 hard, 1 PCF 3x3, 2 PCSS),
                                   // z = bias (0 none, 1 constant, 2 slope-scaled, 3 normal offset, 4 all), w = texel size (m)
    DirectX::XMFLOAT4 bias = {};   // x = constant (m), y = slope scale (texels), z = normal offset (texels), w = depth range (m)
    DirectX::XMFLOAT4 pcss = {};   // x = tan(sun angular radius), y = map size (texels), z = largest blur radius (texels)
};
static_assert(sizeof(ShadowFrame) == 112, "ShadowFrame must match the HLSL ShadowInputs layout");
