#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <string>

class CubemapTexture
{
public:
    // srgb: the file holds sRGB-encoded colors (LDR photos) -> create the *_SRGB view.
    //       Float formats (BC6H, R16F) are linear radiance already and have no *_SRGB variant.
    bool Initialize(ID3D11Device* device, const wchar_t* ddsPath, bool srgb, std::wstring* outError = nullptr);
    void Shutdown();
    ID3D11ShaderResourceView* GetSRV() const { return srv_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
};
