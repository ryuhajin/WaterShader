#pragma once

#include "ShadowFrame.h"

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
        int debugMode,                           // 0 render, 1 depth below the water
        bool writeNormal = false);               // also write the world normal to render target 1 (normal-gbuffer)
    // The water surface the answer key (debug 1 / 2) measures depth from (wave-occlusion): the water's own waves.
    struct WaterSurface
    {
        DirectX::XMFLOAT4 waves[8];  // per wave: (direction.x, direction.y, amplitude, wavelength), (speed, steepness, 0, 0)
        DirectX::XMFLOAT4 camera;    // xyz = camera position (the waves fade with distance)
        DirectX::XMFLOAT4 timeArea;  // x = time, y = water patch half size, z = wave edge fade width
        // Downwelling: light reaching points under the water loses exp(-extinction x path) on the way down.
        // xyz = extinction (1/m), w = cos of the sun's angle under the water from vertical (0 = off).
        DirectX::XMFLOAT4 underwaterLight = {0.0f, 0.0f, 0.0f, 0.0f};
    };
    void SetWaterSurface(const WaterSurface& surface) { waterSurface_ = surface; }
    // sun-shadow: the shadow map the objects receive (t0, comparison sampler s0). Bound by Begin; params.x = 0 = off.
    void SetShadow(const ShadowFrame& shadow, ID3D11ShaderResourceView* map, ID3D11SamplerState* sampler)
    {
        shadow_ = shadow;
        shadowMap_ = map;
        shadowSampler_ = sampler;
    }
    // caustics: the sunlight pattern under the water (t1, repeat sampler s1). params.x = 0 = off.
    struct Caustics
    {
        DirectX::XMFLOAT4 params = {0.0f, 0.0f, 0.0f, 5.0f};  // x = strength, y = 1 / pattern size (1/m), z = pattern time, w = fade-out depth (m)
        DirectX::XMFLOAT4 params2 = {0.0f, 0.0f, 0.0f, 0.0f}; // x = mip bias (softness), zw = xz offset per meter of depth toward the sun
    };
    void SetCaustics(const Caustics& caustics, ID3D11ShaderResourceView* map, ID3D11SamplerState* sampler)
    {
        caustics_ = caustics;
        causticMap_ = map;
        causticSampler_ = sampler;
    }
    // Shadow pass: the same objects seen from the sun, depth only (no pixel shader). Render as usual afterwards.
    void BeginDepthOnly(ID3D11DeviceContext* deviceContext, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);
    void Render(ID3D11DeviceContext* deviceContext, int indexCount, const DirectX::XMMATRIX& world, const Material& material);

    const std::string& GetLastError() const { return lastError_; }
    const std::string& GetLastReloadStamp() const { return lastReloadStamp_; }

private:
    bool InitializeShader(ID3D11Device* device, const wchar_t* shaderPath);
    bool Reload(ID3D11Device* device);
    void ShutdownShader();

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShaderWithNormal_; // PSMainWithNormal: color + world normal (MRT)
    Microsoft::WRL::ComPtr<ID3D11InputLayout> layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> perObjectCB_;

    // Copied into every object's constant buffer.
    DirectX::XMFLOAT4X4 view_{};
    DirectX::XMFLOAT4X4 projection_{};
    DirectX::XMFLOAT4 lightDirection_{};
    DirectX::XMFLOAT4 lightColor_{};
    DirectX::XMFLOAT4 ambientColor_{};
    int debugMode_ = 0;
    WaterSurface waterSurface_{};
    ShadowFrame shadow_{};
    ID3D11ShaderResourceView* shadowMap_ = nullptr;
    ID3D11SamplerState* shadowSampler_ = nullptr;
    Caustics caustics_{};
    ID3D11ShaderResourceView* causticMap_ = nullptr;
    ID3D11SamplerState* causticSampler_ = nullptr;

    std::wstring shaderPath_;
    std::filesystem::file_time_type lastWriteTime_{};
    std::chrono::steady_clock::time_point nextCheckTime_{};
    std::string lastError_;
    std::string lastReloadStamp_;
};
