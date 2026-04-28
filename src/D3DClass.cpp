#include "D3DClass.h"

#include <stdexcept>

namespace
{
void ThrowIfFailed(HRESULT result, const char* message)
{
    if (FAILED(result))
    {
        throw std::runtime_error(message);
    }
}
} // namespace

bool D3DClass::Initialize(int screenWidth, int screenHeight, bool vsync, HWND hwnd, bool fullscreen, float, float)
{
    vsyncEnabled_ = vsync;
    screenWidth_ = static_cast<unsigned int>(screenWidth);
    screenHeight_ = static_cast<unsigned int>(screenHeight);

    try
    {
        if (!CreateDeviceAndSwapChain(screenWidth, screenHeight, hwnd, fullscreen))
        {
            return false;
        }

        return CreateRenderTarget();
    }
    catch (const std::exception& error)
    {
        MessageBoxA(hwnd, error.what(), "D3D11 error", MB_ICONERROR | MB_OK);
        return false;
    }
}

void D3DClass::Shutdown()
{
    if (swapChain_)
    {
        swapChain_->SetFullscreenState(false, nullptr);
    }

    ReleaseRenderTarget();
    deviceContext_.Reset();
    device_.Reset();
    swapChain_.Reset();
}

void D3DClass::Resize(unsigned int width, unsigned int height)
{
    if (!swapChain_ || width == 0 || height == 0)
    {
        return;
    }

    screenWidth_ = width;
    screenHeight_ = height;

    ReleaseRenderTarget();
    ThrowIfFailed(swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0), "ResizeBuffers failed.");
    CreateRenderTarget();
}

void D3DClass::BeginScene(float red, float green, float blue, float alpha)
{
    const float color[4] = {red, green, blue, alpha};
    deviceContext_->ClearRenderTargetView(renderTargetView_.Get(), color);
}

void D3DClass::EndScene()
{
    if (vsyncEnabled_)
    {
        swapChain_->Present(1, 0);
    }
    else
    {
        swapChain_->Present(0, 0);
    }
}

bool D3DClass::CreateDeviceAndSwapChain(int screenWidth, int screenHeight, HWND hwnd, bool fullscreen)
{
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = static_cast<UINT>(screenWidth);
    swapChainDesc.BufferDesc.Height = static_cast<UINT>(screenHeight);
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = !fullscreen;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#if defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        &featureLevel,
        1,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &swapChain_,
        &device_,
        nullptr,
        &deviceContext_);

#if defined(_DEBUG)
    if (result == DXGI_ERROR_SDK_COMPONENT_MISSING)
    {
        createDeviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            &featureLevel,
            1,
            D3D11_SDK_VERSION,
            &swapChainDesc,
            &swapChain_,
            &device_,
            nullptr,
            &deviceContext_);
    }
#endif

    ThrowIfFailed(result, "D3D11CreateDeviceAndSwapChain failed.");
    return true;
}

bool D3DClass::CreateRenderTarget()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)), "GetBuffer failed.");
    ThrowIfFailed(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView_), "CreateRenderTargetView failed.");

    deviceContext_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(screenWidth_);
    viewport.Height = static_cast<float>(screenHeight_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    deviceContext_->RSSetViewports(1, &viewport);

    return true;
}

void D3DClass::ReleaseRenderTarget()
{
    if (deviceContext_)
    {
        deviceContext_->OMSetRenderTargets(0, nullptr, nullptr);
    }

    renderTargetView_.Reset();
}

