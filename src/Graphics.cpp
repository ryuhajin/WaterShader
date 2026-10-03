#include "Graphics.h"

#include "ColorSpace.h"
#include "Input.h"
#include "SystemConfig.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <ScreenGrab.h>
#include <shellapi.h>
#include <wincodec.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>

namespace
{
std::wstring GetAssetPath(const wchar_t* relativePath)
{
#ifdef WATERSHADER_ASSETS_DIR
    std::filesystem::path path = WATERSHADER_ASSETS_DIR;
    path /= relativePath;
    return path.wstring();
#else
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    std::filesystem::path path(modulePath);
    path = path.parent_path() / L"assets" / relativePath;
    return path.wstring();
#endif
}

// std::string(ws.begin(), ws.end()) truncates each wchar_t to one byte (C4244) and mangles any
// non-ASCII path. ImGui and our logs expect UTF-8, so convert explicitly.
std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

struct CaptureShot
{
    const char* name;
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 rotation; // x = pitch, y = yaw (deg)
    float fovDeg;
    bool ocean; // true = large ocean grid, false = 2x2 bench plane
};

// Fixed camera shots for before/after comparison. Bench plane spans x,z in [-1, 1].
// "sunward" looks along the skybox sun (yaw ~33 deg) from a low angle to catch the glint path.
// ocean_* shots use the large grid and only exist from step 6 on.
constexpr CaptureShot kCaptureShots[] = {
    { "oblique",       {  0.00f, 1.00f, -2.20f }, { 24.0f,  0.0f, 0.0f }, 60.0f, false },
    { "top",           {  0.00f, 2.40f,  0.00f }, { 89.9f,  0.0f, 0.0f }, 60.0f, false },
    { "sunward",       { -1.04f, 0.45f, -1.59f }, { 12.0f, 33.0f, 0.0f }, 60.0f, false },
    { "ocean_sunward", { -0.90f, 0.55f, -1.40f }, {  6.0f, 34.5f, 0.0f }, 55.0f, true  },
    { "ocean_wide",    {  0.00f, 1.60f, -3.00f }, { 14.0f, 10.0f, 0.0f }, 60.0f, true  },
};

// Startup / Reset Camera framing: the step2_sun_glint "sunward" shot (bench plane, glint visible).
constexpr int kDefaultShotIndex = 2;

constexpr const char* kPresetFileNames[] = { "basic", "sunset", "tropical" };

constexpr int kPresetFileVersion = 3;

// Optional "key value" pairs appended after the fixed preset fields. Missing keys keep the
// code defaults and unknown keys are skipped, so new parameters don't need a format bump.
template <typename Preset, typename Fn>
void ForEachExtraField(Preset& preset, Fn&& fn)
{
    fn("sunGlintPower", preset.water.sunGlintPower);
    fn("sunGlintIntensity", preset.water.sunGlintIntensity);
    fn("fresnelF0", preset.water.fresnelF0);
    fn("normalStrength", preset.water.normalStrength);
    fn("detailScale", preset.water.detailScale);
    fn("environment", preset.environment);
    fn("exposureEV", preset.exposureEv);
}

// Sky + reflection cube maps a preset can pick. Sun directions were measured from each panorama
// (tools/equirect_to_cube.ps1). The sunset sky was rotated so its sun sits at yaw 34.5 like
// skybox.dds (the fixed "sunward" shots face it); the beach was rotated so its open sea faces the
// shots instead, which puts its sun behind the camera (yaw 236.4).
struct Environment
{
    const wchar_t* file;
    const char* name;
    float sunYawDeg;
    float sunElevationDeg;
};

constexpr Environment kEnvironments[] = {
    { L"textures/skybox.dds",          "Meadow dusk (overcast)",                   34.5f,  4.0f },
    { L"textures/env_sunset_fire.dds", "Sunset sea (PH: the_sky_is_on_fire)",      34.5f,  6.4f },
    { L"textures/env_beach_day.dds",   "Beach day (PH: spiaggia_di_mondello)",    236.4f, 25.3f },
};
constexpr int kEnvironmentCount = static_cast<int>(std::size(kEnvironments));

constexpr float kCaptureTime = 12.0f;
} // namespace

