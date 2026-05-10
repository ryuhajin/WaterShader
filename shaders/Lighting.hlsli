#ifndef WS_LIGHTING_HLSLI
#define WS_LIGHTING_HLSLI

// Lambert Diffuse light from a directional light.
float DiffuseFactor(float3 normalWS, float3 lightDirWS)
{
    return saturate(dot(normalWS, lightDirWS));
}

// View-angle reflection mask.
// 물은 정면에서 보면 색이 더 잘 보이고, 낮은 각도에서 보면 하늘이나 주변 환경이 더 강하게 비침
float ViewFresnelFactor(float viewFacingAmount, float power)
{
    // 좀 더 사실적인 느낌 = 0.04~0.08, 반사를 많이 보여주기 위해 현재 0.5로 설정
    const float baseReflectance = 0.5;
    return baseReflectance + (1.0 - baseReflectance) * pow(1.0 - viewFacingAmount, power);
}

// BlinnPhong highlight from a directional light.
float SpecularFactor(float3 normalWS, float3 lightDirWS, float3 viewDirWS, float sharpness)
{
    float3 halfDirWS = normalize(lightDirWS + viewDirWS);
    return pow(saturate(dot(normalWS, halfDirWS)), sharpness);
}

#endif // WS_LIGHTING_HLSLI

