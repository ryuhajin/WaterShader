#include "Model.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <cmath>
#include <filesystem>
#include <vector>

bool Model::LoadObj(const std::wstring& path, std::vector<VertexType>& outVertices, std::vector<unsigned long>& outIndices)
{
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;

    tinyobj::ObjReader reader;
    const std::filesystem::path objPath(path);
    if (!reader.ParseFromFile(objPath.string(), config))
    {
        return false;
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    outVertices.clear();
    outIndices.clear();

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Model::VertexType vertex{};

            vertex.position[0] = attrib.vertices[3 * index.vertex_index + 0];
            vertex.position[1] = attrib.vertices[3 * index.vertex_index + 1];
            vertex.position[2] = attrib.vertices[3 * index.vertex_index + 2];

            if (index.normal_index >= 0)
            {
                vertex.normal[0] = attrib.normals[3 * index.normal_index + 0];
                vertex.normal[1] = attrib.normals[3 * index.normal_index + 1];
                vertex.normal[2] = attrib.normals[3 * index.normal_index + 2];
            }
            else
            {
                vertex.normal[0] = 0.0f;
                vertex.normal[1] = 1.0f;
                vertex.normal[2] = 0.0f;
            }

            if (index.texcoord_index >= 0)
            {
                vertex.uv[0] = attrib.texcoords[2 * index.texcoord_index + 0];
                vertex.uv[1] = 1.0f - attrib.texcoords[2 * index.texcoord_index + 1];
            }
            else
            {
                vertex.uv[0] = 0.0f;
                vertex.uv[1] = 0.0f;
            }

            outVertices.push_back(vertex);
            outIndices.push_back(static_cast<unsigned long>(outIndices.size()));
        }
    }

    return !outVertices.empty();
}

bool Model::Initialize(ID3D11Device* device, const std::wstring& objPath)
{
    std::vector<VertexType> vertices;
    std::vector<unsigned long> indices;
    if (!LoadObj(objPath, vertices, indices))
    {
        return false;
    }
    return CreateBuffers(device, vertices, indices);
}

bool Model::InitializeGrid(ID3D11Device* device, int quadsPerSide, float halfExtent, float spacingGrowth)
{
    // t in [-1, 1] -> x = sign(t) * halfExtent * (e^(g|t|) - 1) / (e^g - 1)
    const float denom = std::exp(spacingGrowth) - 1.0f;
    auto remap = [&](float t)
    {
        const float x = halfExtent * (std::exp(spacingGrowth * std::fabs(t)) - 1.0f) / denom;
        return t < 0.0f ? -x : x;
    };

    const int vertsPerSide = quadsPerSide + 1;
    std::vector<VertexType> vertices;
    vertices.reserve(static_cast<size_t>(vertsPerSide) * vertsPerSide);
    for (int j = 0; j < vertsPerSide; ++j)
    {
        const float z = remap(-1.0f + 2.0f * j / quadsPerSide);
        for (int i = 0; i < vertsPerSide; ++i)
        {
            const float x = remap(-1.0f + 2.0f * i / quadsPerSide);
            // Same UV layout as the OBJ plane: 1 UV unit per 2 world units, u along +X, v along +Z.
            // (v used to run along -Z here, which laid every normal map mirrored front-to-back and
            // flipped its Z slopes against the shader's TBN.)
            vertices.push_back({ {x, 0.0f, z}, {0.0f, 1.0f, 0.0f}, {x * 0.5f + 0.5f, 0.5f + z * 0.5f} });
        }
    }

    std::vector<unsigned long> indices;
    indices.reserve(static_cast<size_t>(quadsPerSide) * quadsPerSide * 6);
    for (int j = 0; j < quadsPerSide; ++j)
    {
        for (int i = 0; i < quadsPerSide; ++i)
        {
            const unsigned long v0 = static_cast<unsigned long>(j * vertsPerSide + i);
            const unsigned long v1 = v0 + 1;
            const unsigned long v2 = v0 + vertsPerSide;
            const unsigned long v3 = v2 + 1;
            indices.insert(indices.end(), { v0, v2, v1, v1, v2, v3 });
        }
    }

    return CreateBuffers(device, vertices, indices);
}

