#include "Graphics.h"

#include "SystemConfig.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>

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
    camera_->SetPosition(0.0f, 0.0f, -2.5f);

    light_ = std::make_unique<Light>();
    light_->SetDirection(0.0f, -1.0f, 1.0f);
    light_->SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);

    texture_ = std::make_unique<Texture>();

    model_ = std::make_unique<Model>();
    if (!model_->Initialize(d3d_->GetDevice()))
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

bool Graphics::Frame(float deltaTime)
{
    return Render(deltaTime);
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

    camera_->Render();

    const float aspect = (screenHeight_ > 0)
        ? static_cast<float>(screenWidth_) / static_cast<float>(screenHeight_)
        : 1.0f;

    const XMMATRIX world = XMMatrixRotationY(XMConvertToRadians(yRotationDegrees_));
    const XMMATRIX view = camera_->GetViewMatrix();
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(60.0f),
        aspect,
        SCREEN_NEAR,
        SCREEN_DEPTH);
    const XMMATRIX mvp = world * view * projection;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    d3d_->BeginScene(0.02f, 0.08f, 0.11f, 1.0f);
    model_->Render(d3d_->GetDeviceContext());
    colorShader_->Render(d3d_->GetDeviceContext(), model_->GetIndexCount(), mvp, tintColor_);

    DrawImGuiPanel();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    d3d_->EndScene();

    return true;
}

void Graphics::DrawImGuiPanel()
{
    ImGui::Begin("Shader Bench");

    ImGui::Text("Tint Color");
    int tintR = static_cast<int>(tintColor_.x * 255.0f + 0.5f);
    int tintG = static_cast<int>(tintColor_.y * 255.0f + 0.5f);
    int tintB = static_cast<int>(tintColor_.z * 255.0f + 0.5f);
    if (ImGui::SliderInt("R", &tintR, 0, 255)) { tintColor_.x = tintR / 255.0f; }
    if (ImGui::SliderInt("G", &tintG, 0, 255)) { tintColor_.y = tintG / 255.0f; }
    if (ImGui::SliderInt("B", &tintB, 0, 255)) { tintColor_.z = tintB / 255.0f; }

    ImGui::SliderFloat("Y Rotation (deg)", &yRotationDegrees_, 0.0f, 360.0f);
    if (ImGui::Button("Reset Rotation"))
    {
        yRotationDegrees_ = 0.0f;
    }
    ImGui::End();
}