bool Graphics::Initialize(HWND hwnd, int screenWidth, int screenHeight)
{
    screenWidth_ = static_cast<unsigned int>(screenWidth);
    screenHeight_ = static_cast<unsigned int>(screenHeight);

    // Command line:
    //   --capture <label>      save every preset x shot, then quit
    //   --debug <mode>         debug view used for the capture set
    //   --normal-map <path>    normal map relative to assets/, tried before the default DDS
    //   --shot <name>          start from a fixed camera shot (e.g. ocean_wide)
    //   --no-vsync             start uncapped (for FPS / GPU ms measurement)
    //   --capture-feature <f>  captures go to docs/features/<f>/captures (default water-polish)
    //   --tonemap <none|reinhard|aces|aces-hue>  tone curve for this run
    //   --exposure <ev>        overrides every preset's exposure (operator comparisons)
    std::wstring captureLabelArg;
    std::wstring normalMapArg;
    std::wstring shotArg;
    bool vsyncArg = VSYNC_ENABLED;
    {
        int argc = 0;
        if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc))
        {
            for (int i = 1; i < argc; ++i)
            {
                const std::wstring arg = argv[i];
                const bool hasValue = i + 1 < argc;
                if (arg == L"--no-vsync")                    { vsyncArg = false; }
                else if (arg == L"--capture" && hasValue)    { captureLabelArg = argv[++i]; }
                else if (arg == L"--debug" && hasValue)      { captureDebugMode_ = _wtoi(argv[++i]); }
                else if (arg == L"--normal-map" && hasValue) { normalMapArg = argv[++i]; }
                else if (arg == L"--shot" && hasValue)       { shotArg = argv[++i]; }
                else if (arg == L"--capture-feature" && hasValue) { captureFeature_ = argv[++i]; }
                else if (arg == L"--exposure" && hasValue)   { exposureOverrideEv_ = static_cast<float>(_wtof(argv[++i])); hasExposureOverride_ = true; }
                else if (arg == L"--tonemap" && hasValue)
                {
                    const std::wstring op = argv[++i];
                    tonemapOperator_ = op == L"none" ? TonemapShader::None
                                     : op == L"reinhard" ? TonemapShader::Reinhard
                                     : op == L"aces-hue" ? TonemapShader::AcesHuePreserving
                                     : op == L"aces" ? TonemapShader::Aces
                                     : TonemapShader::AcesHuePreserving;
                }
            }
            LocalFree(argv);
        }
    }

    d3d_ = std::make_unique<D3DClass>();
    if (!d3d_->Initialize(screenWidth, screenHeight, vsyncArg, hwnd, FULL_SCREEN, SCREEN_DEPTH, SCREEN_NEAR))
    {
        return false;
    }

    camera_ = std::make_unique<Camera>();
    int startShot = kDefaultShotIndex;
    for (int i = 0; i < static_cast<int>(std::size(kCaptureShots)); ++i)
    {
        if (shotArg == Utf8ToWide(kCaptureShots[i].name))
        {
            startShot = i;
        }
    }
    ApplyCameraShot(startShot);
    LoadCameraSlots();

    light_ = std::make_unique<Light>();
    light_->SetDirection(0.0f, -1.0f, 1.0f);
    light_->SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);

    normalMap_ = std::make_unique<Texture>();
    {
        struct Attempt { std::wstring relPath; const char* label; };
        std::vector<Attempt> attempts = {
            { L"textures/water_normal.dds", "DDS" },
            { L"textures/water_normal.png", "PNG" },
            { L"textures/water_normal.jpg", "JPG" },
        };
        if (!normalMapArg.empty())
        {
            attempts.insert(attempts.begin(), { normalMapArg, "--normal-map" });
        }
        std::string statusLog;
        bool loaded = false;
        for (const auto& a : attempts)
        {
            const std::wstring path = GetAssetPath(a.relPath.c_str());
            const std::string pathUtf8 = WideToUtf8(path);
            std::wstring err;
            if (normalMap_->Initialize(d3d_->GetDevice(), path.c_str(), &err))
            {
                statusLog += std::string("[OK]   ") + a.label + " loaded -> " + pathUtf8 + "\n";
                loaded = true;
                break;
            }
            statusLog += std::string("[FAIL] ") + a.label + " " + WideToUtf8(err) + " -> " + pathUtf8 + "\n";
        }
        if (!loaded)
        {
            // Fallback: 1x1 flat tangent normal so the shader path stays exercised.
            normalMap_->InitializeFlat(d3d_->GetDevice(), 128, 128, 255, 255);
            statusLog += "[FALLBACK] flat (128,128,255) - all sources failed\n";
        }
        normalMapStatus_ = statusLog;
        OutputDebugStringA(("[NormalMap]\n" + statusLog).c_str());
    }

    model_ = std::make_unique<Model>();
    if (!model_->Initialize(d3d_->GetDevice(), GetAssetPath(L"models/32x32Plane.obj")))
    {
        return false;
    }

    // 1024x1024 quads out to +-400 units: ~0.012 spacing at the center, ~0.25 at 20 units, reaching
    // near the horizon. Together with the wave fade in the VS this keeps >= 5 vertices per wavelength.
    oceanGrid_ = std::make_unique<Model>();
    if (!oceanGrid_->InitializeGrid(d3d_->GetDevice(), 1024, 400.0f, 6.0f))
    {
        return false;
    }

    colorShader_ = std::make_unique<ColorShader>();
    if (!colorShader_->Initialize(d3d_->GetDevice()))
    {
        return false;
    }

    for (const Environment& env : kEnvironments)
    {
        auto cubemap = std::make_unique<CubemapTexture>();
        const std::wstring cubemapPath = GetAssetPath(env.file);
        std::wstring cubemapError;
        if (!cubemap->Initialize(d3d_->GetDevice(), cubemapPath.c_str(), &cubemapError))
        {
            const std::wstring msg = cubemapPath + L"\n\n" + cubemapError;
            MessageBoxW(hwnd, msg.c_str(), L"WaterShader: cubemap load failed", MB_ICONERROR | MB_OK);
            return false;
        }
        environments_.push_back(std::move(cubemap));
    }

    skybox_ = std::make_unique<Skybox>();
    if (!skybox_->Initialize(d3d_->GetDevice()))
    {
        return false;
    }

    skyboxShader_ = std::make_unique<SkyboxShader>();
    if (!skyboxShader_->Initialize(d3d_->GetDevice()))
    {
        return false;
    }

    tonemapShader_ = std::make_unique<TonemapShader>();
    if (!tonemapShader_->Initialize(d3d_->GetDevice()))
    {
        return false;
    }

    if (!ImGui_ImplDX11_Init(d3d_->GetDevice(), d3d_->GetDeviceContext()))
    {
        return false;
    }
    imguiInitialized_ = true;

    // Timing is a debugging aid; a device without timestamp queries just shows "n/a".
    gpuTimer_.Initialize(d3d_->GetDevice());

    LoadPresets();
    ApplyPreset(0);

    if (!captureLabelArg.empty())
    {
        StartCaptureSet(captureLabelArg, true);
    }

    return true;
}

void Graphics::ApplyPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(presets_.size()))
    {
        return;
    }

    const int debugMode = water_.debugMode;
    ApplyPresetValues(presets_[index]);
    water_.debugMode = debugMode;
}

void Graphics::ApplyPresetValues(const ShaderPreset& preset)
{
    sunYawDeg_ = preset.sunYawDeg;
    sunElevationDeg_ = preset.sunElevationDeg;
    lightColor_ = preset.lightColor;
    lightIntensity_ = preset.lightIntensity;
    ambientColor_ = preset.ambientColor;
    ambientIntensity_ = preset.ambientIntensity;
    environmentIndex_ = std::clamp(preset.environment, 0, kEnvironmentCount - 1);
    exposureEv_ = hasExposureOverride_ ? exposureOverrideEv_ : preset.exposureEv;
    water_ = preset.water;
}

