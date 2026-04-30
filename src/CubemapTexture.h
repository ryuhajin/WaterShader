#pragma once

#include <d3d11.h>
#include <wrl/client.h>

class CubemapTexture
{
public:
    bool Initialize(ID3D11Device* device, const wchar_t* ddsPath);
    void Shutdown();
    ID3D11ShaderResourceView* GetSRV() const { return srv_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
};
