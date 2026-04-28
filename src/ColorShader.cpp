#include "ColorShader.h"

#include <windows.h>

#include <d3dcompiler.h>

#include <filesystem>
#include <iterator>
#include <string>

namespace
{
bool CompileShader(const wchar_t* path, const char* entryPoint, const char* target, ID3DBlob** bytecode)
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
            OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
        }
        return false;
    }

    return true;
}

std::wstring GetShaderPath(const wchar_t* fileName)
{
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

    std::filesystem::path path(modulePath);
    path = path.parent_path() / L"shaders" / fileName;
    return path.wstring();
}
} // namespace

bool ColorShader::Initialize(ID3D11Device* device)
{
    return InitializeShader(device, GetShaderPath(L"simple.hlsl").c_str());
}

void ColorShader::Shutdown()
{
    ShutdownShader();
}

bool ColorShader::Render(ID3D11DeviceContext* deviceContext, int indexCount)
{
    RenderShader(deviceContext, indexCount);
    return true;
}

bool ColorShader::InitializeShader(ID3D11Device* device, const wchar_t* shaderPath)
{
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBuffer;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBuffer;

    if (!CompileShader(shaderPath, "VSMain", "vs_5_0", &vertexShaderBuffer))
    {
        return false;
    }

    if (!CompileShader(shaderPath, "PSMain", "ps_5_0", &pixelShaderBuffer))
    {
        return false;
    }

    if (FAILED(device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), nullptr, &vertexShader_)))
    {
        return false;
    }

    if (FAILED(device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), nullptr, &pixelShader_)))
    {
        return false;
    }

    const D3D11_INPUT_ELEMENT_DESC polygonLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (FAILED(device->CreateInputLayout(
            polygonLayout,
            static_cast<UINT>(std::size(polygonLayout)),
            vertexShaderBuffer->GetBufferPointer(),
            vertexShaderBuffer->GetBufferSize(),
            &layout_)))
    {
        return false;
    }

    return true;
}

void ColorShader::ShutdownShader()
{
    layout_.Reset();
    pixelShader_.Reset();
    vertexShader_.Reset();
}

void ColorShader::RenderShader(ID3D11DeviceContext* deviceContext, int indexCount)
{
    deviceContext->IASetInputLayout(layout_.Get());
    deviceContext->VSSetShader(vertexShader_.Get(), nullptr, 0);
    deviceContext->PSSetShader(pixelShader_.Get(), nullptr, 0);
    deviceContext->DrawIndexed(indexCount, 0, 0);
}