Graphics::ShaderPreset Graphics::MakePresetFromCurrent() const
{
    ShaderPreset preset;
    preset.sunYawDeg = sunYawDeg_;
    preset.sunElevationDeg = sunElevationDeg_;
    preset.lightColor = lightColor_;
    preset.lightIntensity = lightIntensity_;
    preset.ambientColor = ambientColor_;
    preset.ambientIntensity = ambientIntensity_;
    preset.environment = environmentIndex_;
    preset.exposureEv = exposureEv_;
    preset.water = water_;
    return preset;
}

void Graphics::SaveCurrentPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(presets_.size()))
    {
        return;
    }

    presets_[index] = MakePresetFromCurrent();
    presets_[index].water.debugMode = 0;
    SavePresets();
}

void Graphics::StartCaptureSet(const std::wstring& label, bool quitWhenDone, bool allowOverwrite)
{
    captureQueue_.clear();
    for (int preset = 0; preset < static_cast<int>(presets_.size()); ++preset)
    {
        for (int shot = 0; shot < static_cast<int>(std::size(kCaptureShots)); ++shot)
        {
            captureQueue_.push_back({preset, shot});
        }
    }

    // assets/ -> project root -> docs/features/<feature>/captures/<label>
    const std::filesystem::path projectRoot =
        std::filesystem::path(GetAssetPath(L"shader_presets.txt")).parent_path().parent_path();
    const std::filesystem::path capturesRoot = projectRoot / "docs" / "features" / captureFeature_ / "captures";
    captureDir_ = capturesRoot / label;
    std::error_code ec;
    for (int suffix = 2; !allowOverwrite && std::filesystem::exists(captureDir_, ec); ++suffix)
    {
        captureDir_ = capturesRoot / (label + L"_" + std::to_wstring(suffix));
    }
    std::filesystem::create_directories(captureDir_, ec);

    captureRestore_ = {MakePresetFromCurrent(), MakeViewFromCurrent(), elapsedTime_};
    quitAfterCapture_ = quitWhenDone;
    captureStatus_ = "Capturing -> " + WideToUtf8(captureDir_.wstring());
}

bool Graphics::BeginCaptureFrame()
{
    if (captureQueue_.empty())
    {
        return false;
    }

    const CaptureJob job = captureQueue_.front();
    captureQueue_.erase(captureQueue_.begin());

    ApplyPresetValues(presets_[job.preset]);
    water_.debugMode = captureDebugMode_;

    // Also resets the bench plane rotation, so mouse-drag state never leaks into captures.
    ApplyCameraShot(job.shot);
    const CaptureShot& shot = kCaptureShots[job.shot];

    // Fixed time so before/after frames show the same wave phase.
    elapsedTime_ = kCaptureTime;

    const std::string fileName = std::string(kPresetFileNames[job.preset]) + "_" + shot.name + ".jpg";
    pendingCapturePath_ = captureDir_ / fileName;
    return true;
}

void Graphics::EndCaptureFrame()
{
    // Delete first, then write a new file. Overwriting a batch of existing JPEGs in place from a
    // freshly built exe gets the whole process tree frozen mid-write (ransomware-style heuristic,
    // see docs/features/bench-tools/TROUBLESHOOTING.md); delete + create does not.
    std::error_code removeError;
    std::filesystem::remove(pendingCapturePath_, removeError);

    // JPEG keeps each step's capture set ~2MB in git instead of ~11MB as PNG.
    const auto backBuffer = d3d_->GetBackBuffer();
    const HRESULT hr = DirectX::SaveWICTextureToFile(
        d3d_->GetDeviceContext(),
        backBuffer.Get(),
        GUID_ContainerFormatJpeg,
        pendingCapturePath_.wstring().c_str(),
        nullptr,
        [](IPropertyBag2* props)
        {
            PROPBAG2 option = {};
            option.pstrName = const_cast<wchar_t*>(L"ImageQuality");
            VARIANT value;
            VariantInit(&value);
            value.vt = VT_R4;
            value.fltVal = 0.95f;
            props->Write(1, &option, &value);
        });
    OutputDebugStringW(((SUCCEEDED(hr) ? L"[Capture] saved " : L"[Capture] FAILED ") + pendingCapturePath_.wstring() + L"\n").c_str());

    if (!captureQueue_.empty())
    {
        return;
    }

    ApplyPresetValues(captureRestore_.preset);
    ApplyView(captureRestore_.view);
    elapsedTime_ = captureRestore_.elapsedTime;
    captureStatus_ = "Saved -> " + WideToUtf8(captureDir_.wstring());
    captureFinishedQuit_ = quitAfterCapture_;
}

Graphics::CameraView Graphics::MakeViewFromCurrent() const
{
    return {cameraPosition_, cameraRotation_, cameraFovDeg_, oceanMode_, modelRotation_};
}

void Graphics::ApplyView(const CameraView& view)
{
    cameraPosition_ = view.position;
    cameraRotation_ = view.rotation;
    cameraFovDeg_ = view.fovDeg;
    oceanMode_ = view.ocean;
    modelRotation_ = view.modelRotation;
    camera_->SetPosition(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z);
    camera_->SetRotation(cameraRotation_.x, cameraRotation_.y, cameraRotation_.z);
}

void Graphics::ApplyCameraShot(int shotIndex)
{
    const CaptureShot& shot = kCaptureShots[shotIndex];
    ApplyView({shot.position, shot.rotation, shot.fovDeg, shot.ocean, {0.0f, 0.0f, 0.0f}});
}

// assets/camera_presets.txt: one line per slot
//   used  pos.x pos.y pos.z  pitch yaw roll  fov  ocean  model.x model.y model.z
void Graphics::LoadCameraSlots()
{
    std::ifstream file(GetAssetPath(L"camera_presets.txt"));
    std::string header;
    int version = 0;
    if (!file || !(file >> header >> version) || header != "WaterShaderCameras" || version != 1)
    {
        return;
    }

    for (int i = 0; i < kCameraSlotCount; ++i)
    {
        CameraView& v = cameraSlots_[i];
        int used = 0;
        int ocean = 0;
        file >> used
             >> v.position.x >> v.position.y >> v.position.z
             >> v.rotation.x >> v.rotation.y >> v.rotation.z
             >> v.fovDeg >> ocean
             >> v.modelRotation.x >> v.modelRotation.y >> v.modelRotation.z;
        if (!file)
        {
            break;
        }
        v.ocean = ocean != 0;
        cameraSlotUsed_[i] = used != 0;
    }
}

