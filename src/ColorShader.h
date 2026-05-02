#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <chrono>
#include <filesystem>
#include <string>

class ColorShader
{
public:
    struct WaterParams
    {
        DirectX::XMFLOAT4 shallowColor = {0.50f, 0.85f, 1.00f, 1.0f};
        DirectX::XMFLOAT4 deepColor    = {0.05f, 0.15f, 0.40f, 1.0f};
        DirectX::XMFLOAT2 normalScroll1 = { 0.03f,  0.02f};
        DirectX::XMFLOAT2 normalScroll2 = {-0.02f,  0.04f};
        float fresnelPower       = 5.0f;
        float reflectionStrength = 0.4f;
        float normalScale        = 1.0f;
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
        const DirectX::XMFLOAT4& tintColor,
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
    bool InitializeShader(ID3D11Device* device, const wchar_t* shaderPath);
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
        const DirectX::XMFLOAT4& tintColor,
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

    std::wstring shaderPath_;
    std::filesystem::file_time_type lastWriteTime_{};
    std::chrono::steady_clock::time_point nextCheckTime_{};
    std::string lastError_;
    std::string lastReloadStamp_;
};
