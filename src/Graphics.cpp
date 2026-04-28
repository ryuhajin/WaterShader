#include "Graphics.h"

#include "SystemConfig.h"

bool Graphics::Initialize(HWND hwnd, int screenWidth, int screenHeight)
{
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
    return colorShader_->Initialize(d3d_->GetDevice());
}

void Graphics::Shutdown()
{
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
    if (d3d_)
    {
        d3d_->Resize(width, height);
    }
}

bool Graphics::Render(float)
{
    camera_->Render();

    d3d_->BeginScene(0.02f, 0.08f, 0.11f, 1.0f);
    model_->Render(d3d_->GetDeviceContext());
    colorShader_->Render(d3d_->GetDeviceContext(), model_->GetIndexCount());
    d3d_->EndScene();

    return true;
}