bool Model::InitializeBox(ID3D11Device* device)
{
    // One quad per face (4 vertices, own normal). u/v span the face; the scene shader patterns by world
    // position, so the UVs are only a fallback.
    struct Face { float n[3]; float u[3]; float v[3]; };
    constexpr Face faces[] = {
        { { 1, 0, 0}, { 0, 0, 1}, { 0, 1, 0} },
        { {-1, 0, 0}, { 0, 0,-1}, { 0, 1, 0} },
        { { 0, 1, 0}, { 1, 0, 0}, { 0, 0, 1} },
        { { 0,-1, 0}, { 1, 0, 0}, { 0, 0,-1} },
        { { 0, 0, 1}, {-1, 0, 0}, { 0, 1, 0} },
        { { 0, 0,-1}, { 1, 0, 0}, { 0, 1, 0} },
    };

    std::vector<VertexType> vertices;
    std::vector<unsigned long> indices;
    for (const Face& f : faces)
    {
        const unsigned long base = static_cast<unsigned long>(vertices.size());
        constexpr float corners[4][2] = { {-1, -1}, {1, -1}, {1, 1}, {-1, 1} };
        for (const auto& c : corners)
        {
            VertexType v{};
            for (int k = 0; k < 3; ++k)
            {
                v.position[k] = 0.5f * (f.n[k] + c[0] * f.u[k] + c[1] * f.v[k]);
                v.normal[k] = f.n[k];
            }
            v.uv[0] = 0.5f + 0.5f * c[0];
            v.uv[1] = 0.5f - 0.5f * c[1];
            vertices.push_back(v);
        }
        indices.insert(indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    }
    return CreateBuffers(device, vertices, indices);
}

bool Model::InitializeCylinder(ID3D11Device* device, int segments)
{
    constexpr float kTwoPi = 6.28318530718f;
    std::vector<VertexType> vertices;
    std::vector<unsigned long> indices;

    // Side: a ring at the bottom and the top, smooth normals.
    for (int i = 0; i <= segments; ++i)
    {
        const float a = kTwoPi * i / segments;
        const float x = std::cos(a);
        const float z = std::sin(a);
        const float u = static_cast<float>(i) / segments;
        vertices.push_back({ {0.5f * x, -0.5f, 0.5f * z}, {x, 0.0f, z}, {u, 1.0f} });
        vertices.push_back({ {0.5f * x,  0.5f, 0.5f * z}, {x, 0.0f, z}, {u, 0.0f} });
    }
    for (int i = 0; i < segments; ++i)
    {
        const unsigned long b0 = static_cast<unsigned long>(2 * i);
        indices.insert(indices.end(), { b0, b0 + 1, b0 + 3, b0, b0 + 3, b0 + 2 });
    }

    // Caps: a center vertex and a ring each, flat normals.
    for (const float y : { -0.5f, 0.5f })
    {
        const unsigned long center = static_cast<unsigned long>(vertices.size());
        vertices.push_back({ {0.0f, y, 0.0f}, {0.0f, y * 2.0f, 0.0f}, {0.5f, 0.5f} });
        for (int i = 0; i <= segments; ++i)
        {
            const float a = kTwoPi * i / segments;
            vertices.push_back({ {0.5f * std::cos(a), y, 0.5f * std::sin(a)}, {0.0f, y * 2.0f, 0.0f},
                                 {0.5f + 0.5f * std::cos(a), 0.5f + 0.5f * std::sin(a)} });
        }
        for (int i = 0; i < segments; ++i)
        {
            indices.insert(indices.end(), { center, center + 1 + i, center + 2 + i });
        }
    }
    return CreateBuffers(device, vertices, indices);
}

void Model::Shutdown()
{
    ShutdownBuffers();
}

void Model::Render(ID3D11DeviceContext* deviceContext)
{
    RenderBuffers(deviceContext);
}

bool Model::CreateBuffers(ID3D11Device* device, const std::vector<VertexType>& vertices, const std::vector<unsigned long>& indices)
{
    vertexCount_ = static_cast<int>(vertices.size());
    indexCount_ = static_cast<int>(indices.size());

    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(VertexType) * vertices.size());
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = vertices.data();

    if (FAILED(device->CreateBuffer(&vertexBufferDesc, &vertexData, &vertexBuffer_)))
    {
        return false;
    }

    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(unsigned long) * indices.size());
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices.data();

    if (FAILED(device->CreateBuffer(&indexBufferDesc, &indexData, &indexBuffer_)))
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
