#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

class ColorShader
{
public:
    struct Wave
    {
        DirectX::XMFLOAT2 direction = {1.0f, 0.0f};
        float amplitude  = 0.05f;
        float wavelength = 2.0f;
        float speed      = 0.5f;
        float steepness  = 0.0f; // Gerstner Q: 0 = sine, 1 = sharpest crest before looping
    };

    static constexpr int kWaveCount = 4;

    struct WaterParams
    {
        DirectX::XMFLOAT4 facingColor  = {0.50f, 0.85f, 1.00f, 1.0f};
        DirectX::XMFLOAT4 grazingColor = {0.05f, 0.15f, 0.40f, 1.0f};
        DirectX::XMFLOAT2 normalScroll1 = { 0.03f,  0.02f};
        DirectX::XMFLOAT2 normalScroll2 = {-0.02f,  0.04f};
        float fresnelPower       = 5.0f;
        float reflectionStrength = 0.4f;
        float normalScale        = 1.0f;
        float specularStrength   = 0.25f;
        float specularSharpness  = 64.0f;
        float sunGlintPower      = 800.0f;  // reflect(-V, N) vs sun direction lobe
        float sunGlintIntensity  = 12.0f;   // HDR multiplier, >1 so glints survive the rolloff
        float farGlintSpread     = 1.0f;    // scales the far-field slope variance that widens the glint (1 = physical)
        float fresnelF0          = 0.5f;    // reflectance at normal incidence (water ~0.02)
        float normalStrength     = 1.0f;    // tangent-space XY multiplier
        float detailScale        = 1.0f;    // layer B tiling relative to layer A
        // Ocean detail (ocean-hero). All 0 = off, the image is the same as before.
        float rippleRoughness    = 0.0f;    // ripple slopes lost to the normal-map mips -> glint width (Toksvig)
        float gustStrength       = 0.0f;    // wind gust patches: +- this much ripple strength
        float gustScale          = 30.0f;   // size of a gust patch (world units)
        float hazeStrength       = 0.0f;    // blend toward the horizon sky with distance
        float hazeDistance       = 150.0f;  // distance where the haze reaches 1 - 1/e of hazeStrength
        float farWaveCrests      = 1.0f;    // past the mesh fade: 1 = crests per pixel (as before), 0 = all glint roughness
        // Set every frame from the selected normal maps (NormalMapEntry::slopeVariance), not saved in presets.
        float rippleSlopeVarianceA = 0.0f;
        float rippleSlopeVarianceB = 0.0f;
        // Wavelengths spread ~1.5x apart and directions fanned around the wind so no single crest dominates.
        Wave waves[kWaveCount] = {
            { { 0.940f,  0.342f}, 0.030f, 1.60f, 0.55f, 0.55f },
            { { 0.966f, -0.259f}, 0.018f, 1.05f, 0.45f, 0.60f },
            { { 0.574f,  0.819f}, 0.010f, 0.62f, 0.35f, 0.65f },
            { { 0.766f, -0.643f}, 0.006f, 0.41f, 0.28f, 0.70f },
        };
        int debugMode = 0; // 0=normal, 1=sampled normal map, 2=world-space N, 3=UV, 4=front/back face, 5=lighting terms, 6=wave LOD, 7=ocean detail
        float farWaveNormals = 1.0f; // 1 = pixel shader adds the wave slopes the mesh LOD faded out
        // Normal map rotation on the water (world XZ), as cos/sin: xy = layer A, zw = layer B. (1, 0) = as authored.
        DirectX::XMFLOAT4 normalRotation = {1.0f, 0.0f, 1.0f, 0.0f};
    };

    bool Initialize(ID3D11Device* device);
    void Shutdown();
    void CheckHotReload(ID3D11Device* device, std::chrono::steady_clock::time_point now);
    bool Render(
        ID3D11DeviceContext* deviceContext,
        int indexCount,
        const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const DirectX::XMFLOAT4& lightDirection,
        const DirectX::XMFLOAT4& lightColor,
        const DirectX::XMFLOAT4& ambientColor,
        float time,
        const DirectX::XMFLOAT4& cameraPositionWS,
        const WaterParams& water,
        ID3D11ShaderResourceView* cubemapSRV,
        ID3D11ShaderResourceView* normalSRVA, // layer A (t1)
        ID3D11ShaderResourceView* normalSRVB, // layer B (t2)
        ID3D11SamplerState* clampSampler,
        ID3D11SamplerState* wrapSampler);

    const std::string& GetLastError() const { return lastError_; }
    const std::string& GetLastReloadStamp() const { return lastReloadStamp_; }

private:
    bool InitializeShader(ID3D11Device* device);
    bool Reload(ID3D11Device* device);
    void ShutdownShader();
    void RenderShader(
        ID3D11DeviceContext* deviceContext,
        int indexCount,
        const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const DirectX::XMFLOAT4& lightDirection,
        const DirectX::XMFLOAT4& lightColor,
        const DirectX::XMFLOAT4& ambientColor,
        float time,
        const DirectX::XMFLOAT4& cameraPositionWS,
        const WaterParams& water,
        ID3D11ShaderResourceView* cubemapSRV,
        ID3D11ShaderResourceView* normalSRVA, // layer A (t1)
        ID3D11ShaderResourceView* normalSRVB, // layer B (t2)
        ID3D11SamplerState* clampSampler,
        ID3D11SamplerState* wrapSampler);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShaderDetail_; // OCEAN_DETAIL: used while any ocean detail is on
    Microsoft::WRL::ComPtr<ID3D11InputLayout> layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> perFrameCB_;

    std::wstring vsPath_;
    std::wstring psPath_;

    struct WatchedFile
    {
        std::wstring path;
        std::filesystem::file_time_type mtime{};
    };
    std::vector<WatchedFile> watchedFiles_;

    std::chrono::steady_clock::time_point nextCheckTime_{};
    std::string lastError_;
    std::string lastReloadStamp_;
};
