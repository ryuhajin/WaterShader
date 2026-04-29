#pragma once

#include "SystemConfig.h"

#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

class D3DClass
{
public:
    bool Initialize(int screenWidth, int screenHeight, bool vsync, HWND hwnd, bool fullscreen, float screenDepth, float screenNear);
    void Shutdown();
    void Resize(unsigned int width, unsigned int height);

    void BeginScene(float red, float green, float blue, float alpha);
    void EndScene();

    ID3D11Device* GetDevice() const { return device_.Get(); }
    ID3D11DeviceContext* GetDeviceContext() const { return deviceContext_.Get(); }

private:
    bool CreateDeviceAndSwapChain(int screenWidth, int screenHeight, HWND hwnd, bool fullscreen);
    bool CreateRenderTarget();
    bool CreateDepthStencil();
    bool CreateRasterizerState();
    void ReleaseRenderTarget();

    bool vsyncEnabled_ = true;
    unsigned int numerator_ = 0;
    unsigned int denominator_ = 1;
    unsigned int screenWidth_ = 0;
    unsigned int screenHeight_ = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState_;
};
