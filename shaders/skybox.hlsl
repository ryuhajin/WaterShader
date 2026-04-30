cbuffer SkyboxCB : register(b0)
{
    row_major float4x4 g_ViewNoTranslation;
    row_major float4x4 g_Projection;
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
    return g_Skybox.Sample(g_Sampler, normalize(input.dir));
}
