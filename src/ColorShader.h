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
    };

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
        Wave waves[2] = {
            { { 1.0f, 0.0f}, 0.05f, 2.0f, 0.5f },
            { { 0.7f, 0.7f}, 0.03f, 1.3f, 0.7f },
        };
        int debugMode = 0; // 0=normal, 1=sampled normal map, 2=world-space N, 3=UV, 4=front/back face
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
        ID3D11ShaderResourceView* normalSRV,
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
        ID3D11ShaderResourceView* normalSRV,
        ID3D11SamplerState* clampSampler,
        ID3D11SamplerState* wrapSampler);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
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
