#ifndef WS_DEPTH_BANDS_HLSLI
#define WS_DEPTH_BANDS_HLSLI

// Depth views shared by the answer key (scene.hlsl: water depth from the world position) and the
// reconstruction (PixelShader.hlsl: water depth / view path from the depth buffer), so the two show the
// same colors and the same numbers for the same meters.

// Bands picked so the Water Lab pads (0.2 / 1 / 3 / 6 m) and the 8 m floor each land in their own color.
float3 DepthBandColor(float depth)
{
    if (depth < 0.5) return float3(0.30, 0.95, 1.00); // ~0.2 m
    if (depth < 2.0) return float3(0.35, 0.90, 0.35); // ~1 m
    if (depth < 4.5) return float3(1.00, 0.90, 0.25); // ~3 m
    if (depth < 7.0) return float3(1.00, 0.55, 0.20); // ~6 m
    return float3(0.75, 0.40, 0.95);                  // deeper (lab floor 8 m)
}

// Shown where no surface is behind the water (sky): the max optical distance stands in for it.
static const float3 kNoSurfaceColor = float3(1.0, 0.0, 1.0);

// 1 m contour lines, ~1 px wide, for a value that varies across the screen. 0 on a face that sits on one
// value (the whole face would fill in). Values rebuilt from the 24-bit depth buffer carry ~0.5 mm of noise
// per pixel, so "flat" means less than 2 mm change per pixel, not exactly 0. More than 0.5 m per pixel is
// an object's outline (one surface next to another), not a slope: no line there either.
float MeterContour(float meters)
{
    float width = fwidth(meters);
    if (width <= 2e-3 || width > 0.5) return 0.0;
    return 1.0 - saturate(abs(frac(meters + 0.5) - 0.5) / width);
}

// Meters as a display color for exact comparison (tools/compare_depth.ps1, PNG captures):
//   R = fraction of a meter (1/255 m ~ 4 mm), G = whole meters x 4 / 255 (0..63 m), B = marker.
// Markers sit on exact 8-bit levels so they survive the round trip to the PNG unchanged.
static const float kMarkerMeasured   = 128.0 / 255.0; // a surface was measured
static const float kMarkerAboveWater = 64.0 / 255.0;  // the surface is above the water (answer key only)
static const float kMarkerNoSurface  = 1.0;           // sky behind the water: max optical distance used

float3 EncodeMeters(float meters, float marker)
{
    meters = clamp(meters, 0.0, 63.99);
    float whole = floor(meters);
    return float3(meters - whole, whole * 4.0 / 255.0, marker);
}

#endif // WS_DEPTH_BANDS_HLSLI
