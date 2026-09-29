#include "Graphics.h"

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
#include <filesystem>
#include <fstream>
#include <string>

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

struct CaptureShot
{
    const char* name;
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 rotation; // x = pitch, y = yaw (deg)
    float fovDeg;
};

// Fixed camera shots for before/after comparison. Plane spans x,z in [-1, 1].
// "sunward" looks along the skybox sun (yaw ~33 deg) from a low angle to catch the glint path.
constexpr CaptureShot kCaptureShots[] = {
    { "oblique", {  0.00f, 1.00f, -2.20f }, { 24.0f,  0.0f, 0.0f }, 60.0f },
    { "top",     {  0.00f, 2.40f,  0.00f }, { 89.9f,  0.0f, 0.0f }, 60.0f },
    { "sunward", { -1.04f, 0.45f, -1.59f }, { 12.0f, 33.0f, 0.0f }, 60.0f },
};

constexpr const char* kPresetFileNames[] = { "basic", "sunset", "tropical" };
constexpr float kCaptureTime = 12.0f;
} // namespace

bool Graphics::Initialize(HWND hwnd, int screenWidth, int screenHeight)
{
    screenWidth_ = static_cast<unsigned int>(screenWidth);
    screenHeight_ = static_cast<unsigned int>(screenHeight);

    d3d_ = std::make_unique<D3DClass>();
    if (!d3d_->Initialize(screenWidth, screenHeight, VSYNC_ENABLED, hwnd, FULL_SCREEN, SCREEN_DEPTH, SCREEN_NEAR))
    {
        return false;
    }

    camera_ = std::make_unique<Camera>();
    camera_->SetPosition(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z);
    camera_->SetRotation(cameraRotation_.x, cameraRotation_.y, cameraRotation_.z);

    light_ = std::make_unique<Light>();
    light_->SetDirection(0.0f, -1.0f, 1.0f);
    light_->SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);

    normalMap_ = std::make_unique<Texture>();
    {
        struct Attempt { const wchar_t* relPath; const char* label; };
        const Attempt attempts[] = {
            { L"textures/water_normal.dds", "DDS" },
            { L"textures/water_normal.png", "PNG" },
            { L"textures/water_normal.jpg", "JPG" },
        };
        std::string statusLog;
        bool loaded = false;
        for (const auto& a : attempts)
        {
            const std::wstring path = GetAssetPath(a.relPath);
            const std::string pathUtf8(path.begin(), path.end());
            std::wstring err;
            if (normalMap_->Initialize(d3d_->GetDevice(), path.c_str(), &err))
            {
                statusLog += std::string("[OK]   ") + a.label + " loaded -> " + pathUtf8 + "\n";
                loaded = true;
                break;
            }
            const std::string errUtf8(err.begin(), err.end());
            statusLog += std::string("[FAIL] ") + a.label + " " + errUtf8 + " -> " + pathUtf8 + "\n";
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

    colorShader_ = std::make_unique<ColorShader>();
    if (!colorShader_->Initialize(d3d_->GetDevice()))
    {
        return false;
    }

    cubemap_ = std::make_unique<CubemapTexture>();
    const std::wstring cubemapPath = GetAssetPath(L"textures/skybox.dds");
    std::wstring cubemapError;
    if (!cubemap_->Initialize(d3d_->GetDevice(), cubemapPath.c_str(), &cubemapError))
    {
        const std::wstring msg = cubemapPath + L"\n\n" + cubemapError;
        MessageBoxW(hwnd, msg.c_str(), L"WaterShader: cubemap load failed", MB_ICONERROR | MB_OK);
        return false;
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

    if (!ImGui_ImplDX11_Init(d3d_->GetDevice(), d3d_->GetDeviceContext()))
    {
        return false;
    }
    imguiInitialized_ = true;

    LoadPresets();
    ApplyPreset(0);

    // WaterShader.exe --capture <label> : save every preset x shot, then quit.
    int argc = 0;
    if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc))
    {
        for (int i = 1; i + 1 < argc; ++i)
        {
            if (std::wstring(argv[i]) == L"--capture")
            {
                const std::wstring label = argv[i + 1];
                StartCaptureSet(std::string(label.begin(), label.end()), true);
            }
        }
        LocalFree(argv);
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
    lightYawDeg_ = preset.lightYawDeg;
    lightPitchDeg_ = preset.lightPitchDeg;
    lightColor_ = preset.lightColor;
    lightIntensity_ = preset.lightIntensity;
    ambientColor_ = preset.ambientColor;
    ambientIntensity_ = preset.ambientIntensity;
    water_ = preset.water;
}

Graphics::ShaderPreset Graphics::MakePresetFromCurrent() const
{
    ShaderPreset preset;
    preset.lightYawDeg = lightYawDeg_;
    preset.lightPitchDeg = lightPitchDeg_;
    preset.lightColor = lightColor_;
    preset.lightIntensity = lightIntensity_;
    preset.ambientColor = ambientColor_;
    preset.ambientIntensity = ambientIntensity_;
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

void Graphics::StartCaptureSet(const std::string& label, bool quitWhenDone)
{
    captureQueue_.clear();
    for (int preset = 0; preset < static_cast<int>(presets_.size()); ++preset)
    {
        for (int shot = 0; shot < static_cast<int>(std::size(kCaptureShots)); ++shot)
        {
            captureQueue_.push_back({preset, shot});
        }
    }

    // assets/ -> project root -> docs/features/water-polish/captures/<label>
    const std::filesystem::path projectRoot =
        std::filesystem::path(GetAssetPath(L"shader_presets.txt")).parent_path().parent_path();
    captureDir_ = projectRoot / "docs" / "features" / "water-polish" / "captures" / label;
    std::error_code ec;
    std::filesystem::create_directories(captureDir_, ec);

    captureRestore_ = {MakePresetFromCurrent(), cameraPosition_, cameraRotation_, cameraFovDeg_, elapsedTime_};
    quitAfterCapture_ = quitWhenDone;
    captureStatus_ = "Capturing -> " + captureDir_.string();
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
    water_.debugMode = 0;

    const CaptureShot& shot = kCaptureShots[job.shot];
    cameraPosition_ = shot.position;
    cameraRotation_ = shot.rotation;
    cameraFovDeg_ = shot.fovDeg;
    camera_->SetPosition(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z);
    camera_->SetRotation(cameraRotation_.x, cameraRotation_.y, cameraRotation_.z);

    // Fixed time so before/after frames show the same wave phase.
    elapsedTime_ = kCaptureTime;

    const std::string fileName = std::string(kPresetFileNames[job.preset]) + "_" + shot.name + ".jpg";
    pendingCapturePath_ = captureDir_ / fileName;
    return true;
}

void Graphics::EndCaptureFrame()
{
    // WIC needs COM; S_FALSE / RPC_E_CHANGED_MODE just mean it is already initialized.
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

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
    OutputDebugStringA(((SUCCEEDED(hr) ? "[Capture] saved " : "[Capture] FAILED ") + pendingCapturePath_.string() + "\n").c_str());

    if (!captureQueue_.empty())
    {
        return;
    }

    ApplyPresetValues(captureRestore_.preset);
    cameraPosition_ = captureRestore_.cameraPosition;
    cameraRotation_ = captureRestore_.cameraRotation;
    cameraFovDeg_ = captureRestore_.cameraFovDeg;
    elapsedTime_ = captureRestore_.elapsedTime;
    captureStatus_ = "Saved -> " + captureDir_.string();
    captureFinishedQuit_ = quitAfterCapture_;
}

void Graphics::LoadPresets()
{
    presets_[0] = ShaderPreset{};

    presets_[1] = ShaderPreset{};
    presets_[1].lightYawDeg = 35.0f;
    presets_[1].lightPitchDeg = -10.0f;
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
    presets_[2].lightYawDeg = 70.0f;
    presets_[2].lightPitchDeg = -55.0f;
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

    std::string header;
    int version = 0;
    file >> header >> version;
    if (header != "WaterShaderPresets" || version != 1)
    {
        return;
    }

    for (ShaderPreset& preset : presets_)
    {
        file
            >> preset.lightYawDeg
            >> preset.lightPitchDeg
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
            file
                >> wave.direction.x >> wave.direction.y
                >> wave.amplitude
                >> wave.wavelength
                >> wave.speed;
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

    file << "WaterShaderPresets 1\n";
    for (const ShaderPreset& preset : presets_)
    {
        file
            << preset.lightYawDeg << ' '
            << preset.lightPitchDeg << ' '
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
                << ' ' << wave.speed;
        }

        file << '\n';
    }
}

void Graphics::Shutdown()
{
    if (imguiInitialized_)
    {
        ImGui_ImplDX11_Shutdown();
        imguiInitialized_ = false;
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

    if (cubemap_)
    {
        cubemap_->Shutdown();
        cubemap_.reset();
    }

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
    UpdateCamera(deltaTime, input);
    return Render(deltaTime);
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
    colorShader_->CheckHotReload(d3d_->GetDevice(), now);
    skyboxShader_->CheckHotReload(d3d_->GetDevice(), now);

    const bool capturing = BeginCaptureFrame();

    camera_->Render();

    const float aspect = (screenHeight_ > 0)
        ? static_cast<float>(screenWidth_) / static_cast<float>(screenHeight_)
        : 1.0f;

    const XMMATRIX world =
        XMMatrixRotationX(XMConvertToRadians(modelRotation_.x)) *
        XMMatrixRotationY(XMConvertToRadians(modelRotation_.y)) *
        XMMatrixRotationZ(XMConvertToRadians(modelRotation_.z));
    const XMMATRIX view = camera_->GetViewMatrix();
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(cameraFovDeg_),
        aspect,
        SCREEN_NEAR,
        SCREEN_DEPTH);

    XMMATRIX viewNoTrans = view;
    viewNoTrans.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    const XMFLOAT4 cameraPosWS(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z, 1.0f);

    const float lightYawRad = XMConvertToRadians(lightYawDeg_);
    const float lightPitchRad = XMConvertToRadians(lightPitchDeg_);
    const float cosPitch = cosf(lightPitchRad);
    const XMFLOAT4 lightDir(sinf(lightYawRad) * cosPitch, -sinf(lightPitchRad), cosf(lightYawRad) * cosPitch, 0.0f);
    const XMFLOAT4 lightColorPacked(lightColor_.x, lightColor_.y, lightColor_.z, lightIntensity_);
    const XMFLOAT4 ambientColorPacked(ambientColor_.x, ambientColor_.y, ambientColor_.z, ambientIntensity_);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    d3d_->BeginScene(0.02f, 0.08f, 0.11f, 1.0f);

    if (skyboxVisible_)
    {
        d3d_->SetDepthLessEqual();
        skybox_->Render(d3d_->GetDeviceContext());
        skyboxShader_->Render(
            d3d_->GetDeviceContext(),
            skybox_->GetIndexCount(),
            viewNoTrans,
            projection,
            cubemap_->GetSRV(),
            d3d_->GetSampler());
        d3d_->SetDepthDefault();
    }

    d3d_->SetRasterizerWaterSurface();
    model_->Render(d3d_->GetDeviceContext());
    colorShader_->Render(
        d3d_->GetDeviceContext(),
        model_->GetIndexCount(),
        world,
        view,
        projection,
        lightDir,
        lightColorPacked,
        ambientColorPacked,
        elapsedTime_,
        cameraPosWS,
        water_,
        cubemap_->GetSRV(),
        normalMap_->GetSRV(),
        d3d_->GetSampler(),
        d3d_->GetWrapSampler());
    d3d_->SetRasterizerDefault();

    if (!capturing)
    {
        DrawImGuiPanel();
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    if (capturing)
    {
        EndCaptureFrame();
    }

    d3d_->EndScene();

    return !captureFinishedQuit_;
}

void Graphics::DrawImGuiPanel()
{
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
    ImGui::SliderFloat("X##model", &modelRotation_.x, 0.0f, 360.0f);
    ImGui::SliderFloat("Y##model", &modelRotation_.y, 0.0f, 360.0f);
    ImGui::SliderFloat("Z##model", &modelRotation_.z, 0.0f, 360.0f);
    if (ImGui::Button("Reset Model Rotation"))
    {
        modelRotation_ = {0.0f, 0.0f, 0.0f};
    }

    ImGui::SeparatorText("Camera");
    ImGui::SliderFloat("FOV (deg)", &cameraFovDeg_, 30.0f, 120.0f);
    ImGui::SliderFloat("Move Speed", &cameraMoveSpeed_, 0.1f, 10.0f);
    ImGui::SliderFloat("Turn Speed (deg/sec)", &cameraTurnSpeed_, 30.0f, 360.0f);
    ImGui::Text("WASD: move, Q/E: down/up, Arrows: rotate");
    if (ImGui::Button("Reset Camera"))
    {
        cameraPosition_ = {0.0f, 0.0f, -2.5f};
        cameraRotation_ = {0.0f, 0.0f, 0.0f};
        cameraFovDeg_ = 60.0f;
        cameraMoveSpeed_ = 2.0f;
        cameraTurnSpeed_ = 90.0f;
    }

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
    ImGui::SliderFloat("Light Yaw (deg)", &lightYawDeg_, 0.0f, 360.0f);
    ImGui::SliderFloat("Light Pitch (deg)", &lightPitchDeg_, -90.0f, 90.0f);
    ImGui::ColorEdit3("Light Color", &lightColor_.x);
    ImGui::SliderFloat("Intensity", &lightIntensity_, 0.0f, 3.0f);
    ImGui::SliderFloat("Specular Strength", &water_.specularStrength, 0.0f, 2.0f);
    ImGui::SliderFloat("Specular Sharpness", &water_.specularSharpness, 16.0f, 256.0f);
    ImGui::Text("Time: %.2fs", elapsedTime_);
    if (ImGui::Button("Reset Light"))
    {
        lightYawDeg_ = 45.0f;
        lightPitchDeg_ = -45.0f;
        lightColor_ = {1.0f, 1.0f, 1.0f};
        lightIntensity_ = 1.0f;
        ambientColor_ = {0.10f, 0.14f, 0.18f};
        ambientIntensity_ = 0.35f;
    }

    ImGui::SeparatorText("Ambient");
    ImGui::ColorEdit3("Ambient Color", &ambientColor_.x);
    ImGui::SliderFloat("Ambient Intensity", &ambientIntensity_, 0.0f, 1.0f);

    ImGui::SeparatorText("Environment");
    ImGui::Checkbox("Skybox Visible", &skyboxVisible_);
    ImGui::SliderFloat("Reflection Strength", &water_.reflectionStrength, 0.0f, 1.0f);

    ImGui::SeparatorText("Water");
    ImGui::SliderFloat("Fresnel Power", &water_.fresnelPower, 1.0f, 8.0f);
    ImGui::ColorEdit3("Facing Color", &water_.facingColor.x);
    ImGui::ColorEdit3("Grazing Color", &water_.grazingColor.x);
    ImGui::SliderFloat("Normal Scale (tile)", &water_.normalScale, 0.1f, 5.0f);
    ImGui::TextDisabled("Normal map UV scroll velocity (2 layers blended)");
    ImGui::SliderFloat("Layer A - U speed (per sec)", &water_.normalScroll1.x, -0.2f, 0.2f);
    ImGui::SliderFloat("Layer A - V speed (per sec)", &water_.normalScroll1.y, -0.2f, 0.2f);
    ImGui::SliderFloat("Layer B - U speed (per sec)", &water_.normalScroll2.x, -0.2f, 0.2f);
    ImGui::SliderFloat("Layer B - V speed (per sec)", &water_.normalScroll2.y, -0.2f, 0.2f);

    for (int i = 0; i < 2; ++i)
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
            ImGui::TreePop();
        }
    }

    ImGui::SeparatorText("Capture");
    ImGui::InputText("Label", captureLabel_, sizeof(captureLabel_));
    if (ImGui::Button("Capture Set (presets x shots)"))
    {
        StartCaptureSet(captureLabel_, false);
    }
    ImGui::TextDisabled("Fixed time %.1fs, UI hidden. Saves to docs/features/water-polish/captures/<label>", kCaptureTime);
    if (!captureStatus_.empty())
    {
        ImGui::TextWrapped("%s", captureStatus_.c_str());
    }

    // Debug View — keep this section last so new ImGui controls always go above it.
    ImGui::SeparatorText("Debug View");
    const char* debugLabels[] = { "render", "Sampled normal map", "World-space N", "UV", "Front/back face" };
    ImGui::Combo("Debug Mode", &water_.debugMode, debugLabels, IM_ARRAYSIZE(debugLabels));
    ImGui::TextWrapped("Normal Map Loader: %s", normalMapStatus_.c_str());

    ImGui::End();
}

