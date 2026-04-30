#include "CubemapTexture.h"

#include <DDSTextureLoader.h>

bool CubemapTexture::Initialize(ID3D11Device* device, const wchar_t* ddsPath)
{
    const HRESULT hr = DirectX::CreateDDSTextureFromFile(
        device,
        ddsPath,
        nullptr,
        srv_.GetAddressOf());
    return SUCCEEDED(hr);
}

void CubemapTexture::Shutdown()
{
    srv_.Reset();
}
