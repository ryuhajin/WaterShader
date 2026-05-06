#pragma once

#include <cstdint>
#include <d3d11.h>
#include <string>
#include <wrl/client.h>

class Texture
{
public:
    bool Initialize(ID3D11Device* device, const wchar_t* path, std::wstring* outError = nullptr);
    bool InitializeFlat(ID3D11Device* device, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void Shutdown();
    ID3D11ShaderResourceView* GetSRV() const { return textureView_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView_;
};
