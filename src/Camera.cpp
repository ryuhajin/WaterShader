#include "Camera.h"

void Camera::SetPosition(float x, float y, float z)
{
    position_ = {x, y, z};
}

void Camera::SetRotation(float x, float y, float z)
{
    rotation_ = {x, y, z};
}

void Camera::Render()
{
    using namespace DirectX;

    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR position = XMLoadFloat3(&position_);
    XMVECTOR lookAt = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

    const float pitch = XMConvertToRadians(rotation_.x);
    const float yaw = XMConvertToRadians(rotation_.y);
    const float roll = XMConvertToRadians(rotation_.z);

    const XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
    lookAt = XMVector3TransformCoord(lookAt, rotationMatrix);
    up = XMVector3TransformCoord(up, rotationMatrix);
    lookAt = XMVectorAdd(position, lookAt);

    viewMatrix_ = XMMatrixLookAtLH(position, lookAt, up);
}

