#pragma once

#include "ShadowFrame.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

class ColorShader
{
public:
    struct Wave
    {
        DirectX::XMFLOAT2 direction = {1.0f, 0.0f};
        float amplitude  = 0.05f;
        float wavelength = 2.0f;
        float speed      = 0.5f;
        float steepness  = 0.0f; // Gerstner Q: 0 = sine, 1 = sharpest crest before looping
    };

    static constexpr int kWaveCount = 4;

    struct WaterParams
    {
        DirectX::XMFLOAT4 facingColor  = {0.50f, 0.85f, 1.00f, 1.0f};
        DirectX::XMFLOAT4 grazingColor = {0.05f, 0.15f, 0.40f, 1.0f};
        DirectX::XMFLOAT2 normalScroll1 = { 0.03f,  0.02f};
        DirectX::XMFLOAT2 normalScroll2 = {-0.02f,  0.04f};
        float fresnelPower       = 5.0f;
        float reflectionStrength = 0.4f;
        float normalScale        = 1.0f;
        float specularStrength   = 0.25f;
        float specularSharpness  = 64.0f;
        float sunGlintPower      = 800.0f;  // reflect(-V, N) vs sun direction lobe
        float sunGlintIntensity  = 12.0f;   // HDR multiplier, >1 so glints survive the rolloff
        float farGlintSpread     = 1.0f;    // scales the far-field slope variance that widens the glint (1 = physical)
        float fresnelF0          = 0.5f;    // reflectance at normal incidence (water ~0.02)
        float normalStrength     = 1.0f;    // tangent-space XY multiplier
        float detailScale        = 1.0f;    // layer B tiling relative to layer A
        // Ocean detail (ocean-hero). All 0 = off, the image is the same as before.
        float rippleRoughness    = 0.0f;    // ripple slopes lost to the normal-map mips -> glint width (Toksvig)
        float gustStrength       = 0.0f;    // wind gust patches: +- this much ripple strength
        float gustScale          = 30.0f;   // size of a gust patch (world units)
        float hazeStrength       = 0.0f;    // blend toward the horizon sky with distance
        float hazeDistance       = 150.0f;  // distance where the haze reaches 1 - 1/e of hazeStrength
        float farWaveCrests      = 1.0f;    // past the mesh fade: 1 = crests per pixel (as before), 0 = all glint roughness
        // Underwater (rgb-attenuation, test scenes): how fast light fades per meter of view path, per channel (1/m).
        // 0 = nothing fades. Presets without the keys load as 0 and are then started from the Clear water type.
        DirectX::XMFLOAT3 extinction = {0.0f, 0.0f, 0.0f};
        // The water's own color, filling in what the extinction took (water-scatter, test scenes):
        //   under-water color = scene x T + color x incoming light x strength x (1 - T)
        // Colors are sRGB picker values (like facingColor). Strength 0 = off.
        int waterColorMode = 0;                                  // 0 physical (one scatter color), 1 ramp (art)
        DirectX::XMFLOAT3 scatterColor = {0.10f, 0.35f, 0.55f};  // physical: how much light the water sends back
        float scatterStrength = 0.0f;
        DirectX::XMFLOAT3 rampShallow = {0.35f, 0.95f, 0.90f};   // ramp: shallow -> mid -> deep
        DirectX::XMFLOAT3 rampMid     = {0.05f, 0.65f, 0.80f};
        DirectX::XMFLOAT3 rampDeep    = {0.02f, 0.20f, 0.55f};
        float rampMidDepth  = 1.0f;  // m: shallow reaches mid here
        float rampDeepDepth = 4.0f;  // m: mid reaches deep here
        int rampByPath = 0;          // 0 = ramp by water depth (same bed, same color), 1 = by view path
        // Contact foam (contact-foam, test scenes): a white band where a surface comes up through the water.
        // Presets without the keys load these defaults (on). Strength 0 = off.
        float foamStrength    = 1.0f;
        float foamWidth       = 0.25f;  // m of water depth the band reaches down
        float foamSoftness    = 0.08f;  // edge: small = hard stylized edge, large = soft fade
        float foamNoiseSize   = 1.5f;   // m: one repeat of the foam texture on the water
        float foamFlow        = 0.05f;  // m/s the noise drifts
        float foamSlopeFilter = 1.0f;   // 0 = water depth alone (classic, flat shallows foam too), 1 = steep surfaces only
        float foamSteepStart  = 0.35f;  // steepness (1 - |normal.y|) where foam starts ...
        float foamSteepFull   = 0.70f;  // ... and where it is full
        DirectX::XMFLOAT3 foamColor = {0.95f, 0.97f, 1.0f}; // sRGB picker value
        // contact-foam-polish (reference: fire-face.com/water). 0 / off = the band as before (presets without the keys
        // load these defaults).
        float foamEdgeBias = 0.2f;     // closeness + bias: a thicker, harder band that stops at the width (0 = off)
        float foamLeadingEdge = 0.15f; // share of the width over which the water and foam fade out at the contact line (0 = off)
        bool foamRidesWaves = true;    // the foam pattern is laid on the water before the waves move it (it sways along)
        int foamMeasure = 0;           // set every frame, not saved: 0 = water depth below the surface, 1 = view path in the water
        // foam-masks: which mask (kFoamMaps in Graphics.cpp, default 3 = cells + bubbles + breakup) and how much of its
        // bubbles (G) and breakup (B) to use.
        int foamMap = 3;
        float foamBubbles = 0.6f;
        float foamErosion = 0.5f;
        // Set every frame from the picked mask, not saved: x = repeats without a seam (else the old mirrored read),
        // y = G holds bubbles, z = B holds breakup (1 / 0).
        DirectX::XMFLOAT3 foamMapFlags = {1.0f, 1.0f, 1.0f};
        // Set every frame from the selected normal maps (NormalMapEntry::slopeVariance), not saved in presets.
        float rippleSlopeVarianceA = 0.0f;
        float rippleSlopeVarianceB = 0.0f;
        // --render-size only: render height / 720, so pixel-based LOD (far wave bands, normal map mips) picks
        // what a 720p frame would. 1 = off. Set every frame, not saved.
        float lodScale = 1.0f;
        // Wavelengths spread ~1.5x apart and directions fanned around the wind so no single crest dominates.
        Wave waves[kWaveCount] = {
            { { 0.940f,  0.342f}, 0.030f, 1.60f, 0.55f, 0.55f },
            { { 0.966f, -0.259f}, 0.018f, 1.05f, 0.45f, 0.60f },
            { { 0.574f,  0.819f}, 0.010f, 0.62f, 0.35f, 0.65f },
            { { 0.766f, -0.643f}, 0.006f, 0.41f, 0.28f, 0.70f },
        };
        int debugMode = 0; // 0=normal, 1=sampled normal map, 2=world-space N, 3=UV, 4=front/back face, 5=lighting terms, 6=wave LOD, 7=ocean detail,
                           // 8=scene depth raw, 9=water coverage (8 and 9: test scenes only)
        float farWaveNormals = 1.0f; // 1 = pixel shader adds the wave slopes the mesh LOD faded out
        // Test scenes only (scene-color-depth), set every frame, not saved: 0 = the opaque water as before,
        // 1 = pass-through (the scene without the water, unchanged), 2 = attenuation (rgb-attenuation).
        int transmissionMode = 0;
        float maxOpticalDistance = 50.0f; // m: the view path where no surface is behind the water (sky)
        // surface-reflection: the sky and sun mirrored on top of the water below (false = the water below alone).
        bool surfaceReflection = true;
        // normal-gbuffer: where the water gets the normal of the surface behind it. 0 = estimated from the depth
        // buffer, 1 = stored by the opaque pass (scene normal buffer).
        int normalSource = 0;
        // wave-occlusion: x = half size of the square water patch (m, 0 = none), y = width over which the waves fade
        // out toward its edge (test scenes; set every frame, not saved).
        DirectX::XMFLOAT4 waterArea = {0.0f, 0.0f, 0.0f, 0.0f};
        bool depthOnly = false; // water depth prepass draw: vertex shader alone, no pixel shader
        // refraction: where the water below is looked up. Mode 0 = normal offset (default; the view moves by how far
        // the surface tilts from flat, flat water does not move), 1 = physical (the line of sight bent by Snell).
        // Presets without the keys load these defaults; 0 strength = straight.
        int refractionMode = 0;
        float refractionDistortion = 1.0f;  // normal offset: 1 = the sideways shift Snell gives a tilted surface
        float refractionWobbleDepth = 2.0f; // normal offset: the water depth the shift is computed for (m, stylized: the same everywhere)
        float refractionShoreFade = 0.15f;  // normal offset: m of water depth over which it fades in from the shore
        float refractionStrength = 1.0f;    // physical: 1 = Snell, more = exaggerated
        float refractionIndex = 1.33f;      // water
        DirectX::XMFLOAT4X4 invViewProjection = {}; // inverse of view x projection: depth buffer -> world position
        // Normal map rotation on the water (world XZ), as cos/sin: xy = layer A, zw = layer B. (1, 0) = as authored.
        DirectX::XMFLOAT4 normalRotation = {1.0f, 0.0f, 1.0f, 0.0f};
        // sun-shadow (test scenes, set every frame, not saved): the sun shadow map (t7) and its comparison sampler (s2).
        // shadow.params.x = 0 = off: the surface is lit everywhere as before.
        ShadowFrame shadow{};
        ID3D11ShaderResourceView* shadowSRV = nullptr;
        ID3D11SamplerState* shadowSampler = nullptr;
    };

