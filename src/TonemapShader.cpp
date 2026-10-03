#include "TonemapShader.h"

#include <windows.h>

#include <d3dcompiler.h>

#include <algorithm>
#include <system_error>

namespace
{
struct TonemapCB
{
    float exposure;
    unsigned int op;
    float padding[2];
};

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

bool Compile(const std::wstring& path, const char* entry, const char* target, ID3DBlob** out, std::string* error)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    if (FAILED(D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, target, flags, 0, out, &errors)))
    {
        *error = errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Tonemap shader compile failed.";
        OutputDebugStringA(error->c_str());
        return false;
    }
    return true;
}

std::filesystem::file_time_type NewestWriteTime(const std::wstring& a, const std::wstring& b)
{
    std::error_code ec;
    auto ta = std::filesystem::last_write_time(a, ec);
    auto tb = std::filesystem::last_write_time(b, ec);
    return (std::max)(ta, tb); // parenthesized: <windows.h> defines a max() macro
}
} // namespace

bool TonemapShader::Initialize(ID3D11Device* device)
{
    shaderPath_ = GetShaderPath(L"tonemap.hlsl");
    includePath_ = GetShaderPath(L"Color.hlsli");

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(TonemapCB);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &constantBuffer_)))
    {
        return false;
    }

    lastWriteTime_ = NewestWriteTime(shaderPath_, includePath_);
    return Reload(device);
}

void TonemapShader::Shutdown()
{
    constantBuffer_.Reset();
    pixelShader_.Reset();
    vertexShader_.Reset();
}

bool TonemapShader::Reload(ID3D11Device* device)
{
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    std::string error;
    if (!Compile(shaderPath_, "VSMain", "vs_5_0", &vsBlob, &error) ||
        !Compile(shaderPath_, "PSMain", "ps_5_0", &psBlob, &error))
    {
        lastError_ = error;
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
    if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs)) ||
        FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps)))
    {
        lastError_ = "Tonemap shader creation failed.";
        return false;
    }

    vertexShader_ = vs;
    pixelShader_ = ps;
    lastError_.clear();
    return true;
}

void TonemapShader::CheckHotReload(ID3D11Device* device, std::chrono::steady_clock::time_point now)
{
    if (now < nextCheckTime_)
    {
        return;
    }
    nextCheckTime_ = now + std::chrono::milliseconds(200);

    const auto newest = NewestWriteTime(shaderPath_, includePath_);
    if (newest != lastWriteTime_)
    {
        lastWriteTime_ = newest;
        Reload(device);
    }
}

void TonemapShader::Render(ID3D11DeviceContext* deviceContext, ID3D11ShaderResourceView* hdrScene, float exposure, Operator op)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(deviceContext->Map(constantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        auto* data = static_cast<TonemapCB*>(mapped.pData);
        data->exposure = exposure;
        data->op = op;
        data->padding[0] = data->padding[1] = 0.0f;
        deviceContext->Unmap(constantBuffer_.Get(), 0);
    }

    // Fullscreen triangle from SV_VertexID: no vertex buffer or input layout.
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext->VSSetShader(vertexShader_.Get(), nullptr, 0);
    deviceContext->PSSetShader(pixelShader_.Get(), nullptr, 0);
    deviceContext->PSSetConstantBuffers(0, 1, constantBuffer_.GetAddressOf());
    deviceContext->PSSetShaderResources(0, 1, &hdrScene);
    deviceContext->Draw(3, 0);

    // Unbind so the HDR texture can be a render target again next frame without a D3D hazard warning.
    ID3D11ShaderResourceView* nullSRV = nullptr;
    deviceContext->PSSetShaderResources(0, 1, &nullSRV);
}
