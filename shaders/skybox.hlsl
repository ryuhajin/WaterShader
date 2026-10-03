cbuffer SkyboxCB : register(b0)
{
    row_major float4x4 g_ViewNoTranslation;
    row_major float4x4 g_Projection;
    float4 g_SunDirection; // xyz = toward the sun, w = cos(angular radius)
    float4 g_SunRadiance;  // rgb = linear radiance of the disk (0 = sky texture has its own sun)
};

TextureCube  g_Skybox  : register(t0);
SamplerState g_Sampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 dir      : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;

    float4 viewPos = mul(float4(input.position, 1.0f), g_ViewNoTranslation);
    float4 clip    = mul(viewPos, g_Projection);

    // depth = w/w = 1.0 (가장 먼 평면)
    output.position = clip.xyww;

    // local cube vertex pos = sample direction
    output.dir = input.position;

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 dir = normalize(input.dir);
    float3 sky = g_Skybox.Sample(g_Sampler, dir).rgb;

    // Analytic sun disk. HDR skies have their sun cut out by equirect_to_cube.ps1; it is drawn here
    // with the same irradiance that lights the water, so the sky, the glint and the diffuse light
    // all come from one sun. The edge is softened over ~15% of the radius to avoid aliasing.
    float cosAngle = dot(dir, g_SunDirection.xyz);
    float cosRadius = g_SunDirection.w;
    float edge = (1.0 - cosRadius) * 0.3;
    float disk = smoothstep(cosRadius - edge, cosRadius + edge, cosAngle);
    return float4(sky + disk * g_SunRadiance.rgb, 1.0);
}
