#include "ColorShader.h"

#include "ColorSpace.h"

#include <windows.h>

#include <d3dcompiler.h>

#include <cmath>
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
    float steepness;
    float padding[2];
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
    
    // x = strength, y = sharpness, z = sun glint power, w = sun glint intensity
    DirectX::XMFLOAT4 specularParams;

    WaveParams waves[ColorShader::kWaveCount];
    DirectX::XMFLOAT4 debugParams; // x = debug mode

    // x = Fresnel F0, y = normal strength, z = detail layer scale, w = far wave normals (0/1)
    DirectX::XMFLOAT4 surfaceParams;
    DirectX::XMFLOAT4 normalRotation; // xy = cos/sin layer A, zw = layer B
    DirectX::XMFLOAT4 farParams;      // x = far glint spread (slope variance scale), y = haze distance, zw = map A / B slope variance
    // x = ripple roughness, y = gust strength, z = gust scale, w = haze strength
    DirectX::XMFLOAT4 detailParams;
    DirectX::XMFLOAT4 detailParams2; // x = far wave crests (1 = as before)
    DirectX::XMFLOAT4 sceneParams;   // x = transmission mode, y = max optical distance, z = surface reflection, w = normal source (SCENE_INPUTS only)
    DirectX::XMFLOAT4X4 invViewProjection; // test scenes: depth buffer -> world position
    DirectX::XMFLOAT4 extinction;          // rgb = extinction per meter (test scenes)
    DirectX::XMFLOAT4 scatterColor;        // rgb = linear scatter color, a = strength (test scenes)
    DirectX::XMFLOAT4 rampShallow;         // rgb = linear ramp colors
    DirectX::XMFLOAT4 rampMid;
    DirectX::XMFLOAT4 rampDeep;
    DirectX::XMFLOAT4 rampParams;          // x = mid depth (m), y = deep depth (m), z = by view path (0/1), w = color mode (0 physical, 1 ramp)
    DirectX::XMFLOAT4 foamParams;          // x = width (m), y = softness, z = noise size (m), w = strength (test scenes)
    DirectX::XMFLOAT4 foamParams2;         // x = slope filter, y / z = steepness start / full, w = flow speed (m/s)
    DirectX::XMFLOAT4 foamColor;           // rgb = linear foam color
    DirectX::XMFLOAT4 waterArea;           // x = water patch half size (m, 0 = none), y = wave edge fade width (m)
    DirectX::XMFLOAT4 refractionParams;    // x = strength (0 = straight), y = index of refraction, z = frame-edge fade (px)
};
static_assert(sizeof(WaveParams) == 32, "WaveParams must match the 2-register HLSL layout");
static_assert(sizeof(PerFrameCB) % 16 == 0, "Constant buffer size must be a multiple of 16 bytes");