void Graphics::SaveCameraSlots() const
{
    std::ofstream file(GetAssetPath(L"camera_presets.txt"));
    if (!file)
    {
        return;
    }

    file << "WaterShaderCameras 1\n";
    for (int i = 0; i < kCameraSlotCount; ++i)
    {
        const CameraView& v = cameraSlots_[i];
        file << (cameraSlotUsed_[i] ? 1 : 0) << ' '
             << v.position.x << ' ' << v.position.y << ' ' << v.position.z << ' '
             << v.rotation.x << ' ' << v.rotation.y << ' ' << v.rotation.z << ' '
             << v.fovDeg << ' ' << (v.ocean ? 1 : 0) << ' '
             << v.modelRotation.x << ' ' << v.modelRotation.y << ' ' << v.modelRotation.z << '\n';
    }
}

void Graphics::LoadPresets()
{
    presets_[0] = ShaderPreset{};

    presets_[1] = ShaderPreset{};
    presets_[1].sunElevationDeg = 5.0f;
    presets_[1].lightColor = {1.0f, 0.58f, 0.35f};
    presets_[1].lightIntensity = 1.2f;
    presets_[1].ambientColor = {0.18f, 0.10f, 0.16f};
    presets_[1].ambientIntensity = 0.45f;
    presets_[1].water.facingColor = {0.58f, 0.48f, 0.78f, 1.0f};
    presets_[1].water.grazingColor = {0.08f, 0.03f, 0.10f, 1.0f};
    presets_[1].water.reflectionStrength = 0.75f;
    presets_[1].water.fresnelPower = 4.0f;
    presets_[1].water.specularStrength = 0.35f;
    presets_[1].water.specularSharpness = 72.0f;

    presets_[2] = ShaderPreset{};
    presets_[2].sunElevationDeg = 32.0f;
    presets_[2].lightColor = {0.92f, 1.0f, 0.94f};
    presets_[2].lightIntensity = 1.35f;
    presets_[2].ambientColor = {0.08f, 0.22f, 0.24f};
    presets_[2].ambientIntensity = 0.55f;
    presets_[2].water.facingColor = {0.30f, 0.92f, 1.0f, 1.0f};
    presets_[2].water.grazingColor = {0.00f, 0.20f, 0.34f, 1.0f};
    presets_[2].water.reflectionStrength = 0.65f;
    presets_[2].water.fresnelPower = 3.5f;
    presets_[2].water.specularStrength = 0.30f;
    presets_[2].water.specularSharpness = 96.0f;

    const std::filesystem::path presetPath = GetAssetPath(L"shader_presets.txt");
    std::ifstream file(presetPath);
    if (!file)
    {
        return;
    }

    // v2: second field is sun elevation (> 0 above the horizon); v1 stored an inverted pitch.
    // v3: 4 Gerstner waves (dir.x dir.y amplitude wavelength speed steepness). Older files are ignored.
    std::string line;
    std::getline(file, line);
    std::istringstream headerStream(line);
    std::string header;
    int version = 0;
    headerStream >> header >> version;
    if (header != "WaterShaderPresets" || version != kPresetFileVersion)
    {
        return;
    }

    for (ShaderPreset& preset : presets_)
    {
        if (!std::getline(file, line))
        {
            break;
        }
        std::istringstream in(line);
        in
            >> preset.sunYawDeg
            >> preset.sunElevationDeg
            >> preset.lightColor.x >> preset.lightColor.y >> preset.lightColor.z
            >> preset.lightIntensity
            >> preset.ambientColor.x >> preset.ambientColor.y >> preset.ambientColor.z
            >> preset.ambientIntensity
            >> preset.water.facingColor.x >> preset.water.facingColor.y >> preset.water.facingColor.z
            >> preset.water.grazingColor.x >> preset.water.grazingColor.y >> preset.water.grazingColor.z
            >> preset.water.reflectionStrength
            >> preset.water.fresnelPower
            >> preset.water.normalScale
            >> preset.water.specularStrength
            >> preset.water.specularSharpness
            >> preset.water.normalScroll1.x >> preset.water.normalScroll1.y
            >> preset.water.normalScroll2.x >> preset.water.normalScroll2.y;

        for (auto& wave : preset.water.waves)
        {
            in
                >> wave.direction.x >> wave.direction.y
                >> wave.amplitude
                >> wave.wavelength
                >> wave.speed
                >> wave.steepness;
        }

        std::string key;
        float value = 0.0f;
        while (in >> key >> value)
        {
            ForEachExtraField(preset, [&](const char* name, auto& field)
            {
                if (key == name)
                {
                    field = static_cast<std::decay_t<decltype(field)>>(value);
                }
            });
        }
    }
}

