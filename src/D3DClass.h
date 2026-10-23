#pragma once

#include "SystemConfig.h"

#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include <string>
#include <vector>

class D3DClass
{
public:
    bool Initialize(int screenWidth, int screenHeight, bool vsync, HWND hwnd, bool fullscreen, float screenDepth, float screenNear);
    void Shutdown();
    void Resize(unsigned int width, unsigned int height);

    void BeginScene(float red, float green, float blue, float alpha);
    void EndScene();

    void SetDepthLessEqual();
    void SetDepthDefault();
    void SetRasterizerDoubleSided();
    void SetRasterizerWaterSurface();
    void SetRasterizerDefault();

    ID3D11Device* GetDevice() const { return device_.Get(); }
    ID3D11DeviceContext* GetDeviceContext() const { return deviceContext_.Get(); }
    ID3D11SamplerState* GetSampler() const { return defaultSampler_.Get(); }
    ID3D11SamplerState* GetWrapSampler() const { return wrapSampler_.Get(); }
    Microsoft::WRL::ComPtr<ID3D11Texture2D> GetBackBuffer() const;

    // Scene renders into a linear float16 HDR target; the tonemap pass resolves it into the back buffer.
    ID3D11ShaderResourceView* GetHdrSRV() const { return hdrSRV_.Get(); }
    void BindBackBuffer();

    // Water inputs (scene-color-depth): the scene as it was before the water is drawn.
    // CopySceneColor snapshots the HDR target; BindHdrTargetReadOnlyDepth keeps depth testing while the
    // same depth texture is read through GetSceneDepthSRV (a writable DSV and an SRV can't be bound together).
    void CopySceneColor();
    void BindHdrTargetReadOnlyDepth();
    void SetDepthTestNoWrite();
    ID3D11ShaderResourceView* GetSceneColorSRV() const { return sceneColorSRV_.Get(); }
    ID3D11ShaderResourceView* GetSceneDepthSRV() const { return depthSRV_.Get(); }

    // Debug layer (Debug builds): warnings and errors since the last call. Empty without the layer.
    // allMessages (optional) counts every stored message, info included: proof the queue is live.
    std::vector<std::string> TakeDebugMessages(size_t* allMessages = nullptr);
    bool HasDebugLayer() const;
    // Binds the depth SRV while the writable depth is bound: a hazard the debug layer must report.
    // Proves the message log works (--d3d-selftest); the runtime unbinds it and nothing is drawn with it.
    void TriggerHazardForTest();
    bool GetVSync() const { return vsyncEnabled_; }
    void SetVSync(bool enabled) { vsyncEnabled_ = enabled; }

private:
    bool CreateDeviceAndSwapChain(int screenWidth, int screenHeight, HWND hwnd, bool fullscreen);
    bool CreateRenderTarget();
    bool CreateHdrTarget();
    bool CreateDepthStencil();
    bool CreateRasterizerState();
    bool CreateDepthStates();
    bool CreateSampler();
    void ReleaseRenderTarget();

    bool vsyncEnabled_ = true;
    unsigned int numerator_ = 0;
    unsigned int denominator_ = 1;
    unsigned int screenWidth_ = 0;
    unsigned int screenHeight_ = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView_; // back buffer (sRGB-encoded output)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> hdrTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> hdrRTV_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> hdrSRV_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneColorTexture_; // copy of the HDR target before the water
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneColorSRV_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture_;      // R24G8_TYPELESS: depth-stencil and readable
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilViewReadOnly_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthSRV_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthTestNoWriteState_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerStateDoubleSided_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerStateWaterSurface_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthLessEqualState_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> defaultSampler_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> wrapSampler_;
};
