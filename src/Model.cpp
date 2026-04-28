#include "Model.h"

#include <array>

namespace
{
bool Failed(HRESULT result)
{
    return FAILED(result);
}
} // namespace

bool Model::Initialize(ID3D11Device* device)
{
    return InitializeBuffers(device);
}

void Model::Shutdown()
{
    ShutdownBuffers();
}

void Model::Render(ID3D11DeviceContext* deviceContext)
{
    RenderBuffers(deviceContext);
}

bool Model::InitializeBuffers(ID3D11Device* device)
{
    const std::array<VertexType, 3> vertices = {{
        {{0.0f, 0.55f, 0.0f}, {0.28f, 0.85f, 1.0f}},
        {{0.55f, -0.45f, 0.0f}, {0.08f, 0.35f, 0.62f}},
        {{-0.55f, -0.45f, 0.0f}, {0.68f, 0.96f, 0.93f}},
    }};

    const std::array<unsigned long, 3> indices = {{0, 1, 2}};

    vertexCount_ = static_cast<int>(vertices.size());
    indexCount_ = static_cast<int>(indices.size());

    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(VertexType) * vertices.size());
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = vertices.data();

    if (Failed(device->CreateBuffer(&vertexBufferDesc, &vertexData, &vertexBuffer_)))
    {
        return false;
    }

    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(unsigned long) * indices.size());
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices.data();

    if (Failed(device->CreateBuffer(&indexBufferDesc, &indexData, &indexBuffer_)))
    {
        return false;
    }

    return true;
}

void Model::ShutdownBuffers()
{
    indexBuffer_.Reset();
    vertexBuffer_.Reset();
}

void Model::RenderBuffers(ID3D11DeviceContext* deviceContext)
{
    constexpr UINT stride = sizeof(VertexType);
    constexpr UINT offset = 0;

    deviceContext->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    deviceContext->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
