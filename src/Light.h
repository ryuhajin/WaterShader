#pragma once

#include <DirectXMath.h>

class Light
{
public:
    void SetDiffuseColor(float red, float green, float blue, float alpha);
    void SetDirection(float x, float y, float z);

    DirectX::XMFLOAT4 GetDiffuseColor() const { return diffuseColor_; }
    DirectX::XMFLOAT3 GetDirection() const { return direction_; }

private:
    DirectX::XMFLOAT4 diffuseColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3 direction_ = {0.0f, -1.0f, 0.0f};
};

