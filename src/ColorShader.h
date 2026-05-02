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
        float reflectionStrength,
        float fresnelPower,
        ID3D11ShaderResourceView* cubemapSRV,
        ID3D11SamplerState* sampler);

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
        float reflectionStrength,
        float fresnelPower,
        ID3D11ShaderResourceView* cubemapSRV,
        ID3D11SamplerState* sampler);

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
