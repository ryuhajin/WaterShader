#include "SkyboxShader.h"

#include <windows.h>

#include <d3dcompiler.h>

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iterator>
#include <string>
#include <system_error>

namespace
{
struct SkyboxCB
{
    DirectX::XMFLOAT4X4 viewNoTranslation;
    DirectX::XMFLOAT4X4 projection;
};

bool CompileShader(const wchar_t* path, const char* entryPoint, const char* target, ID3DBlob** bytecode, std::string* outError)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompileFromFile(
        path,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint,
        target,
        flags,
        0,
        bytecode,
        &errors);

    if (FAILED(result))
    {
        if (errors)
        {
            const char* msg = static_cast<const char*>(errors->GetBufferPointer());
            if (outError)
            {
                *outError = msg;
            }
            OutputDebugStringA(msg);
        }
        else if (outError)
        {
            *outError = "Skybox shader compile failed (no error blob).";
        }
        return false;
    }

    return true;
}

std::string MakeTimeStamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

std::wstring GetShaderPath(const wchar_t* fileName)
{
#ifdef WATERSHADER_SHADER_DIR
    std::filesystem::path path = WATERSHADER_SHADER_DIR;
    path /= fileName;
    return path.wstring();
#else
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

    std::filesystem::path path(modulePath);
    path = path.parent_path() / L"shaders" / fileName;
    return path.wstring();
#endif
}
} // namespace

bool SkyboxShader::Initialize(ID3D11Device* device)
{
    return InitializeShader(device, GetShaderPath(L"skybox.hlsl").c_str());
}

void SkyboxShader::Shutdown()
{
    ShutdownShader();
}

bool SkyboxShader::Render(
    ID3D11DeviceContext* deviceContext,
    int indexCount,
    const DirectX::XMMATRIX& viewNoTranslation,
    const DirectX::XMMATRIX& projection,
    ID3D11ShaderResourceView* cubemapSRV,
    ID3D11SamplerState* sampler)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(deviceContext->Map(perFrameCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        auto* data = static_cast<SkyboxCB*>(mapped.pData);
        DirectX::XMStoreFloat4x4(&data->viewNoTranslation, viewNoTranslation);
        DirectX::XMStoreFloat4x4(&data->projection, projection);
        deviceContext->Unmap(perFrameCB_.Get(), 0);
    }

    deviceContext->IASetInputLayout(layout_.Get());
    deviceContext->VSSetShader(vertexShader_.Get(), nullptr, 0);
    deviceContext->PSSetShader(pixelShader_.Get(), nullptr, 0);
    deviceContext->VSSetConstantBuffers(0, 1, perFrameCB_.GetAddressOf());
    deviceContext->PSSetShaderResources(0, 1, &cubemapSRV);
    deviceContext->PSSetSamplers(0, 1, &sampler);
    deviceContext->DrawIndexed(indexCount, 0, 0);
    return true;
}

bool SkyboxShader::InitializeShader(ID3D11Device* device, const wchar_t* shaderPath)
{
    shaderPath_ = shaderPath;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(SkyboxCB);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &perFrameCB_)))
    {
        return false;
    }

    if (!Reload(device))
    {
        return false;
    }

    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(shaderPath_, ec);
    if (!ec)
    {
        lastWriteTime_ = mtime;
    }

    return true;
}

bool SkyboxShader::Reload(ID3D11Device* device)
{
    Microsoft::WRL::ComPtr<ID3DBlob> vsBuffer;
    Microsoft::WRL::ComPtr<ID3DBlob> psBuffer;

    std::string error;
    if (!CompileShader(shaderPath_.c_str(), "VSMain", "vs_5_0", &vsBuffer, &error))
    {
        lastError_ = error;
        return false;
    }
    if (!CompileShader(shaderPath_.c_str(), "PSMain", "ps_5_0", &psBuffer, &error))
    {
        lastError_ = error;
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11VertexShader> newVS;
    if (FAILED(device->CreateVertexShader(vsBuffer->GetBufferPointer(), vsBuffer->GetBufferSize(), nullptr, &newVS)))
    {
        lastError_ = "CreateVertexShader failed.";
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11PixelShader> newPS;
    if (FAILED(device->CreatePixelShader(psBuffer->GetBufferPointer(), psBuffer->GetBufferSize(), nullptr, &newPS)))
    {
        lastError_ = "CreatePixelShader failed.";
        return false;
    }

    const D3D11_INPUT_ELEMENT_DESC polygonLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    Microsoft::WRL::ComPtr<ID3D11InputLayout> newLayout;
    if (FAILED(device->CreateInputLayout(
            polygonLayout,
            static_cast<UINT>(std::size(polygonLayout)),
            vsBuffer->GetBufferPointer(),
            vsBuffer->GetBufferSize(),
            &newLayout)))
    {
        lastError_ = "CreateInputLayout failed.";
        return false;
    }

    vertexShader_ = newVS;
    pixelShader_ = newPS;
    layout_ = newLayout;

    lastError_.clear();
    lastReloadStamp_ = MakeTimeStamp();
    return true;
}

void SkyboxShader::CheckHotReload(ID3D11Device* device, std::chrono::steady_clock::time_point now)
{
    constexpr auto kPollInterval = std::chrono::milliseconds(200);

    if (now < nextCheckTime_)
    {
        return;
    }
    nextCheckTime_ = now + kPollInterval;

    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(shaderPath_, ec);
    if (ec)
    {
        return;
    }
    if (mtime == lastWriteTime_)
    {
        return;
    }
    lastWriteTime_ = mtime;

    Reload(device);
}

void SkyboxShader::ShutdownShader()
{
    perFrameCB_.Reset();
    layout_.Reset();
    pixelShader_.Reset();
    vertexShader_.Reset();
}
