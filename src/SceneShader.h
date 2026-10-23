#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <chrono>
#include <filesystem>
#include <string>

// Opaque objects of the test scenes (water-lab): sun Lambert + ambient, world-space patterns
// (checker / ruler / pebbles) so sizes and depths can be read off the image.
class SceneShader
{
public:
    enum Pattern { Solid = 0, Checker = 1, Ruler = 2, Pebbles = 3 };

    struct Material
    {
        DirectX::XMFLOAT3 baseColor = {0.7f, 0.7f, 0.7f}; // sRGB albedo, like the other UI colors
        int pattern = Solid;
    };

    bool Initialize(ID3D11Device* device);
    void Shutdown();
    void CheckHotReload(ID3D11Device* device, std::chrono::steady_clock::time_point now);
    // Per-frame state, then one Render per object.
    void Begin(
        ID3D11DeviceContext* deviceContext,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const DirectX::XMFLOAT4& lightDirection, // xyz = direction light travels
        const DirectX::XMFLOAT4& lightColor,     // sRGB rgb, a = intensity
        const DirectX::XMFLOAT4& ambientColor,   // sRGB rgb, a = intensity
        int debugMode);                          // 0 render, 1 depth below the water
    void Render(ID3D11DeviceContext* deviceContext, int indexCount, const DirectX::XMMATRIX& world, const Material& material);

    const std::string& GetLastError() const { return lastError_; }
    const std::string& GetLastReloadStamp() const { return lastReloadStamp_; }

private:
    bool InitializeShader(ID3D11Device* device, const wchar_t* shaderPath);
    bool Reload(ID3D11Device* device);
    void ShutdownShader();

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> perObjectCB_;

    // Copied into every object's constant buffer.
    DirectX::XMFLOAT4X4 view_{};
    DirectX::XMFLOAT4X4 projection_{};
    DirectX::XMFLOAT4 lightDirection_{};
    DirectX::XMFLOAT4 lightColor_{};
    DirectX::XMFLOAT4 ambientColor_{};
    int debugMode_ = 0;

    std::wstring shaderPath_;
    std::filesystem::file_time_type lastWriteTime_{};
    std::chrono::steady_clock::time_point nextCheckTime_{};
    std::string lastError_;
    std::string lastReloadStamp_;
};