void Graphics::SavePresets() const
{
    const std::filesystem::path presetPath = GetAssetPath(L"shader_presets.txt");
    std::filesystem::create_directories(presetPath.parent_path());

    std::ofstream file(presetPath);
    if (!file)
    {
        return;
    }

    file << "WaterShaderPresets " << kPresetFileVersion << '\n';
    for (const ShaderPreset& preset : presets_)
    {
        file
            << preset.sunYawDeg << ' '
            << preset.sunElevationDeg << ' '
            << preset.lightColor.x << ' ' << preset.lightColor.y << ' ' << preset.lightColor.z << ' '
            << preset.lightIntensity << ' '
            << preset.ambientColor.x << ' ' << preset.ambientColor.y << ' ' << preset.ambientColor.z << ' '
            << preset.ambientIntensity << ' '
            << preset.water.facingColor.x << ' ' << preset.water.facingColor.y << ' ' << preset.water.facingColor.z << ' '
            << preset.water.grazingColor.x << ' ' << preset.water.grazingColor.y << ' ' << preset.water.grazingColor.z << ' '
            << preset.water.reflectionStrength << ' '
            << preset.water.fresnelPower << ' '
            << preset.water.normalScale << ' '
            << preset.water.specularStrength << ' '
            << preset.water.specularSharpness << ' '
            << preset.water.normalScroll1.x << ' ' << preset.water.normalScroll1.y << ' '
            << preset.water.normalScroll2.x << ' ' << preset.water.normalScroll2.y;

        for (const auto& wave : preset.water.waves)
        {
            file
                << ' ' << wave.direction.x << ' ' << wave.direction.y
                << ' ' << wave.amplitude
                << ' ' << wave.wavelength
                << ' ' << wave.speed
                << ' ' << wave.steepness;
        }

        ForEachExtraField(preset, [&](const char* name, const auto& field)
        {
            file << ' ' << name << ' ' << field;
        });

        file << '\n';
    }
}

void Graphics::Shutdown()
{
    gpuTimer_.Shutdown();

    if (imguiInitialized_)
    {
        ImGui_ImplDX11_Shutdown();
        imguiInitialized_ = false;
    }

    if (tonemapShader_)
    {
        tonemapShader_->Shutdown();
        tonemapShader_.reset();
    }

    if (skyboxShader_)
    {
        skyboxShader_->Shutdown();
        skyboxShader_.reset();
    }

    if (skybox_)
    {
        skybox_->Shutdown();
        skybox_.reset();
    }

    for (auto& cubemap : environments_)
    {
        cubemap->Shutdown();
    }
    environments_.clear();

    if (colorShader_)
    {
        colorShader_->Shutdown();
        colorShader_.reset();
    }

    if (model_)
    {
        model_->Shutdown();
        model_.reset();
    }

    if (oceanGrid_)
    {
        oceanGrid_->Shutdown();
        oceanGrid_.reset();
    }

    if (normalMap_)
    {
        normalMap_->Shutdown();
        normalMap_.reset();
    }
    light_.reset();
    camera_.reset();

    if (d3d_)
    {
        d3d_->Shutdown();
        d3d_.reset();
    }
}

bool Graphics::Frame(float deltaTime, const Input& input)
{
    elapsedTime_ += deltaTime;
    UpdateMouseDrag(); // before UpdateCamera, which pushes cameraRotation_ to the camera
    UpdateCamera(deltaTime, input);
    return Render(deltaTime);
}

void Graphics::UpdateMouseDrag()
{
    // Viewport drags (ImGui's Win32 backend already tracks the mouse, so no extra input plumbing):
    //   bench plane: left-drag = rotate the plane, right-drag = look around
    //   ocean grid : left- or right-drag = look around (the grid itself stays level)
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
    {
        return;
    }

    constexpr float kDegreesPerPixel = 0.3f;
    const bool leftDrag = io.MouseDown[0];
    const bool rightDrag = io.MouseDown[1];

    if (!oceanMode_ && leftDrag)
    {
        modelRotation_.y -= io.MouseDelta.x * kDegreesPerPixel;
        modelRotation_.x = std::clamp(modelRotation_.x + io.MouseDelta.y * kDegreesPerPixel, -89.0f, 89.0f);
        modelRotation_.y = std::fmod(modelRotation_.y, 360.0f);
    }
    else if (rightDrag || (oceanMode_ && leftDrag))
    {
        // Mouse-look: drag right = turn right, drag down = look down (pitch > 0 looks down).
        cameraRotation_.y = std::fmod(cameraRotation_.y + io.MouseDelta.x * kDegreesPerPixel, 360.0f);
        cameraRotation_.x = std::clamp(cameraRotation_.x + io.MouseDelta.y * kDegreesPerPixel, -89.0f, 89.0f);
    }
}

void Graphics::UpdateCamera(float deltaTime, const Input& input)
{
    using namespace DirectX;

    const float turn = cameraTurnSpeed_ * deltaTime;
    if (input.IsKeyDown(VK_LEFT))  { cameraRotation_.y -= turn; }
    if (input.IsKeyDown(VK_RIGHT)) { cameraRotation_.y += turn; }
    if (input.IsKeyDown(VK_UP))    { cameraRotation_.x -= turn; }
    if (input.IsKeyDown(VK_DOWN))  { cameraRotation_.x += turn; }

    const float yawRad = XMConvertToRadians(cameraRotation_.y);
    const float pitchRad = XMConvertToRadians(cameraRotation_.x);
    const float cosPitch = cosf(pitchRad);
    const XMVECTOR forward = XMVectorSet(sinf(yawRad) * cosPitch, -sinf(pitchRad), cosf(yawRad) * cosPitch, 0.0f);
    const XMVECTOR right   = XMVectorSet(cosf(yawRad), 0.0f, -sinf(yawRad), 0.0f);
    const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMVECTOR position = XMLoadFloat3(&cameraPosition_);
    const float move = cameraMoveSpeed_ * deltaTime;
    if (input.IsKeyDown('W')) { position = XMVectorAdd(position, XMVectorScale(forward, move)); }
    if (input.IsKeyDown('S')) { position = XMVectorSubtract(position, XMVectorScale(forward, move)); }
    if (input.IsKeyDown('D')) { position = XMVectorAdd(position, XMVectorScale(right, move)); }
    if (input.IsKeyDown('A')) { position = XMVectorSubtract(position, XMVectorScale(right, move)); }
    if (input.IsKeyDown('E')) { position = XMVectorAdd(position, XMVectorScale(worldUp, move)); }
    if (input.IsKeyDown('Q')) { position = XMVectorSubtract(position, XMVectorScale(worldUp, move)); }
    XMStoreFloat3(&cameraPosition_, position);

    camera_->SetPosition(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z);
    camera_->SetRotation(cameraRotation_.x, cameraRotation_.y, cameraRotation_.z);
}

void Graphics::Resize(unsigned int width, unsigned int height)
{
    screenWidth_ = width;
    screenHeight_ = std::max<unsigned int>(height, 1u);

    if (d3d_)
    {
        d3d_->Resize(width, screenHeight_);
    }
}

