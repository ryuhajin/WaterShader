#pragma once

#include "Camera.h"
#include "ColorShader.h"
#include "CubemapTexture.h"
#include "D3DClass.h"
#include "Light.h"
#include "Model.h"
#include "Skybox.h"
#include "SkyboxShader.h"
#include "Texture.h"

#include <DirectXMath.h>

#include <memory>

class Input;

class Graphics
{
public:
    bool Initialize(HWND hwnd, int screenWidth, int screenHeight);
    void Shutdown();
    bool Frame(float deltaTime, const Input& input);
    void Resize(unsigned int width, unsigned int height);

private:
    bool Render(float deltaTime);
    void DrawImGuiPanel();
    void UpdateCamera(float deltaTime, const Input& input);

    std::unique_ptr<D3DClass> d3d_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Light> light_;
    std::unique_ptr<Texture> texture_;
    std::unique_ptr<Model> model_;
    std::unique_ptr<ColorShader> colorShader_;
    std::unique_ptr<CubemapTexture> cubemap_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<SkyboxShader> skyboxShader_;

    bool imguiInitialized_ = false;
    unsigned int screenWidth_ = 0;
    unsigned int screenHeight_ = 0;

    DirectX::XMFLOAT3 modelRotation_ = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 cameraPosition_ = {0.0f, 0.0f, -2.5f};
    DirectX::XMFLOAT3 cameraRotation_ = {0.0f, 0.0f, 0.0f};
    float cameraFovDeg_ = 60.0f;
    float cameraMoveSpeed_ = 2.0f;
    float cameraTurnSpeed_ = 90.0f;
    DirectX::XMFLOAT4 tintColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
    float lightYawDeg_ = 45.0f;
    float lightPitchDeg_ = -45.0f;
    DirectX::XMFLOAT3 lightColor_ = {1.0f, 1.0f, 1.0f};
    float lightIntensity_ = 1.0f;
    float elapsedTime_ = 0.0f;
    bool skyboxVisible_ = true;
    float reflectionStrength_ = 0.4f;
    float fresnelPower_ = 5.0f;
};
