#include "ColorShader.h"

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
struct WaveParams
{
    DirectX::XMFLOAT2 direction;
    float amplitude;
    float wavelength;
    float speed;
    float padding[3];
};

struct PerFrameCB
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 projection;

    DirectX::XMFLOAT4 lightDirection; // xyz = direction, w unused
    DirectX::XMFLOAT4 lightColor; // rgb = color, a = intensity
    DirectX::XMFLOAT4 ambientColor; // rgb = color, a = intensity
    DirectX::XMFLOAT4 cameraPositionWS;

    DirectX::XMFLOAT4 facingColor; // 카메라 정면에서 보이는 water color. high viewFacingAmount
    DirectX::XMFLOAT4 grazingColor; // 카메라 비스듬히 볼 때 보이는 water color. low viewFacingAmount
    DirectX::XMFLOAT4 normalScroll;     // xy = scroll1, zw = scroll2

    // x = time, y = reflectionStrength, z = fresnelPower, w = normalScale
    DirectX::XMFLOAT4 waterParams; 
    
    // x = strength, y = sharpness, z/w unused
    DirectX::XMFLOAT4 specularParams;

    WaveParams waves[2];
    DirectX::XMFLOAT4 debugParams; // x = debug mode
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
            *outError = "Shader compile failed (no error blob).";
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

bool ColorShader::Initialize(ID3D11Device* device)
{
    return InitializeShader(device);
}

void ColorShader::Shutdown()
{
    ShutdownShader();
}

bool ColorShader::Render(
    ID3D11DeviceContext* deviceContext,
    int indexCount,
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& projection,
    const DirectX::XMFLOAT4& lightDirection,
    const DirectX::XMFLOAT4& lightColor,
    const DirectX::XMFLOAT4& ambientColor,
    float time,
    const DirectX::XMFLOAT4& cameraPositionWS,
    const WaterParams& water,
    ID3D11ShaderResourceView* cubemapSRV,
    ID3D11ShaderResourceView* normalSRV,
    ID3D11SamplerState* clampSampler,
    ID3D11SamplerState* wrapSampler)
{
    RenderShader(deviceContext, indexCount, world, view, projection, lightDirection, lightColor, ambientColor, time, cameraPositionWS, water, cubemapSRV, normalSRV, clampSampler, wrapSampler);
    return true;
}

bool ColorShader::InitializeShader(ID3D11Device* device)
{
    vsPath_ = GetShaderPath(L"vertexShader.hlsl");
    psPath_ = GetShaderPath(L"PixelShader.hlsl");

    watchedFiles_ = {
        {vsPath_,                          {}},
        {psPath_,                          {}},
        {GetShaderPath(L"Common.hlsli"),   {}},
        {GetShaderPath(L"Lighting.hlsli"), {}},
        {GetShaderPath(L"Cubemap.hlsli"),  {}},
    };

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(PerFrameCB);
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
    for (auto& w : watchedFiles_)
    {
        const auto mtime = std::filesystem::last_write_time(w.path, ec);
        if (!ec)
        {
            w.mtime = mtime;
        }
    }

    return true;
}

bool ColorShader::Reload(ID3D11Device* device)
{
    Microsoft::WRL::ComPtr<ID3DBlob> vsBuffer;
    Microsoft::WRL::ComPtr<ID3DBlob> psBuffer;

    std::string error;
    if (!CompileShader(vsPath_.c_str(), "VSMain", "vs_5_0", &vsBuffer, &error))
    {
        lastError_ = error;
        return false;
    }
    if (!CompileShader(psPath_.c_str(), "PSMain", "ps_5_0", &psBuffer, &error))
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
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
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

void ColorShader::CheckHotReload(ID3D11Device* device, std::chrono::steady_clock::time_point now)
{
    constexpr auto kPollInterval = std::chrono::milliseconds(200);

    if (now < nextCheckTime_)
    {
        return;
    }
    nextCheckTime_ = now + kPollInterval;

    bool changed = false;
    std::error_code ec;
    for (auto& w : watchedFiles_)
    {
        const auto mtime = std::filesystem::last_write_time(w.path, ec);
        if (ec)
        {
            continue;
        }
        if (mtime != w.mtime)
        {
            w.mtime = mtime;
            changed = true;
        }
    }

    if (changed)
    {
        Reload(device);
    }
}

void ColorShader::ShutdownShader()
{
    perFrameCB_.Reset();
    layout_.Reset();
    pixelShader_.Reset();
    vertexShader_.Reset();
}

void ColorShader::RenderShader(
    ID3D11DeviceContext* deviceContext,
    int indexCount,
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& projection,
    const DirectX::XMFLOAT4& lightDirection,
    const DirectX::XMFLOAT4& lightColor,
    const DirectX::XMFLOAT4& ambientColor,
    float time,
    const DirectX::XMFLOAT4& cameraPositionWS,
    const WaterParams& water,
    ID3D11ShaderResourceView* cubemapSRV,
    ID3D11ShaderResourceView* normalSRV,
    ID3D11SamplerState* clampSampler,
    ID3D11SamplerState* wrapSampler)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(deviceContext->Map(perFrameCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        auto* data = static_cast<PerFrameCB*>(mapped.pData);
        DirectX::XMStoreFloat4x4(&data->world, world);
        DirectX::XMStoreFloat4x4(&data->view, view);
        DirectX::XMStoreFloat4x4(&data->projection, projection);
        data->lightDirection = lightDirection;
        data->lightColor = lightColor;
        data->ambientColor = ambientColor;
        data->cameraPositionWS = cameraPositionWS;
        data->facingColor = water.facingColor;
        data->grazingColor = water.grazingColor;
        data->normalScroll = DirectX::XMFLOAT4(
            water.normalScroll1.x, water.normalScroll1.y,
            water.normalScroll2.x, water.normalScroll2.y);
        data->waterParams = DirectX::XMFLOAT4(
            time,
            water.reflectionStrength,
            water.fresnelPower,
            water.normalScale);
        data->specularParams = DirectX::XMFLOAT4(
            water.specularStrength,
            water.specularSharpness,
            0.0f,
            0.0f);
        for (int i = 0; i < 2; ++i)
        {
            data->waves[i].direction  = water.waves[i].direction;
            data->waves[i].amplitude  = water.waves[i].amplitude;
            data->waves[i].wavelength = water.waves[i].wavelength;
            data->waves[i].speed      = water.waves[i].speed;
            data->waves[i].padding[0] = data->waves[i].padding[1] = data->waves[i].padding[2] = 0.0f;
        }
        data->debugParams = DirectX::XMFLOAT4(static_cast<float>(water.debugMode), 0.0f, 0.0f, 0.0f);
        deviceContext->Unmap(perFrameCB_.Get(), 0);
    }

    ID3D11ShaderResourceView* srvs[2] = {cubemapSRV, normalSRV};
    ID3D11SamplerState* samplers[2] = {clampSampler, wrapSampler};

    deviceContext->IASetInputLayout(layout_.Get());
    deviceContext->VSSetShader(vertexShader_.Get(), nullptr, 0);
    deviceContext->PSSetShader(pixelShader_.Get(), nullptr, 0);
    deviceContext->VSSetConstantBuffers(0, 1, perFrameCB_.GetAddressOf());
    deviceContext->PSSetConstantBuffers(0, 1, perFrameCB_.GetAddressOf());
    deviceContext->PSSetShaderResources(0, 2, srvs);
    deviceContext->PSSetSamplers(0, 2, samplers);
    deviceContext->DrawIndexed(indexCount, 0, 0);
}



