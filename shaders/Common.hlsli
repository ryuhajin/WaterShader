#ifndef WS_COMMON_HLSLI
#define WS_COMMON_HLSLI

struct WaveParams
{
    float2 direction;
    float  amplitude;
    float  wavelength;
    float  speed;
    float  steepness;  // Gerstner Q (0 = plain sine)
    float2 padding;
};

cbuffer PerFrameCB : register(b0)
{
    row_major float4x4 g_World;
    row_major float4x4 g_View;
    row_major float4x4 g_Projection;

    float4 g_LightDirection;   // xyz = direction light travels (away from the sun), w unused
    float4 g_LightColor;       // rgb = color, a = intensity
    float4 g_AmbientColor;     // rgb = color, a = intensity
    float4 g_CameraPositionWS;

    float4 g_FacingColor;      // high viewFacingAmount
    float4 g_GrazingColor;     // low viewFacingAmount
    float4 g_NormalScroll;     // xy = uv1 scroll, zw = uv2 scroll

    float4 g_WaterParams;      // x=time, y=reflectionStrength, z=fresnelPower, w=normalScale
    float4 g_SpecularParams;   // x=strength, y=sharpness, z=sun glint power, w=sun glint intensity

    WaveParams g_Waves[4];
    float4 g_DebugParams;      // x = debug mode: 0 render, 1 normal map, 2 world N, 3 UV, 4 front/back face, 5 lighting terms, 6 wave LOD, 7 ocean detail
    float4 g_SurfaceParams;    // x = Fresnel F0, y = normal strength, z = detail layer scale, w = far wave normals (0/1)
    float4 g_NormalRotation;   // normal map rotation on the water as cos/sin: xy = layer A, zw = layer B
    float4 g_FarParams;        // x = far glint spread: scales the far-field slope variance (1 = physical, 0 = off)
                               // y = haze distance (world units), zw = slope variance of normal map A / B (measured)
    float4 g_DetailParams;     // ocean detail, 0 = off: x = ripple roughness, y = gust strength, z = gust scale, w = haze strength
    float4 g_DetailParams2;    // x = far wave crests (1 = as before; lower turns the pixel-band crests into glint roughness)
                               // y = render height / 720 (--render-size, else 1), z = log2(y): LOD as a 720p frame
    float4 g_SceneParams;      // test scenes (SCENE_INPUTS program): x = transmission mode (0 opaque water, 1 pass-through)
                               // y = max optical distance (m): view path used where no surface is behind the water
    row_major float4x4 g_InvViewProjection; // test scenes: inverse of view x projection (depth -> world position)
    float4 g_Extinction;       // test scenes: rgb = extinction per meter of view path (1/m)
    float4 g_ScatterColor;     // test scenes: rgb = water color (physical mode, linear), a = strength
    float4 g_RampShallow;      // test scenes: ramp colors (linear)
    float4 g_RampMid;
    float4 g_RampDeep;
    float4 g_RampParams;       // x = mid depth (m), y = deep depth (m), z = by view path (0/1), w = color mode (0 physical, 1 ramp)
    float4 g_FoamParams;       // test scenes (contact-foam): x = width (m), y = edge softness, z = noise size (m), w = strength
    float4 g_FoamParams2;      // x = slope filter (0 = water depth alone, 1 = walls only), y / z = steepness where foam starts / is full,
                               // w = noise flow speed (m/s)
    float4 g_FoamColor;        // rgb = foam color (linear)
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normalWS : NORMAL;
    float3 worldPos : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float3 restPosLocal : TEXCOORD2; // before wave displacement: wave phase for the far-field normals
};

#endif // WS_COMMON_HLSLI


