#pragma once

#include "Camera.h"
#include "ColorShader.h"
#include "D3DClass.h"
#include "Light.h"
#include "Model.h"
#include "Texture.h"

#include <DirectXMath.h>

#include <memory>

class Graphics
{
public:
    bool Initialize(HWND hwnd, int screenWidth, int screenHeight);
    void Shutdown();
    bool Frame(float deltaTime);
    void Resize(unsigned int width, unsigned int height);

private:
    bool Render(float deltaTime);
    void DrawImGuiPanel();

    std::unique_ptr<D3DClass> d3d_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Light> light_;
    std::unique_ptr<Texture> texture_;
    std::unique_ptr<Model> model_;
    std::unique_ptr<ColorShader> colorShader_;

    bool imguiInitialized_ = false;
    unsigned int screenWidth_ = 0;
    unsigned int screenHeight_ = 0;

    float yRotationDegrees_ = 0.0f;
    DirectX::XMFLOAT4 tintColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
};
