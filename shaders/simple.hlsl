cbuffer PerFrameCB : register(b0)
{
    row_major float4x4 g_World;
    row_major float4x4 g_View;
    row_major float4x4 g_Projection;
    float4 g_LightDirection;
    float4 g_LightColor;
    float4 g_TintColor;
    float4 g_CameraPositionWS;
    float4 g_ShallowColor;
    float4 g_DeepColor;
    float4 g_NormalScroll;        // xy = scroll1, zw = scroll2
    float  g_Time;
    float  g_ReflectionStrength;
    float  g_FresnelPower;
    float  g_NormalScale;
    // Wave 0
    float2 g_Wave0_Dir;
    float  g_Wave0_Amp;
    float  g_Wave0_Wavelen;
    float  g_Wave0_Speed;
    float3 _wave0_pad;
    // Wave 1
    float2 g_Wave1_Dir;
    float  g_Wave1_Amp;
    float  g_Wave1_Wavelen;
    float  g_Wave1_Speed;
    float3 _wave1_pad;
};

TextureCube  g_Skybox        : register(t0);
SamplerState g_Sampler       : register(s0);
Texture2D    g_NormalMap     : register(t1);
SamplerState g_NormalSampler : register(s1);

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
};

void AccumulateSineWave(
    float2 dir, float amp, float wavelen, float speed,
    float2 xz,
    inout float h, inout float dx, inout float dz)
{
    if (amp <= 0.0f || wavelen <= 0.0f) return;
    float k     = 6.2831853f / wavelen;
    float omega = k * speed;
    float phase = dot(dir, xz) * k - omega * g_Time;
    float s = sin(phase);
    float c = cos(phase);
    h  += amp * s;
    dx += dir.x * k * amp * c;
    dz += dir.y * k * amp * c;
}

float3 SineDisplace(float3 p, out float3 normalOut)
{
    float h = 0.0f;
    float dx = 0.0f;
    float dz = 0.0f;

    AccumulateSineWave(g_Wave0_Dir, g_Wave0_Amp, g_Wave0_Wavelen, g_Wave0_Speed, p.xz, h, dx, dz);
    AccumulateSineWave(g_Wave1_Dir, g_Wave1_Amp, g_Wave1_Wavelen, g_Wave1_Speed, p.xz, h, dx, dz);

    normalOut = normalize(float3(-dx, 1.0f, -dz));
    return float3(p.x, p.y + h, p.z);
}

PSInput VSMain(VSInput input)
{
    PSInput output;
    float3 waveNormal;
    float3 displaced = SineDisplace(input.position, waveNormal);
    float4 worldPos = mul(float4(displaced, 1.0f), g_World);
    output.position = mul(mul(worldPos, g_View), g_Projection);
    output.normalWS = normalize(mul(waveNormal, (float3x3)g_World));
    output.worldPos = worldPos.xyz;
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    // Two scrolling normal map samples blended in tangent space.
    float2 uv1 = input.uv * g_NormalScale + g_NormalScroll.xy * g_Time;
    float2 uv2 = input.uv * g_NormalScale + g_NormalScroll.zw * g_Time;
    float3 n1 = g_NormalMap.Sample(g_NormalSampler, uv1).xyz * 2.0f - 1.0f;
    float3 n2 = g_NormalMap.Sample(g_NormalSampler, uv2).xyz * 2.0f - 1.0f;
    float3 nTan = normalize(n1 + n2);

    // Plane TBN: tangent = world X, bitangent = world Z, normal = vertex normal.
    float3 N0 = normalize(input.normalWS);
    float3 T  = normalize(mul(float3(1, 0, 0), (float3x3)g_World));
    float3 B  = normalize(mul(float3(0, 0, 1), (float3x3)g_World));
    float3 N  = normalize(nTan.x * T + nTan.y * B + nTan.z * N0);

    float3 V = normalize(g_CameraPositionWS.xyz - input.worldPos);
    float  NdotV = saturate(dot(N, V));

    float3 L = normalize(-g_LightDirection.xyz);
    float  NdotL = saturate(dot(N, L));

    float3 waterCol = lerp(g_DeepColor.rgb, g_ShallowColor.rgb, NdotV);
    float3 litWater = waterCol * g_LightColor.rgb * g_LightColor.a * NdotL;

    float  fresnel = pow(1.0f - NdotV, g_FresnelPower);

    float3 R = reflect(-V, N);
    float3 envColor = g_Skybox.Sample(g_Sampler, R).rgb;

    float3 lit = lerp(litWater, envColor, saturate(fresnel * g_ReflectionStrength));
    return float4(lit * g_TintColor.rgb, 1.0f);
}
