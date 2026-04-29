cbuffer PerFrameCB : register(b0)
{
    row_major float4x4 g_MVP;
    float4 g_TintColor;
};

struct VSInput
{
    float3 position : POSITION;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), g_MVP);
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(g_TintColor.rgb, 1.0f);
}
