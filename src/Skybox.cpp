#include "Skybox.h"

bool Skybox::Initialize(ID3D11Device* device)
{
    const VertexType vertices[8] = {
        {{-1.0f, -1.0f, -1.0f}}, // 0
        {{ 1.0f, -1.0f, -1.0f}}, // 1
        {{ 1.0f,  1.0f, -1.0f}}, // 2
        {{-1.0f,  1.0f, -1.0f}}, // 3
        {{-1.0f, -1.0f,  1.0f}}, // 4
        {{ 1.0f, -1.0f,  1.0f}}, // 5
        {{ 1.0f,  1.0f,  1.0f}}, // 6
        {{-1.0f,  1.0f,  1.0f}}, // 7
    };

    // Camera sits inside the cube; inside faces must be visible.
    // Our RS = FrontCCW + CullBack. Outside-CW winding becomes inside-CCW (front)
    // when seen from inside, surviving back-face culling.
    const unsigned long indices[36] = {
        0, 2, 1,  0, 3, 2,  // Front  (z=-1)
        5, 7, 4,  5, 6, 7,  // Back   (z=+1)
        4, 3, 0,  4, 7, 3,  // Left   (x=-1)
        1, 6, 5,  1, 2, 6,  // Right  (x=+1)
        3, 6, 2,  3, 7, 6,  // Top    (y=+1)
        4, 1, 5,  4, 0, 1,  // Bottom (y=-1)
    };

    indexCount_ = 36;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;

    if (FAILED(device->CreateBuffer(&vbDesc, &vbData, &vertexBuffer_)))
    {
        return false;
    }

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;

    if (FAILED(device->CreateBuffer(&ibDesc, &ibData, &indexBuffer_)))
    {
        return false;
    }

    return true;
}

void Skybox::Shutdown()
{
    indexBuffer_.Reset();
    vertexBuffer_.Reset();
}

void Skybox::Render(ID3D11DeviceContext* deviceContext)
{
    constexpr UINT stride = sizeof(VertexType);
    constexpr UINT offset = 0;

    deviceContext->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    deviceContext->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