bool CompileShader(const wchar_t* path, const char* entryPoint, const char* target, ID3DBlob** bytecode, std::string* outError,
    const D3D_SHADER_MACRO* defines = nullptr)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompileFromFile(
        path,
        defines,
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
    ID3D11ShaderResourceView* normalSRVA, // layer A (t1)
    ID3D11ShaderResourceView* normalSRVB, // layer B (t2)
    ID3D11SamplerState* clampSampler,
    ID3D11SamplerState* wrapSampler,
    ID3D11ShaderResourceView* sceneColorSRV,
    ID3D11ShaderResourceView* sceneDepthSRV,
    ID3D11ShaderResourceView* foamSRV,
    ID3D11ShaderResourceView* sceneNormalSRV)
{
    RenderShader(deviceContext, indexCount, world, view, projection, lightDirection, lightColor, ambientColor, time, cameraPositionWS, water, cubemapSRV, normalSRVA, normalSRVB, clampSampler, wrapSampler, sceneColorSRV, sceneDepthSRV, foamSRV, sceneNormalSRV);
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
        {GetShaderPath(L"Color.hlsli"),    {}},
        {GetShaderPath(L"Waves.hlsli"),    {}},
        {GetShaderPath(L"DepthBands.hlsli"), {}},
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
    // Test scene vertex shader (wave-occlusion): the waves fade out at the edge of the finite water patch.
    Microsoft::WRL::ComPtr<ID3DBlob> vsSceneBuffer;
    const D3D_SHADER_MACRO vsSceneDefines[] = { {"SCENE_INPUTS", "1"}, {nullptr, nullptr} };
    if (!CompileShader(vsPath_.c_str(), "VSMain", "vs_5_0", &vsSceneBuffer, &error, vsSceneDefines))
    {
        lastError_ = "[VS SCENE_INPUTS] " + error;
        return false;
    }
    if (!CompileShader(psPath_.c_str(), "PSMain", "ps_5_0", &psBuffer, &error))
    {
        lastError_ = error;
        return false;
    }
    // Ocean detail permutation. A separate program, so the plain one stays the exact code older presets
    // were made with: any extra code, even in a branch that is never taken, changes the driver's
    // instruction scheduling and moves a few pixels by 1-4 levels (ocean-hero NOTES).
    Microsoft::WRL::ComPtr<ID3DBlob> psDetailBuffer;
    const D3D_SHADER_MACRO detailDefines[] = { {"OCEAN_DETAIL", "1"}, {nullptr, nullptr} };
    if (!CompileShader(psPath_.c_str(), "PSMain", "ps_5_0", &psDetailBuffer, &error, detailDefines))
    {
        lastError_ = "[OCEAN_DETAIL] " + error;
        return false;
    }
    // Test scene permutation (scene-color-depth): reads the scene without the water. Its own program for the
    // same reason; OCEAN_DETAIL is on too, so the ocean presets' detail values still apply there.
    Microsoft::WRL::ComPtr<ID3DBlob> psSceneBuffer;
    const D3D_SHADER_MACRO sceneDefines[] = { {"OCEAN_DETAIL", "1"}, {"SCENE_INPUTS", "1"}, {nullptr, nullptr} };
    if (!CompileShader(psPath_.c_str(), "PSMain", "ps_5_0", &psSceneBuffer, &error, sceneDefines))
    {
        lastError_ = "[SCENE_INPUTS] " + error;
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11VertexShader> newVS;
    if (FAILED(device->CreateVertexShader(vsBuffer->GetBufferPointer(), vsBuffer->GetBufferSize(), nullptr, &newVS)))
    {
        lastError_ = "CreateVertexShader failed.";
        return false;
    }
    Microsoft::WRL::ComPtr<ID3D11VertexShader> newSceneVS;
    if (FAILED(device->CreateVertexShader(vsSceneBuffer->GetBufferPointer(), vsSceneBuffer->GetBufferSize(), nullptr, &newSceneVS)))
    {
        lastError_ = "CreateVertexShader (SCENE_INPUTS) failed.";
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11PixelShader> newPS;
    if (FAILED(device->CreatePixelShader(psBuffer->GetBufferPointer(), psBuffer->GetBufferSize(), nullptr, &newPS)))
    {
        lastError_ = "CreatePixelShader failed.";
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11PixelShader> newDetailPS;
    if (FAILED(device->CreatePixelShader(psDetailBuffer->GetBufferPointer(), psDetailBuffer->GetBufferSize(), nullptr, &newDetailPS)))
    {
        lastError_ = "CreatePixelShader (OCEAN_DETAIL) failed.";
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11PixelShader> newScenePS;
    if (FAILED(device->CreatePixelShader(psSceneBuffer->GetBufferPointer(), psSceneBuffer->GetBufferSize(), nullptr, &newScenePS)))
    {
        lastError_ = "CreatePixelShader (SCENE_INPUTS) failed.";
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
    vertexShaderScene_ = newSceneVS;
    pixelShader_ = newPS;
    pixelShaderDetail_ = newDetailPS;
    pixelShaderScene_ = newScenePS;
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
    pixelShaderScene_.Reset();
    pixelShaderDetail_.Reset();
    pixelShader_.Reset();
    vertexShaderScene_.Reset();
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
    ID3D11ShaderResourceView* normalSRVA, // layer A (t1)
    ID3D11ShaderResourceView* normalSRVB, // layer B (t2)
    ID3D11SamplerState* clampSampler,
    ID3D11SamplerState* wrapSampler,
    ID3D11ShaderResourceView* sceneColorSRV,
    ID3D11ShaderResourceView* sceneDepthSRV,
    ID3D11ShaderResourceView* foamSRV,
    ID3D11ShaderResourceView* sceneNormalSRV)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(deviceContext->Map(perFrameCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        auto* data = static_cast<PerFrameCB*>(mapped.pData);
        DirectX::XMStoreFloat4x4(&data->world, world);
        DirectX::XMStoreFloat4x4(&data->view, view);
        DirectX::XMStoreFloat4x4(&data->projection, projection);
        data->lightDirection = lightDirection;
        // Colors arrive as sRGB (what the pickers show); the shader lights in linear space.
        data->lightColor = SrgbToLinear(lightColor);
        data->ambientColor = SrgbToLinear(ambientColor);
        data->cameraPositionWS = cameraPositionWS;
        data->facingColor = SrgbToLinear(water.facingColor);
        data->grazingColor = SrgbToLinear(water.grazingColor);
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
            water.sunGlintPower,
            water.sunGlintIntensity);
        for (int i = 0; i < kWaveCount; ++i)
        {
            data->waves[i].direction  = water.waves[i].direction;
            data->waves[i].amplitude  = water.waves[i].amplitude;
            data->waves[i].wavelength = water.waves[i].wavelength;
            data->waves[i].speed      = water.waves[i].speed;
            data->waves[i].steepness  = water.waves[i].steepness;
            data->waves[i].padding[0] = data->waves[i].padding[1] = 0.0f;
        }
        data->debugParams = DirectX::XMFLOAT4(static_cast<float>(water.debugMode), 0.0f, 0.0f, 0.0f);
        data->surfaceParams = DirectX::XMFLOAT4(
            water.fresnelF0,
            water.normalStrength,
            water.detailScale,
            water.farWaveNormals);
        data->normalRotation = water.normalRotation;
        data->farParams = DirectX::XMFLOAT4(water.farGlintSpread, water.hazeDistance,
            water.rippleSlopeVarianceA, water.rippleSlopeVarianceB);
        data->detailParams = DirectX::XMFLOAT4(
            water.rippleRoughness,
            water.gustStrength,
            water.gustScale,
            water.hazeStrength);
        data->detailParams2 = DirectX::XMFLOAT4(water.farWaveCrests, water.lodScale, std::log2(water.lodScale), 0.0f);
        data->sceneParams = DirectX::XMFLOAT4(static_cast<float>(water.transmissionMode), water.maxOpticalDistance,
            water.surfaceReflection ? 1.0f : 0.0f, static_cast<float>(water.normalSource));
        data->invViewProjection = water.invViewProjection;
        data->extinction = DirectX::XMFLOAT4(water.extinction.x, water.extinction.y, water.extinction.z, 0.0f);
        const auto linear = [](const DirectX::XMFLOAT3& c, float w) { return SrgbToLinear(DirectX::XMFLOAT4(c.x, c.y, c.z, w)); };
        data->scatterColor = linear(water.scatterColor, water.scatterStrength);
        data->rampShallow = linear(water.rampShallow, 0.0f);
        data->rampMid = linear(water.rampMid, 0.0f);
        data->rampDeep = linear(water.rampDeep, 0.0f);
        data->rampParams = DirectX::XMFLOAT4(water.rampMidDepth, water.rampDeepDepth,
            static_cast<float>(water.rampByPath), static_cast<float>(water.waterColorMode));
        data->foamParams = DirectX::XMFLOAT4(water.foamWidth, water.foamSoftness, water.foamNoiseSize, water.foamStrength);
        data->foamParams2 = DirectX::XMFLOAT4(water.foamSlopeFilter, water.foamSteepStart, water.foamSteepFull, water.foamFlow);
        data->foamColor = linear(water.foamColor, 0.0f);
        data->waterArea = water.waterArea;
        data->refractionParams = DirectX::XMFLOAT4(water.refractionStrength, water.refractionIndex, 32.0f, 0.0f);
        deviceContext->Unmap(perFrameCB_.Get(), 0);
    }

    ID3D11ShaderResourceView* srvs[3] = {cubemapSRV, normalSRVA, normalSRVB};
    ID3D11SamplerState* samplers[2] = {clampSampler, wrapSampler};

    deviceContext->IASetInputLayout(layout_.Get());
    const bool sceneInputs = sceneColorSRV != nullptr && sceneDepthSRV != nullptr;
    // The depth prepass and the color pass must run the same vertex shader, or EQUAL would miss.
    deviceContext->VSSetShader(sceneInputs || water.depthOnly ? vertexShaderScene_.Get() : vertexShader_.Get(), nullptr, 0);
    deviceContext->VSSetConstantBuffers(0, 1, perFrameCB_.GetAddressOf());
    if (water.depthOnly)
    {
        // Water depth prepass (wave-occlusion): no pixel shader, the rasterizer writes the depth alone.
        deviceContext->PSSetShader(nullptr, nullptr, 0);
        deviceContext->DrawIndexed(indexCount, 0, 0);
        return;
    }
    const bool oceanDetail = water.rippleRoughness > 0.0f || water.gustStrength > 0.0f ||
        water.hazeStrength > 0.0f || water.farWaveCrests < 1.0f || water.lodScale != 1.0f || water.debugMode == 7;
    deviceContext->PSSetShader(sceneInputs ? pixelShaderScene_.Get()
                             : oceanDetail ? pixelShaderDetail_.Get() : pixelShader_.Get(), nullptr, 0);
    deviceContext->PSSetConstantBuffers(0, 1, perFrameCB_.GetAddressOf());
    deviceContext->PSSetShaderResources(0, 3, srvs);
    if (sceneInputs)
    {
        ID3D11ShaderResourceView* sceneSrvs[4] = {sceneColorSRV, sceneDepthSRV, foamSRV, sceneNormalSRV};
        deviceContext->PSSetShaderResources(3, 4, sceneSrvs);
    }
    deviceContext->PSSetSamplers(0, 2, samplers);
    deviceContext->DrawIndexed(indexCount, 0, 0);
}



