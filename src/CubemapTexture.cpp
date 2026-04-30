#include "CubemapTexture.h"

#include <DDSTextureLoader.h>

#include <cstdio>

bool CubemapTexture::Initialize(ID3D11Device* device, const wchar_t* ddsPath, std::wstring* outError)
{
    const HRESULT hr = DirectX::CreateDDSTextureFromFile(
        device,
        ddsPath,
        nullptr,
        srv_.GetAddressOf());

    if (FAILED(hr))
    {
        if (outError)
        {
            wchar_t buf[64];
            std::swprintf(buf, 64, L"Failed to load DDS file (HRESULT: 0x%08X).", static_cast<unsigned>(hr));
            *outError = buf;
        }
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
    srv_->GetDesc(&desc);
    if (desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURECUBE)
    {
        srv_.Reset();
        if (outError)
        {
            *outError =
                L"DDS file was loaded, but it is not a cubemap "
                L"(ViewDimension is 2D, not TextureCube).\n\n"
                L"A single \"cross-layout\" PNG converted directly to DDS becomes a 2D texture, not a cubemap. "
                L"D3D11 needs a cubemap DDS built from 6 separate face images.\n\n"
                L"Recommended: HDRI-to-CubeMap \"Separate\" layout (6 PNGs) -> texconv -cubemap.";
        }
        return false;
    }

    return true;
}

void CubemapTexture::Shutdown()
{
    srv_.Reset();
}
