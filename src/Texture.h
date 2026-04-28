#pragma once

#include <d3d11.h>
#include <wrl/client.h>

class Texture
{
public:
    bool Initialize(ID3D11Device*, const wchar_t*) { return true; }
    void Shutdown() { textureView_.Reset(); }
    ID3D11ShaderResourceView* GetTexture() const { return textureView_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView_;
};

