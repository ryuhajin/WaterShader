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

        if (!CreateDepthStencil())
        {
            return false;
        }

        if (!CreateRenderTarget())
        {
            return false;
        }

        if (!CreateRasterizerState())
        {
            return false;
        }

        if (!CreateDepthStates())
        {
            return false;
        }

        return CreateSampler();
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
    CreateDepthStencil();
    CreateRenderTarget();
}

void D3DClass::BeginScene(float red, float green, float blue, float alpha)
{
    const float color[4] = {red, green, blue, alpha};
    deviceContext_->ClearRenderTargetView(renderTargetView_.Get(), color);
    if (depthStencilView_)
    {
        deviceContext_->ClearDepthStencilView(depthStencilView_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
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

void D3DClass::SetDepthLessEqual()
{
    deviceContext_->OMSetDepthStencilState(depthLessEqualState_.Get(), 0);
}

void D3DClass::SetDepthDefault()
{
    deviceContext_->OMSetDepthStencilState(nullptr, 0);
}

void D3DClass::SetRasterizerDoubleSided()
{
    deviceContext_->RSSetState(rasterizerStateDoubleSided_.Get());
}

void D3DClass::SetRasterizerDefault()
{
    deviceContext_->RSSetState(rasterizerState_.Get());
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

    deviceContext_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), depthStencilView_.Get());

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(screenWidth_);
    viewport.Height = static_cast<float>(screenHeight_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    deviceContext_->RSSetViewports(1, &viewport);

    return true;
}

bool D3DClass::CreateRasterizerState()
{
    D3D11_RASTERIZER_DESC desc = {};
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = D3D11_CULL_BACK;
    desc.FrontCounterClockwise = TRUE;
    desc.DepthClipEnable = TRUE;

    ThrowIfFailed(device_->CreateRasterizerState(&desc, &rasterizerState_), "CreateRasterizerState failed.");

    D3D11_RASTERIZER_DESC doubleSidedDesc = desc;
    doubleSidedDesc.CullMode = D3D11_CULL_NONE;
    ThrowIfFailed(
        device_->CreateRasterizerState(&doubleSidedDesc, &rasterizerStateDoubleSided_),
        "CreateRasterizerState (double-sided) failed.");

    deviceContext_->RSSetState(rasterizerState_.Get());
    return true;
}

bool D3DClass::CreateDepthStates()
{
    D3D11_DEPTH_STENCIL_DESC desc = {};
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    desc.StencilEnable = FALSE;

    ThrowIfFailed(device_->CreateDepthStencilState(&desc, &depthLessEqualState_), "CreateDepthStencilState (LessEqual) failed.");
    return true;
}

bool D3DClass::CreateSampler()
{
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.MaxAnisotropy = 1;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MinLOD = 0;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    ThrowIfFailed(device_->CreateSamplerState(&desc, &defaultSampler_), "CreateSamplerState failed.");

    D3D11_SAMPLER_DESC wrapDesc = {};
    wrapDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    wrapDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    wrapDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    wrapDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    wrapDesc.MaxAnisotropy = 1;
    wrapDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    wrapDesc.MinLOD = 0;
    wrapDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ThrowIfFailed(device_->CreateSamplerState(&wrapDesc, &wrapSampler_), "CreateSamplerState (wrap) failed.");
    return true;
}

bool D3DClass::CreateDepthStencil()
{
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = screenWidth_;
    depthDesc.Height = screenHeight_;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ThrowIfFailed(device_->CreateTexture2D(&depthDesc, nullptr, &depthTexture_), "Depth texture creation failed.");

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    ThrowIfFailed(device_->CreateDepthStencilView(depthTexture_.Get(), &dsvDesc, &depthStencilView_), "DSV creation failed.");

    return true;
}

void D3DClass::ReleaseRenderTarget()
{
    if (deviceContext_)
    {
        deviceContext_->OMSetRenderTargets(0, nullptr, nullptr);
    }

    depthStencilView_.Reset();
    depthTexture_.Reset();
    renderTargetView_.Reset();
}

