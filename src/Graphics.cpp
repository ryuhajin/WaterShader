#include "Graphics.h"

#include "Input.h"
#include "SystemConfig.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
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

    texture_ = std::make_unique<Texture>();

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

    if (!ImGui_ImplDX11_Init(d3d_->GetDevice(), d3d_->GetDeviceContext()))
    {
        return false;
    }
    imguiInitialized_ = true;

    return true;
}

void Graphics::Shutdown()
{
    if (imguiInitialized_)
    {
        ImGui_ImplDX11_Shutdown();
        imguiInitialized_ = false;
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

    texture_.reset();
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

    colorShader_->CheckHotReload(d3d_->GetDevice(), std::chrono::steady_clock::now());

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

    const float lightYawRad = XMConvertToRadians(lightYawDeg_);
    const float lightPitchRad = XMConvertToRadians(lightPitchDeg_);
    const float cosPitch = cosf(lightPitchRad);
    const XMFLOAT4 lightDir(sinf(lightYawRad) * cosPitch, -sinf(lightPitchRad), cosf(lightYawRad) * cosPitch, 0.0f);
    const XMFLOAT4 lightColorPacked(lightColor_.x, lightColor_.y, lightColor_.z, lightIntensity_);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    d3d_->BeginScene(0.02f, 0.08f, 0.11f, 1.0f);
    model_->Render(d3d_->GetDeviceContext());
    colorShader_->Render(d3d_->GetDeviceContext(), model_->GetIndexCount(), world, view, projection, lightDir, lightColorPacked, tintColor_, elapsedTime_);

    DrawImGuiPanel();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    d3d_->EndScene();

    return true;
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

    ImGui::SeparatorText("Tint Color");
    int tintR = static_cast<int>(tintColor_.x * 255.0f + 0.5f);
    int tintG = static_cast<int>(tintColor_.y * 255.0f + 0.5f);
    int tintB = static_cast<int>(tintColor_.z * 255.0f + 0.5f);
    if (ImGui::SliderInt("R", &tintR, 0, 255)) { tintColor_.x = tintR / 255.0f; }
    if (ImGui::SliderInt("G", &tintG, 0, 255)) { tintColor_.y = tintG / 255.0f; }
    if (ImGui::SliderInt("B", &tintB, 0, 255)) { tintColor_.z = tintB / 255.0f; }

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

    ImGui::SeparatorText("Lighting");
    ImGui::SliderFloat("Light Yaw (deg)", &lightYawDeg_, 0.0f, 360.0f);
    ImGui::SliderFloat("Light Pitch (deg)", &lightPitchDeg_, -90.0f, 90.0f);
    int lightR = static_cast<int>(lightColor_.x * 255.0f + 0.5f);
    int lightG = static_cast<int>(lightColor_.y * 255.0f + 0.5f);
    int lightB = static_cast<int>(lightColor_.z * 255.0f + 0.5f);
    if (ImGui::SliderInt("R##light", &lightR, 0, 255)) { lightColor_.x = lightR / 255.0f; }
    if (ImGui::SliderInt("G##light", &lightG, 0, 255)) { lightColor_.y = lightG / 255.0f; }
    if (ImGui::SliderInt("B##light", &lightB, 0, 255)) { lightColor_.z = lightB / 255.0f; }
    ImGui::SliderFloat("Intensity", &lightIntensity_, 0.0f, 3.0f);
    ImGui::Text("Time: %.2fs", elapsedTime_);
    if (ImGui::Button("Reset Light"))
    {
        lightYawDeg_ = 45.0f;
        lightPitchDeg_ = -45.0f;
        lightColor_ = {1.0f, 1.0f, 1.0f};
        lightIntensity_ = 1.0f;
    }

    ImGui::End();
}
