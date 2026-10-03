#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <chrono>
#include <filesystem>
#include <string>

// Resolves the linear HDR scene into the sRGB back buffer (exposure, tone curve, sRGB encode).
// Hot-reloads shaders/tonemap.hlsl and its Color.hlsli include like the other shaders.
class TonemapShader
{
public:
    enum Operator : unsigned int
    {
        None = 0,     // clamp to [0, 1]
        Reinhard = 1,
        Aces = 2,
        AcesHuePreserving = 3, // luminance curve for in-range colors, per-channel for HDR highlights
    };

    bool Initialize(ID3D11Device* device);
    void Shutdown();
    void CheckHotReload(ID3D11Device* device, std::chrono::steady_clock::time_point now);

    // Expects the back buffer to be bound as the render target.
    void Render(ID3D11DeviceContext* deviceContext, ID3D11ShaderResourceView* hdrScene, float exposure, Operator op);

    const std::string& GetLastError() const { return lastError_; }

private:
    bool Reload(ID3D11Device* device);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;

    std::wstring shaderPath_;
    std::wstring includePath_;
    std::filesystem::file_time_type lastWriteTime_{};
    std::chrono::steady_clock::time_point nextCheckTime_{};
    std::string lastError_;
};
