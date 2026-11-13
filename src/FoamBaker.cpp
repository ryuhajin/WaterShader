#include "FoamBaker.h"

#include <ScreenGrab.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdio>
#include <iterator>
#include <system_error>

namespace
{
using Microsoft::WRL::ComPtr;

constexpr FoamBaker::Mask kMasks[] = {
    { 1, L"foam_cells.png" },
    { 2, L"foam_bubbles.png" },
    { 3, L"foam_packed.png" },
    { 4, L"foam_streaks.png" },
    { 5, L"caustics.png" }, // caustics step: the sunlight pattern under the water
};

std::wstring BakeShaderPath()
{
#ifdef WATERSHADER_SHADER_DIR
    std::filesystem::path path = WATERSHADER_SHADER_DIR;
#else
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    std::filesystem::path path = std::filesystem::path(modulePath).parent_path() / L"shaders";
#endif
    return (path / L"foam_bake.hlsl").wstring();
}

struct BakeConstants
{
    unsigned int mask;
    unsigned int size;
    unsigned int seed;
    unsigned int padding;
};
} // namespace

bool FoamBaker::BakeAll(ID3D11Device* device, ID3D11DeviceContext* context, const std::filesystem::path& outputDir,
                        std::string* error)
{
    // The same flags in Debug and Release builds, so a bake gives the same bytes from either.
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_IEEE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    if (FAILED(D3DCompileFromFile(BakeShaderPath().c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain", "cs_5_0",
                                  flags, 0, &bytecode, &errors)))
    {
        *error = errors ? static_cast<const char*>(errors->GetBufferPointer()) : "foam_bake.hlsl: compile failed";
        return false;
    }
    ComPtr<ID3D11ComputeShader> shader;
    if (FAILED(device->CreateComputeShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &shader)))
    {
        *error = "CreateComputeShader failed";
        return false;
    }

    // The target the compute shader writes (UAV), read back by ScreenGrab.
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = kSize;
    desc.Height = kSize;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> target;
    ComPtr<ID3D11UnorderedAccessView> targetUav;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &target)) ||
        FAILED(device->CreateUnorderedAccessView(target.Get(), nullptr, &targetUav)))
    {
        *error = "foam mask target creation failed";
        return false;
    }

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(BakeConstants);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> constants;
    if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &constants)))
    {
        *error = "foam bake constant buffer creation failed";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    for (const Mask& mask : kMasks)
    {
        const BakeConstants values = { mask.kind, kSize, kSeed, 0 };
        context->UpdateSubresource(constants.Get(), 0, nullptr, &values, 0, 0);
        context->CSSetShader(shader.Get(), nullptr, 0);
        context->CSSetConstantBuffers(0, 1, constants.GetAddressOf());
        context->CSSetUnorderedAccessViews(0, 1, targetUav.GetAddressOf(), nullptr);
        context->Dispatch((kSize + 7) / 8, (kSize + 7) / 8, 1); // [numthreads(8, 8, 1)]
        // Unbind before the copy: a texture still bound as a UAV can't be read back.
        ID3D11UnorderedAccessView* nullUav = nullptr;
        context->CSSetUnorderedAccessViews(0, 1, &nullUav, nullptr);
        context->CSSetShader(nullptr, nullptr, 0);

        // Delete first, then write (overwriting files in place from a fresh exe can be blocked; see EndCaptureFrame).
        const std::filesystem::path file = outputDir / mask.fileName;
        std::filesystem::remove(file, ec);
        // forceSRGB = false: the values are data (masks), saved as they are.
        const HRESULT hr = DirectX::SaveWICTextureToFile(context, target.Get(), GUID_ContainerFormatPng, file.wstring().c_str(),
                                                         nullptr, nullptr, false);
        if (FAILED(hr))
        {
            char code[16];
            std::snprintf(code, sizeof(code), "0x%08X", static_cast<unsigned int>(hr));
            *error = "saving " + file.string() + " failed (" + code + ")";
            return false;
        }
    }
    return true;
}
