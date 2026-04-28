#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <vector>

class Model
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
        float color[3];
    };

    bool InitializeBuffers(ID3D11Device* device);
    void ShutdownBuffers();
    void RenderBuffers(ID3D11DeviceContext* deviceContext);

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer_;
    int vertexCount_ = 0;
    int indexCount_ = 0;
};
