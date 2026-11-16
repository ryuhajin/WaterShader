#pragma once

#include "Camera.h"
#include "ColorShader.h"
#include "CubemapTexture.h"
#include "D3DClass.h"
#include "GpuTimer.h"
#include "Light.h"
#include "Model.h"
#include "SceneShader.h"
#include "Skybox.h"
#include "SkyboxShader.h"
#include "TonemapShader.h"
#include "Texture.h"
#include "WaveMacro.h"

#include <DirectXMath.h>

#include <array>
#include <filesystem>
#include <map>
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
    // Settings windows, toggled with keys 1 / 2 / 3 / 4 (UpdateWindowToggles).
    void DrawViewWindow();    // presets, camera, scene, capture, debug view
    void DrawLightWindow();   // sky, sun, glint, ambient, underwater light (New water), tonemapping
    void DrawWaterWindow();   // the water version's own settings (underwater, foam), then the reflection
    void DrawNewWaterSettings(); // test scenes: underwater, refraction, contact foam
    void DrawUnderwaterLightSettings(); // test scenes: water type, light loss, water color (Light window)
    void DrawSurfaceWindow(); // shape and motion of the surface, both versions (DrawSurfaceSettings)
    void DrawSurfaceSettings();  // normal maps, ocean detail, waves
    void DrawStatsOverlay();
    void UpdateFrameStats(float deltaTime);
    void UpdateCamera(float deltaTime, const Input& input);
    void UpdateMouseDrag();
    void UpdateWindowToggles(const Input& input);
    void ApplyPreset(int theme); // the version for the current mesh (bench plane / ocean grid)
    void SaveCurrentPreset(int theme);
    std::filesystem::path PresetFilePath() const; // --preset-file, else assets/shader_presets.txt
    void LoadPresets();
    void SavePresets() const;

    // Before/after capture: renders preset x fixed camera shots at a fixed time and saves JPEGs.
    void StartCaptureSet(const std::wstring& label, bool quitWhenDone, bool allowOverwrite = true);
    void FinishCaptureSet();
    bool UsesStoredNormal() const; // normal-gbuffer: the opaque pass writes the scene normal buffer this frame
    DirectX::XMFLOAT4 SceneWaterArea() const; // wave-occlusion: x = water patch half size, y = wave edge fade width
    void EndBenchFrame();
    std::filesystem::path CapturesRoot() const; // <capture root>/<feature>/captures
    bool BeginCaptureFrame();
    void EndCaptureFrame();
    void DrainD3dMessages();                                      // debug layer messages so far -> d3dMessages_
    void WriteD3dMessages(const std::filesystem::path& path);    // debug layer log of this run

    // Sun used only while the camera sits exactly on the fixed "top" shot. Looking straight down, the glint
    // shows only with the sun in front of the plane (high), which the other shots and the theme don't want.
    struct TopViewSun
    {
        int enabled = 0;
        float yawDeg = 0.0f;
        float elevationDeg = 90.0f;
        float intensity = 1.0f; // color is the main sun's
    };

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
        TopViewSun topSun;
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
        int scene = 0; // TestScene: opaque test scene around the water (needs ocean = true)
    };

    // Opaque scenes under / around a finite water patch (water-lab). They use the ocean grid presets
    // (same 1 unit = 1 m scale) with the Gerstner waves off unless "Scene Waves" is on.
    enum TestScene { NoScene = 0, DeepStair = 1, ShallowRail = 2 };

    struct SceneObject
    {
        bool cylinder = false; // unit cylinder, else unit box
        DirectX::XMFLOAT3 center = {0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 size = {1.0f, 1.0f, 1.0f};
        SceneShader::Material material;
    };

    static std::vector<SceneObject> BuildDeepStair();    // known depths: pads, stairs, pillar, near-surface pads
    static std::vector<SceneObject> BuildShallowRail();  // graybox of the target scene
    bool IsNumericView() const; // meters-as-color views: no sky, no lit pixels (depth-reconstruct)
    // Scene Debug <-> water Debug Mode take turns in the test scenes (the other is parked, then restored).
    static void SwapDebugViews(int& changed, int& changedParked, int& other, int& otherParked);
    void RenderTestScene(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection,
                         const DirectX::XMFLOAT4& lightDir, const DirectX::XMFLOAT4& lightColor,
                         const DirectX::XMFLOAT4& ambientColor);
    // sun-shadow: the objects of the current test scene (plus the moving test box in the Deep Stair, when on).
    std::vector<SceneObject> CurrentSceneObjects() const;
    // Fits the sun's orthographic box around the scene and draws the objects' depth into the shadow map.
    // Fills shadowFrame_ (off: params.x = 0, nothing drawn).
    void RenderShadowPass(const DirectX::XMFLOAT4& lightDir);
    void DrawShadowSettings(); // Light window, Shadows section
    void DrawCausticsSettings(); // Light window, Caustics section
    SceneShader::Caustics MakeCaustics(const DirectX::XMFLOAT4& lightDir) const; // this frame's caustic inputs

    CameraView MakeViewFromCurrent() const;
    bool IsTopShotView() const; // camera exactly on the fixed "top" shot (position, rotation, mesh)
    void ApplyView(const CameraView& view);
    void ApplyCameraShot(int shotIndex);
    std::filesystem::path CameraFilePath() const; // --camera-file, else assets/camera_presets.txt
    void LoadCameraSlots();
    void SaveCameraSlots() const;

    struct CaptureJob
    {
        int preset = 0;
        int shot = 0; // index into kCaptureShots, or kCaptureShots size + camera slot (slot1..slot4)
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
    // water-lab: unit shapes, the finite water patch per scene, the opaque shader and the scene layouts.
    std::unique_ptr<Model> unitBox_;
    std::unique_ptr<Model> unitCylinder_;
    std::unique_ptr<Model> stairWater_;
    std::unique_ptr<Model> railWater_;
    std::unique_ptr<SceneShader> sceneShader_;
    std::vector<SceneObject> stairObjects_;
    std::vector<SceneObject> railObjects_;
    int scene_ = NoScene;
    bool showWater_ = true;
    bool sceneWaves_ = true; // test scenes: Gerstner waves from the Surface window (wave-occlusion: on by default)
    int sceneDebug_ = 0; // 0 render, 1 depth below the water (the water is not drawn)
    int parkedSceneDebug_ = 0; // Scene Debug view put aside while a water Debug Mode is on (SwapDebugViews)
    int parkedWaterDebug_ = 0; // water Debug Mode put aside while a Scene Debug view is on
    int transmissionMode_ = 3; // test scenes: 0 opaque water (as before), 1 pass-through (scene-color-depth),
                               // 2 attenuation (rgb-attenuation), 3 scattering (water-scatter)
    int lastNewTransmission_ = 3; // the New water's mode to go back to after the version switch picked Old
    // Water version on screen: New = the water below (test scenes, transmission 1-3), otherwise the Old opaque water.
    bool IsNewWater() const { return scene_ != NoScene && transmissionMode_ != 0; }
    float maxUnderwaterDistance_ = 50.0f; // m: view path where no surface is behind the water (depth-reconstruct)
    bool surfaceReflection_ = true; // test scenes: sky and sun mirrored on top of the water below (surface-reflection)
    int normalSource_ = 0;          // test scenes: 0 normal from the depth buffer, 1 stored scene normal (normal-gbuffer)
    int waterOcclusion_ = 2;        // test scenes: 0 none (step 6), 1 single pass writes the water depth, 2 depth prepass + EQUAL
    // sun-shadow (test scenes, not saved in presets): one 1024 x 1024 shadow map from the sun.
    bool shadowsEnabled_ = true;
    int shadowFilter_ = 1;              // 0 hard, 1 PCF 3 x 3, 2 PCSS
    int shadowBiasMode_ = 4;            // 0 none, 1 constant, 2 slope-scaled, 3 normal offset, 4 all
    float shadowConstantBias_ = 0.02f;  // m
    float shadowSlopeBias_ = 1.0f;      // texels x tan(angle to the sun)
    float shadowNormalOffset_ = 1.0f;   // texels along the surface normal
    float shadowSunSizeDeg_ = 2.0f;     // PCSS: angular diameter of the sun (the real sun is 0.53 deg)
    bool shadowTestBox_ = false;        // Deep Stair: a box circling above the water (its shadow should follow)
    ShadowFrame shadowFrame_{};         // this frame's shadow inputs for the receivers
    float shadowBoxWidth_ = 0.0f;       // m: side of the sun's box this frame (texel = width / 1024)
    float captureTime_ = 12.0f;         // --capture-time: the fixed clock of captured frames (s)
    // caustics (test scenes, not saved in presets): the sunlight pattern on surfaces under the water.
    bool causticsEnabled_ = true;
    float causticStrength_ = 0.8f;      // 0 = flat sunlight; 1 = the dark gaps reach black
    float causticPatternSize_ = 2.0f;   // m: one repeat of the baked pattern (its large cells are a sixth of this)
    float causticFlowSpeed_ = 1.0f;     // how fast the pattern bends and drifts (1 = default)
    float causticSharpness_ = 1.0f;     // 1 = the texture's full detail, 0 = three mip levels softer
    float causticFadeDepth_ = 5.0f;     // m: the pattern's contrast is gone at this depth (user decision 2026-11-10)
    float causticWaveSway_ = 1.0f;      // the big waves bend the light sideways (1 = Snell, 0 = off)
    float causticWaveBands_ = 1.0f;     // crests gather the light into bands, troughs spread it (1 = lens strength, 0 = off)
    int causticProjection_ = 0;         // 0 = along the bent sun ray, 1 = straight down (comparison)
    bool causticFreeze_ = false;        // hold the pattern still (check that it sticks to the floor)
    float causticFreezeTime_ = 0.0f;    // the clock the pattern was frozen at
    std::unique_ptr<Texture> causticMap_; // assets/textures/caustics.dds (tools/bake_foam_masks.ps1)
    // Rendering review (2026-11-09), test scenes, not saved in presets:
    bool downwelling_ = true;           // light reaching points under the water fades on the way down (--downwelling)
    bool fresnelF0FromIor_ = false;     // F0 = ((n - 1) / (n + 1))^2 from the refraction IOR instead of the slider (--f0-from-ior)
    // Debug layer messages (Debug builds), counted per text; written next to a capture set.
    std::map<std::string, int> d3dMessages_;
    size_t d3dMessageTotal_ = 0;
    std::vector<std::unique_ptr<CubemapTexture>> environments_; // one per kEnvironments entry
    int environmentIndex_ = 0;
    // Water normal maps (kNormalMaps, plus the --normal-map file if given). Each layer picks its own.
    std::vector<std::unique_ptr<Texture>> normalMaps_;
    std::vector<std::unique_ptr<Texture>> foamMaps_; // contact foam masks, one per kFoamMaps entry (foam-masks)
    std::vector<std::string> normalMapNames_;
    int normalMapA_ = 0;
    int normalMapB_ = 0;
    int normalOverrideA_ = -1; // --normal-a / --normal-b / --normal-map: win over the preset
    int foamMapOverride_ = -1; // --foam-map <index>: wins over the preset (foam-masks)
    int normalOverrideB_ = -1;
    std::vector<float> normalMapAxisDeg_; // per normal map: ripple travel axis (NormalMapEntry::rippleAxisDeg)
    std::vector<float> normalMapSlopeVariance_; // per normal map: NormalMapEntry::slopeVariance
    bool rippleAlignA_ = false; // "Align to wind" per layer (saved in the presets)
    bool rippleAlignB_ = false;
    int rippleAlignOverride_ = -1; // --align-ripples: wins over the presets
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<SkyboxShader> skyboxShader_;
    std::unique_ptr<TonemapShader> tonemapShader_;

    // Final pass: exposure (per preset, EV) and tone curve (global, --tonemap to override).
    float exposureEv_ = 0.0f;
    bool hasExposureOverride_ = false;
    bool hasExtinctionOverride_ = false; // --extinction: wins over the presets
    DirectX::XMFLOAT3 extinctionOverride_ = {0.0f, 0.0f, 0.0f};
    int waterTypeOverride_ = -1;            // --water-type clear|green|lagoon (water-scatter)
    int rampByOverride_ = -1;               // --ramp-by depth|path: 0 / 1
    bool hasScatterOverride_ = false;       // --scatter r,g,b,strength (sRGB color)
    DirectX::XMFLOAT4 scatterOverride_ = {0.0f, 0.0f, 0.0f, 0.0f};
    float foamStrengthOverride_ = -1.0f;    // --foam <strength> (contact-foam), negative = presets decide
    float foamSlopeOverride_ = -1.0f;       // --foam-slope <0..1>: slope filter
    float refractionOverride_ = -1.0f;      // --refraction <strength> (refraction, both modes), negative = presets decide
    int refractionModeOverride_ = -1;       // --refraction-mode normal|physical: 0 / 1
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
    TopViewSun topSun_;
    float elapsedTime_ = 0.0f;
    bool skyboxVisible_ = true;
    bool oceanMode_ = false;
    ColorShader::WaterParams water_;
    WaveMacro waveMacro_; // Simple wave sliders; moving one regenerates water_.waves
    bool farWaveNormals_ = true; // --far-waves off / Water window checkbox
    std::string normalMapStatus_;
    // Presets per mesh: [0] = bench plane, [1] = ocean grid; each holds Basic / Sunset / Tropical.
    // The 2x2 bench plane and the open ocean need different waves, so every theme has one of each.
    static constexpr int kPresetThemeCount = 3;
    using PresetSet = std::array<ShaderPreset, kPresetThemeCount>;
    std::array<PresetSet, 2> presets_{};
    int activeTheme_ = 0;          // last preset applied or saved; switching mesh re-applies it
    bool presetOceanMesh_ = false; // which mesh the current values belong to (oceanMode_ at apply time)

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
    // --bench (contact-foam): the capture queue, timed instead of saved.
    static constexpr int kBenchWarmupFrames = 60;
    bool benchMode_ = false;
    bool benchJobActive_ = false;
    bool benchRestoreVsync_ = true;
    int benchFrame_ = 0;
    int benchSampleCount_ = 300;
    unsigned long long benchLastResolved_ = 0;
    std::string benchJobName_;
    std::vector<std::array<float, 5>> benchSamples_; // gpu frame, opaque, water, post, cpu (ms)
    std::string benchCsv_;
    std::wstring captureFeature_ = L"water-polish";
    std::filesystem::path captureRootOverride_; // --capture-root
    std::filesystem::path presetFileOverride_; // --preset-file
    std::filesystem::path cameraFileOverride_; // --camera-file
    std::vector<std::string> capturePresetFilter_; // --capture-presets (file names, e.g. "hero"); empty = all
    std::vector<std::string> captureShotFilter_;   // --capture-shots (shot names or slot1..slot4); empty = all fixed shots
    bool capturePng_ = false;                      // --capture-format png (lossless; default JPEG)
    bool fixedRenderSize_ = false;                 // --render-size: back buffer size independent of the window
    char captureLabel_[64] = "manual";

    // Settings windows (closed at start; --ui opens them for screenshots).
    bool showViewWindow_ = false;
    bool showLightWindow_ = false;
    bool showWaterWindow_ = false;
    bool showSurfaceWindow_ = false;
    std::array<bool, 4> toggleKeyWasDown_ = {}; // edge detection for keys 1 / 2 / 3 / 4
    bool swapKeyWasDown_ = false; // edge detection for X (swap the two debug views)
    // Last flow direction per normal layer, so the direction slider keeps its value at speed 0.
    std::array<float, 2> flowDirectionDeg_ = {0.0f, 0.0f};
    std::string captureStatus_;
};
