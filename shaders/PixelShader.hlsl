#include "Common.hlsli"
#include "Lighting.hlsli"
#include "Cubemap.hlsli"
#include "Color.hlsli"

// Water-only resources. Cubemap.hlsli already reserves t0/s0; do not reuse here.
Texture2D    g_NormalMap     : register(t1); // layer A
Texture2D    g_NormalMapB    : register(t2); // layer B (may be the same texture as A)
SamplerState g_NormalSampler : register(s1);

float4 DebugOut(float3 value)
{
    return float4(SrgbToLinear(saturate(value)), 1.0);
}

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

    float fresnelF0 = g_SurfaceParams.x;
    float normalStrength = g_SurfaceParams.y;
    float detailScale = g_SurfaceParams.z;

    // Layer A = broad ripples, layer B = finer chop at a different tiling, so the repeat is harder to spot.
    // Each layer has its own normal map; with two different maps the B layer no longer repeats A's shapes.
    float2 uv1 = input.uv * normalScale + g_NormalScroll.xy * time;
    float2 uv2 = input.uv * normalScale * detailScale + g_NormalScroll.zw * time;
    // rgb [0~1] -> normal vector [-1~1] 범위로 만들기 위해 * 2.0 - 1.0
    float3 n1 = g_NormalMap.Sample(g_NormalSampler, uv1).xyz * 2.0 - 1.0;
    float3 n2 = g_NormalMapB.Sample(g_NormalSampler, uv2).xyz * 2.0 - 1.0;
    // Whiteout blend: add the slopes (xy), multiply the z. Plain n1 + n2 halves the detail of each layer.
    float3 blendedNormalTS = float3((n1.xy + n2.xy) * normalStrength, n1.z * n2.z);
    blendedNormalTS = normalize(blendedNormalTS);

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
    // Debug values are data, not light: DebugOut pre-decodes them so the final LinearToSrgb in the
    // tonemap pass (forced to "no tone curve" while debugging) gives back exactly these numbers.
    int debugMode = (int)g_DebugParams.x;
    if (debugMode == 1) { return DebugOut(g_NormalMap.Sample(g_NormalSampler, uv1).rgb); }
    if (debugMode == 2) { return DebugOut(finalNormalWS * 0.5 + 0.5); }
    if (debugMode == 3) { return DebugOut(float3(frac(input.uv), 0.0)); }
    if (debugMode == 4) { return DebugOut(isFrontFace ? float3(0.1, 0.9, 0.2) : float3(0.9, 0.1, 0.1)); }

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

    float reflectionByViewAngle = ViewFresnelFactor(viewFacingAmount, fresnelPower, fresnelF0);

    float3 reflectionDirWS = reflect(-viewDirWS, finalNormalWS);
    // A steep ripple back-face can reflect below the horizon and pick up the ground of the skybox.
    // Mirror it back into the sky instead (the ground is not really visible in a water reflection).
    float3 envLookupDirWS = float3(reflectionDirWS.x, abs(reflectionDirWS.y), reflectionDirWS.z);
    float3 reflectedSceneColor = SampleEnv(envLookupDirWS);

    float reflectionAmount = saturate(reflectionByViewAngle * reflectionStrength);
    if (debugMode == 5) { return DebugOut(float3(diffuseAmount, specular, reflectionAmount)); }

    float3 finalColor = lerp(litWaterColor, reflectedSceneColor, reflectionAmount);

    // Sun glint = mirror reflection of the analytic sun (cut out of HDR skies, so it is not counted twice).
    // Energy-normalized Phong lobe: (n + 2) / (2 pi) * pow(R.L, n) integrates to 1 over the hemisphere,
    // so the reflected sun energy is F * E_sun whatever the lobe width. g_LightColor holds E_sun / pi
    // (Lambert convention), hence pi * (n + 2) / (2 pi) = (n + 2) / 2.
    // sunGlintIntensity is now a multiplier on that physical value (1 = physically based).
    float sunGlintPower = g_SpecularParams.z;
    float sunGlintIntensity = g_SpecularParams.w;
    float lobe = SunGlintFactor(reflectionDirWS, lightDirWS, sunGlintPower) * (sunGlintPower + 2.0) * 0.5;
    finalColor += lobe * sunGlintIntensity * reflectionAmount * g_LightColor.rgb * g_LightColor.a;

    // Linear HDR radiance. Values above 1 (glint, bright sky) are kept; the tonemap pass compresses
    // the whole frame, sky included, with one curve.
    return float4(finalColor, 1.0);
}

