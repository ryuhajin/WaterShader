#pragma once

#include <d3d11.h>

#include <filesystem>
#include <string>

// Bakes the contact foam masks (foam-masks) with a compute shader (shaders/foam_bake.hlsl) and saves them as PNG.
// Run once from the command line (--bake-foam-masks <folder>); the app itself only loads the saved textures,
// like the normal maps. tools/bake_foam_masks.ps1 runs it and converts the PNGs to DDS with mips.
class FoamBaker
{
public:
    static constexpr unsigned int kSize = 512;
    static constexpr unsigned int kSeed = 20261109;

    struct Mask
    {
        unsigned int kind;      // foam_bake.hlsl g_Bake.x
        const wchar_t* fileName; // PNG written into the output folder
    };

    // Bakes every mask into outputDir. Returns false (and fills error) on the first failure.
    static bool BakeAll(ID3D11Device* device, ID3D11DeviceContext* context, const std::filesystem::path& outputDir,
                        std::string* error);
};
