#pragma once

#include "Camera.h"
#include "ColorShader.h"
#include "CubemapTexture.h"
#include "D3DClass.h"
#include "GpuTimer.h"
#include "Light.h"
#include "Model.h"
#include "Skybox.h"
#include "SkyboxShader.h"
#include "TonemapShader.h"
#include "Texture.h"
#include "WaveMacro.h"

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
    // Settings windows, toggled with keys 1 / 2 / 3 (UpdateWindowToggles).
    void DrawViewWindow();  // presets, camera, scene, capture, debug view
    void DrawLightWindow(); // sky, sun, glint, ambient, tonemapping
    void DrawWaterWindow(); // water color, reflection, normal maps, waves
    void DrawStatsOverlay();
    void UpdateFrameStats(float deltaTime);
    void UpdateCamera(float deltaTime, const Input& input);
    void UpdateMouseDrag();
    void UpdateWindowToggles(const Input& input);
    void ApplyPreset(int index);
    void SaveCurrentPreset(int index);
    std::filesystem::path PresetFilePath() const; // --preset-file, else assets/shader_presets.txt
    void LoadPresets();
    void SavePresets() const;

    // Before/after capture: renders preset x fixed camera shots at a fixed time and saves JPEGs.
    void StartCaptureSet(const std::wstring& label, bool quitWhenDone, bool allowOverwrite = true);
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
        int environment = 0; // index into kEnvironments (sky + reflection cube map)
        float exposureEv = 0.0f; // tonemap pass multiplies the HDR scene by 2^EV
        int normalMapA = 0; // index into kNormalMaps: layer A (broad ripples)
        int normalMapB = 0; // layer B (fine chop, tiled detailScale times smaller)
        int rippleAlignA = 0; // 1 = rotate layer A so its ripples run with Wave 1 ("Align to wind")
        int rippleAlignB = 0;
        ColorShader::WaterParams water;
        WaveMacro waveMacro; // Simple wave controls; water.waves is what actually renders
    };

    ShaderPreset MakePresetFromCurrent() const;
    void ApplyPresetValues(const ShaderPreset& preset);

    // Everything that defines a framing: fixed capture shots, save slots and capture restore share it.
    struct CameraView
    {
        DirectX::XMFLOAT3 position = {0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 rotation = {0.0f, 0.0f, 0.0f}; // x = pitch, y = yaw (deg)
        float fovDeg = 60.0f;
        bool ocean = false;
        DirectX::XMFLOAT3 modelRotation = {0.0f, 0.0f, 0.0f}; // bench plane only
    };

    CameraView MakeViewFromCurrent() const;
    void ApplyView(const CameraView& view);
    void ApplyCameraShot(int shotIndex);
    void LoadCameraSlots();
    void SaveCameraSlots() const;

    struct CaptureJob
    {
        int preset = 0;
        int shot = 0;
    };

    struct CaptureRestoreState
    {
        ShaderPreset preset;
        CameraView view;
        float elapsedTime = 0.0f;
    };

    std::unique_ptr<D3DClass> d3d_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Light> light_;
    std::unique_ptr<Model> model_;
    std::unique_ptr<Model> oceanGrid_;
    std::unique_ptr<ColorShader> colorShader_;
    std::vector<std::unique_ptr<CubemapTexture>> environments_; // one per kEnvironments entry
    int environmentIndex_ = 0;
    // Water normal maps (kNormalMaps, plus the --normal-map file if given). Each layer picks its own.
    std::vector<std::unique_ptr<Texture>> normalMaps_;
    std::vector<std::string> normalMapNames_;
    int normalMapA_ = 0;
    int normalMapB_ = 0;
    int normalOverrideA_ = -1; // --normal-a / --normal-b / --normal-map: win over the preset
    int normalOverrideB_ = -1;
    std::vector<float> normalMapAxisDeg_; // per normal map: ripple travel axis (NormalMapEntry::rippleAxisDeg)
    bool rippleAlignA_ = false; // "Align to wind" per layer (saved in the presets)
    bool rippleAlignB_ = false;
    int rippleAlignOverride_ = -1; // --align-ripples: wins over the presets
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<SkyboxShader> skyboxShader_;
    std::unique_ptr<TonemapShader> tonemapShader_;

    // Final pass: exposure (per preset, EV) and tone curve (global, --tonemap to override).
    float exposureEv_ = 0.0f;
    bool hasExposureOverride_ = false;
    float exposureOverrideEv_ = 0.0f;
    TonemapShader::Operator tonemapOperator_ = TonemapShader::AcesHuePreserving;

    bool imguiInitialized_ = false;
    unsigned int screenWidth_ = 0;
    unsigned int screenHeight_ = 0;

    // Initial values come from the default capture shot (ApplyCameraShot in Initialize).
    DirectX::XMFLOAT3 modelRotation_ = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 cameraPosition_ = {0.0f, 0.0f, 0.0f};
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
    bool oceanMode_ = false;
    ColorShader::WaterParams water_;
    WaveMacro waveMacro_; // Simple wave sliders; moving one regenerates water_.waves
    bool farWaveNormals_ = true; // --far-waves off / Water window checkbox
    std::string normalMapStatus_;
    std::array<ShaderPreset, 3> presets_{};

    // Stats overlay (top-left). CPU ms = Render() start to just before Present (excludes vsync wait).
    GpuTimer gpuTimer_;
    float cpuFrameMs_ = 0.0f;
    float displayedFps_ = 0.0f;
    float displayedCpuMs_ = 0.0f;
    float displayedGpuMs_ = 0.0f;
    float statsAccumTime_ = 0.0f;
    int statsAccumFrames_ = 0;
    float statsAccumCpuMs_ = 0.0f;

    static constexpr int kCameraSlotCount = 4;
    std::array<CameraView, kCameraSlotCount> cameraSlots_{};
    std::array<bool, kCameraSlotCount> cameraSlotUsed_{};

    std::vector<CaptureJob> captureQueue_;
    std::filesystem::path captureDir_;
    std::filesystem::path pendingCapturePath_;
    CaptureRestoreState captureRestore_{};
    bool quitAfterCapture_ = false;
    bool captureFinishedQuit_ = false;
    int captureDebugMode_ = 0;
    std::wstring captureFeature_ = L"water-polish";
    std::filesystem::path presetFileOverride_; // --preset-file
    char captureLabel_[64] = "manual";

    // Settings windows (closed at start; --ui opens them for screenshots).
    bool showViewWindow_ = false;
    bool showLightWindow_ = false;
    bool showWaterWindow_ = false;
    std::array<bool, 3> toggleKeyWasDown_ = {}; // edge detection for keys 1 / 2 / 3
    // Last flow direction per normal layer, so the direction slider keeps its value at speed 0.
    std::array<float, 2> flowDirectionDeg_ = {0.0f, 0.0f};
    std::string captureStatus_;
};
