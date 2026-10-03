#ifndef WS_LIGHTING_HLSLI
#define WS_LIGHTING_HLSLI

// Lambert Diffuse light from a directional light.
float DiffuseFactor(float3 normalWS, float3 lightDirWS)
{
    return saturate(dot(normalWS, lightDirWS));
}

// View-angle reflection mask (Schlick).
// 물은 정면에서 보면 색이 더 잘 보이고, 낮은 각도에서 보면 하늘이나 주변 환경이 더 강하게 비침
// baseReflectance(F0): 실제 물 ≈ 0.02. 크게 잡을수록 정면에서도 하늘이 비쳐 뿌옇게 보임.
float ViewFresnelFactor(float viewFacingAmount, float power, float baseReflectance)
{
    return baseReflectance + (1.0 - baseReflectance) * pow(1.0 - viewFacingAmount, power);
}

// BlinnPhong highlight from a directional light.
float SpecularFactor(float3 normalWS, float3 lightDirWS, float3 viewDirWS, float sharpness)
{
    float3 halfDirWS = normalize(lightDirWS + viewDirWS);
    return pow(saturate(dot(normalWS, halfDirWS)), sharpness);
}

// Mirror reflection of the sun disk. Each ripple facet whose reflect(-V, N) lines up with the sun
// flashes, which together form the glitter path toward the sun.
float SunGlintFactor(float3 reflectionDirWS, float3 lightDirWS, float power)
{
    return pow(saturate(dot(reflectionDirWS, lightDirWS)), power);
}

// Keeps [0, threshold] untouched (so the water still matches the LDR skybox) and compresses
// HDR values above it toward 1 instead of hard clipping.
float3 HighlightRolloff(float3 color)
{
    const float threshold = 0.8;
    float3 over = max(color - threshold, 0.0);
    float3 compressed = threshold + (1.0 - threshold) * (1.0 - exp(-over / (1.0 - threshold)));
    return color <= threshold ? color : compressed;
}

#endif // WS_LIGHTING_HLSLI

