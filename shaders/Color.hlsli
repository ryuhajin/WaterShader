#ifndef WS_COLOR_HLSLI
#define WS_COLOR_HLSLI

// Exact sRGB transfer functions (IEC 61966-2-1), not the pow(2.2) approximation.
// Lighting runs in linear space; values only become sRGB at the very end (tonemap.hlsl).
float3 SrgbToLinear(float3 c)
{
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

float3 LinearToSrgb(float3 c)
{
    c = saturate(c);
    return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

#endif // WS_COLOR_HLSLI