    bool Initialize(ID3D11Device* device);
    void Shutdown();
    void CheckHotReload(ID3D11Device* device, std::chrono::steady_clock::time_point now);
    bool Render(
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
        // The scene without the water (t3 color, t4 depth). Given = the SCENE_INPUTS program; the caller
        // unbinds them after the draw.
        ID3D11ShaderResourceView* sceneColorSRV = nullptr,
        ID3D11ShaderResourceView* sceneDepthSRV = nullptr,
        ID3D11ShaderResourceView* foamSRV = nullptr,   // contact foam noise (t5), SCENE_INPUTS only
        ID3D11ShaderResourceView* sceneNormalSRV = nullptr); // normal stored by the opaque pass (t6)

    const std::string& GetLastError() const { return lastError_; }
    const std::string& GetLastReloadStamp() const { return lastReloadStamp_; }

private:
    bool InitializeShader(ID3D11Device* device);
    bool Reload(ID3D11Device* device);
    void ShutdownShader();
    void RenderShader(
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
        ID3D11ShaderResourceView* sceneNormalSRV);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShaderScene_; // SCENE_INPUTS: waves fade at the water patch edge
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShaderDetail_; // OCEAN_DETAIL: used while any ocean detail is on
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShaderScene_;  // SCENE_INPUTS (+ OCEAN_DETAIL): water over the test scenes
    Microsoft::WRL::ComPtr<ID3D11InputLayout> layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> perFrameCB_;

    std::wstring vsPath_;
    std::wstring psPath_;

    struct WatchedFile
    {
        std::wstring path;
        std::filesystem::file_time_type mtime{};
    };
    std::vector<WatchedFile> watchedFiles_;

    std::chrono::steady_clock::time_point nextCheckTime_{};
    std::string lastError_;
    std::string lastReloadStamp_;
};