bool Graphics::Render(float deltaTime)
{
    using namespace DirectX;

    const auto now = std::chrono::steady_clock::now();
    UpdateFrameStats(deltaTime);
    colorShader_->CheckHotReload(d3d_->GetDevice(), now);
    skyboxShader_->CheckHotReload(d3d_->GetDevice(), now);
    tonemapShader_->CheckHotReload(d3d_->GetDevice(), now);

    const bool capturing = BeginCaptureFrame();

    camera_->Render();

    const float aspect = (screenHeight_ > 0)
        ? static_cast<float>(screenWidth_) / static_cast<float>(screenHeight_)
        : 1.0f;

    // Bench plane: pitch in local space, then yaw around world Y (turntable-style drag).
    const XMMATRIX world = oceanMode_
        ? XMMatrixIdentity()
        : XMMatrixRotationX(XMConvertToRadians(modelRotation_.x)) *
          XMMatrixRotationY(XMConvertToRadians(modelRotation_.y));
    const XMMATRIX view = camera_->GetViewMatrix();
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(cameraFovDeg_),
        aspect,
        SCREEN_NEAR,
        SCREEN_DEPTH);

    XMMATRIX viewNoTrans = view;
    viewNoTrans.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    const XMFLOAT4 cameraPosWS(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z, 1.0f);

    // Sun position on the sky (yaw uses the camera convention, elevation > 0 = above the horizon).
    // g_LightDirection is the direction light travels, i.e. away from the sun.
    const float sunYawRad = XMConvertToRadians(sunYawDeg_);
    const float sunElevationRad = XMConvertToRadians(sunElevationDeg_);
    const float cosElevation = cosf(sunElevationRad);
    const XMFLOAT4 lightDir(
        -sinf(sunYawRad) * cosElevation,
        -sinf(sunElevationRad),
        -cosf(sunYawRad) * cosElevation,
        0.0f);
    const XMFLOAT4 lightColorPacked(lightColor_.x, lightColor_.y, lightColor_.z, lightIntensity_);
    const XMFLOAT4 ambientColorPacked(ambientColor_.x, ambientColor_.y, ambientColor_.z, ambientIntensity_);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    gpuTimer_.Begin(d3d_->GetDeviceContext());
    // Clear color is the old sRGB background, converted because the HDR target holds linear values.
    d3d_->BeginScene(SrgbToLinear(0.02f), SrgbToLinear(0.08f), SrgbToLinear(0.11f), 1.0f);

    if (skyboxVisible_)
    {
        d3d_->SetDepthLessEqual();
        skybox_->Render(d3d_->GetDeviceContext());
        skyboxShader_->Render(
            d3d_->GetDeviceContext(),
            skybox_->GetIndexCount(),
            viewNoTrans,
            projection,
            environments_[environmentIndex_]->GetSRV(),
            d3d_->GetSampler());
        d3d_->SetDepthDefault();
    }

    d3d_->SetRasterizerWaterSurface();
    Model* waterMesh = oceanMode_ ? oceanGrid_.get() : model_.get();
    waterMesh->Render(d3d_->GetDeviceContext());
    colorShader_->Render(
        d3d_->GetDeviceContext(),
        waterMesh->GetIndexCount(),
        world,
        view,
        projection,
        lightDir,
        lightColorPacked,
        ambientColorPacked,
        elapsedTime_,
        cameraPosWS,
        water_,
        environments_[environmentIndex_]->GetSRV(),
        normalMap_->GetSRV(),
        d3d_->GetSampler(),
        d3d_->GetWrapSampler());
    d3d_->SetRasterizerDefault();

    // HDR scene -> sRGB back buffer. Debug views carry data, not light, so they skip exposure and the
    // tone curve (their values were pre-decoded in the pixel shader and come back out unchanged).
    d3d_->BindBackBuffer();
    d3d_->SetRasterizerDoubleSided(); // the fullscreen triangle is clockwise; don't let it get culled
    const bool debugView = water_.debugMode != 0;
    tonemapShader_->Render(
        d3d_->GetDeviceContext(),
        d3d_->GetHdrSRV(),
        debugView ? 1.0f : std::exp2(exposureEv_),
        debugView ? TonemapShader::None : tonemapOperator_);
    d3d_->SetRasterizerDefault();

    if (!capturing)
    {
        DrawStatsOverlay();
        DrawImGuiPanel();
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    gpuTimer_.End(d3d_->GetDeviceContext());

    if (capturing)
    {
        EndCaptureFrame();
    }

    // CPU cost of this frame, excluding the Present / vsync wait below.
    cpuFrameMs_ = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - now).count();

    d3d_->EndScene();

    return !captureFinishedQuit_;
}

void Graphics::UpdateFrameStats(float deltaTime)
{
    // Values shown in the overlay are averaged over 0.5 s so they are readable.
    statsAccumTime_ += deltaTime;
    statsAccumCpuMs_ += cpuFrameMs_;
    ++statsAccumFrames_;
    if (statsAccumTime_ >= 0.5f)
    {
        displayedFps_ = statsAccumFrames_ / statsAccumTime_;
        displayedCpuMs_ = statsAccumCpuMs_ / statsAccumFrames_;
        displayedGpuMs_ = gpuTimer_.GetLastMs();
        statsAccumTime_ = 0.0f;
        statsAccumCpuMs_ = 0.0f;
        statsAccumFrames_ = 0;
    }
}

