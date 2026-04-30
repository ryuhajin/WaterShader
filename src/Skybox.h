#pragma once

#include <d3d11.h>
#include <wrl/client.h>

class Skybox
{
public:
    bool Initialize(ID3D11Device* device);
    void Shutdown();
    void Render(ID3D11DeviceContext* deviceContext);
    int GetIndexCount() const { return indexCount_; }

private:
    struct VertexType
    {
        float position[3];
    };

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer_;
    int indexCount_ = 0;
};
