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

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

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
    void ApplyPreset(int index);
    void SaveCurrentPreset(int index);
    void LoadPresets();
    void SavePresets() const;

    // Before/after capture: renders preset x fixed camera shots at a fixed time and saves JPEGs.
    void StartCaptureSet(const std::string& label, bool quitWhenDone);
    bool BeginCaptureFrame();
    void EndCaptureFrame();

    struct ShaderPreset
    {
        float sunYawDeg = 34.5f;
        float sunElevationDeg = 20.0f;
        DirectX::XMFLOAT3 lightColor = {1.0f, 1.0f, 1.0f};
        float lightIntensity = 1.0f;
        DirectX::XMFLOAT3 ambientColor = {0.10f, 0.14f, 0.18f};
        float ambientIntensity = 0.35f;
        ColorShader::WaterParams water;
    };

    ShaderPreset MakePresetFromCurrent() const;
    void ApplyPresetValues(const ShaderPreset& preset);

    struct CaptureJob
    {
        int preset = 0;
        int shot = 0;
    };

    struct CaptureRestoreState
    {
        ShaderPreset preset;
        DirectX::XMFLOAT3 cameraPosition;
        DirectX::XMFLOAT3 cameraRotation;
        float cameraFovDeg;
        float elapsedTime;
    };

    std::unique_ptr<D3DClass> d3d_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Light> light_;
    std::unique_ptr<Texture> normalMap_;
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
    float sunYawDeg_ = 34.5f;
    float sunElevationDeg_ = 20.0f;
    DirectX::XMFLOAT3 lightColor_ = {1.0f, 1.0f, 1.0f};
    float lightIntensity_ = 1.0f;
    DirectX::XMFLOAT3 ambientColor_ = {0.10f, 0.14f, 0.18f};
    float ambientIntensity_ = 0.35f;
    float elapsedTime_ = 0.0f;
    bool skyboxVisible_ = true;
    ColorShader::WaterParams water_;
    std::string normalMapStatus_;
    std::array<ShaderPreset, 3> presets_{};

    std::vector<CaptureJob> captureQueue_;
    std::filesystem::path captureDir_;
    std::filesystem::path pendingCapturePath_;
    CaptureRestoreState captureRestore_{};
    bool quitAfterCapture_ = false;
    bool captureFinishedQuit_ = false;
    int captureDebugMode_ = 0;
    char captureLabel_[64] = "manual";
    std::string captureStatus_;
};
