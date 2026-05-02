#include "Texture.h"

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <sstream>

namespace
{
bool HasExtension(const std::wstring& path, const wchar_t* ext)
{
    std::filesystem::path p(path);
    std::wstring actual = p.extension().wstring();
    std::transform(actual.begin(), actual.end(), actual.begin(), [](wchar_t c) { return std::towlower(c); });
    return actual == ext;
}

void FormatHResult(HRESULT hr, std::wstring* outError)
{
    if (!outError) return;
    std::wstringstream ss;
    ss << L"HRESULT 0x" << std::hex << static_cast<unsigned long>(hr);
    *outError = ss.str();
}
} // namespace

bool Texture::Initialize(ID3D11Device* device, const wchar_t* path, std::wstring* outError)
{
    if (!device || !path)
    {
        if (outError) *outError = L"Invalid device or path";
        return false;
    }

    HRESULT hr = E_FAIL;
    if (HasExtension(path, L".dds"))
    {
        hr = DirectX::CreateDDSTextureFromFile(device, path, nullptr, textureView_.GetAddressOf());
    }
    else
    {
        hr = DirectX::CreateWICTextureFromFile(device, path, nullptr, textureView_.GetAddressOf());
    }

    if (FAILED(hr))
    {
        FormatHResult(hr, outError);
        textureView_.Reset();
        return false;
    }

    return true;
}

bool Texture::InitializeFlat(ID3D11Device* device, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    const uint8_t pixel[4] = {r, g, b, a};

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 1;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixel;
    initData.SysMemPitch = 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(&desc, &initData, &texture)))
    {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    if (FAILED(device->CreateShaderResourceView(texture.Get(), &srvDesc, textureView_.GetAddressOf())))
    {
        return false;
    }

    return true;
}

void Texture::Shutdown()
{
    textureView_.Reset();
}
