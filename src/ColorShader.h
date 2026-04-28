#pragma once

#include <d3d11.h>
#include <wrl/client.h>

class ColorShader
{
public:
    bool Initialize(ID3D11Device* device);
    void Shutdown();
    bool Render(ID3D11DeviceContext* deviceContext, int indexCount);

private:
    bool InitializeShader(ID3D11Device* device, const wchar_t* shaderPath);
    void ShutdownShader();
    void RenderShader(ID3D11DeviceContext* deviceContext, int indexCount);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> layout_;
};

