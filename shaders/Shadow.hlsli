#ifndef WS_SHADOW_HLSLI
#define WS_SHADOW_HLSLI

// sun-shadow: one orthographic shadow map from the sun (1024 x 1024, 32-bit depth).
// A receiver point is moved into the sun's view; if the map holds something nearer the sun at that spot, the sun
// is blocked there. The result is the share of sunlight that arrives (1 = lit, 0 = shadow); only the sun term is
// multiplied by it, the sky light stays.
// Same layout as ShadowFrame (src/ShadowFrame.h).
struct ShadowInputs
{
    row_major float4x4 lightViewProjection; // world -> shadow map: x, y in -1..1, z 0 (sun end) .. 1
    float4 params; // x = on, y = filter (0 hard, 1 PCF 3x3, 2 PCSS), z = bias (0 none, 1 constant, 2 slope, 3 normal offset, 4 all), w = texel size (m)
    float4 bias;   // x = constant (m), y = slope scale (texels), z = normal offset (texels), w = depth range (m)
    float4 pcss;   // x = tan(sun angular radius), y = map size (texels), z = largest blur radius (texels)
};

// Points on a disk spread evenly without a grid pattern (Vogel / sunflower spiral): sample i of count, radius <= 1.
float2 ShadowVogelDisk(int i, int count)
{
    float r = sqrt((i + 0.5) / count);
    float theta = i * 2.39996323; // golden angle
    return r * float2(cos(theta), sin(theta));
}

// Sunlight share at a world point. normal = the receiver's surface normal, toSun = unit vector toward the sun.
float SunShadow(Texture2D<float> map, SamplerComparisonState cmp, ShadowInputs s, float3 worldPos, float3 normal, float3 toSun)
{
    if (s.params.x < 0.5)
    {
        return 1.0;
    }
    int filter = (int)(s.params.y + 0.5);
    int biasMode = (int)(s.params.z + 0.5);
    float texelMeters = s.params.w;
    float depthRange = s.bias.w;
    float mapSize = s.pcss.y;

    // Bias (shadow acne): one map texel covers a patch of the receiver, and a surface tilted away from the sun spans a
    // range of depths inside that one texel. Half the patch is then "behind" the stored depth of its own surface and
    // shades itself in stripes. Pushing the comparison toward the sun hides that; pushing too far detaches the shadow
    // from its caster (peter-panning).
    //   constant:      a fixed distance in meters
    //   slope-scaled:  texel x tan(angle to the sun): exactly the depth step one texel makes on a tilted surface
    //   normal offset: look the map up from a point moved off the surface along its normal (texels)
    float cosToSun = abs(dot(normal, toSun));
    float tanToSun = min(sqrt(saturate(1.0 - cosToSun * cosToSun)) / max(cosToSun, 1e-3), 10.0);
    bool useConstant = biasMode == 1 || biasMode == 4;
    bool useSlope = biasMode == 2 || biasMode == 4;
    bool useNormal = biasMode == 3 || biasMode == 4;
    if (useNormal)
    {
        worldPos += normal * (s.bias.z * texelMeters);
    }
    float biasMeters = (useConstant ? s.bias.x : 0.0) + (useSlope ? s.bias.y * texelMeters * tanToSun : 0.0);

    float4 lightH = mul(float4(worldPos, 1.0), s.lightViewProjection);
    float3 ndc = lightH.xyz / lightH.w; // w = 1 (orthographic)
    float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    if (any(uv < 0.0) || any(uv > 1.0) || ndc.z > 1.0)
    {
        return 1.0; // outside the sun's box: nothing there casts a shadow
    }
    float receiverDepth = ndc.z - biasMeters / depthRange;

    // Hard: one texel, one yes / no. The edge follows the texel squares.
    if (filter == 0)
    {
        int2 texel = min(int2(uv * mapSize), int2(mapSize - 1, mapSize - 1));
        return receiverDepth <= map.Load(int3(texel, 0)) ? 1.0 : 0.0;
    }

    float2 texelUv = 1.0 / mapSize;
    // PCF 3 x 3: nine comparisons around the point, averaged. Each SampleCmp already blends the answers of its 4
    // nearest texels, so the edge is a smooth ramp about 3 texels wide everywhere.
    if (filter == 1)
    {
        float lit = 0.0;
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            [unroll]
            for (int x = -1; x <= 1; ++x)
            {
                lit += map.SampleCmpLevelZero(cmp, uv + float2(x, y) * texelUv, receiverDepth);
            }
        }
        return lit / 9.0;
    }

    // PCSS (percentage-closer soft shadows): the sun is a disk, not a point, so the half-shadow widens with the
    // distance from the caster: width = distance x tan(sun radius). 1) Look around for casters and average their
    // depth. 2) Blur radius = (receiver - caster) x tan(sun radius). 3) PCF over that radius.
    float tanSun = s.pcss.x;
    float maxRadius = s.pcss.z;
    float receiverMeters = receiverDepth * depthRange; // from the sun end of the box: the farthest a caster can be
    float searchTexels = clamp(receiverMeters * tanSun / texelMeters, 1.0, maxRadius);
    float blockerSum = 0.0;
    float blockerCount = 0.0;
    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        float2 p = uv * mapSize + ShadowVogelDisk(i, 16) * searchTexels;
        int2 texel = clamp(int2(p), int2(0, 0), int2(mapSize - 1, mapSize - 1));
        float stored = map.Load(int3(texel, 0));
        if (stored < receiverDepth)
        {
            blockerSum += stored;
            blockerCount += 1.0;
        }
    }
    if (blockerCount == 0.0)
    {
        return 1.0; // no caster near: fully lit, no need to filter
    }
    float blockerDepth = blockerSum / blockerCount;
    float radiusTexels = clamp((receiverDepth - blockerDepth) * depthRange * tanSun / texelMeters, 1.0, maxRadius);
    float lit = 0.0;
    [unroll]
    for (int j = 0; j < 32; ++j)
    {
        lit += map.SampleCmpLevelZero(cmp, uv + ShadowVogelDisk(j, 32) * radiusTexels * texelUv, receiverDepth);
    }
    return lit / 32.0;
}

#endif // WS_SHADOW_HLSLI
