#pragma once

#include <DirectXMath.h>

class Camera
{
public:
    void SetPosition(float x, float y, float z);
    void SetRotation(float x, float y, float z);
    void Render();

    const DirectX::XMMATRIX& GetViewMatrix() const { return viewMatrix_; }

private:
    DirectX::XMFLOAT3 position_ = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 rotation_ = {0.0f, 0.0f, 0.0f};
    DirectX::XMMATRIX viewMatrix_ = DirectX::XMMatrixIdentity();
};