void Graphics::DrawStatsOverlay()
{
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    // The title-bar arrow collapses / expands it.
    if (ImGui::Begin("Stats", nullptr, flags))
    {
        ImGui::Text("Time    %8.2f s", elapsedTime_);
        ImGui::Text("FPS     %8.1f", displayedFps_);
        ImGui::Text("Frame   %8.2f ms", displayedFps_ > 0.0f ? 1000.0f / displayedFps_ : 0.0f);
        ImGui::Text("CPU     %8.2f ms", displayedCpuMs_);
        if (displayedGpuMs_ >= 0.0f)
        {
            ImGui::Text("GPU     %8.2f ms", displayedGpuMs_);
        }
        else
        {
            ImGui::Text("GPU          n/a");
        }

        bool vsync = d3d_->GetVSync();
        if (ImGui::Checkbox("VSync", &vsync))
        {
            d3d_->SetVSync(vsync);
        }
        ImGui::SameLine();
        ImGui::TextDisabled(vsync ? "(FPS capped by display)" : "(uncapped)");
        ImGui::Text("Mesh: %s", oceanMode_ ? "ocean grid 1024^2" : "bench plane 32^2");
    }
    ImGui::End();
}

void Graphics::DrawImGuiPanel()
{
    // Default to the right edge so it never covers the Stats overlay (imgui.ini overrides after first run).
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10.0f, 10.0f), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(360.0f, io.DisplaySize.y - 20.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Shader Bench");

    const std::string& shaderError = colorShader_->GetLastError();
    if (!shaderError.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Compile error:");
        ImGui::TextWrapped("%s", shaderError.c_str());
    }
    else
    {
        const std::string& reloadStamp = colorShader_->GetLastReloadStamp();
        ImGui::Text("Shader: reloaded %s", reloadStamp.empty() ? "ready" : reloadStamp.c_str());
    }
    ImGui::Separator();

    ImGui::SeparatorText("Model Rotation");
    if (oceanMode_)
    {
        ImGui::TextDisabled("Ocean Grid stays level (drag = look around)");
    }
    else
    {
        ImGui::TextDisabled("Left-drag: rotate plane, right-drag: look around");
        ImGui::Text("Pitch %.1f  Yaw %.1f", modelRotation_.x, modelRotation_.y);
    }
    if (ImGui::Button("Reset Model Rotation"))
    {
        modelRotation_ = {0.0f, 0.0f, 0.0f};
    }

    ImGui::SeparatorText("Camera");
    ImGui::SliderFloat("FOV (deg)", &cameraFovDeg_, 30.0f, 120.0f);
    ImGui::SliderFloat("Move Speed", &cameraMoveSpeed_, 0.1f, 10.0f);
    ImGui::SliderFloat("Turn Speed (deg/sec)", &cameraTurnSpeed_, 30.0f, 360.0f);
    ImGui::Text("WASD: move, Q/E: down/up, Arrows or drag: look");
    if (ImGui::Button("Reset Camera"))
    {
        ApplyCameraShot(kDefaultShotIndex);
        cameraMoveSpeed_ = 2.0f;
        cameraTurnSpeed_ = 90.0f;
    }

    ImGui::SeparatorText("Camera Presets");
    ImGui::TextDisabled("Fixed shots (same as --capture)");
    for (int i = 0; i < static_cast<int>(std::size(kCaptureShots)); ++i)
    {
        if (i > 0 && i != 3)
        {
            ImGui::SameLine();
        }
        if (ImGui::Button(kCaptureShots[i].name))
        {
            ApplyCameraShot(i);
        }
    }
    ImGui::TextDisabled("Slots (saved to assets/camera_presets.txt)");
    for (int i = 0; i < kCameraSlotCount; ++i)
    {
        ImGui::PushID(i);
        ImGui::Text("Slot %d%s", i + 1, cameraSlotUsed_[i] ? (cameraSlots_[i].ocean ? " [ocean]" : " [bench]") : " (empty)");
        ImGui::SameLine(120.0f);
        if (ImGui::Button("Save"))
        {
            cameraSlots_[i] = MakeViewFromCurrent();
            cameraSlotUsed_[i] = true;
            SaveCameraSlots();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!cameraSlotUsed_[i]);
        if (ImGui::Button("Load"))
        {
            ApplyView(cameraSlots_[i]);
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::Text("Pos (%.2f, %.2f, %.2f)  Pitch %.1f Yaw %.1f",
        cameraPosition_.x, cameraPosition_.y, cameraPosition_.z, cameraRotation_.x, cameraRotation_.y);

    ImGui::SeparatorText("Settings");
    const char* presetNames[] = { "Basic", "Sunset", "Tropical" };
    for (int i = 0; i < 3; ++i)
    {
        char applyLabel[32];
        std::snprintf(applyLabel, sizeof(applyLabel), "Apply %s", presetNames[i]);
        if (ImGui::Button(applyLabel))
        {
            ApplyPreset(i);
        }

        ImGui::SameLine();

        char saveLabel[40];
        std::snprintf(saveLabel, sizeof(saveLabel), "Save Current##%s", presetNames[i]);
        if (ImGui::Button(saveLabel))
        {
            SaveCurrentPreset(i);
        }
    }

    ImGui::SeparatorText("Lighting");
    ImGui::SliderFloat("Sun Yaw (deg)", &sunYawDeg_, 0.0f, 360.0f);
    ImGui::SliderFloat("Sun Elevation (deg)", &sunElevationDeg_, 0.0f, 90.0f);
    const Environment& currentEnv = kEnvironments[environmentIndex_];
    ImGui::TextDisabled("Sky sun: yaw %.1f, elevation %.1f", currentEnv.sunYawDeg, currentEnv.sunElevationDeg);
    if (ImGui::Button("Match Skybox Sun"))
    {
        sunYawDeg_ = currentEnv.sunYawDeg;
        sunElevationDeg_ = currentEnv.sunElevationDeg;
    }
    ImGui::ColorEdit3("Light Color", &lightColor_.x);
    ImGui::SliderFloat("Intensity", &lightIntensity_, 0.0f, 3.0f);
    ImGui::SliderFloat("Specular Strength", &water_.specularStrength, 0.0f, 2.0f);
    ImGui::SliderFloat("Specular Sharpness", &water_.specularSharpness, 16.0f, 256.0f);
    ImGui::SliderFloat("Sun Glint Power", &water_.sunGlintPower, 64.0f, 4096.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Sun Glint Intensity", &water_.sunGlintIntensity, 0.0f, 50.0f);
    ImGui::Text("Time: %.2fs", elapsedTime_);
    if (ImGui::Button("Reset Light"))
    {
        sunYawDeg_ = currentEnv.sunYawDeg;
        sunElevationDeg_ = 20.0f;
        lightColor_ = {1.0f, 1.0f, 1.0f};
        lightIntensity_ = 1.0f;
        ambientColor_ = {0.10f, 0.14f, 0.18f};
        ambientIntensity_ = 0.35f;
    }

    ImGui::SeparatorText("Ambient");
    ImGui::ColorEdit3("Ambient Color", &ambientColor_.x);
    ImGui::SliderFloat("Ambient Intensity", &ambientIntensity_, 0.0f, 1.0f);

    ImGui::SeparatorText("Environment");
    if (ImGui::BeginCombo("Sky / Reflection", kEnvironments[environmentIndex_].name))
    {
        for (int i = 0; i < kEnvironmentCount; ++i)
        {
            if (ImGui::Selectable(kEnvironments[i].name, i == environmentIndex_))
            {
                environmentIndex_ = i;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Checkbox("Skybox Visible", &skyboxVisible_);
    ImGui::Checkbox("Ocean Grid (open water to the horizon)", &oceanMode_);
    ImGui::SliderFloat("Reflection Strength", &water_.reflectionStrength, 0.0f, 1.0f);

    ImGui::SeparatorText("Water");
    ImGui::SliderFloat("Fresnel Power", &water_.fresnelPower, 1.0f, 8.0f);
    ImGui::ColorEdit3("Facing Color", &water_.facingColor.x);
    ImGui::ColorEdit3("Grazing Color", &water_.grazingColor.x);
    ImGui::SliderFloat("Fresnel F0", &water_.fresnelF0, 0.0f, 0.6f);
    ImGui::SliderFloat("Normal Scale (tile)", &water_.normalScale, 0.1f, 5.0f);
    ImGui::SliderFloat("Normal Strength", &water_.normalStrength, 0.0f, 3.0f);
    ImGui::SliderFloat("Detail Layer Scale (B / A)", &water_.detailScale, 1.0f, 6.0f);
    ImGui::TextDisabled("Normal map UV scroll velocity (2 layers blended)");
    ImGui::SliderFloat("Layer A - U speed (per sec)", &water_.normalScroll1.x, -0.2f, 0.2f);
    ImGui::SliderFloat("Layer A - V speed (per sec)", &water_.normalScroll1.y, -0.2f, 0.2f);
    ImGui::SliderFloat("Layer B - U speed (per sec)", &water_.normalScroll2.x, -0.2f, 0.2f);
    ImGui::SliderFloat("Layer B - V speed (per sec)", &water_.normalScroll2.y, -0.2f, 0.2f);

    for (int i = 0; i < ColorShader::kWaveCount; ++i)
    {
        char label[32];
        std::snprintf(label, sizeof(label), "Wave %d", i);
        if (ImGui::TreeNode(label))
        {
            auto& w = water_.waves[i];
            float angleDeg = DirectX::XMConvertToDegrees(std::atan2(w.direction.y, w.direction.x));
            if (ImGui::SliderFloat("Direction (deg)", &angleDeg, -180.0f, 180.0f))
            {
                const float r = DirectX::XMConvertToRadians(angleDeg);
                w.direction = { std::cos(r), std::sin(r) };
            }
            ImGui::SliderFloat("Amplitude", &w.amplitude, 0.0f, 0.3f);
            ImGui::SliderFloat("Wavelength", &w.wavelength, 0.2f, 8.0f);
            ImGui::SliderFloat("Speed", &w.speed, 0.0f, 3.0f);
            ImGui::SliderFloat("Steepness (Gerstner Q)", &w.steepness, 0.0f, 1.0f);
            ImGui::TreePop();
        }
    }

    ImGui::SeparatorText("Tonemapping");
    const char* operatorNames[] = { "None (clamp)", "Reinhard", "ACES (per channel)", "ACES (hue-preserving)" };
    int op = static_cast<int>(tonemapOperator_);
    if (ImGui::Combo("Tone Curve", &op, operatorNames, IM_ARRAYSIZE(operatorNames)))
    {
        tonemapOperator_ = static_cast<TonemapShader::Operator>(op);
    }
    ImGui::SliderFloat("Exposure (EV)", &exposureEv_, -4.0f, 4.0f, "%+.2f");
    ImGui::TextDisabled("x%.3f linear, saved with the preset", std::exp2(exposureEv_));

    ImGui::SeparatorText("Capture");
    ImGui::InputText("Label", captureLabel_, sizeof(captureLabel_));
    // Two-step: a stray click only opens the confirmation, and UI captures never overwrite an
    // existing folder (StartCaptureSet picks <label>_2, _3, ... instead). --capture still overwrites.
    if (ImGui::Button("Capture Set (presets x shots)..."))
    {
        ImGui::OpenPopup("Confirm capture");
    }
    if (ImGui::BeginPopupModal("Confirm capture", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Capture %d images to", static_cast<int>(presets_.size() * std::size(kCaptureShots)));
        ImGui::Text("docs/features/%s/captures/%s ?", WideToUtf8(captureFeature_).c_str(), captureLabel_);
        ImGui::TextDisabled("An existing folder is kept; a numbered folder is created instead.");
        if (ImGui::Button("Capture"))
        {
            StartCaptureSet(Utf8ToWide(captureLabel_), false, false);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::TextDisabled("Fixed time %.1fs, UI hidden.", kCaptureTime);
    if (!captureStatus_.empty())
    {
        ImGui::TextWrapped("%s", captureStatus_.c_str());
    }

    // Debug View — keep this section last so new ImGui controls always go above it.
    ImGui::SeparatorText("Debug View");
    const char* debugLabels[] = { "render", "Sampled normal map", "World-space N", "UV", "Front/back face", "Lighting terms (R diffuse, G spec, B fresnel)" };
    ImGui::Combo("Debug Mode", &water_.debugMode, debugLabels, IM_ARRAYSIZE(debugLabels));
    ImGui::TextWrapped("Normal Map Loader: %s", normalMapStatus_.c_str());

    ImGui::End();
}

