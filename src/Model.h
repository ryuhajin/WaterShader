#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <string>
#include <vector>

class Model
{
public:
    bool Initialize(ID3D11Device* device, const std::wstring& objPath);
    // Flat XZ grid for open water. Vertex spacing grows exponentially from the center
    // (dense near the camera, sparse toward the horizon); UVs match the 2x2 OBJ plane density.
    bool InitializeGrid(ID3D11Device* device, int quadsPerSide, float halfExtent, float spacingGrowth);
    // Unit shapes for the opaque test scenes (water-lab), sized and placed by each object's world matrix.
    bool InitializeBox(ID3D11Device* device);                     // [-0.5, 0.5]^3, flat normals per face
    bool InitializeCylinder(ID3D11Device* device, int segments);  // radius 0.5, y in [-0.5, 0.5], capped
    void Shutdown();
    void Render(ID3D11DeviceContext* deviceContext);
    int GetIndexCount() const { return indexCount_; }

private:
    struct VertexType
    {
        float position[3];
        float normal[3];
        float uv[2];
    };

    bool CreateBuffers(ID3D11Device* device, const std::vector<VertexType>& vertices, const std::vector<unsigned long>& indices);
    void ShutdownBuffers();
    void RenderBuffers(ID3D11DeviceContext* deviceContext);
    static bool LoadObj(const std::wstring& path, std::vector<VertexType>& outVertices, std::vector<unsigned long>& outIndices);

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer_;
    int vertexCount_ = 0;
    int indexCount_ = 0;
};
