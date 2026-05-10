#include "Common.hlsli"
#include "Lighting.hlsli"
#include "Cubemap.hlsli"

// Water-only resources. Cubemap.hlsli already reserves t0/s0; do not reuse here.
Texture2D    g_NormalMap     : register(t1);
SamplerState g_NormalSampler : register(s1);

// SV_IsFrontFace = rasterizer stage에서 결정되는 system value. 픽셀이 front face에 속하면 true.
float4 PSMain(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    // Two scrolling normal map samples blended in tangent space.
    float time = g_WaterParams.x;
    float reflectionStrength = g_WaterParams.y;
    float fresnelPower = g_WaterParams.z;
    float normalScale = g_WaterParams.w;
    float specularStrength = g_SpecularParams.x;
    float specularSharpness = g_SpecularParams.y;

    float2 uv1 = input.uv * normalScale + g_NormalScroll.xy * time;
    float2 uv2 = input.uv * normalScale + g_NormalScroll.zw * time;
    // rgb [0~1] -> normal vector [-1~1] 범위로 만들기 위해 * 2.0 - 1.0
    float3 n1 = g_NormalMap.Sample(g_NormalSampler, uv1).xyz * 2.0 - 1.0;
    float3 n2 = g_NormalMap.Sample(g_NormalSampler, uv2).xyz * 2.0 - 1.0;
    float3 blendedNormalTS = normalize(n1 + n2);

    // Plane TBN: tangent = world X, bitangent = world Z, normal = vertex normal.
    float3 baseNormalWS = normalize(input.normalWS);
    float3 tangentWS  = normalize(mul(float3(1, 0, 0), (float3x3)g_World));
    float3 bitangentWS  = normalize(mul(float3(0, 0, 1), (float3x3)g_World));
    float3 finalNormalWS  = normalize(blendedNormalTS.x * tangentWS + blendedNormalTS.y * bitangentWS + blendedNormalTS.z * baseNormalWS);

    // Debug visualizations (g_DebugParams.x)
    // 0 = final render
    // 1 = normal map layer A
    // 2 = final world-space normal
    // 3 = mesh UV
    // 4 = front/back face
    int debugMode = (int)g_DebugParams.x;
    if (debugMode == 1) { return float4(g_NormalMap.Sample(g_NormalSampler, uv1).rgb, 1.0); }
    if (debugMode == 2) { return float4(finalNormalWS * 0.5 + 0.5, 1.0); }
    if (debugMode == 3) { return float4(frac(input.uv), 0.0, 1.0); }
    if (debugMode == 4) { return isFrontFace ? float4(0.1, 0.9, 0.2, 1.0) : float4(0.9, 0.1, 0.1, 1.0); }

    float3 viewDirWS = normalize(g_CameraPositionWS.xyz - input.worldPos);
    float  viewFacingAmount = saturate(dot(finalNormalWS, viewDirWS));

    float3 lightDirWS = normalize(-g_LightDirection.xyz);
    float  diffuseAmount = DiffuseFactor(finalNormalWS, lightDirWS);

    // view-angle 기반 water color 계산
    float3 waterBaseColor = lerp(g_GrazingColor.rgb, g_FacingColor.rgb, viewFacingAmount);
    float3 ambientLight = waterBaseColor * g_AmbientColor.rgb * g_AmbientColor.a; // ambientColor.a = 환경광 세기
    float3 diffuseLight = waterBaseColor * g_LightColor.rgb * g_LightColor.a * diffuseAmount;
    float specular = SpecularFactor(finalNormalWS, lightDirWS, viewDirWS, specularSharpness) * specularStrength * g_LightColor.a;
    float3 litWaterColor = ambientLight + diffuseLight + specular.xxx;

    float reflectionByViewAngle = ViewFresnelFactor(viewFacingAmount, fresnelPower);

    float3 reflectionDirWS = reflect(-viewDirWS, finalNormalWS);
    float3 reflectedSceneColor = SampleEnv(reflectionDirWS);

    float reflectionAmount = saturate(reflectionByViewAngle * reflectionStrength);
    float3 finalColor = lerp(litWaterColor, reflectedSceneColor, reflectionAmount);
    return float4(finalColor, 1.0);
}

