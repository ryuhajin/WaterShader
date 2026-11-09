#include "Graphics.h"

#include "ColorSpace.h"
#include "FoamBaker.h"
#include "Input.h"
#include "SystemConfig.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <ScreenGrab.h>
#include <shellapi.h>
#include <wincodec.h>

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>

namespace
{
std::wstring GetAssetPath(const wchar_t* relativePath)
{
#ifdef WATERSHADER_ASSETS_DIR
    std::filesystem::path path = WATERSHADER_ASSETS_DIR;
    path /= relativePath;
    return path.wstring();
#else
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    std::filesystem::path path(modulePath);
    path = path.parent_path() / L"assets" / relativePath;
    return path.wstring();
#endif
}

// Captures are kept outside the repository, in a ws-capture folder next to it, so the docs stay light
// and the same place is used by Debug and Release (Release used to write next to the exe).
std::filesystem::path GetDefaultCaptureRoot()
{
#ifdef WATERSHADER_CAPTURE_DIR
    return std::filesystem::path(WATERSHADER_CAPTURE_DIR);
#else
    // assets/ -> project root -> its parent
    return std::filesystem::path(GetAssetPath(L"shader_presets.txt")).parent_path().parent_path().parent_path() / L"ws-capture";
#endif
}

// std::string(ws.begin(), ws.end()) truncates each wchar_t to one byte (C4244) and mangles any
// non-ASCII path. ImGui and our logs expect UTF-8, so convert explicitly.
std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

// "a,b,c" -> {"a", "b", "c"} (command line lists).
std::vector<std::string> SplitList(const std::string& text)
{
    std::vector<std::string> items;
    std::istringstream in(text);
    std::string item;
    while (std::getline(in, item, ','))
    {
        if (!item.empty())
        {
            items.push_back(item);
        }
    }
    return items;
}

bool ListContains(const std::vector<std::string>& list, const std::string& value)
{
    return std::find(list.begin(), list.end(), value) != list.end();
}

struct CaptureShot
{
    const char* name;
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 rotation; // x = pitch, y = yaw (deg)
    float fovDeg;
    bool ocean; // true = large ocean grid, false = 2x2 bench plane
    int scene;  // Graphics::TestScene (left out = 0, no test scene)
};

// Fixed camera shots for before/after comparison. Bench plane spans x,z in [-1, 1].
// "sunward" looks along the skybox sun (yaw ~33 deg) from a low angle to catch the glint path.
// ocean_* shots use the large grid. They differ in height, lens and direction to the sun (all theme suns
// sit near yaw 35), so the three read as different pictures (ocean-shots).
constexpr CaptureShot kCaptureShots[] = {
    { "oblique",       {  0.00f,  1.00f,  -2.20f }, { 24.0f,  0.0f, 0.0f }, 60.0f, false },
    { "top",           {  0.00f,  2.40f,   0.00f }, { 89.9f,  0.0f, 0.0f }, 60.0f, false },
    { "sunward",       { -1.04f,  0.45f,  -1.59f }, { 12.0f, 33.0f, 0.0f }, 60.0f, false },
    // Wave level into the sun, wide lens: big crests up front, glint path to the horizon.
    // 0.3 stays above the tallest crests (amplitude sum ~0.19, Sunset).
    { "ocean_surface", {  0.00f,  0.30f,   0.00f }, {  3.0f, 34.5f, 0.0f }, 70.0f, true  },
    // From above, aimed at the origin (dense grid): the wave pattern, the far-field fade and a wide glitter.
    // Yaw 15 keeps the sun (yaw ~35) right of centre: the wave rows cross the frame at an angle instead of
    // lying flat, and the glint stays in frame. The frame top stays 5 deg below the horizon, so the grid
    // edge (1.7 deg below) is never in view.
    { "ocean_aerial",  { -5.38f, 12.00f, -20.09f }, { 30.0f, 15.0f, 0.0f }, 50.0f, true  },
    // Away from the sun through a long lens: compressed rows of waves, shore reflections, no glint.
    { "ocean_tele",    {  0.00f,  1.80f,   0.00f }, {  3.0f, 214.5f, 0.0f }, 28.0f, true  },
    // Portfolio main image (ocean-hero): same spot as ocean_aerial but lower and flatter, so the far shore
    // and sky take the top ~1/5 and the water ~4/5, with the glint path right of centre.
    { "ocean_hero",    { -5.38f,  6.00f, -20.09f }, { 14.0f, 18.0f, 0.0f }, 44.0f, true  },
    // water-lab test scenes (BuildWaterLab / BuildWaterfront): straight down, the stairs at an angle,
    // just above the water toward the depth pads, close to the pillar through the surface.
    { "lab_top",       {  0.00f, 20.00f,   0.00f }, { 89.9f,   0.0f, 0.0f }, 60.0f, true, 1 },
    { "lab_stairs",    { -3.00f,  4.00f, -10.00f }, { 35.0f, 335.0f, 0.0f }, 60.0f, true, 1 },
    { "lab_low",       {  0.00f,  0.40f, -10.00f }, {  3.0f,   0.0f, 0.0f }, 60.0f, true, 1 },
    { "lab_pillar",    {  5.50f,  1.20f,  -6.50f }, { 23.5f, 320.0f, 0.0f }, 55.0f, true, 1 },
    // Target scene graybox: standing on the flooded track, looking down the line.
    { "front_rails",   {  0.00f,  1.70f,  -9.00f }, { 10.0f,   0.0f, 0.0f }, 60.0f, true, 2 },
    // contact-foam: close to the pillar looking down, where Fresnel alone shows no waterline (surface-reflection
    // NOTES). The view ray through the frame center meets the water right in front of the pillar.
    { "lab_pillar_near", { 3.00f,  2.20f,  -6.30f }, { 35.0f,   0.0f, 0.0f }, 60.0f, true, 1 },
};
constexpr int kFirstLabShotIndex = 7; // UI: the lab shots start a new button row

// Water patches of the test scenes (half size, m) and how far in from their edge the waves die out
// (wave-occlusion: the sideways motion would otherwise pull the edge in and open a gap).
constexpr float kLabWaterHalfSize = 12.0f;
constexpr float kFrontWaterHalfSize = 40.0f;
constexpr float kWaveEdgeFade = 1.0f;
// sun-shadow: the Water Lab test box circles above the open water (center, radius, height, one turn per period).
constexpr float kTestBoxSize = 1.0f;
constexpr float kTestBoxCircleRadius = 3.0f;
constexpr float kTestBoxBottomY = 0.5f;
constexpr float kTestBoxPeriod = 20.0f; // s

// Index of the fixed shot with this name (0 if there is none).
int FindCaptureShot(const char* name)
{
    for (int i = 0; i < static_cast<int>(std::size(kCaptureShots)); ++i)
    {
        if (std::string(kCaptureShots[i].name) == name)
        {
            return i;
        }
    }
    return 0;
}

// Startup / Reset Camera framing: the step2_sun_glint "sunward" shot (bench plane, glint visible).
constexpr int kDefaultShotIndex = 2;
// The shot that switches to the preset's Top View Sun.
constexpr int kTopShotIndex = 1;
static_assert(kCaptureShots[kTopShotIndex].position.y == 2.40f && kCaptureShots[kTopShotIndex].rotation.x == 89.9f,
    "kTopShotIndex must point at the \"top\" shot");

constexpr const char* kPresetFileNames[] = { "basic", "sunset", "tropical" };
constexpr const char* kPresetLabels[] = { "Basic", "Sunset", "Tropical" };
constexpr int kCaptureShotCount = static_cast<int>(std::size(kCaptureShots));

// v4: one line per mesh x theme, each starting with its mesh ("bench" / "ocean"). v3 (one line per
// theme, no mesh) is still read and used for both meshes.
constexpr int kPresetFileVersion = 4;
constexpr const char* kPresetMeshNames[] = { "bench", "ocean" };

// Optional "key value" pairs appended after the fixed preset fields. Missing keys keep the
// code defaults and unknown keys are skipped, so new parameters don't need a format bump.
template <typename Preset, typename Fn>
void ForEachExtraField(Preset& preset, Fn&& fn)
{
    fn("sunGlintPower", preset.water.sunGlintPower);
    fn("sunGlintIntensity", preset.water.sunGlintIntensity);
    fn("farGlintSpread", preset.water.farGlintSpread);
    fn("fresnelF0", preset.water.fresnelF0);
    fn("normalStrength", preset.water.normalStrength);
    fn("detailScale", preset.water.detailScale);
    fn("environment", preset.environment);
    fn("exposureEV", preset.exposureEv);
    fn("normalMapA", preset.normalMapA);
    fn("normalMapB", preset.normalMapB);
    fn("rippleAlignA", preset.rippleAlignA);
    fn("rippleAlignB", preset.rippleAlignB);
    fn("waveWindDeg", preset.waveMacro.windDeg);
    fn("waveSpreadDeg", preset.waveMacro.spreadDeg);
    fn("waveSize", preset.waveMacro.size);
    fn("waveHeight", preset.waveMacro.height);
    fn("waveChop", preset.waveMacro.chop);
    fn("waveSpeed", preset.waveMacro.speedScale);
    fn("topSun", preset.topSun.enabled);
    fn("topSunYawDeg", preset.topSun.yawDeg);
    fn("topSunElevationDeg", preset.topSun.elevationDeg);
    fn("topSunIntensity", preset.topSun.intensity);
    fn("rippleRoughness", preset.water.rippleRoughness);
    fn("gustStrength", preset.water.gustStrength);
    fn("gustScale", preset.water.gustScale);
    fn("hazeStrength", preset.water.hazeStrength);
    fn("hazeDistance", preset.water.hazeDistance);
    fn("farWaveCrests", preset.water.farWaveCrests);
    fn("extinctionR", preset.water.extinction.x);
    fn("extinctionG", preset.water.extinction.y);
    fn("extinctionB", preset.water.extinction.z);
    fn("waterColorMode", preset.water.waterColorMode);
    fn("scatterR", preset.water.scatterColor.x);
    fn("scatterG", preset.water.scatterColor.y);
    fn("scatterB", preset.water.scatterColor.z);
    fn("scatterStrength", preset.water.scatterStrength);
    fn("rampShallowR", preset.water.rampShallow.x);
    fn("rampShallowG", preset.water.rampShallow.y);
    fn("rampShallowB", preset.water.rampShallow.z);
    fn("rampMidR", preset.water.rampMid.x);
    fn("rampMidG", preset.water.rampMid.y);
    fn("rampMidB", preset.water.rampMid.z);
    fn("rampDeepR", preset.water.rampDeep.x);
    fn("rampDeepG", preset.water.rampDeep.y);
    fn("rampDeepB", preset.water.rampDeep.z);
    fn("rampMidDepth", preset.water.rampMidDepth);
    fn("rampDeepDepth", preset.water.rampDeepDepth);
    fn("rampByPath", preset.water.rampByPath);
    fn("foamStrength", preset.water.foamStrength);
    fn("foamWidth", preset.water.foamWidth);
    fn("foamSoftness", preset.water.foamSoftness);
    fn("foamNoiseSize", preset.water.foamNoiseSize);
    fn("foamFlow", preset.water.foamFlow);
    fn("foamSlopeFilter", preset.water.foamSlopeFilter);
    fn("foamSteepStart", preset.water.foamSteepStart);
    fn("foamSteepFull", preset.water.foamSteepFull);
    fn("foamR", preset.water.foamColor.x);
    fn("foamG", preset.water.foamColor.y);
    fn("foamB", preset.water.foamColor.z);
    fn("foamMap", preset.water.foamMap);
    fn("foamBubbles", preset.water.foamBubbles);
    fn("foamErosion", preset.water.foamErosion);
    fn("refractionMode", preset.water.refractionMode);
    fn("refractionDistortion", preset.water.refractionDistortion);
    fn("refractionShoreFade", preset.water.refractionShoreFade);
    fn("refractionWobbleDepth", preset.water.refractionWobbleDepth);
    fn("refractionStrength", preset.water.refractionStrength);
}

// Tangent-space water normal maps (RGBA8 UNORM + mips, converted with texconv --ignore-srgb so the
// stored values are not gamma-decoded; flat = 128,128,255). Presets store the index, so only append.
struct NormalMapEntry
{
    const wchar_t* file;
    const char* name;
    // Travel axis of the ripples as laid on the water (0 = +X, 90 = +Z, mod 180). "Align to wind"
    // rotates the map by (wind - this). Measured by tools/measure_normal_orientation.ps1 (pattern axis).
    float rippleAxisDeg;
    // Mean squared slope (x^2 + y^2) / z^2 of the full-resolution map: what "Far ripple glitter" turns into
    // glint width once the mips have averaged the ripples away (ocean-hero NOTES, measured from the jpg sources).
    float slopeVariance;
};
constexpr NormalMapEntry kNormalMaps[] = {
    { L"textures/water_normal.dds",  "0 Diagonal ripples (water_normal)", -76.0f, 0.0600f },
    { L"textures/water_normal1.dds", "1 Soft swell (water_normal1)",       84.0f, 0.0125f },
    { L"textures/water_normal2.dds", "2 Soft chop (water_normal2)",        70.0f, 0.0072f },
    { L"textures/water_normal3.dds", "3 Long streaks (water_normal3)",     89.0f, 0.0229f },
    { L"textures/water_normal4.dds", "4 Fine chop (water_normal4)",        67.0f, 0.0619f },
};

// Contact foam masks (foam-masks). The new ones are baked by a compute shader (tools/bake_foam_masks.ps1) and repeat
// without a seam; R = foam (1 = forms first), G = bubbles, B = breakup (0.5 = neutral). Presets store the index,
// so only append.
struct FoamMapEntry
{
    const wchar_t* file;
    const char* name;
    bool tileable;   // false: the old cloud texture, read mirrored and used as before (contact-foam)
    bool bubbles;    // G holds bubbles
    bool breakup;    // B holds breakup noise
};
constexpr FoamMapEntry kFoamMaps[] = {
    { L"textures/wave_foam.dds",    "0 Clouds (old, wave_foam)",            false, false, false },
    { L"textures/foam_cells.dds",   "1 Cells (foam network)",               true,  false, false },
    { L"textures/foam_bubbles.dds", "2 Bubbles",                            true,  true,  false },
    { L"textures/foam_packed.dds",  "3 Cells + bubbles + breakup",          true,  true,  true  },
    { L"textures/foam_streaks.dds", "4 Streaks (stretched cells)",          true,  true,  true  },
};
constexpr int kFoamMapCount = static_cast<int>(std::size(kFoamMaps));

// Water types (water-scatter): light loss and water color set together. A starting point to tune from, not a
// measurement. Colors are sRGB picker values.
//   Clear  - the bed shows far down, deep water turns blue.
//   Green  - murky green water (reference photo 2): strong loss, green passes best, stairs vanish quickly.
//   Lagoon - art ramp by water depth: saturated cyan over the shallows, deep blue beyond 4 m.
constexpr int kWaterTypeCount = 3;
constexpr const char* kWaterTypeLabels[kWaterTypeCount] = { "Clear", "Green", "Lagoon (ramp)" };
constexpr const char* kWaterTypeNames[kWaterTypeCount] = { "clear", "green", "lagoon" };

void ApplyWaterType(ColorShader::WaterParams& water, int type)
{
    switch (type)
    {
    case 0:
        water.extinction = {0.35f, 0.07f, 0.03f};
        water.waterColorMode = 0;
        water.scatterColor = {0.10f, 0.35f, 0.55f};
        water.scatterStrength = 1.0f;
        break;
    case 1:
        water.extinction = {0.80f, 0.25f, 0.70f};
        water.waterColorMode = 0;
        water.scatterColor = {0.15f, 0.45f, 0.25f};
        water.scatterStrength = 1.0f;
        break;
    case 2:
        water.extinction = {0.60f, 0.20f, 0.15f};
        water.waterColorMode = 1;
        water.rampShallow = {0.35f, 0.95f, 0.90f};
        water.rampMid = {0.05f, 0.65f, 0.80f};
        water.rampDeep = {0.02f, 0.20f, 0.55f};
        water.rampMidDepth = 1.0f;
        water.rampDeepDepth = 4.0f;
        water.rampByPath = 0;
        water.scatterStrength = 1.0f;
        break;
    default:
        break;
    }
}

// Rotation (cos, sin) that turns a normal map whose ripples travel along axisDeg so they travel along
// windDeg. Axes are mod 180, so the smaller of the two turns is used.
DirectX::XMFLOAT2 RippleRotation(float windDeg, float axisDeg)
{
    float turn = std::fmod(windDeg - axisDeg, 180.0f);
    if (turn > 90.0f) turn -= 180.0f;
    if (turn < -90.0f) turn += 180.0f;
    const float r = DirectX::XMConvertToRadians(turn);
    return { std::cos(r), std::sin(r) };
}

// Sky + reflection cube maps a preset can pick. Sun directions were measured from each panorama
// (tools/equirect_to_cube.ps1). The sunset sky was rotated so its sun sits at yaw 34.5 like
// skybox.dds (the fixed "sunward" shots face it); the beach was rotated so its open sea faces the
// shots instead, which puts its sun behind the camera (yaw 236.4).
struct Environment
{
    const wchar_t* file;
    const char* name;
    float sunYawDeg;
    float sunElevationDeg;
    bool hdr; // false: LDR sRGB-encoded (read through an *_SRGB view); true: BC6H linear radiance
    // Measured from the .hdr by equirect_to_cube.ps1, already divided by pi (Lambert convention):
    DirectX::XMFLOAT3 sunLight;     // sun irradiance normal to the sun / pi (0 = no visible disk)
    DirectX::XMFLOAT3 ambientLight; // sky irradiance on an upward plane / pi (sun excluded)
    float keyExposureEv;            // log-average luminance -> 0.18
};

// 0-2: LDR "Tonemapped JPG" skies (kept for LDR vs HDR comparison).
// 3-5: the same Poly Haven skies from the .hdr originals (BC6H_UF16, measured by equirect_to_cube.ps1).
//      The HDR sunset is rotated by its real radiance peak (yaw 64 in the source), the LDR one by the
//      centroid of its clipped glow (yaw 103.8) - the clipped JPG had pointed ~40 deg off.
constexpr Environment kEnvironments[] = {
    { L"textures/skybox.dds",              "LDR Meadow dusk",                       34.5f,  4.0f, false, {}, {}, 0.0f },
    { L"textures/env_sunset_fire.dds",     "LDR Sunset sea (the_sky_is_on_fire)",   34.5f,  6.4f, false, {}, {}, 0.0f },
    { L"textures/env_beach_day.dds",       "LDR Beach day (spiaggia_di_mondello)", 236.4f, 25.3f, false, {}, {}, 0.0f },
    { L"textures/env_meadow_hdr.dds",      "HDR Meadow dusk (grasslands_sunset)",   34.5f,  3.3f, true,
      {0.4223f, 0.0803f, 0.0101f}, {1.0182f, 1.2756f, 1.6360f}, -0.96f },
    { L"textures/env_sunset_fire_hdr.dds", "HDR Sunset sea (the_sky_is_on_fire)",   34.5f,  4.0f, true,
      {0.0f, 0.0f, 0.0f},          {0.8992f, 0.7332f, 1.0624f}, -0.27f },
    { L"textures/env_beach_day_hdr.dds",   "HDR Beach day (spiaggia_di_mondello)", 236.4f, 25.2f, true,
      {1.6680f, 1.7421f, 1.4529f}, {0.1819f, 0.3168f, 0.6009f}, -0.81f },
    { L"textures/env_field_day_hdr.dds",   "HDR Field day (belfast_farmhouse)",     34.5f, 21.6f, true,
      {3.4202f, 3.1902f, 2.6979f}, {0.1512f, 0.2329f, 0.3562f},  0.15f },
};

// Real sun: angular radius ~0.2665 deg.
constexpr float kSunAngularRadiusDeg = 0.2665f;
// Status text that needs to stand out (wave "Custom", Top View Sun in use).
const ImVec4 kHighlightColor(1.0f, 0.78f, 0.3f, 1.0f);
constexpr int kEnvironmentCount = static_cast<int>(std::size(kEnvironments));

constexpr float kCaptureTime = 12.0f;
} // namespace

bool Graphics::Initialize(HWND hwnd, int screenWidth, int screenHeight)
{
    screenWidth_ = static_cast<unsigned int>(screenWidth);
    screenHeight_ = static_cast<unsigned int>(screenHeight);

    // Command line:
    //   --capture <label>      save every preset x shot, then quit
    //   --bench <label>        time every preset x shot instead (VSync off, GPU ms per pass) -> <label>/timings.csv
    //   --bake-foam-masks <dir>  bake the contact foam masks (compute shader) as PNG into <dir>, then quit
    //   --bench-frames <n>     timed frames per shot (default 300, after 60 warm-up frames)
    //   --debug <mode>         debug view used for the capture set
    //   --normal-map <path>    extra normal map (relative to assets/), used for both layers
    //   --normal-a / --normal-b <index>  normal map per layer (kNormalMaps), overrides the presets
    //   --foam-map <index>     contact foam mask (kFoamMaps), overrides the presets
    //   --shot <name>          start from a fixed camera shot (e.g. ocean_aerial)
    //   --ui <view|light|water|surface|all>  open settings windows at start (screenshots; keys 1-4 otherwise)
    //   --no-vsync             start uncapped (for FPS / GPU ms measurement)
    //   --capture-feature <f>  captures go to <capture root>/<f>/captures (default water-polish)
    //   --capture-root <dir>   capture root instead of ../ws-capture next to the repository
    //   --tonemap <none|reinhard|aces|aces-hue>  tone curve for this run
    //   --exposure <ev>        overrides every preset's exposure (operator comparisons)
    //   --far-waves <on|off>   pixel-shader wave slopes past the mesh wave fade (default on)
    //   --water <on|off>       draw the water in the test scenes (lab_* / front_* shots, default on)
    //   --lab-waves <on|off>   Gerstner waves in the test scenes (Surface window values, default on)
    //   --water-occlusion <none|single|prepass>  test scenes: how overlapping waves hide each other (default prepass)
    //   --scene-debug <mode>   test scene debug view: 1 = depth below the water (the water is hidden)
    //   --transmission <mode>  test scene water: 0 = opaque (old), 1 = pass-through, 2 = attenuation, 3 = scattering (default)
    //   --reflection <on|off>  test scene water: sky and sun mirrored on top of the water below (default on)
    //   --extinction <r,g,b>   underwater extinction per meter, overrides every preset (rgb-attenuation)
    //   --water-type <clear|green|lagoon>  underwater loss + color together, overrides every preset (water-scatter)
    //   --scatter <r,g,b,s>    water color (sRGB) and strength, overrides every preset (after --water-type)
    //   --ramp-by <depth|path> what the ramp color follows, overrides every preset (after --water-type)
    //   --foam <strength>      contact foam strength, overrides every preset (0 = off)
    //   --foam-slope <0..1>    contact foam slope filter (0 = water depth alone, 1 = steep surfaces only)
    //   --refraction <strength>  test scene refraction strength of both modes, overrides every preset (0 = straight)
    //   --refraction-mode <normal|physical>  normal offset (default) or Snell-bent line of sight
    //   --normal-source <depth|stored>  where the water gets the normal behind it: depth buffer (default) or normal buffer
    //   --resize-cycle         recreate the size-dependent targets once before rendering (resize test)
    //   --d3d-selftest         cause one known debug layer hazard (checks that d3d_messages.txt catches it)
    //   --align-ripples <on|off>  "Align to wind" for both normal map layers, overrides the presets
    //   --preset-file <path>   load/save presets from this file instead of assets/shader_presets.txt
    //                          (captures with fixed values while the working presets keep changing)
    //   --camera-file <path>   load/save camera slots from this file instead of assets/camera_presets.txt
    //   --capture-presets <a,b>   capture only these presets (basic, sunset, tropical)
    //   --capture-shots <a,b>     capture only these shots; slot1..slot4 are the saved camera slots
    //   --capture-format <jpg|png>  png = lossless (final images, exact comparisons)
    //   --render-size <WxH>    back buffer size independent of the window (high-resolution captures)
    std::wstring captureLabelArg;
    std::wstring bakeFoamDir; // --bake-foam-masks
    std::wstring normalMapArg;
    std::wstring shotArg;
    bool vsyncArg = VSYNC_ENABLED;
    bool resizeCycle = false;
    bool d3dSelfTest = false;
    {
        int argc = 0;
        if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc))
        {
            for (int i = 1; i < argc; ++i)
            {
                const std::wstring arg = argv[i];
                const bool hasValue = i + 1 < argc;
                if (arg == L"--no-vsync")                    { vsyncArg = false; }
                else if (arg == L"--capture" && hasValue)    { captureLabelArg = argv[++i]; }
                else if (arg == L"--bake-foam-masks" && hasValue) { bakeFoamDir = argv[++i]; }
                else if (arg == L"--bench" && hasValue)      { captureLabelArg = argv[++i]; benchMode_ = true; }
                else if (arg == L"--bench-frames" && hasValue) { benchSampleCount_ = std::max<int>(_wtoi(argv[++i]), 10); }
                else if (arg == L"--debug" && hasValue)      { captureDebugMode_ = _wtoi(argv[++i]); }
                else if (arg == L"--normal-map" && hasValue) { normalMapArg = argv[++i]; }
                else if (arg == L"--normal-a" && hasValue)   { normalOverrideA_ = _wtoi(argv[++i]); }
                else if (arg == L"--foam-map" && hasValue)   { foamMapOverride_ = _wtoi(argv[++i]); }
                else if (arg == L"--normal-b" && hasValue)   { normalOverrideB_ = _wtoi(argv[++i]); }
                else if (arg == L"--shot" && hasValue)       { shotArg = argv[++i]; }
                else if (arg == L"--ui" && hasValue)
                {
                    const std::wstring ui = argv[++i];
                    showViewWindow_ = ui == L"view" || ui == L"all";
                    showLightWindow_ = ui == L"light" || ui == L"all";
                    showWaterWindow_ = ui == L"water" || ui == L"all";
                    showSurfaceWindow_ = ui == L"surface" || ui == L"all";
                }
                else if (arg == L"--capture-feature" && hasValue) { captureFeature_ = argv[++i]; }
                else if (arg == L"--capture-root" && hasValue) { captureRootOverride_ = argv[++i]; }
                else if (arg == L"--preset-file" && hasValue) { presetFileOverride_ = argv[++i]; }
                else if (arg == L"--camera-file" && hasValue) { cameraFileOverride_ = argv[++i]; }
                else if (arg == L"--capture-presets" && hasValue) { capturePresetFilter_ = SplitList(WideToUtf8(argv[++i])); }
                else if (arg == L"--capture-shots" && hasValue)   { captureShotFilter_ = SplitList(WideToUtf8(argv[++i])); }
                else if (arg == L"--capture-format" && hasValue)  { capturePng_ = std::wstring(argv[++i]) == L"png"; }
                else if (arg == L"--render-size" && hasValue)
                {
                    int width = 0;
                    int height = 0;
                    if (swscanf_s(argv[++i], L"%dx%d", &width, &height) == 2 && width > 0 && height > 0)
                    {
                        screenWidth = width;
                        screenHeight = height;
                        screenWidth_ = static_cast<unsigned int>(width);
                        screenHeight_ = static_cast<unsigned int>(height);
                        fixedRenderSize_ = true;
                    }
                }
                else if (arg == L"--align-ripples" && hasValue) { rippleAlignOverride_ = std::wstring(argv[++i]) == L"off" ? 0 : 1; }
                else if (arg == L"--far-waves" && hasValue)  { farWaveNormals_ = std::wstring(argv[++i]) != L"off"; }
                else if (arg == L"--water" && hasValue)      { showWater_ = std::wstring(argv[++i]) != L"off"; }
                else if (arg == L"--lab-waves" && hasValue)  { labWaves_ = std::wstring(argv[++i]) != L"off"; }
                else if (arg == L"--water-occlusion" && hasValue)
                {
                    const std::wstring mode = argv[++i];
                    waterOcclusion_ = mode == L"none" ? 0 : mode == L"single" ? 1 : 2;
                }
                else if (arg == L"--scene-debug" && hasValue) { sceneDebug_ = _wtoi(argv[++i]); }
                else if (arg == L"--shadows" && hasValue)    { shadowsEnabled_ = std::wstring(argv[++i]) != L"off"; }
                else if (arg == L"--shadow-filter" && hasValue)
                {
                    const std::wstring value = argv[++i];
                    shadowFilter_ = value == L"hard" ? 0 : value == L"pcss" ? 2 : 1;
                }
                else if (arg == L"--shadow-bias" && hasValue)
                {
                    const std::wstring value = argv[++i];
                    shadowBiasMode_ = value == L"none" ? 0 : value == L"constant" ? 1 : value == L"slope" ? 2
                                    : value == L"normal" ? 3 : 4;
                }
                else if (arg == L"--shadow-bias-values" && hasValue)
                {
                    // constant (m), slope (texels), normal offset (texels)
                    swscanf_s(argv[++i], L"%f,%f,%f", &shadowConstantBias_, &shadowSlopeBias_, &shadowNormalOffset_);
                }
                else if (arg == L"--shadow-sun-size" && hasValue) { shadowSunSizeDeg_ = static_cast<float>(_wtof(argv[++i])); }
                else if (arg == L"--shadow-test-box" && hasValue) { shadowTestBox_ = std::wstring(argv[++i]) != L"off"; }
                else if (arg == L"--capture-time" && hasValue) { captureTime_ = static_cast<float>(_wtof(argv[++i])); }
                else if (arg == L"--transmission" && hasValue) { transmissionMode_ = _wtoi(argv[++i]); }
                else if (arg == L"--reflection" && hasValue) { surfaceReflection_ = std::wstring(argv[++i]) != L"off"; }
                else if (arg == L"--water-type" && hasValue)
                {
                    const std::string name = WideToUtf8(argv[++i]);
                    for (int type = 0; type < kWaterTypeCount; ++type)
                    {
                        if (name == kWaterTypeNames[type])
                        {
                            waterTypeOverride_ = type;
                        }
                    }
                }
                else if (arg == L"--ramp-by" && hasValue) { rampByOverride_ = std::wstring(argv[++i]) == L"path" ? 1 : 0; }
                else if (arg == L"--foam" && hasValue)       { foamStrengthOverride_ = static_cast<float>(_wtof(argv[++i])); }
                else if (arg == L"--foam-slope" && hasValue) { foamSlopeOverride_ = static_cast<float>(_wtof(argv[++i])); }
                else if (arg == L"--refraction" && hasValue) { refractionOverride_ = static_cast<float>(_wtof(argv[++i])); }
                else if (arg == L"--refraction-mode" && hasValue) { refractionModeOverride_ = std::wstring(argv[++i]) == L"physical" ? 1 : 0; }
                else if (arg == L"--normal-source" && hasValue) { normalSource_ = std::wstring(argv[++i]) == L"stored" ? 1 : 0; }
                else if (arg == L"--scatter" && hasValue)
                {
                    DirectX::XMFLOAT4 s{};
                    if (swscanf_s(argv[++i], L"%f,%f,%f,%f", &s.x, &s.y, &s.z, &s.w) == 4)
                    {
                        scatterOverride_ = s;
                        hasScatterOverride_ = true;
                    }
                }
                else if (arg == L"--extinction" && hasValue)
                {
                    DirectX::XMFLOAT3 k{};
                    if (swscanf_s(argv[++i], L"%f,%f,%f", &k.x, &k.y, &k.z) == 3)
                    {
                        extinctionOverride_ = k;
                        hasExtinctionOverride_ = true;
                    }
                }
                else if (arg == L"--resize-cycle")            { resizeCycle = true; }
                else if (arg == L"--d3d-selftest")            { d3dSelfTest = true; }
                else if (arg == L"--exposure" && hasValue)   { exposureOverrideEv_ = static_cast<float>(_wtof(argv[++i])); hasExposureOverride_ = true; }
                else if (arg == L"--tonemap" && hasValue)
                {
                    const std::wstring op = argv[++i];
                    tonemapOperator_ = op == L"none" ? TonemapShader::None
                                     : op == L"reinhard" ? TonemapShader::Reinhard
                                     : op == L"aces-hue" ? TonemapShader::AcesHuePreserving
                                     : op == L"aces" ? TonemapShader::Aces
                                     : TonemapShader::AcesHuePreserving;
                }
            }
            LocalFree(argv);
        }
    }

    d3d_ = std::make_unique<D3DClass>();
    if (!d3d_->Initialize(screenWidth, screenHeight, vsyncArg, hwnd, FULL_SCREEN, SCREEN_DEPTH, SCREEN_NEAR))
    {
        return false;
    }

    // --bake-foam-masks: compute the foam masks into PNGs and quit (tools/bake_foam_masks.ps1 makes the DDS files).
    if (!bakeFoamDir.empty())
    {
        std::string bakeError;
        const bool baked = FoamBaker::BakeAll(d3d_->GetDevice(), d3d_->GetDeviceContext(), bakeFoamDir, &bakeError);
        std::ofstream log(std::filesystem::path(bakeFoamDir) / L"bake_log.txt");
        log << (baked ? "baked" : "FAILED: " + bakeError) << '\n';
        DrainD3dMessages();
        log << "D3D11 debug layer: " << (d3d_->HasDebugLayer() ? "on" : "off") << ", warnings and errors: "
            << d3dMessages_.size() << " different\n";
        for (const auto& [message, count] : d3dMessages_)
        {
            log << count << "x  " << message << '\n';
        }
        captureFinishedQuit_ = true; // the first Render returns false: the app closes
    }

    camera_ = std::make_unique<Camera>();
    int startShot = kDefaultShotIndex;
    for (int i = 0; i < static_cast<int>(std::size(kCaptureShots)); ++i)
    {
        if (shotArg == Utf8ToWide(kCaptureShots[i].name))
        {
            startShot = i;
        }
    }
    ApplyCameraShot(startShot);
    LoadCameraSlots();

    light_ = std::make_unique<Light>();
    light_->SetDirection(0.0f, -1.0f, 1.0f);
    light_->SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);

    {
        std::vector<std::pair<std::wstring, std::string>> files;
        for (const NormalMapEntry& entry : kNormalMaps)
        {
            files.emplace_back(entry.file, entry.name);
            normalMapAxisDeg_.push_back(entry.rippleAxisDeg);
            normalMapSlopeVariance_.push_back(entry.slopeVariance);
        }
        if (!normalMapArg.empty())
        {
            files.emplace_back(normalMapArg, std::to_string(files.size()) + " --normal-map " + WideToUtf8(normalMapArg));
            normalMapAxisDeg_.push_back(0.0f); // not measured: Align to wind treats it as running along +X
            normalMapSlopeVariance_.push_back(0.03f); // not measured: a typical value of the maps above
            normalOverrideA_ = normalOverrideB_ = static_cast<int>(files.size()) - 1;
        }
        std::string statusLog;
        for (const auto& [relPath, name] : files)
        {
            auto texture = std::make_unique<Texture>();
            const std::wstring path = GetAssetPath(relPath.c_str());
            std::wstring err;
            if (!texture->Initialize(d3d_->GetDevice(), path.c_str(), &err))
            {
                // A 1x1 flat tangent normal keeps the index valid and the shader path exercised.
                texture->InitializeFlat(d3d_->GetDevice(), 128, 128, 255, 255);
                statusLog += "[FAIL -> flat] " + WideToUtf8(err) + " " + WideToUtf8(path) + "\n";
            }
            normalMaps_.push_back(std::move(texture));
            normalMapNames_.push_back(name);
        }
        normalMapStatus_ = statusLog; // empty = every map loaded
        OutputDebugStringA(("[NormalMap]\n" + statusLog).c_str());
    }

    // Contact foam masks (foam-masks): loaded like the normal maps, picked in the Water window.
    for (const FoamMapEntry& entry : kFoamMaps)
    {
        auto texture = std::make_unique<Texture>();
        std::wstring err;
        if (!texture->Initialize(d3d_->GetDevice(), GetAssetPath(entry.file).c_str(), &err))
        {
            // Mid gray foam, neutral breakup: the band still shows, with straight edges.
            texture->InitializeFlat(d3d_->GetDevice(), 128, 0, 128, 255);
            OutputDebugStringW((L"[FoamMap] FAIL -> flat gray: " + err + L"\n").c_str());
        }
        foamMaps_.push_back(std::move(texture));
    }

    model_ = std::make_unique<Model>();
    if (!model_->Initialize(d3d_->GetDevice(), GetAssetPath(L"models/32x32Plane.obj")))
    {
        return false;
    }

    // 1024x1024 quads out to +-400 units: ~0.012 spacing at the center, ~0.25 at 20 units, reaching
    // near the horizon. Together with the wave fade in the VS this keeps >= 5 vertices per wavelength.
    oceanGrid_ = std::make_unique<Model>();
    if (!oceanGrid_->InitializeGrid(d3d_->GetDevice(), 1024, 400.0f, 6.0f))
    {
        return false;
    }

    // water-lab: finite water patches (growth ~0 = even spacing) and the unit shapes of the test scenes.
    unitBox_ = std::make_unique<Model>();
    unitCylinder_ = std::make_unique<Model>();
    labWater_ = std::make_unique<Model>();
    frontWater_ = std::make_unique<Model>();
    if (!unitBox_->InitializeBox(d3d_->GetDevice())
        || !unitCylinder_->InitializeCylinder(d3d_->GetDevice(), 32)
        || !labWater_->InitializeGrid(d3d_->GetDevice(), 192, kLabWaterHalfSize, 0.001f)
        || !frontWater_->InitializeGrid(d3d_->GetDevice(), 256, kFrontWaterHalfSize, 0.001f))
    {
        return false;
    }
    labObjects_ = BuildWaterLab();
    frontObjects_ = BuildWaterfront();

    sceneShader_ = std::make_unique<SceneShader>();
    if (!sceneShader_->Initialize(d3d_->GetDevice()))
    {
        return false;
    }

    colorShader_ = std::make_unique<ColorShader>();
    if (!colorShader_->Initialize(d3d_->GetDevice()))
    {
        return false;
    }

    for (const Environment& env : kEnvironments)
    {
        auto cubemap = std::make_unique<CubemapTexture>();
        const std::wstring cubemapPath = GetAssetPath(env.file);
        std::wstring cubemapError;
        if (!cubemap->Initialize(d3d_->GetDevice(), cubemapPath.c_str(), !env.hdr, &cubemapError))
        {
            const std::wstring msg = cubemapPath + L"\n\n" + cubemapError;
            MessageBoxW(hwnd, msg.c_str(), L"WaterShader: cubemap load failed", MB_ICONERROR | MB_OK);
            return false;
        }
        environments_.push_back(std::move(cubemap));
    }

    skybox_ = std::make_unique<Skybox>();
    if (!skybox_->Initialize(d3d_->GetDevice()))
    {
        return false;
    }

    skyboxShader_ = std::make_unique<SkyboxShader>();
    if (!skyboxShader_->Initialize(d3d_->GetDevice()))
    {
        return false;
    }

    tonemapShader_ = std::make_unique<TonemapShader>();
    if (!tonemapShader_->Initialize(d3d_->GetDevice()))
    {
        return false;
    }

    if (!ImGui_ImplDX11_Init(d3d_->GetDevice(), d3d_->GetDeviceContext()))
    {
        return false;
    }
    imguiInitialized_ = true;

    // Timing is a debugging aid; a device without timestamp queries just shows "n/a".
    gpuTimer_.Initialize(d3d_->GetDevice());

    LoadPresets();
    ApplyPreset(0);

    // --resize-cycle: recreate every size-dependent resource once (half size, then back) before rendering,
    // so a capture set shows whether the recreated targets give the same frame.
    if (d3dSelfTest)
    {
        d3d_->TriggerHazardForTest();
    }
    if (resizeCycle)
    {
        Resize(std::max<unsigned int>(screenWidth_ / 2, 1u), std::max<unsigned int>(screenHeight_ / 2, 1u));
        Resize(static_cast<unsigned int>(screenWidth), static_cast<unsigned int>(screenHeight));
    }

    if (!captureLabelArg.empty())
    {
        StartCaptureSet(captureLabelArg, true);
    }

    return true;
}

void Graphics::ApplyPreset(int theme)
{
    if (theme < 0 || theme >= kPresetThemeCount)
    {
        return;
    }

    activeTheme_ = theme;
    presetOceanMesh_ = oceanMode_;
    const int debugMode = water_.debugMode;
    ApplyPresetValues(presets_[oceanMode_ ? 1 : 0][theme]);
    water_.debugMode = debugMode;
}

void Graphics::ApplyPresetValues(const ShaderPreset& preset)
{
    sunYawDeg_ = preset.sunYawDeg;
    sunElevationDeg_ = preset.sunElevationDeg;
    lightColor_ = preset.lightColor;
    lightIntensity_ = preset.lightIntensity;
    topSun_ = preset.topSun;
    ambientColor_ = preset.ambientColor;
    ambientIntensity_ = preset.ambientIntensity;
    environmentIndex_ = std::clamp(preset.environment, 0, kEnvironmentCount - 1);
    exposureEv_ = hasExposureOverride_ ? exposureOverrideEv_ : preset.exposureEv;
    const int lastNormalMap = static_cast<int>(normalMaps_.size()) - 1;
    normalMapA_ = std::clamp(normalOverrideA_ >= 0 ? normalOverrideA_ : preset.normalMapA, 0, lastNormalMap);
    normalMapB_ = std::clamp(normalOverrideB_ >= 0 ? normalOverrideB_ : preset.normalMapB, 0, lastNormalMap);
    rippleAlignA_ = rippleAlignOverride_ >= 0 ? rippleAlignOverride_ != 0 : preset.rippleAlignA != 0;
    rippleAlignB_ = rippleAlignOverride_ >= 0 ? rippleAlignOverride_ != 0 : preset.rippleAlignB != 0;
    water_ = preset.water;
    // A preset saved before the underwater keys existed has no light loss and no water color, so a test scene
    // would open as clear glass. Start it from Clear instead. Only the test scene program reads these values,
    // so the open water frames stay the same.
    const bool noUnderwater = water_.extinction.x == 0.0f && water_.extinction.y == 0.0f
        && water_.extinction.z == 0.0f && water_.scatterStrength == 0.0f;
    if (noUnderwater)
    {
        ApplyWaterType(water_, 0);
    }
    // Command line overrides for captures, in this order: water type, then single values on top.
    if (waterTypeOverride_ >= 0)
    {
        ApplyWaterType(water_, waterTypeOverride_);
    }
    if (hasExtinctionOverride_)
    {
        water_.extinction = extinctionOverride_;
    }
    if (rampByOverride_ >= 0)
    {
        water_.rampByPath = rampByOverride_;
    }
    if (hasScatterOverride_)
    {
        water_.scatterColor = {scatterOverride_.x, scatterOverride_.y, scatterOverride_.z};
        water_.scatterStrength = scatterOverride_.w;
    }
    if (foamStrengthOverride_ >= 0.0f)
    {
        water_.foamStrength = foamStrengthOverride_;
    }
    if (foamMapOverride_ >= 0)
    {
        water_.foamMap = foamMapOverride_;
    }
    if (foamSlopeOverride_ >= 0.0f)
    {
        water_.foamSlopeFilter = foamSlopeOverride_;
    }
    if (refractionOverride_ >= 0.0f)
    {
        water_.refractionStrength = refractionOverride_;
        water_.refractionDistortion = refractionOverride_;
    }
    if (refractionModeOverride_ >= 0)
    {
        water_.refractionMode = refractionModeOverride_;
    }
    waveMacro_ = preset.waveMacro;
}

Graphics::ShaderPreset Graphics::MakePresetFromCurrent() const
{
    ShaderPreset preset;
    preset.sunYawDeg = sunYawDeg_;
    preset.sunElevationDeg = sunElevationDeg_;
    preset.lightColor = lightColor_;
    preset.lightIntensity = lightIntensity_;
    preset.topSun = topSun_;
    preset.ambientColor = ambientColor_;
    preset.ambientIntensity = ambientIntensity_;
    preset.environment = environmentIndex_;
    preset.exposureEv = exposureEv_;
    preset.normalMapA = normalMapA_;
    preset.normalMapB = normalMapB_;
    preset.rippleAlignA = rippleAlignA_ ? 1 : 0;
    preset.rippleAlignB = rippleAlignB_ ? 1 : 0;
    preset.water = water_;
    preset.waveMacro = waveMacro_;
    return preset;
}

void Graphics::SaveCurrentPreset(int theme)
{
    if (theme < 0 || theme >= kPresetThemeCount)
    {
        return;
    }

    ShaderPreset& slot = presets_[oceanMode_ ? 1 : 0][theme];
    slot = MakePresetFromCurrent();
    slot.water.debugMode = 0;
    activeTheme_ = theme;
    presetOceanMesh_ = oceanMode_;
    SavePresets();
}

std::filesystem::path Graphics::CapturesRoot() const
{
    const std::filesystem::path root = captureRootOverride_.empty() ? GetDefaultCaptureRoot() : captureRootOverride_;
    return root / captureFeature_ / L"captures";
}

void Graphics::StartCaptureSet(const std::wstring& label, bool quitWhenDone, bool allowOverwrite)
{
    // Shots: the fixed shots, or the ones named by --capture-shots (which may also name camera slots).
    std::vector<int> shots;
    for (int shot = 0; shot < kCaptureShotCount; ++shot)
    {
        if (captureShotFilter_.empty() || ListContains(captureShotFilter_, kCaptureShots[shot].name))
        {
            shots.push_back(shot);
        }
    }
    for (int slot = 0; slot < kCameraSlotCount; ++slot)
    {
        if (cameraSlotUsed_[slot] && ListContains(captureShotFilter_, "slot" + std::to_string(slot + 1)))
        {
            shots.push_back(kCaptureShotCount + slot);
        }
    }

    captureQueue_.clear();
    for (int preset = 0; preset < kPresetThemeCount; ++preset)
    {
        if (!capturePresetFilter_.empty() && !ListContains(capturePresetFilter_, kPresetFileNames[preset]))
        {
            continue;
        }
        for (int shot : shots)
        {
            captureQueue_.push_back({preset, shot});
        }
    }

    // <capture root>/<feature>/captures/<label>
    const std::filesystem::path capturesRoot = CapturesRoot();
    captureDir_ = capturesRoot / label;
    std::error_code ec;
    for (int suffix = 2; !allowOverwrite && std::filesystem::exists(captureDir_, ec); ++suffix)
    {
        captureDir_ = capturesRoot / (label + L"_" + std::to_wstring(suffix));
    }
    std::filesystem::create_directories(captureDir_, ec);

    captureRestore_ = {MakePresetFromCurrent(), MakeViewFromCurrent(), elapsedTime_};
    quitAfterCapture_ = quitWhenDone;
    if (benchMode_)
    {
        // Uncapped frames: with VSync on every frame would take the display interval.
        benchRestoreVsync_ = d3d_->GetVSync();
        d3d_->SetVSync(false);
        benchJobActive_ = false;
        benchLastResolved_ = gpuTimer_.GetResolvedCount();
        std::string adapterName = "unknown";
        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        DXGI_ADAPTER_DESC adapterDesc = {};
        if (SUCCEEDED(d3d_->GetDevice()->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) &&
            SUCCEEDED(dxgiDevice->GetAdapter(&adapter)) && SUCCEEDED(adapter->GetDesc(&adapterDesc)))
        {
            adapterName = WideToUtf8(adapterDesc.Description);
        }
        benchCsv_ = "# GPU: " + adapterName + ", " + std::to_string(screenWidth_) + "x" + std::to_string(screenHeight_)
#if defined(_DEBUG)
            + ", Debug build (not for timing)"
#endif
            + ", warm-up " + std::to_string(kBenchWarmupFrames) + " frames, ms per frame\n"
            "# sections: opaque = clear + sky + test scene objects, water = scene color copy + water draw, post = tonemap + UI\n"
            "preset,shot,samples,gpu_median,gpu_mean,gpu_min,opaque_median,water_median,water_mean,post_median,cpu_median\n";
    }
    captureStatus_ = "Capturing -> " + WideToUtf8(captureDir_.wstring());
}

bool Graphics::BeginCaptureFrame()
{
    // --bench: a job keeps its preset and shot for all its frames; only the clock is held.
    if (benchMode_ && benchJobActive_)
    {
        elapsedTime_ = captureTime_;
        return true;
    }
    if (captureQueue_.empty())
    {
        return false;
    }

    const CaptureJob job = captureQueue_.front();
    captureQueue_.erase(captureQueue_.begin());

    // A fixed shot, or a saved camera slot (index past the fixed shots).
    const bool isSlot = job.shot >= kCaptureShotCount;
    const int slot = job.shot - kCaptureShotCount;
    const CameraView view = isSlot
        ? cameraSlots_[slot]
        : CameraView{kCaptureShots[job.shot].position, kCaptureShots[job.shot].rotation,
                     kCaptureShots[job.shot].fovDeg, kCaptureShots[job.shot].ocean, {0.0f, 0.0f, 0.0f}, kCaptureShots[job.shot].scene};
    const std::string shotName = isSlot ? "slot" + std::to_string(slot + 1) : std::string(kCaptureShots[job.shot].name);

    // Each shot uses the preset version for its own mesh (bench plane / ocean grid).
    ApplyPresetValues(presets_[view.ocean ? 1 : 0][job.preset]);
    water_.debugMode = captureDebugMode_;

    // Fixed shots also reset the bench plane rotation, so mouse-drag state never leaks into captures.
    ApplyView(view);
    presetOceanMesh_ = view.ocean;

    // Fixed time so before/after frames show the same wave phase.
    elapsedTime_ = captureTime_;

    const std::string fileName = std::string(kPresetFileNames[job.preset]) + "_" + shotName + (capturePng_ ? ".png" : ".jpg");
    pendingCapturePath_ = captureDir_ / fileName;
    if (benchMode_)
    {
        benchJobActive_ = true;
        benchFrame_ = 0;
        benchSamples_.clear();
        benchJobName_ = std::string(kPresetFileNames[job.preset]) + "," + shotName;
    }
    return true;
}

// --bench: one capture job = kBenchWarmupFrames untimed frames, then benchSampleCount_ resolved GPU timings
// (VSync off, fixed clock, UI hidden). One CSV row per job: median / mean / min of the frame and of each section.
void Graphics::EndBenchFrame()
{
    ++benchFrame_;
    const unsigned long long resolved = gpuTimer_.GetResolvedCount();
    if (benchFrame_ > kBenchWarmupFrames && resolved != benchLastResolved_)
    {
        const auto& sections = gpuTimer_.GetLastSectionsMs();
        // cpuFrameMs_ still holds the previous frame here (it is measured after this call).
        benchSamples_.push_back({gpuTimer_.GetLastMs(), sections[0], sections[1], sections[2], cpuFrameMs_});
    }
    benchLastResolved_ = resolved;
    if (static_cast<int>(benchSamples_.size()) < benchSampleCount_)
    {
        return;
    }

    const auto column = [this](int index, float* median, float* mean, float* minimum)
    {
        std::vector<float> values;
        for (const auto& sample : benchSamples_)
        {
            values.push_back(sample[index]);
        }
        std::sort(values.begin(), values.end());
        *median = values[values.size() / 2];
        *minimum = values.front();
        double sum = 0.0;
        for (float value : values)
        {
            sum += value;
        }
        *mean = static_cast<float>(sum / values.size());
    };
    char row[256];
    float median[5], mean[5], minimum[5];
    for (int i = 0; i < 5; ++i)
    {
        column(i, &median[i], &mean[i], &minimum[i]);
    }
    std::snprintf(row, sizeof(row), "%s,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
        benchJobName_.c_str(), static_cast<int>(benchSamples_.size()),
        median[0], mean[0], minimum[0], median[1], median[2], mean[2], median[3], median[4]);
    benchCsv_ += row;
    benchJobActive_ = false;

    if (!captureQueue_.empty())
    {
        return;
    }
    std::ofstream file(captureDir_ / L"timings.csv");
    file << benchCsv_;
    d3d_->SetVSync(benchRestoreVsync_);
    FinishCaptureSet();
}

void Graphics::EndCaptureFrame()
{
    if (benchMode_)
    {
        EndBenchFrame();
        return;
    }

    // Delete first, then write a new file. Overwriting a batch of existing JPEGs in place from a
    // freshly built exe gets the whole process tree frozen mid-write (ransomware-style heuristic,
    // see docs/features/bench-tools/TROUBLESHOOTING.md); delete + create does not.
    std::error_code removeError;
    std::filesystem::remove(pendingCapturePath_, removeError);

    // JPEG keeps each step's capture set ~2MB in git instead of ~11MB as PNG (--capture-format png for finals).
    const auto backBuffer = d3d_->GetBackBuffer();
    const HRESULT hr = capturePng_
        ? DirectX::SaveWICTextureToFile(
            d3d_->GetDeviceContext(),
            backBuffer.Get(),
            GUID_ContainerFormatPng,
            pendingCapturePath_.wstring().c_str(),
            nullptr,
            nullptr,
            // The back buffer is UNORM but holds sRGB-encoded values (the tonemap pass encodes them).
            // Without this ScreenGrab tags the PNG gAMA 1.0 (linear) and viewers show it washed out.
            true)
        : DirectX::SaveWICTextureToFile(
        d3d_->GetDeviceContext(),
        backBuffer.Get(),
        GUID_ContainerFormatJpeg,
        pendingCapturePath_.wstring().c_str(),
        nullptr,
        [](IPropertyBag2* props)
        {
            PROPBAG2 option = {};
            option.pstrName = const_cast<wchar_t*>(L"ImageQuality");
            VARIANT value;
            VariantInit(&value);
            value.vt = VT_R4;
            value.fltVal = 0.95f;
            props->Write(1, &option, &value);
        });
    OutputDebugStringW(((SUCCEEDED(hr) ? L"[Capture] saved " : L"[Capture] FAILED ") + pendingCapturePath_.wstring() + L"\n").c_str());

    if (!captureQueue_.empty())
    {
        return;
    }
    FinishCaptureSet();
}

void Graphics::FinishCaptureSet()
{
    ApplyPresetValues(captureRestore_.preset);
    ApplyView(captureRestore_.view);
    presetOceanMesh_ = oceanMode_; // the restored values (unsaved tweaks included) belong to the restored mesh
    elapsedTime_ = captureRestore_.elapsedTime;
    captureStatus_ = "Saved -> " + WideToUtf8(captureDir_.wstring());
    captureFinishedQuit_ = quitAfterCapture_;
    WriteD3dMessages(captureDir_ / L"d3d_messages.txt"); // drains first: this frame and the start-up are included
}

void Graphics::DrainD3dMessages()
{
    // Debug layer (Debug builds): keep every warning / error, the capture set writes them out.
    for (const std::string& message : d3d_->TakeDebugMessages(&d3dMessageTotal_))
    {
        ++d3dMessages_[message];
    }
}

void Graphics::WriteD3dMessages(const std::filesystem::path& path)
{
    DrainD3dMessages();
    std::ofstream file(path);
    if (!file)
    {
        return;
    }
    file << "D3D11 debug layer: " << (d3d_->HasDebugLayer() ? "on (Debug build)" : "off - nothing was checked") << '\n';
    file << "Messages since start (info included): " << d3dMessageTotal_ << '\n';
    file << "Warnings and errors since start: " << d3dMessages_.size() << " different\n";
    for (const auto& [message, count] : d3dMessages_)
    {
        file << count << "x  " << message << '\n';
    }
}

Graphics::CameraView Graphics::MakeViewFromCurrent() const
{
    return {cameraPosition_, cameraRotation_, cameraFovDeg_, oceanMode_, modelRotation_, scene_};
}

// Exact match on purpose: the Top View Sun belongs to the fixed shot, so any camera move leaves it.
// FOV is not compared (zooming keeps the straight-down framing).
bool Graphics::IsTopShotView() const
{
    const CaptureShot& top = kCaptureShots[kTopShotIndex];
    return oceanMode_ == top.ocean && scene_ == top.scene
        && cameraPosition_.x == top.position.x && cameraPosition_.y == top.position.y && cameraPosition_.z == top.position.z
        && cameraRotation_.x == top.rotation.x && cameraRotation_.y == top.rotation.y && cameraRotation_.z == top.rotation.z;
}

void Graphics::ApplyView(const CameraView& view)
{
    cameraPosition_ = view.position;
    cameraRotation_ = view.rotation;
    cameraFovDeg_ = view.fovDeg;
    oceanMode_ = view.ocean;
    modelRotation_ = view.modelRotation;
    scene_ = view.ocean ? view.scene : NoScene; // the test scenes sit on the ocean presets
    camera_->SetPosition(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z);
    camera_->SetRotation(cameraRotation_.x, cameraRotation_.y, cameraRotation_.z);
}

void Graphics::ApplyCameraShot(int shotIndex)
{
    const CaptureShot& shot = kCaptureShots[shotIndex];
    ApplyView({shot.position, shot.rotation, shot.fovDeg, shot.ocean, {0.0f, 0.0f, 0.0f}, shot.scene});
}

std::filesystem::path Graphics::CameraFilePath() const
{
    return cameraFileOverride_.empty() ? std::filesystem::path(GetAssetPath(L"camera_presets.txt")) : cameraFileOverride_;
}

// assets/camera_presets.txt: one line per slot
//   used  pos.x pos.y pos.z  pitch yaw roll  fov  ocean  model.x model.y model.z  scene
// v2 added the test scene at the end; v1 lines (no scene) still load.
void Graphics::LoadCameraSlots()
{
    std::ifstream file(CameraFilePath());
    std::string header;
    int version = 0;
    if (!file || !(file >> header >> version) || header != "WaterShaderCameras" || (version != 1 && version != 2))
    {
        return;
    }

    for (int i = 0; i < kCameraSlotCount; ++i)
    {
        CameraView& v = cameraSlots_[i];
        int used = 0;
        int ocean = 0;
        file >> used
             >> v.position.x >> v.position.y >> v.position.z
             >> v.rotation.x >> v.rotation.y >> v.rotation.z
             >> v.fovDeg >> ocean
             >> v.modelRotation.x >> v.modelRotation.y >> v.modelRotation.z;
        v.scene = NoScene;
        if (version >= 2)
        {
            file >> v.scene;
        }
        if (!file)
        {
            break;
        }
        v.ocean = ocean != 0;
        cameraSlotUsed_[i] = used != 0;
    }
}

void Graphics::SaveCameraSlots() const
{
    std::ofstream file(CameraFilePath());
    if (!file)
    {
        return;
    }

    file << "WaterShaderCameras 2\n";
    for (int i = 0; i < kCameraSlotCount; ++i)
    {
        const CameraView& v = cameraSlots_[i];
        file << (cameraSlotUsed_[i] ? 1 : 0) << ' '
             << v.position.x << ' ' << v.position.y << ' ' << v.position.z << ' '
             << v.rotation.x << ' ' << v.rotation.y << ' ' << v.rotation.z << ' '
             << v.fovDeg << ' ' << (v.ocean ? 1 : 0) << ' '
             << v.modelRotation.x << ' ' << v.modelRotation.y << ' ' << v.modelRotation.z << ' '
             << v.scene << '\n';
    }
}

std::filesystem::path Graphics::PresetFilePath() const
{
    return presetFileOverride_.empty() ? std::filesystem::path(GetAssetPath(L"shader_presets.txt")) : presetFileOverride_;
}

void Graphics::LoadPresets()
{
    PresetSet& defaults = presets_[0];
    defaults[0] = ShaderPreset{};

    defaults[1] = ShaderPreset{};
    defaults[1].sunElevationDeg = 5.0f;
    defaults[1].lightColor = {1.0f, 0.58f, 0.35f};
    defaults[1].lightIntensity = 1.2f;
    defaults[1].ambientColor = {0.18f, 0.10f, 0.16f};
    defaults[1].ambientIntensity = 0.45f;
    defaults[1].water.facingColor = {0.58f, 0.48f, 0.78f, 1.0f};
    defaults[1].water.grazingColor = {0.08f, 0.03f, 0.10f, 1.0f};
    defaults[1].water.reflectionStrength = 0.75f;
    defaults[1].water.fresnelPower = 4.0f;
    defaults[1].water.specularStrength = 0.35f;
    defaults[1].water.specularSharpness = 72.0f;

    defaults[2] = ShaderPreset{};
    defaults[2].sunElevationDeg = 32.0f;
    defaults[2].lightColor = {0.92f, 1.0f, 0.94f};
    defaults[2].lightIntensity = 1.35f;
    defaults[2].ambientColor = {0.08f, 0.22f, 0.24f};
    defaults[2].ambientIntensity = 0.55f;
    defaults[2].water.facingColor = {0.30f, 0.92f, 1.0f, 1.0f};
    defaults[2].water.grazingColor = {0.00f, 0.20f, 0.34f, 1.0f};
    defaults[2].water.reflectionStrength = 0.65f;
    defaults[2].water.fresnelPower = 3.5f;
    defaults[2].water.specularStrength = 0.30f;
    defaults[2].water.specularSharpness = 96.0f;

    presets_[1] = defaults; // the ocean grid starts from the same code defaults

    const std::filesystem::path presetPath = PresetFilePath();
    std::ifstream file(presetPath);
    if (!file)
    {
        return;
    }

    // v2: second field is sun elevation (> 0 above the horizon); v1 stored an inverted pitch.
    // v3: 4 Gerstner waves (dir.x dir.y amplitude wavelength speed steepness). Older files are ignored.
    // v4: the v3 line per mesh x theme, led by the mesh name.
    std::string line;
    std::getline(file, line);
    std::istringstream headerStream(line);
    std::string header;
    int version = 0;
    headerStream >> header >> version;
    if (header != "WaterShaderPresets" || (version != 3 && version != kPresetFileVersion))
    {
        return;
    }

    const auto readPreset = [](std::istringstream& in, ShaderPreset& preset)
    {
        in
            >> preset.sunYawDeg
            >> preset.sunElevationDeg
            >> preset.lightColor.x >> preset.lightColor.y >> preset.lightColor.z
            >> preset.lightIntensity
            >> preset.ambientColor.x >> preset.ambientColor.y >> preset.ambientColor.z
            >> preset.ambientIntensity
            >> preset.water.facingColor.x >> preset.water.facingColor.y >> preset.water.facingColor.z
            >> preset.water.grazingColor.x >> preset.water.grazingColor.y >> preset.water.grazingColor.z
            >> preset.water.reflectionStrength
            >> preset.water.fresnelPower
            >> preset.water.normalScale
            >> preset.water.specularStrength
            >> preset.water.specularSharpness
            >> preset.water.normalScroll1.x >> preset.water.normalScroll1.y
            >> preset.water.normalScroll2.x >> preset.water.normalScroll2.y;

        for (auto& wave : preset.water.waves)
        {
            in
                >> wave.direction.x >> wave.direction.y
                >> wave.amplitude
                >> wave.wavelength
                >> wave.speed
                >> wave.steepness;
        }

        std::string key;
        float value = 0.0f;
        bool hasWaveMacro = false;
        while (in >> key >> value)
        {
            ForEachExtraField(preset, [&](const char* name, auto& field)
            {
                if (key == name)
                {
                    field = static_cast<std::decay_t<decltype(field)>>(value);
                }
            });
            hasWaveMacro = hasWaveMacro || key.rfind("wave", 0) == 0;
        }
        // Saved before the Simple wave controls: guess them from the waves. The waves themselves
        // stay untouched, so the preset renders as before and shows as Custom if the guess is off.
        if (!hasWaveMacro)
        {
            preset.waveMacro = EstimateWaveMacro(preset.water.waves);
        }
    };

    if (version == 3)
    {
        // One preset per theme, used for both meshes until one of them is saved on its own.
        for (int theme = 0; theme < kPresetThemeCount && std::getline(file, line); ++theme)
        {
            std::istringstream in(line);
            readPreset(in, presets_[0][theme]);
            presets_[1][theme] = presets_[0][theme];
        }
        return;
    }

    // Themes in order within each mesh; a line for an unknown mesh or a fourth theme is skipped.
    int nextTheme[2] = {0, 0};
    while (std::getline(file, line))
    {
        std::istringstream in(line);
        std::string meshName;
        in >> meshName;
        const int mesh = meshName == kPresetMeshNames[0] ? 0 : meshName == kPresetMeshNames[1] ? 1 : -1;
        if (mesh < 0 || nextTheme[mesh] >= kPresetThemeCount)
        {
            continue;
        }
        readPreset(in, presets_[mesh][nextTheme[mesh]++]);
    }
}

void Graphics::SavePresets() const
{
    const std::filesystem::path presetPath = PresetFilePath();
    std::filesystem::create_directories(presetPath.parent_path());

    std::ofstream file(presetPath);
    if (!file)
    {
        return;
    }

    file << "WaterShaderPresets " << kPresetFileVersion << '\n';
    for (int mesh = 0; mesh < static_cast<int>(presets_.size()); ++mesh)
    {
        for (const ShaderPreset& preset : presets_[mesh])
        {
            file
                << kPresetMeshNames[mesh] << ' '
                << preset.sunYawDeg << ' '
                << preset.sunElevationDeg << ' '
                << preset.lightColor.x << ' ' << preset.lightColor.y << ' ' << preset.lightColor.z << ' '
                << preset.lightIntensity << ' '
                << preset.ambientColor.x << ' ' << preset.ambientColor.y << ' ' << preset.ambientColor.z << ' '
                << preset.ambientIntensity << ' '
                << preset.water.facingColor.x << ' ' << preset.water.facingColor.y << ' ' << preset.water.facingColor.z << ' '
                << preset.water.grazingColor.x << ' ' << preset.water.grazingColor.y << ' ' << preset.water.grazingColor.z << ' '
                << preset.water.reflectionStrength << ' '
                << preset.water.fresnelPower << ' '
                << preset.water.normalScale << ' '
                << preset.water.specularStrength << ' '
                << preset.water.specularSharpness << ' '
                << preset.water.normalScroll1.x << ' ' << preset.water.normalScroll1.y << ' '
                << preset.water.normalScroll2.x << ' ' << preset.water.normalScroll2.y;

            for (const auto& wave : preset.water.waves)
            {
                file
                    << ' ' << wave.direction.x << ' ' << wave.direction.y
                    << ' ' << wave.amplitude
                    << ' ' << wave.wavelength
                    << ' ' << wave.speed
                    << ' ' << wave.steepness;
            }

            ForEachExtraField(preset, [&](const char* name, const auto& field)
            {
                file << ' ' << name << ' ' << field;
            });

            file << '\n';
        }
    }
}

void Graphics::Shutdown()
{
    gpuTimer_.Shutdown();

    if (imguiInitialized_)
    {
        ImGui_ImplDX11_Shutdown();
        imguiInitialized_ = false;
    }

    if (tonemapShader_)
    {
        tonemapShader_->Shutdown();
        tonemapShader_.reset();
    }

    if (skyboxShader_)
    {
        skyboxShader_->Shutdown();
        skyboxShader_.reset();
    }

    if (skybox_)
    {
        skybox_->Shutdown();
        skybox_.reset();
    }

    for (auto& cubemap : environments_)
    {
        cubemap->Shutdown();
    }
    environments_.clear();

    if (colorShader_)
    {
        colorShader_->Shutdown();
        colorShader_.reset();
    }

    if (sceneShader_)
    {
        sceneShader_->Shutdown();
        sceneShader_.reset();
    }

    for (auto* mesh : { &unitBox_, &unitCylinder_, &labWater_, &frontWater_ })
    {
        if (*mesh)
        {
            (*mesh)->Shutdown();
            mesh->reset();
        }
    }

    if (model_)
    {
        model_->Shutdown();
        model_.reset();
    }

    if (oceanGrid_)
    {
        oceanGrid_->Shutdown();
        oceanGrid_.reset();
    }

    for (auto& normalMap : normalMaps_)
    {
        normalMap->Shutdown();
    }
    normalMaps_.clear();
    light_.reset();
    camera_.reset();

    if (d3d_)
    {
        d3d_->Shutdown();
        d3d_.reset();
    }
}

bool Graphics::Frame(float deltaTime, const Input& input)
{
    elapsedTime_ += deltaTime;
    // Ocean Grid checkbox, a camera slot or a fixed shot changed the mesh: switch to that mesh's version
    // of the current preset (unsaved tweaks are dropped, like pressing the preset button).
    if (oceanMode_ != presetOceanMesh_)
    {
        ApplyPreset(activeTheme_);
    }
    UpdateWindowToggles(input);
    UpdateMouseDrag(); // before UpdateCamera, which pushes cameraRotation_ to the camera
    UpdateCamera(deltaTime, input);
    return Render(deltaTime);
}

void Graphics::UpdateWindowToggles(const Input& input)
{
    // Toggle on the press, not while held. Digits typed into a text field (capture label) are text.
    const bool typing = imguiInitialized_ && ImGui::GetIO().WantTextInput;
    bool* windows[] = { &showViewWindow_, &showLightWindow_, &showWaterWindow_, &showSurfaceWindow_ };
    for (int i = 0; i < 4; ++i)
    {
        const bool down = input.IsKeyDown('1' + i) || input.IsKeyDown(VK_NUMPAD1 + i);
        if (down && !toggleKeyWasDown_[i] && !typing)
        {
            *windows[i] = !*windows[i];
        }
        toggleKeyWasDown_[i] = down;
    }

    // X: in the test scenes, swap the active debug view with the parked one (answer key <-> reconstruction).
    const bool swapDown = input.IsKeyDown('X');
    if (swapDown && !swapKeyWasDown_ && !typing && scene_ != NoScene)
    {
        if (sceneDebug_ != 0 && parkedWaterDebug_ != 0)
        {
            water_.debugMode = parkedWaterDebug_;
            SwapDebugViews(water_.debugMode, parkedWaterDebug_, sceneDebug_, parkedSceneDebug_);
        }
        else if (water_.debugMode != 0 && parkedSceneDebug_ != 0)
        {
            sceneDebug_ = parkedSceneDebug_;
            SwapDebugViews(sceneDebug_, parkedSceneDebug_, water_.debugMode, parkedWaterDebug_);
        }
    }
    swapKeyWasDown_ = swapDown;
}

void Graphics::UpdateMouseDrag()
{
    // Viewport drags (ImGui's Win32 backend already tracks the mouse, so no extra input plumbing):
    //   bench plane: left-drag = rotate the plane, right-drag = look around
    //   ocean grid : left- or right-drag = look around (the grid itself stays level)
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
    {
        return;
    }

    constexpr float kDegreesPerPixel = 0.3f;
    const bool leftDrag = io.MouseDown[0];
    const bool rightDrag = io.MouseDown[1];

    if (!oceanMode_ && leftDrag)
    {
        modelRotation_.y -= io.MouseDelta.x * kDegreesPerPixel;
        modelRotation_.x = std::clamp(modelRotation_.x + io.MouseDelta.y * kDegreesPerPixel, -89.0f, 89.0f);
        modelRotation_.y = std::fmod(modelRotation_.y, 360.0f);
    }
    else if (rightDrag || (oceanMode_ && leftDrag))
    {
        // Mouse-look: drag right = turn right, drag down = look down (pitch > 0 looks down).
        cameraRotation_.y = std::fmod(cameraRotation_.y + io.MouseDelta.x * kDegreesPerPixel, 360.0f);
        cameraRotation_.x = std::clamp(cameraRotation_.x + io.MouseDelta.y * kDegreesPerPixel, -89.0f, 89.0f);
    }
}

void Graphics::UpdateCamera(float deltaTime, const Input& input)
{
    using namespace DirectX;

    const float turn = cameraTurnSpeed_ * deltaTime;
    if (input.IsKeyDown(VK_LEFT))  { cameraRotation_.y -= turn; }
    if (input.IsKeyDown(VK_RIGHT)) { cameraRotation_.y += turn; }
    if (input.IsKeyDown(VK_UP))    { cameraRotation_.x -= turn; }
    if (input.IsKeyDown(VK_DOWN))  { cameraRotation_.x += turn; }

    const float yawRad = XMConvertToRadians(cameraRotation_.y);
    const float pitchRad = XMConvertToRadians(cameraRotation_.x);
    const float cosPitch = cosf(pitchRad);
    const XMVECTOR forward = XMVectorSet(sinf(yawRad) * cosPitch, -sinf(pitchRad), cosf(yawRad) * cosPitch, 0.0f);
    const XMVECTOR right   = XMVectorSet(cosf(yawRad), 0.0f, -sinf(yawRad), 0.0f);
    const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMVECTOR position = XMLoadFloat3(&cameraPosition_);
    const float move = cameraMoveSpeed_ * deltaTime;
    if (input.IsKeyDown('W')) { position = XMVectorAdd(position, XMVectorScale(forward, move)); }
    if (input.IsKeyDown('S')) { position = XMVectorSubtract(position, XMVectorScale(forward, move)); }
    if (input.IsKeyDown('D')) { position = XMVectorAdd(position, XMVectorScale(right, move)); }
    if (input.IsKeyDown('A')) { position = XMVectorSubtract(position, XMVectorScale(right, move)); }
    if (input.IsKeyDown('E')) { position = XMVectorAdd(position, XMVectorScale(worldUp, move)); }
    if (input.IsKeyDown('Q')) { position = XMVectorSubtract(position, XMVectorScale(worldUp, move)); }
    XMStoreFloat3(&cameraPosition_, position);

    camera_->SetPosition(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z);
    camera_->SetRotation(cameraRotation_.x, cameraRotation_.y, cameraRotation_.z);
}

void Graphics::Resize(unsigned int width, unsigned int height)
{
    // --render-size keeps its back buffer; the window only shows it scaled.
    if (fixedRenderSize_)
    {
        return;
    }

    screenWidth_ = width;
    screenHeight_ = std::max<unsigned int>(height, 1u);

    if (d3d_)
    {
        d3d_->Resize(width, screenHeight_);
    }
}

// Water Lab: every object stands on the floor 8 m down and ends at a known height, water at y = 0.
std::vector<Graphics::SceneObject> Graphics::BuildWaterLab()
{
    using P = SceneShader::Pattern;
    constexpr float kFloorY = -8.0f;
    std::vector<SceneObject> objects;
    const auto box = [&](float x, float z, float sizeX, float sizeZ, float bottomY, float topY,
                         DirectX::XMFLOAT3 color, int pattern)
    {
        SceneObject o;
        o.center = {x, 0.5f * (bottomY + topY), z};
        o.size = {sizeX, topY - bottomY, sizeZ};
        o.material = {color, pattern};
        objects.push_back(o);
    };
    // One albedo for every test surface: later steps compare what depth alone does to the same color.
    const DirectX::XMFLOAT3 testColor = {0.78f, 0.75f, 0.68f};

    // Floor under the whole 24 x 24 m water patch.
    box(0.0f, 0.0f, 24.0f, 24.0f, kFloorY - 1.0f, kFloorY, {0.62f, 0.62f, 0.60f}, P::Checker);

    // Depth pads, 3 x 3 m, tops 0.2 / 1 / 3 / 6 m under the water.
    const float padTops[] = { -0.2f, -1.0f, -3.0f, -6.0f };
    for (int i = 0; i < 4; ++i)
    {
        box(-6.0f + 4.0f * i, 5.0f, 3.0f, 3.0f, kFloorY, padTops[i], testColor, P::Ruler);
    }

    // Stairs down along +x: 16 steps, 0.25 m rise, 0.5 m run, 4 m wide, from +1.125 to -2.625.
    // The 0.125 offset keeps every step top off y = 0 (it would z-fight with the flat water).
    for (int i = 0; i < 16; ++i)
    {
        box(-9.75f + 0.5f * i, -3.5f, 0.5f, 4.0f, kFloorY, 1.125f - 0.25f * i, testColor, P::Checker);
    }

    // Pillar through the surface: 0.8 m wide, from the floor to 3 m above the water.
    {
        SceneObject pillar;
        pillar.cylinder = true;
        pillar.center = {3.0f, 0.5f * (kFloorY + 3.0f), -3.5f};
        pillar.size = {0.8f, 3.0f - kFloorY, 0.8f};
        pillar.material = {{0.85f, 0.85f, 0.85f}, P::Ruler};
        objects.push_back(pillar);
    }

    // Near-surface pads: 5 cm under and 5 cm over the water.
    box(6.5f, -4.5f, 1.2f, 1.2f, kFloorY, -0.05f, testColor, P::Ruler);
    box(8.5f, -4.5f, 1.2f, 1.2f, kFloorY, 0.05f, testColor, P::Ruler);

    // Pebble bed 0.4 m down: fine detail for refraction and caustics later.
    box(0.0f, 10.0f, 10.0f, 3.0f, kFloorY, -0.4f, {0.75f, 0.70f, 0.62f}, P::Pebbles);
    return objects;
}

// Graybox of the target scene: a flooded track (ballast bank, sleepers, rails a few cm under the water),
// a platform with stairs into the water, and catenary poles. Track along +z, seabed 2.5 m down.
std::vector<Graphics::SceneObject> Graphics::BuildWaterfront()
{
    using P = SceneShader::Pattern;
    constexpr float kSeabedY = -2.5f;
    std::vector<SceneObject> objects;
    const auto box = [&](float x, float z, float sizeX, float sizeZ, float bottomY, float topY,
                         DirectX::XMFLOAT3 color, int pattern)
    {
        SceneObject o;
        o.center = {x, 0.5f * (bottomY + topY), z};
        o.size = {sizeX, topY - bottomY, sizeZ};
        o.material = {color, pattern};
        objects.push_back(o);
    };
    const auto pole = [&](float x, float z, float armCenterX, float armLength)
    {
        SceneObject o;
        o.cylinder = true;
        o.center = {x, 0.5f * (kSeabedY + 6.0f), z};
        o.size = {0.3f, 6.0f - kSeabedY, 0.3f};
        o.material = {{0.62f, 0.62f, 0.60f}, P::Solid};
        objects.push_back(o);
        box(armCenterX, z, armLength, 0.12f, 5.54f, 5.66f, {0.45f, 0.45f, 0.44f}, P::Solid);
    };

    // Seabed under the whole 80 x 80 m water patch.
    box(0.0f, 0.0f, 80.0f, 80.0f, kSeabedY - 1.0f, kSeabedY, {0.52f, 0.52f, 0.45f}, P::Pebbles);

    // Track: ballast bank 0.35 m under, sleepers 0.23 m under, rail heads 8 cm under the water.
    box(0.0f, 0.0f, 4.4f, 80.0f, kSeabedY, -0.35f, {0.64f, 0.62f, 0.58f}, P::Pebbles);
    for (float z = -38.0f; z <= 38.0f; z += 0.65f)
    {
        box(0.0f, z, 2.6f, 0.25f, -0.35f, -0.23f, {0.36f, 0.31f, 0.27f}, P::Solid);
    }
    for (const float x : { -0.75f, 0.75f })
    {
        box(x, 0.0f, 0.07f, 80.0f, -0.23f, -0.08f, {0.48f, 0.45f, 0.42f}, P::Solid);
    }

    // Platform 0.8 m above the water, right of the track, and stairs from its near end down along -z:
    // 11 steps, 0.2 m rise, 0.35 m run, tops +0.7 .. -1.3 (none at y = 0).
    box(6.5f, 15.0f, 6.0f, 30.0f, kSeabedY, 0.8f, {0.72f, 0.70f, 0.66f}, P::Checker);
    for (int i = 0; i < 11; ++i)
    {
        box(6.5f, -0.35f * i - 0.175f, 4.0f, 0.35f, kSeabedY, 0.7f - 0.2f * i, {0.72f, 0.70f, 0.66f}, P::Checker);
    }

    // Catenary poles standing in the water on both sides, arms reaching over the track.
    for (const float z : { 4.0f, 14.0f, 24.0f })
    {
        pole(-3.2f, z, -1.1f, 4.4f);
    }
    for (const float z : { 9.0f, 19.0f })
    {
        pole(2.8f, z, 1.0f, 3.6f);
    }
    return objects;
}

// Scene Debug and the water Debug Mode both paint the whole frame (and Scene Debug hides the water), so in the
// test scenes only one runs at a time. Picking a view in one parks the other; setting the first back to Render
// brings the parked view back. "changed" is the combo the user just moved.
void Graphics::SwapDebugViews(int& changed, int& changedParked, int& other, int& otherParked)
{
    if (changed != 0)
    {
        if (other != 0)
        {
            otherParked = other;
            other = 0;
        }
        changedParked = 0; // this one is in use now; nothing of it is waiting
    }
    else if (otherParked != 0)
    {
        other = otherParked;
        otherParked = 0;
    }
}

// Test scene views that write meters as colors for tools/compare_depth.ps1 (DepthBands.hlsli, EncodeMeters).
bool Graphics::IsNumericView() const
{
    return scene_ != NoScene && (sceneDebug_ == 2 || water_.debugMode == 12 || water_.debugMode == 13);
}

void Graphics::RenderTestScene(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection,
                               const DirectX::XMFLOAT4& lightDir, const DirectX::XMFLOAT4& lightColor,
                               const DirectX::XMFLOAT4& ambientColor)
{
    using namespace DirectX;
    ID3D11DeviceContext* context = d3d_->GetDeviceContext();
    // Closed shapes, drawn double-sided so the generated winding never has to match the cull mode.
    d3d_->SetRasterizerDoubleSided();
    // With the water's numeric views, whatever the water does not cover is drawn as "not a measurement" (scene
    // debug 3), so neither a lit pixel nor the answer key can pass for a reconstructed value.
    const bool waterNumeric = scene_ != NoScene && (water_.debugMode == 12 || water_.debugMode == 13);
    const int sceneDebugMode = waterNumeric ? 3 : sceneDebug_;
    const bool writeNormal = UsesStoredNormal();
    // wave-occlusion: the answer key measures water depth from the same wave surface the water draws.
    SceneShader::WaterSurface surface{};
    for (int i = 0; i < ColorShader::kWaveCount; ++i)
    {
        const ColorShader::Wave& wave = water_.waves[i];
        const float amplitude = labWaves_ ? wave.amplitude : 0.0f;
        surface.waves[i * 2] = DirectX::XMFLOAT4(wave.direction.x, wave.direction.y, amplitude, wave.wavelength);
        surface.waves[i * 2 + 1] = DirectX::XMFLOAT4(wave.speed, wave.steepness, 0.0f, 0.0f);
    }
    surface.camera = DirectX::XMFLOAT4(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z, 0.0f);
    const DirectX::XMFLOAT4 area = SceneWaterArea();
    surface.timeArea = DirectX::XMFLOAT4(elapsedTime_, area.x, area.y, 0.0f);
    sceneShader_->SetWaterSurface(surface);
    sceneShader_->SetShadow(shadowFrame_, d3d_->GetShadowSRV(), d3d_->GetShadowSampler());
    sceneShader_->Begin(context, view, projection, lightDir, lightColor, ambientColor, sceneDebugMode, writeNormal);
    // normal-gbuffer: the objects also write their world normal to a second target (MRT), only when the water reads
    // it, so the depth-reconstructed source keeps the cost of 6B.
    if (writeNormal)
    {
        d3d_->ClearSceneNormal();
        d3d_->BindSceneTargetsWithNormal();
    }
    const std::vector<SceneObject> objects = CurrentSceneObjects();
    Model* boundMesh = nullptr;
    for (const SceneObject& object : objects)
    {
        Model* mesh = object.cylinder ? unitCylinder_.get() : unitBox_.get();
        if (mesh != boundMesh)
        {
            mesh->Render(context);
            boundMesh = mesh;
        }
        const XMMATRIX world = XMMatrixScaling(object.size.x, object.size.y, object.size.z)
                             * XMMatrixTranslation(object.center.x, object.center.y, object.center.z);
        sceneShader_->Render(context, mesh->GetIndexCount(), world, object.material);
    }
    if (writeNormal)
    {
        d3d_->BindHdrTarget();
    }
    d3d_->SetRasterizerDefault();
}

std::vector<Graphics::SceneObject> Graphics::CurrentSceneObjects() const
{
    std::vector<SceneObject> objects = scene_ == WaterLab ? labObjects_ : frontObjects_;
    if (scene_ == WaterLab && shadowTestBox_)
    {
        // sun-shadow: a box circling above the open water, so a shadow that follows its caster can be checked.
        const float angle = DirectX::XM_2PI * elapsedTime_ / kTestBoxPeriod;
        SceneObject box;
        box.center = {kTestBoxCircleRadius * std::cos(angle), kTestBoxBottomY + 0.5f * kTestBoxSize,
                      kTestBoxCircleRadius * std::sin(angle)};
        box.size = {kTestBoxSize, kTestBoxSize, kTestBoxSize};
        box.material = {{0.80f, 0.36f, 0.25f}, SceneShader::Solid};
        objects.push_back(box);
    }
    return objects;
}

void Graphics::RenderShadowPass(const DirectX::XMFLOAT4& lightDir)
{
    using namespace DirectX;
    shadowFrame_ = ShadowFrame{};
    shadowBoxWidth_ = 0.0f;
    if (scene_ == NoScene)
    {
        return;
    }
    const std::vector<SceneObject> objects = CurrentSceneObjects();

    // The sun's box: everything the scene holds, plus the test box's whole circle so the box (and every texel)
    // stays put while the test box moves. Only the sun direction changes it.
    XMFLOAT3 lo(FLT_MAX, FLT_MAX, FLT_MAX);
    XMFLOAT3 hi(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    const auto grow = [&](const XMFLOAT3& c, const XMFLOAT3& half)
    {
        lo = XMFLOAT3((std::min)(lo.x, c.x - half.x), (std::min)(lo.y, c.y - half.y), (std::min)(lo.z, c.z - half.z));
        hi = XMFLOAT3((std::max)(hi.x, c.x + half.x), (std::max)(hi.y, c.y + half.y), (std::max)(hi.z, c.z + half.z));
    };
    for (const SceneObject& object : scene_ == WaterLab ? labObjects_ : frontObjects_)
    {
        grow(object.center, XMFLOAT3(0.5f * object.size.x, 0.5f * object.size.y, 0.5f * object.size.z));
    }
    if (scene_ == WaterLab)
    {
        const float reach = kTestBoxCircleRadius + kTestBoxSize; // past the corners of the box on its circle
        grow(XMFLOAT3(0.0f, kTestBoxBottomY + 0.5f * kTestBoxSize, 0.0f), XMFLOAT3(reach, 0.5f * kTestBoxSize, reach));
    }

    // Light camera: looks along the sunlight from the middle of the box. Orthographic, because the sun is so far
    // away that its rays are parallel. The box is fitted around the 8 corners as the sun sees them, and made square
    // so one texel covers the same meters across and down.
    const XMVECTOR dir = XMVector3Normalize(XMVectorSet(lightDir.x, lightDir.y, lightDir.z, 0.0f));
    const XMVECTOR up = std::fabs(lightDir.y) > 0.99f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f) : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMVECTOR center = XMVectorSet(0.5f * (lo.x + hi.x), 0.5f * (lo.y + hi.y), 0.5f * (lo.z + hi.z), 1.0f);
    const XMMATRIX lightView = XMMatrixLookToLH(center, dir, up);
    XMFLOAT3 boxLo(FLT_MAX, FLT_MAX, FLT_MAX);
    XMFLOAT3 boxHi(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int corner = 0; corner < 8; ++corner)
    {
        const XMVECTOR p = XMVectorSet((corner & 1) ? hi.x : lo.x, (corner & 2) ? hi.y : lo.y, (corner & 4) ? hi.z : lo.z, 1.0f);
        XMFLOAT3 q;
        XMStoreFloat3(&q, XMVector3TransformCoord(p, lightView));
        boxLo = XMFLOAT3((std::min)(boxLo.x, q.x), (std::min)(boxLo.y, q.y), (std::min)(boxLo.z, q.z));
        boxHi = XMFLOAT3((std::max)(boxHi.x, q.x), (std::max)(boxHi.y, q.y), (std::max)(boxHi.z, q.z));
    }
    const float width = 1.01f * (std::max)(boxHi.x - boxLo.x, boxHi.y - boxLo.y); // 1 % margin: no object on the border
    const float midX = 0.5f * (boxLo.x + boxHi.x);
    const float midY = 0.5f * (boxLo.y + boxHi.y);
    const float nearZ = boxLo.z - 0.5f;
    const float farZ = boxHi.z + 0.5f;
    const XMMATRIX lightProjection = XMMatrixOrthographicOffCenterLH(
        midX - 0.5f * width, midX + 0.5f * width, midY - 0.5f * width, midY + 0.5f * width, nearZ, farZ);
    shadowBoxWidth_ = width;

    if (!shadowsEnabled_)
    {
        return; // params.x stays 0: the receivers skip the lookup
    }
    const float texelMeters = width / static_cast<float>(D3DClass::kShadowMapSize);
    XMStoreFloat4x4(&shadowFrame_.lightViewProjection, lightView * lightProjection);
    shadowFrame_.params = XMFLOAT4(1.0f, static_cast<float>(shadowFilter_), static_cast<float>(shadowBiasMode_), texelMeters);
    shadowFrame_.bias = XMFLOAT4(shadowConstantBias_, shadowSlopeBias_, shadowNormalOffset_, farZ - nearZ);
    shadowFrame_.pcss = XMFLOAT4(std::tan(XMConvertToRadians(0.5f * shadowSunSizeDeg_)),
                                 static_cast<float>(D3DClass::kShadowMapSize), 16.0f, 0.0f);

    // Shadow pass: the objects' depth as the sun sees them. The water casts none (it lets the light through).
    ID3D11DeviceContext* context = d3d_->GetDeviceContext();
    d3d_->BeginShadowPass();
    d3d_->SetRasterizerDoubleSided();
    sceneShader_->BeginDepthOnly(context, lightView, lightProjection);
    Model* boundMesh = nullptr;
    for (const SceneObject& object : objects)
    {
        Model* mesh = object.cylinder ? unitCylinder_.get() : unitBox_.get();
        if (mesh != boundMesh)
        {
            mesh->Render(context);
            boundMesh = mesh;
        }
        const XMMATRIX world = XMMatrixScaling(object.size.x, object.size.y, object.size.z)
                             * XMMatrixTranslation(object.center.x, object.center.y, object.center.z);
        sceneShader_->Render(context, mesh->GetIndexCount(), world, object.material);
    }
    d3d_->SetRasterizerDefault();
    d3d_->EndShadowPass();
}

// The scene normal buffer is written when the water reads it: the stored source, or the debug views that show it.
bool Graphics::UsesStoredNormal() const
{
    return scene_ != NoScene && (normalSource_ == 1 || water_.debugMode == 23 || water_.debugMode == 24);
}

DirectX::XMFLOAT4 Graphics::SceneWaterArea() const
{
    const float halfSize = scene_ == WaterLab ? kLabWaterHalfSize : scene_ == Waterfront ? kFrontWaterHalfSize : 0.0f;
    return DirectX::XMFLOAT4(halfSize, kWaveEdgeFade, 0.0f, 0.0f);
}

bool Graphics::Render(float deltaTime)
{
    using namespace DirectX;

    const auto now = std::chrono::steady_clock::now();
    UpdateFrameStats(deltaTime);
    colorShader_->CheckHotReload(d3d_->GetDevice(), now);
    sceneShader_->CheckHotReload(d3d_->GetDevice(), now);
    skyboxShader_->CheckHotReload(d3d_->GetDevice(), now);
    tonemapShader_->CheckHotReload(d3d_->GetDevice(), now);

    const bool capturing = BeginCaptureFrame();

    camera_->Render();

    const float aspect = (screenHeight_ > 0)
        ? static_cast<float>(screenWidth_) / static_cast<float>(screenHeight_)
        : 1.0f;

    // Bench plane: pitch in local space, then yaw around world Y (turntable-style drag).
    const XMMATRIX world = oceanMode_
        ? XMMatrixIdentity()
        : XMMatrixRotationX(XMConvertToRadians(modelRotation_.x)) *
          XMMatrixRotationY(XMConvertToRadians(modelRotation_.y));
    const XMMATRIX view = camera_->GetViewMatrix();
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(cameraFovDeg_),
        aspect,
        SCREEN_NEAR,
        SCREEN_DEPTH);

    XMMATRIX viewNoTrans = view;
    viewNoTrans.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    const XMFLOAT4 cameraPosWS(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z, 1.0f);

    // Sun position on the sky (yaw uses the camera convention, elevation > 0 = above the horizon).
    // g_LightDirection is the direction light travels, i.e. away from the sun.
    // On the fixed top shot the preset may swap in its Top View Sun (direction and intensity).
    const bool topSunActive = topSun_.enabled != 0 && IsTopShotView();
    const float sunYawRad = XMConvertToRadians(topSunActive ? topSun_.yawDeg : sunYawDeg_);
    const float sunElevationRad = XMConvertToRadians(topSunActive ? topSun_.elevationDeg : sunElevationDeg_);
    const float sunIntensity = topSunActive ? topSun_.intensity : lightIntensity_;
    const float cosElevation = cosf(sunElevationRad);
    const XMFLOAT4 lightDir(
        -sinf(sunYawRad) * cosElevation,
        -sinf(sunElevationRad),
        -cosf(sunYawRad) * cosElevation,
        0.0f);
    const XMFLOAT4 lightColorPacked(lightColor_.x, lightColor_.y, lightColor_.z, sunIntensity);
    const XMFLOAT4 ambientColorPacked(ambientColor_.x, ambientColor_.y, ambientColor_.z, ambientIntensity_);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    gpuTimer_.Begin(d3d_->GetDeviceContext());
    // sun-shadow: the sun's depth first; the opaque objects and the water read it.
    RenderShadowPass(lightDir);
    // Clear color is the old sRGB background, converted because the HDR target holds linear values.
    d3d_->BeginScene(SrgbToLinear(0.02f), SrgbToLinear(0.08f), SrgbToLinear(0.11f), 1.0f);

    // Numeric views (answer key, Scene Debug 2; reconstruction, water debug 12 / 13) leave the sky out: a sky
    // pixel whose blue happens to be 128 would read as a measurement (it did, in the first path comparison).
    if (skyboxVisible_ && !IsNumericView())
    {
        d3d_->SetDepthLessEqual();
        skybox_->Render(d3d_->GetDeviceContext());
        // Analytic sun disk for HDR skies (their sun was cut out of the texture). Radiance = irradiance /
        // solid angle of the disk; lightColor holds E/pi, so E = pi * light.
        const float cosSunRadius = cosf(XMConvertToRadians(kSunAngularRadiusDeg));
        const float sunSolidAngle = XM_2PI * (1.0f - cosSunRadius);
        const XMFLOAT4 sunLightLinear = SrgbToLinear(lightColorPacked);
        const float sunRadianceScale = kEnvironments[environmentIndex_].hdr ? XM_PI * sunIntensity / sunSolidAngle : 0.0f;
        skyboxShader_->Render(
            d3d_->GetDeviceContext(),
            skybox_->GetIndexCount(),
            viewNoTrans,
            projection,
            XMFLOAT4(-lightDir.x, -lightDir.y, -lightDir.z, cosSunRadius),
            XMFLOAT4(sunLightLinear.x * sunRadianceScale, sunLightLinear.y * sunRadianceScale, sunLightLinear.z * sunRadianceScale, 0.0f),
            environments_[environmentIndex_]->GetSRV(),
            d3d_->GetSampler());
        d3d_->SetDepthDefault();
    }

    if (scene_ != NoScene)
    {
        RenderTestScene(view, projection, lightDir, lightColorPacked, ambientColorPacked);
    }

    // The depth debug view is for looking under the water, so it leaves the (still opaque) water out.
    const bool drawWater = scene_ == NoScene || (showWater_ && sceneDebug_ == 0);
    gpuTimer_.Mark(d3d_->GetDeviceContext()); // end of the opaque section (clear, sky, test scene objects)
    if (drawWater)
    {
        d3d_->SetRasterizerWaterSurface();
        water_.farWaveNormals = farWaveNormals_ ? 1.0f : 0.0f; // a renderer switch, not part of the presets
        {
            // "Align to wind" follows Wave 1 as rendered (also right when the waves were edited in Advanced).
            const DirectX::XMFLOAT2& wind = water_.waves[0].direction;
            const float windDeg = DirectX::XMConvertToDegrees(std::atan2(wind.y, wind.x));
            const auto rotation = [&](bool align, int map) {
                return align ? RippleRotation(windDeg, normalMapAxisDeg_[map]) : DirectX::XMFLOAT2(1.0f, 0.0f);
            };
            const DirectX::XMFLOAT2 a = rotation(rippleAlignA_, normalMapA_);
            const DirectX::XMFLOAT2 b = rotation(rippleAlignB_, normalMapB_);
            water_.normalRotation = { a.x, a.y, b.x, b.y };
        }
        water_.rippleSlopeVarianceA = normalMapSlopeVariance_[normalMapA_];
        // foam-masks: what the picked mask holds (the shader reads its channels accordingly).
        water_.foamMap = std::clamp(water_.foamMap, 0, kFoamMapCount - 1);
        const FoamMapEntry& foamEntry = kFoamMaps[water_.foamMap];
        water_.foamMapFlags = DirectX::XMFLOAT3(foamEntry.tileable ? 1.0f : 0.0f, foamEntry.bubbles ? 1.0f : 0.0f,
                                                foamEntry.breakup ? 1.0f : 0.0f);
        water_.rippleSlopeVarianceB = normalMapSlopeVariance_[normalMapB_];
        // A --render-size frame keeps the look of the 720p window it was set up in (ocean-hero NOTES).
        water_.lodScale = fixedRenderSize_ ? static_cast<float>(screenHeight_) / static_cast<float>(SCREEN_HEIGHT) : 1.0f;
        Model* waterMesh = scene_ == WaterLab ? labWater_.get()
                         : scene_ == Waterfront ? frontWater_.get()
                         : oceanMode_ ? oceanGrid_.get() : model_.get();
        // Test scenes: the Surface window waves, or flat at y = 0 (known depths) with Lab Waves off. Not saved in presets.
        ColorShader::WaterParams waterDraw = water_;
        if (scene_ != NoScene && !labWaves_)
        {
            for (auto& wave : waterDraw.waves)
            {
                wave.amplitude = 0.0f;
            }
        }
        // Test scenes: the water reads the scene without the water (scene-color-depth). Snapshot the color,
        // then rebind the depth read-only so it can be tested against and read in the same draw.
        const bool sceneInputs = scene_ != NoScene;
        waterMesh->Render(d3d_->GetDeviceContext());
        if (sceneInputs)
        {
            d3d_->CopySceneColor();
            waterDraw.transmissionMode = transmissionMode_;
            waterDraw.surfaceReflection = surfaceReflection_;
            waterDraw.normalSource = normalSource_;
            waterDraw.waterArea = SceneWaterArea();
            waterDraw.shadow = shadowFrame_;
            waterDraw.shadowSRV = d3d_->GetShadowSRV();
            waterDraw.shadowSampler = d3d_->GetShadowSampler();
            // depth-reconstruct: pixel + depth -> world position goes back through view x projection.
            waterDraw.maxOpticalDistance = maxUnderwaterDistance_;
            DirectX::XMStoreFloat4x4(&waterDraw.invViewProjection, DirectX::XMMatrixInverse(nullptr, view * projection));
            // wave-occlusion: which water fragment wins where waves overlap on screen. The water reads the scene
            // depth, so it cannot write it too; the nearest wave is kept in a separate water depth buffer.
            if (waterOcclusion_ == 0)
            {
                // None (up to step 6): tested against the scene only, the last triangle drawn wins.
                d3d_->BindHdrTargetReadOnlyDepth();
                d3d_->SetDepthTestNoWrite();
            }
            else
            {
                d3d_->CopySceneDepthToWaterDepth();
                if (waterOcclusion_ == 2)
                {
                    // Prepass: the water's depth alone, so every pixel ends with the nearest water surface (or the
                    // scene in front of it). The color pass then shades only that fragment (EQUAL), once.
                    ColorShader::WaterParams depthPass = waterDraw;
                    depthPass.depthOnly = true;
                    d3d_->ClearWaterStencil();
                    d3d_->BindWaterDepthOnly();
                    d3d_->SetWaterPrepassDepth();
                    colorShader_->Render(d3d_->GetDeviceContext(), waterMesh->GetIndexCount(), world, view, projection,
                        lightDir, lightColorPacked, ambientColorPacked, elapsedTime_, cameraPosWS, depthPass,
                        nullptr, nullptr, nullptr, d3d_->GetSampler(), d3d_->GetWrapSampler());
                    d3d_->BindHdrTargetWaterDepth(true);
                    d3d_->SetDepthEqualNoWrite();
                }
                else
                {
                    // Single pass: the color pass writes the water depth itself (right result, overlapped fragments
                    // are shaded more than once).
                    d3d_->BindHdrTargetWaterDepth(false);
                    d3d_->SetDepthDefault();
                }
            }
        }
        colorShader_->Render(
            d3d_->GetDeviceContext(),
            waterMesh->GetIndexCount(),
            world,
            view,
            projection,
            lightDir,
            lightColorPacked,
            ambientColorPacked,
            elapsedTime_,
            cameraPosWS,
            waterDraw,
            environments_[environmentIndex_]->GetSRV(),
            normalMaps_[normalMapA_]->GetSRV(),
            normalMaps_[normalMapB_]->GetSRV(),
            d3d_->GetSampler(),
            d3d_->GetWrapSampler(),
            sceneInputs ? d3d_->GetSceneColorSRV() : nullptr,
            sceneInputs ? d3d_->GetSceneDepthSRV() : nullptr,
            sceneInputs ? foamMaps_[std::clamp(water_.foamMap, 0, kFoamMapCount - 1)]->GetSRV() : nullptr,
            sceneInputs && UsesStoredNormal() ? d3d_->GetSceneNormalSRV() : nullptr);
        d3d_->SetRasterizerDefault();
        if (sceneInputs)
        {
            // Unbind before the next frame binds the depth writable again (and the copy as a copy target).
            ID3D11ShaderResourceView* nullSrvs[5] = {};
            d3d_->GetDeviceContext()->PSSetShaderResources(3, 5, nullSrvs);
            d3d_->SetDepthDefault();
        }
    }
    gpuTimer_.Mark(d3d_->GetDeviceContext()); // end of the water section

    // HDR scene -> sRGB back buffer. Debug views carry data, not light, so they skip exposure and the
    // tone curve (their values were pre-decoded in the pixel shader and come back out unchanged).
    d3d_->BindBackBuffer();
    d3d_->SetRasterizerDoubleSided(); // the fullscreen triangle is clockwise; don't let it get culled
    // Water coverage (9), the water color part (15) and the reflection / underwater parts (18, 19) are HDR frames,
    // so they keep exposure and the tone curve.
    const bool hdrDebug = water_.debugMode == 9 || water_.debugMode == 15 || water_.debugMode == 18 || water_.debugMode == 19;
    const bool debugView = (water_.debugMode != 0 && !hdrDebug) || (scene_ != NoScene && sceneDebug_ != 0);
    tonemapShader_->Render(
        d3d_->GetDeviceContext(),
        d3d_->GetHdrSRV(),
        debugView ? 1.0f : std::exp2(exposureEv_),
        debugView ? TonemapShader::None : tonemapOperator_);
    d3d_->SetRasterizerDefault();

    if (!capturing)
    {
        DrawStatsOverlay();
        DrawViewWindow();
        DrawLightWindow();
        DrawWaterWindow();
        DrawSurfaceWindow();
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    gpuTimer_.End(d3d_->GetDeviceContext());

    if (capturing)
    {
        EndCaptureFrame();
    }

    // CPU cost of this frame, excluding the Present / vsync wait below.
    cpuFrameMs_ = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - now).count();

    d3d_->EndScene();

    DrainD3dMessages();

    return !captureFinishedQuit_;
}

void Graphics::UpdateFrameStats(float deltaTime)
{
    // Values shown in the overlay are averaged over 0.5 s so they are readable.
    statsAccumTime_ += deltaTime;
    statsAccumCpuMs_ += cpuFrameMs_;
    ++statsAccumFrames_;
    if (statsAccumTime_ >= 0.5f)
    {
        displayedFps_ = statsAccumFrames_ / statsAccumTime_;
        displayedCpuMs_ = statsAccumCpuMs_ / statsAccumFrames_;
        displayedGpuMs_ = gpuTimer_.GetLastMs();
        statsAccumTime_ = 0.0f;
        statsAccumCpuMs_ = 0.0f;
        statsAccumFrames_ = 0;
    }
}

void Graphics::DrawStatsOverlay()
{
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    // The title-bar arrow collapses / expands it.
    if (ImGui::Begin("Stats", nullptr, flags))
    {
        ImGui::Text("Time    %8.2f s", elapsedTime_);
        ImGui::Text("FPS     %8.1f", displayedFps_);
        ImGui::Text("Frame   %8.2f ms", displayedFps_ > 0.0f ? 1000.0f / displayedFps_ : 0.0f);
        ImGui::Text("CPU     %8.2f ms", displayedCpuMs_);
        if (displayedGpuMs_ >= 0.0f)
        {
            ImGui::Text("GPU     %8.2f ms", displayedGpuMs_);
        }
        else
        {
            ImGui::Text("GPU          n/a");
        }

        bool vsync = d3d_->GetVSync();
        if (ImGui::Checkbox("VSync", &vsync))
        {
            d3d_->SetVSync(vsync);
        }
        ImGui::SameLine();
        ImGui::TextDisabled(vsync ? "(FPS capped by display)" : "(uncapped)");
        ImGui::Text("Mesh: %s", oceanMode_ ? "ocean grid 1024^2" : "bench plane 32^2");
        if (topSun_.enabled != 0 && IsTopShotView())
        {
            ImGui::TextColored(kHighlightColor, "Sun:  Top View Sun (top shot)");
        }
        else
        {
            ImGui::Text("Sun:  preset sun");
        }
        ImGui::Separator();
        ImGui::TextDisabled("Keys: [1] View  [2] Light  [3] Water  [4] Surface");
    }
    ImGui::End();
}

namespace
{
// Water window style: the name sits above a full-width control, so it can be a plain description
// instead of a squeezed abbreviation next to the slider.
bool LabeledSlider(const char* label, float* value, float minValue, float maxValue,
    const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
    ImGui::TextWrapped("%s", label);
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::SliderFloat("##value", value, minValue, maxValue, format, flags);
    ImGui::PopID();
    return changed;
}

bool LabeledColor(const char* label, float* rgb)
{
    ImGui::TextUnformatted(label);
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::ColorEdit3("##color", rgb);
    ImGui::PopID();
    return changed;
}

// Top-level section of a settings window: bigger text on a full-width highlighted bar that folds its contents away
// (the windows grew long). Smaller groups inside a section keep the normal text size. The fold state lasts while
// the app runs.
bool SectionHeader(const char* label, bool defaultOpen = true)
{
    ImGui::Spacing();
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.3f);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.34f, 0.56f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.24f, 0.44f, 0.70f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.28f, 0.52f, 0.80f, 1.00f));
    // Own ID scope: a header named like a widget inside it (the View window's "Scene" header and "Scene" combo)
    // would otherwise share its ID (ImGui: "2 visible items with conflicting ID").
    ImGui::PushID("section");
    const bool open = ImGui::CollapsingHeader(label, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None);
    ImGui::PopID();
    ImGui::PopStyleColor(3);
    ImGui::PopFont();
    return open;
}

// Direction of travel on the water as the camera sees it, in degrees: 0 = away from the camera,
// +90 = across the screen to the right, 180 = toward the camera. worldDeg uses the wave convention
// (0 = +X, 90 = +Z); cameraYawDeg is cameraRotation_.y (forward = (sin yaw, cos yaw) in x/z).
float ViewRelativeDeg(float worldDeg, float cameraYawDeg)
{
    const float d = DirectX::XMConvertToRadians(worldDeg);
    const float yaw = DirectX::XMConvertToRadians(cameraYawDeg);
    const float dx = std::cos(d), dz = std::sin(d);
    const float alongForward = dx * std::sin(yaw) + dz * std::cos(yaw);
    const float alongRight = dx * std::cos(yaw) - dz * std::sin(yaw); // UpdateCamera's right vector
    return DirectX::XMConvertToDegrees(std::atan2(alongRight, alongForward));
}

const char* DescribeViewDirection(float relativeDeg)
{
    // 8 sectors of 45 degrees, centered on the 4 main directions.
    const char* names[] = {
        "away from camera",
        "away, to the right",
        "left -> right",
        "toward, to the right",
        "toward camera",
        "toward, to the left",
        "right -> left",
        "away, to the left",
    };
    const float wrapped = std::fmod(relativeDeg + 360.0f + 22.5f, 360.0f);
    return names[static_cast<int>(wrapped / 45.0f) % 8];
}

// Small dial on the current line: the camera always looks "up" on it (tick at the top), the arrow
// is the direction of travel relative to that. Reads like the screen: up = away, right = right.
void DrawDirectionDial(float relativeDeg)
{
    const float size = ImGui::GetFrameHeight();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 center(origin.x + size * 0.5f, origin.y + size * 0.5f);
    const float radius = size * 0.5f - 1.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 rimColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const ImU32 arrowColor = ImGui::GetColorU32(ImGuiCol_PlotHistogram);

    drawList->AddCircle(center, radius, rimColor, 24);
    drawList->AddTriangleFilled(ImVec2(center.x, center.y - radius), ImVec2(center.x - 3.0f, center.y - radius + 4.0f),
        ImVec2(center.x + 3.0f, center.y - radius + 4.0f), rimColor);

    const float r = DirectX::XMConvertToRadians(relativeDeg);
    const ImVec2 dir(std::sin(r), -std::cos(r));
    const ImVec2 tip(center.x + dir.x * (radius - 2.0f), center.y + dir.y * (radius - 2.0f));
    const ImVec2 tail(center.x - dir.x * (radius - 4.0f), center.y - dir.y * (radius - 4.0f));
    drawList->AddLine(tail, tip, arrowColor, 2.0f);
    const ImVec2 side(-dir.y * 3.5f, dir.x * 3.5f);
    const ImVec2 back(tip.x - dir.x * 5.0f, tip.y - dir.y * 5.0f);
    drawList->AddTriangleFilled(tip, ImVec2(back.x + side.x, back.y + side.y), ImVec2(back.x - side.x, back.y - side.y), arrowColor);

    ImGui::Dummy(ImVec2(size, size));
}

// Normal map scroll shown as "which way the ripples drift + how fast". The stored value stays the
// UV velocity the shader adds to the sample position. Both water meshes run u = +X, v = +Z (Model.cpp),
// and sampling at uv + velocity * t moves the pattern by -velocity, so in world terms the ripples
// drift along (-u, -v). The value is only rewritten when a slider moves, so presets round-trip exactly.
void FlowControls(DirectX::XMFLOAT2& velocity, float& rememberedDeg, float cameraYawDeg)
{
    float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    if (speed > 0.0f)
    {
        rememberedDeg = DirectX::XMConvertToDegrees(std::atan2(-velocity.y, -velocity.x));
    }
    float directionDeg = rememberedDeg;
    const float relativeDeg = ViewRelativeDeg(directionDeg, cameraYawDeg);

    ImGui::TextUnformatted("Flow direction (deg, 0 = +X, 90 = +Z)");
    DrawDirectionDial(relativeDeg);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", DescribeViewDirection(relativeDeg));
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool changed = ImGui::SliderFloat("##flowDirection", &directionDeg, -180.0f, 180.0f, "%.0f");
    changed |= LabeledSlider("Flow speed", &speed, 0.0f, 0.2f, "%.3f");
    if (changed)
    {
        rememberedDeg = directionDeg;
        const float r = DirectX::XMConvertToRadians(directionDeg);
        velocity = { -std::cos(r) * speed, -std::sin(r) * speed };
    }
}
} // namespace

void Graphics::DrawViewWindow()
{
    if (!showViewWindow_)
    {
        return;
    }
    // Left side, under the Stats overlay (imgui.ini keeps the user's layout after the first run).
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(10.0f, 190.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.0f, io.DisplaySize.y - 200.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("View Settings  [1]", &showViewWindow_))
    {
        ImGui::End();
        return;
    }
    ImGui::PushItemWidth(-100.0f); // room for the labels on the right

    if (SectionHeader("Presets"))
    {
        ImGui::TextDisabled("Sky, lights and water together");
        ImGui::TextDisabled("Kept per mesh - now: %s", oceanMode_ ? "ocean grid" : "bench plane");
        for (int i = 0; i < kPresetThemeCount; ++i)
        {
            ImGui::PushID(i);
            if (ImGui::Button(kPresetLabels[i], ImVec2(80.0f, 0.0f)))
            {
                ApplyPreset(i);
            }
            ImGui::SameLine();
            if (ImGui::Button("Save Current"))
            {
                SaveCurrentPreset(i);
            }
            if (i == activeTheme_)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("< current");
            }
            ImGui::PopID();
        }
    }

    if (SectionHeader("Camera"))
    {
        ImGui::TextDisabled("WASD move, Q/E down/up, arrows or drag: look");
        ImGui::SliderFloat("FOV (deg)", &cameraFovDeg_, 30.0f, 120.0f);
        ImGui::SliderFloat("Move Speed", &cameraMoveSpeed_, 0.1f, 10.0f);
        ImGui::SliderFloat("Turn Speed", &cameraTurnSpeed_, 30.0f, 360.0f, "%.0f deg/s");
        if (ImGui::Button("Reset Camera"))
        {
            ApplyCameraShot(kDefaultShotIndex);
            cameraMoveSpeed_ = 2.0f;
            cameraTurnSpeed_ = 90.0f;
        }
    }

    if (SectionHeader("Camera Presets", false))
    {
        ImGui::TextDisabled("Fixed shots (same as --capture)");
        for (int i = 0; i < static_cast<int>(std::size(kCaptureShots)); ++i)
        {
            if (i > 0 && i != 3 && i != kFirstLabShotIndex)
            {
                ImGui::SameLine();
            }
            if (ImGui::Button(kCaptureShots[i].name))
            {
                ApplyCameraShot(i);
            }
        }
        ImGui::TextDisabled("Slots (assets/camera_presets.txt)");
        for (int i = 0; i < kCameraSlotCount; ++i)
        {
            ImGui::PushID(i);
            const CameraView& slotView = cameraSlots_[i];
            const char* slotScene = !slotView.ocean ? " [bench]"
                                  : slotView.scene == WaterLab ? " [lab]"
                                  : slotView.scene == Waterfront ? " [front]" : " [ocean]";
            ImGui::Text("Slot %d%s", i + 1, cameraSlotUsed_[i] ? slotScene : " (empty)");
            ImGui::SameLine(120.0f);
            if (ImGui::Button("Save"))
            {
                cameraSlots_[i] = MakeViewFromCurrent();
                cameraSlotUsed_[i] = true;
                SaveCameraSlots();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!cameraSlotUsed_[i]);
            if (ImGui::Button("Load"))
            {
                ApplyView(cameraSlots_[i]);
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::TextDisabled("Pos (%.2f, %.2f, %.2f)  Pitch %.1f  Yaw %.1f",
            cameraPosition_.x, cameraPosition_.y, cameraPosition_.z, cameraRotation_.x, cameraRotation_.y);
    }

    if (SectionHeader("Scene"))
    {
        // Two water versions: Old = the painted opaque water (open water, bench, and the test scenes on request),
        // New = the water below computed from the scene (test scenes only, transmissionMode_ 1-3). The Light and
        // Water windows show the picked version's settings. The camera is not touched (shared by both).
        const bool testScene = scene_ != NoScene;
        const bool newWater = IsNewWater();
        ImGui::TextUnformatted("Water version");
        ImGui::BeginDisabled(!testScene);
        if (ImGui::RadioButton("Old (opaque, painted)", !newWater) && newWater)
        {
            lastNewTransmission_ = transmissionMode_;
            transmissionMode_ = 0;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("New (underwater)", newWater) && !newWater)
        {
            transmissionMode_ = lastNewTransmission_;
        }
        ImGui::EndDisabled();
        if (!testScene)
        {
            ImGui::TextDisabled("New needs a test scene (Water Lab / Waterfront).");
        }
        const char* sceneLabels[] = { "Bench plane", "Ocean grid (open water)", "Water Lab (test depths)", "Waterfront (graybox)" };
        int sceneChoice = scene_ != NoScene ? 1 + scene_ : (oceanMode_ ? 1 : 0);
        if (ImGui::Combo("Scene", &sceneChoice, sceneLabels, IM_ARRAYSIZE(sceneLabels)))
        {
            oceanMode_ = sceneChoice != 0;
            scene_ = sceneChoice >= 2 ? sceneChoice - 1 : NoScene;
            // Entering a test scene from anywhere else would leave the camera far off: start from its overview shot.
            // Picking a test scene here also picks the New water, the version the test scenes are built for.
            if (scene_ != NoScene)
            {
                ApplyCameraShot(FindCaptureShot(scene_ == WaterLab ? "lab_stairs" : "front_rails"));
                if (transmissionMode_ == 0)
                {
                    transmissionMode_ = lastNewTransmission_;
                }
            }
        }
        ImGui::Checkbox("Skybox Visible", &skyboxVisible_);
        if (scene_ != NoScene)
        {
            ImGui::Checkbox("Show Water", &showWater_);
            ImGui::SameLine();
            ImGui::Checkbox("Lab Waves", &labWaves_);
            if (scene_ == WaterLab)
            {
                ImGui::Checkbox("Shadow test box", &shadowTestBox_);
                ImGui::SameLine();
                ImGui::TextDisabled("(circles above the water)");
            }
            // Water depth = 0 - world position y (the water is at y = 0): the answer key the depth buffer is
            // checked against later, not the depth buffer itself.
            const char* sceneDebugLabels[] = { "Render", "Water depth (WP, m) - water hidden", "Water depth numeric (WP) - compare tool",
                "Not measured marker (numeric views)", "World normal (answer key) - water hidden",
                "Sun shadow (white = sun, gray = facing away) - water hidden" };
            if (ImGui::Combo("Scene Debug", &sceneDebug_, sceneDebugLabels, IM_ARRAYSIZE(sceneDebugLabels)))
            {
                SwapDebugViews(sceneDebug_, parkedSceneDebug_, water_.debugMode, parkedWaterDebug_);
            }
            if (sceneDebug_ == 1)
            {
                // Same bands and display colors as DepthBandColor in scene.hlsl.
                struct Band { ImVec4 color; const char* label; };
                const Band bands[] = {
                    { ImVec4(0.35f, 0.35f, 0.35f, 1.0f), "above water (y > 0)" },
                    { ImVec4(0.30f, 0.95f, 1.00f, 1.0f), "0 - 0.5 m  (0.2 m pad)" },
                    { ImVec4(0.35f, 0.90f, 0.35f, 1.0f), "0.5 - 2 m  (1 m pad)" },
                    { ImVec4(1.00f, 0.90f, 0.25f, 1.0f), "2 - 4.5 m  (3 m pad)" },
                    { ImVec4(1.00f, 0.55f, 0.20f, 1.0f), "4.5 - 7 m  (6 m pad)" },
                    { ImVec4(0.75f, 0.40f, 0.95f, 1.0f), "7 m and deeper  (lab floor 8 m)" },
                };
                for (const Band& band : bands)
                {
                    ImGui::TextColored(band.color, "#");
                    ImGui::SameLine();
                    ImGui::TextUnformatted(band.label);
                }
                ImGui::TextDisabled("Dark lines every 1 m, red line = water surface (y = 0)");
            }
            ImGui::TextDisabled("The water version's settings: Water window (3).");
            ImGui::TextDisabled("1 unit = 1 m, water at y = 0, ocean presets.");
            ImGui::TextDisabled("Lab Waves: the Surface window (4) waves; off = flat.");
        }
        else if (oceanMode_)
        {
            ImGui::TextDisabled("Ocean grid stays level (drag = look around)");
        }
        else
        {
            ImGui::TextDisabled("Left-drag: rotate plane (pitch %.0f, yaw %.0f)", modelRotation_.x, modelRotation_.y);
            if (ImGui::Button("Reset Plane Rotation"))
            {
                modelRotation_ = {0.0f, 0.0f, 0.0f};
            }
        }
    }

    if (SectionHeader("Capture", false))
    {
        ImGui::InputText("Label", captureLabel_, sizeof(captureLabel_));
        // Two-step: a stray click only opens the confirmation, and UI captures never overwrite an
        // existing folder (StartCaptureSet picks <label>_2, _3, ... instead). --capture still overwrites.
        if (ImGui::Button("Capture Set (presets x shots)..."))
        {
            ImGui::OpenPopup("Confirm capture");
        }
        if (ImGui::BeginPopupModal("Confirm capture", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Capture %d images to", static_cast<int>(kPresetThemeCount * std::size(kCaptureShots)));
            ImGui::Text("%s ?", WideToUtf8((CapturesRoot() / Utf8ToWide(captureLabel_)).wstring()).c_str());
            ImGui::TextDisabled("An existing folder is kept; a numbered folder is created instead.");
            if (ImGui::Button("Capture"))
            {
                StartCaptureSet(Utf8ToWide(captureLabel_), false, false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::TextDisabled("Fixed time %.1fs, UI hidden.", captureTime_);
        if (!captureStatus_.empty())
        {
            ImGui::TextWrapped("%s", captureStatus_.c_str());
        }
    }

    if (SectionHeader("Debug View"))
    {
        const char* debugLabels[] = { "render", "Sampled normal map", "World-space N", "UV", "Front/back face", "Lighting terms (R diffuse, G spec, B fresnel)", "Wave LOD (R mesh, G pixel, B roughness)", "Ocean detail (R ripple roughness, G gust, B haze)",
            "Scene depth, water only (test scenes)", "Water coverage (test scenes)",
            "Water depth (from depth buffer)", "Underwater view path", "Water depth numeric (compare tool)", "View path numeric (compare tool)",
            "Transmittance (RGB, compare tool)", "Water color part only (scattering)", "Ramp color (as picked)",
            "Reflection amount (Fresnel x strength)", "Reflection part only", "Underwater part only",
            "Contact foam amount", "Steepness behind the water (active normal)", "Normal from depth (world, color)",
            "Stored scene normal (G-buffer, color)", "Normal difference: depth vs stored (0-30 deg)",
            "Wave height lookup error (white = 1 mm)", "Refraction offset (pixels)",
            "Refraction result (green ok, cyan reduced, red foreground leak, blue off screen)", "Refracted ray miss (white = 10 cm)",
            "Sun shadow at the water surface (white = sun)" };
        if (ImGui::Combo("Debug Mode", &water_.debugMode, debugLabels, IM_ARRAYSIZE(debugLabels)) && scene_ != NoScene)
        {
            SwapDebugViews(water_.debugMode, parkedWaterDebug_, sceneDebug_, parkedSceneDebug_);
        }
        if (scene_ != NoScene)
        {
            ImGui::TextDisabled("Scene Debug and Debug Mode take turns: picking one");
            ImGui::TextDisabled("parks the other, Render brings it back. X = swap.");
        }
    }
    const std::string& shaderError = colorShader_->GetLastError();
    if (!shaderError.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Shader compile error:");
        ImGui::TextWrapped("%s", shaderError.c_str());
    }
    else
    {
        const std::string& reloadStamp = colorShader_->GetLastReloadStamp();
        ImGui::TextDisabled("Shader hot reload: %s", reloadStamp.empty() ? "ready" : reloadStamp.c_str());
    }
    const std::string& sceneShaderError = sceneShader_->GetLastError();
    if (!sceneShaderError.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Scene shader (scene.hlsl) compile error:");
        ImGui::TextWrapped("%s", sceneShaderError.c_str());
    }
    if (!normalMapStatus_.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Normal map load failed:");
        ImGui::TextWrapped("%s", normalMapStatus_.c_str());
    }

    ImGui::PopItemWidth();
    ImGui::End();
}

void Graphics::DrawLightWindow()
{
    if (!showLightWindow_)
    {
        return;
    }
    // Right side, left of the Water window.
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 380.0f, 10.0f), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Light Settings  [2]", &showLightWindow_))
    {
        ImGui::End();
        return;
    }
    ImGui::PushItemWidth(-110.0f); // room for the labels on the right

    const Environment& currentEnv = kEnvironments[environmentIndex_];
    if (SectionHeader("Sky / Environment"))
    {
        ImGui::TextDisabled("Seen in the background and in reflections");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##sky", currentEnv.name))
        {
            for (int i = 0; i < kEnvironmentCount; ++i)
            {
                if (ImGui::Selectable(kEnvironments[i].name, i == environmentIndex_))
                {
                    environmentIndex_ = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(!currentEnv.hdr);
        if (ImGui::Button("Calibrate From Sky"))
        {
            // Sun direction, sun light, ambient and exposure as measured from the HDR sky.
            sunYawDeg_ = currentEnv.sunYawDeg;
            sunElevationDeg_ = currentEnv.sunElevationDeg;
            SplitLinearLight(currentEnv.sunLight, lightColor_, lightIntensity_);
            SplitLinearLight(currentEnv.ambientLight, ambientColor_, ambientIntensity_);
            exposureEv_ = currentEnv.keyExposureEv;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Sky Sun Direction Only"))
        {
            sunYawDeg_ = currentEnv.sunYawDeg;
            sunElevationDeg_ = currentEnv.sunElevationDeg;
        }
        ImGui::TextDisabled("Sun in this sky: yaw %.1f, elevation %.1f", currentEnv.sunYawDeg, currentEnv.sunElevationDeg);
    }

    // Which sun lights the frame is easy to miss (the Sun sliders don't move when the top sun takes over),
    // so both sections say it in the highlight color.
    const bool topShot = IsTopShotView();
    const bool topSunActive = topSun_.enabled != 0 && topShot;
    // Sun and ambient light both versions; what they light differs (the picked version: View window > Scene).
    const bool newWater = IsNewWater();
    if (SectionHeader("Sun"))
    {
        if (topSunActive)
        {
            ImGui::TextColored(kHighlightColor, "Not in use on the top shot: Top View Sun below\nlights it. These sliders are for every other view.");
        }
        ImGui::TextDisabled(newWater ? "New: lights the scene and the water's own color"
                                     : "Old: lights the painted water color");
        ImGui::SliderFloat("Yaw (deg)", &sunYawDeg_, 0.0f, 360.0f, "%.1f");
        ImGui::SliderFloat("Elevation (deg)", &sunElevationDeg_, 0.0f, 90.0f, "%.1f");
        ImGui::ColorEdit3("Color", &lightColor_.x);
        ImGui::SliderFloat("Intensity", &lightIntensity_, 0.0f, 5.0f);
    }

    // The top shot is an open water shot, so its own sun never lights a test scene.
    if (scene_ == NoScene)
    {
        if (SectionHeader("Top View Sun", false))
        {
            ImGui::TextDisabled("Its own sun for the top shot only");
            bool topSunEnabled = topSun_.enabled != 0;
            if (ImGui::Checkbox("Own sun for the top shot", &topSunEnabled))
            {
                topSun_.enabled = topSunEnabled ? 1 : 0;
            }
            if (topSunActive)
            {
                ImGui::TextColored(kHighlightColor, "IN USE: the camera is on the top shot");
            }
            else if (topSunEnabled)
            {
                ImGui::TextDisabled("Waiting: used once the camera is on\nView > Camera Presets > top (any move leaves it)");
            }
            else
            {
                ImGui::TextDisabled("Off: the top shot uses the Sun above");
            }
            ImGui::BeginDisabled(!topSunEnabled);
            ImGui::SliderFloat("Yaw (deg)##top", &topSun_.yawDeg, 0.0f, 360.0f, "%.1f");
            ImGui::SliderFloat("Elevation (deg)##top", &topSun_.elevationDeg, 0.0f, 90.0f, "%.1f");
            ImGui::SliderFloat("Intensity##top", &topSun_.intensity, 0.0f, 5.0f);
            ImGui::EndDisabled();
            ImGui::TextDisabled("Elevation 90 = sun right above the plane,\nso its glint faces the top camera. Color = Sun color.");
        }
    }

    if (SectionHeader("Shadows"))
    {
        if (scene_ != NoScene)
        {
            DrawShadowSettings();
        }
        else
        {
            ImGui::TextDisabled("Test scenes only: pick Water Lab / Waterfront.");
        }
    }

    if (SectionHeader("Sun Glint"))
    {
        ImGui::TextDisabled("The sun mirrored on the water");
        ImGui::TextDisabled(newWater ? "New: part of the reflection, x R" : "Old: added on top of the painted water, x R");
        ImGui::SliderFloat("Sharpness", &water_.sunGlintPower, 64.0f, 4096.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Strength", &water_.sunGlintIntensity, 0.0f, 5.0f, "%.2f");
        ImGui::TextDisabled("Strength 1 = physically based");
        ImGui::SliderFloat("Far spread", &water_.farGlintSpread, 0.0f, 3.0f, "%.2f");
        ImGui::TextDisabled("How much far, unresolved waves widen the glint\n(1 = physical, 0 = sharp, >1 = wider glitter\npath, e.g. a sun on the horizon)");
    }

    if (SectionHeader("Ambient"))
    {
        ImGui::TextDisabled("Light from the whole sky");
        ImGui::ColorEdit3("Color##ambient", &ambientColor_.x);
        ImGui::SliderFloat("Intensity##ambient", &ambientIntensity_, 0.0f, 2.0f);
    }

    // rgb-attenuation + water-scatter: how light fades in the New water and what color fills it in. Here rather
    // than in the Water window, next to the sun and sky that light that color.
    if (SectionHeader("Underwater Light (test scenes)"))
    {
        if (newWater)
        {
            DrawUnderwaterLightSettings();
        }
        else
        {
            ImGui::TextDisabled("New water only: pick Water Lab / Waterfront.");
        }
    }

    if (SectionHeader("Tonemapping", false))
    {
        const char* operatorNames[] = { "None (clamp)", "Reinhard", "ACES (per channel)", "ACES (hue-preserving)" };
        int op = static_cast<int>(tonemapOperator_);
        if (ImGui::Combo("Tone Curve", &op, operatorNames, IM_ARRAYSIZE(operatorNames)))
        {
            tonemapOperator_ = static_cast<TonemapShader::Operator>(op);
        }
        ImGui::SliderFloat("Exposure (EV)", &exposureEv_, -4.0f, 4.0f, "%+.2f");
        ImGui::TextDisabled("x%.3f brightness, saved with the preset", std::exp2(exposureEv_));
    }

    ImGui::PopItemWidth();
    ImGui::End();
}

// sun-shadow: Light window > Shadows. Filters and biases are lists so each can be compared on the same frame.
void Graphics::DrawShadowSettings()
{
    ImGui::TextDisabled("The sun's depth map (1024 x 1024). Shadows dim the\nsun alone; the sky light still reaches them.");
    ImGui::Checkbox("Shadows##enabled", &shadowsEnabled_);
    ImGui::BeginDisabled(!shadowsEnabled_);

    // Texel size: the sun's box is spread over 1024 texels, so one texel = box width / 1024. On flat ground the
    // sun's rays come in slanted, so a texel stretches by 1 / sin(elevation) along the sun's direction.
    if (shadowBoxWidth_ > 0.0f)
    {
        const float texelCm = 100.0f * shadowBoxWidth_ / static_cast<float>(D3DClass::kShadowMapSize);
        const float sinElevation = std::sin(DirectX::XMConvertToRadians((std::max)(sunElevationDeg_, 1.0f)));
        ImGui::Text("Sun box %.1f m  ->  1 texel = %.1f cm", shadowBoxWidth_, texelCm);
        ImGui::TextDisabled("On flat ground: %.1f cm along the sun (/ sin %.0f deg)", texelCm / sinElevation, sunElevationDeg_);
    }

    const char* filterLabels[] = { "Hard (1 texel)", "PCF 3 x 3", "PCSS (softer away from the caster)" };
    ImGui::Combo("Filtering", &shadowFilter_, filterLabels, IM_ARRAYSIZE(filterLabels));
    if (shadowFilter_ == 0)
    {
        ImGui::TextDisabled("One yes / no per texel: the edge shows the texel squares.");
    }
    else if (shadowFilter_ == 1)
    {
        ImGui::TextDisabled("9 comparisons averaged: an even soft edge, the same\nwidth everywhere.");
    }
    else
    {
        ImGui::TextDisabled("Finds the caster's distance first: sharp at the foot of\na pillar, softer toward the far end of its shadow.");
        ImGui::SliderFloat("Sun size (deg)", &shadowSunSizeDeg_, 0.25f, 8.0f, "%.2f");
        ImGui::TextDisabled("Real sun 0.53 deg. Blur radius is capped at 16 texels.");
    }

    const char* biasLabels[] = { "None", "Constant", "Slope-scaled", "Normal offset", "All (default)" };
    ImGui::Combo("Bias", &shadowBiasMode_, biasLabels, IM_ARRAYSIZE(biasLabels));
    ImGui::TextDisabled("Too little: a surface shades itself in stripes (acne).\nToo much: the shadow lets go of its caster (peter-panning).");
    if (shadowBiasMode_ == 1 || shadowBiasMode_ == 4)
    {
        ImGui::SliderFloat("Constant (m)", &shadowConstantBias_, 0.0f, 0.2f, "%.3f");
    }
    if (shadowBiasMode_ == 2 || shadowBiasMode_ == 4)
    {
        ImGui::SliderFloat("Slope (texels x tan)", &shadowSlopeBias_, 0.0f, 4.0f, "%.2f");
        ImGui::TextDisabled("More where the sun grazes the surface.");
    }
    if (shadowBiasMode_ == 3 || shadowBiasMode_ == 4)
    {
        ImGui::SliderFloat("Normal offset (texels)", &shadowNormalOffset_, 0.0f, 4.0f, "%.2f");
        ImGui::TextDisabled("Looks the map up from a point lifted off the surface.");
    }

    if (ImGui::TreeNode("Shadow map preview"))
    {
        // R32 depth shows in the red channel: dark = near the sun, bright = far from it.
        ImGui::Image(reinterpret_cast<ImTextureID>(d3d_->GetShadowSRV()), ImVec2(256.0f, 256.0f));
        ImGui::TextDisabled("Red = depth from the sun (dark = nearer the sun).");
        ImGui::TreePop();
    }
    ImGui::EndDisabled();
}

void Graphics::DrawWaterWindow()
{
    if (!showWaterWindow_)
    {
        return;
    }
    // Right edge, full height.
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10.0f, 10.0f), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(360.0f, io.DisplaySize.y - 20.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Water Settings  [3]", &showWaterWindow_))
    {
        ImGui::End();
        return;
    }

    // Only the water version's own settings are shown (switch: View window, Scene); the hidden values stay as
    // they are.
    const bool newWater = IsNewWater();
    ImGui::TextDisabled("Water version: %s (View window > Scene)", newWater ? "New (underwater)" : "Old (opaque, painted)");

    if (!newWater)
    {
        if (SectionHeader("Old: Painted Color"))
        {
            ImGui::TextDisabled("The color the opaque water is painted with.");
            LabeledColor("Color looking straight down", &water_.facingColor.x);
            LabeledColor("Color at a low angle", &water_.grazingColor.x);
        }
    }
    else
    {
        DrawNewWaterSettings();
    }

    if (SectionHeader("Reflection (both versions)"))
    {
        ImGui::TextDisabled(newWater ? "New: final = water below x (1 - R) + sky x R"
                                     : "Old: final = painted color blended to the sky by R");
        LabeledSlider("Reflection strength", &water_.reflectionStrength, 0.0f, 1.0f, "%.2f");
        LabeledSlider("Reflectivity head-on (F0, real water = 0.02)", &water_.fresnelF0, 0.0f, 0.6f, "%.3f");
        LabeledSlider("Fresnel curve (5 = physical; higher = mirror only at low angles)", &water_.fresnelPower, 1.0f, 8.0f, "%.1f");
    }

    ImGui::Separator();
    if (newWater)
    {
        ImGui::TextDisabled("Light loss and water color: Light window (2).");
    }
    ImGui::TextDisabled("Ripples, waves, far detail: Surface window (4).");

    ImGui::End();
}

// Shape and motion of the water surface (both versions); the color and light are in the Water and Light windows.
void Graphics::DrawSurfaceWindow()
{
    if (!showSurfaceWindow_)
    {
        return;
    }
    // Under the Light window, left of the Water window.
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 380.0f, io.DisplaySize.y * 0.45f), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(340.0f, io.DisplaySize.y * 0.55f - 10.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Surface Settings  [4]", &showSurfaceWindow_))
    {
        ImGui::End();
        return;
    }
    ImGui::TextDisabled("Shape and motion of the water surface (both versions).");
    DrawSurfaceSettings();
    ImGui::End();
}

// The New water's own settings (test scenes): what comes up from under the water.
void Graphics::DrawNewWaterSettings()
{
    if (SectionHeader("Underwater (New water)"))
    {
        const char* modeLabels[] = { "Pass-through (no effect)", "Attenuation (light fades with the view path)",
            "Scattering (+ the water's own color)" };
        int modeIndex = transmissionMode_ - 1;
        if (ImGui::Combo("Transmission", &modeIndex, modeLabels, IM_ARRAYSIZE(modeLabels)))
        {
            transmissionMode_ = modeIndex + 1;
            lastNewTransmission_ = transmissionMode_;
        }
        ImGui::Checkbox("Surface reflection (sky + sun, Fresnel)", &surfaceReflection_);
        // wave-occlusion: where waves overlap on screen, which water fragment wins.
        const char* occlusionModes[] = { "None (last drawn wins)", "Single pass (writes water depth)", "Depth prepass (nearest, shaded once)" };
        ImGui::Combo("Water occlusion", &waterOcclusion_, occlusionModes, IM_ARRAYSIZE(occlusionModes));
        LabeledSlider("Max underwater distance", &maxUnderwaterDistance_, 5.0f, 200.0f, "%.0f m");
        ImGui::TextDisabled("View path where no surface is behind the water (sky).");
    }

    // refraction: where the water below is looked up. Normal offset = Unreal's Pixel Normal Offset (flat water does
    // not move); Physical = the Snell-bent line of sight, kept to compare (needs the frame below the screen).
    if (SectionHeader("Refraction"))
    {
        const char* refractionModes[] = { "Normal offset (tilt from flat)", "Physical (Snell, compare)" };
        ImGui::Combo("Mode", &water_.refractionMode, refractionModes, IM_ARRAYSIZE(refractionModes));
        if (water_.refractionMode == 0)
        {
            LabeledSlider("Distortion (1 = Snell shift of the tilt)", &water_.refractionDistortion, 0.0f, 3.0f, "%.2f");
            LabeledSlider("Shore fade (water depth)", &water_.refractionShoreFade, 0.0f, 1.0f, "%.2f m");
            LabeledSlider("Wobble depth (same everywhere)", &water_.refractionWobbleDepth, 0.1f, 8.0f, "%.1f m");
            ImGui::TextDisabled("Ripples and waves wobble the view; flat water stays put.");
        }
        else
        {
            LabeledSlider("Refraction strength (1 = Snell)", &water_.refractionStrength, 0.0f, 2.0f, "%.2f");
            ImGui::TextDisabled("Deep beds look shallow; low views reach below the frame edge.");
        }
    }

    // contact-foam: a white band where a surface comes up through the water (not in Pass-through).
    if (SectionHeader("Contact Foam"))
    {
        ImGui::TextDisabled("Band down to Width of water depth on the surface behind.");
        LabeledSlider("Strength (0 = off)", &water_.foamStrength, 0.0f, 1.0f, "%.2f");
        LabeledSlider("Width (water depth)", &water_.foamWidth, 0.02f, 1.0f, "%.2f m");
        LabeledSlider("Edge softness (small = hard edge)", &water_.foamSoftness, 0.01f, 0.5f, "%.2f");
        LabeledSlider("Noise size", &water_.foamNoiseSize, 0.2f, 8.0f, "%.2f m");
        LabeledSlider("Noise drift (downwind)", &water_.foamFlow, 0.0f, 0.5f, "%.2f m/s");
        LabeledSlider("Slope filter (0 = depth alone, 1 = walls only)", &water_.foamSlopeFilter, 0.0f, 1.0f, "%.2f");
        ImGui::TextDisabled(water_.foamSlopeFilter < 0.5f ? "Flat shallows (rail heads, a 0.2 m pad) foam too."
                                                          : "Steep surfaces only.");
        // normal-gbuffer: compare the screen-space estimate with the normal the opaque pass stored.
        const char* normalSources[] = { "Depth reconstructed (4 depth reads + cross)", "Stored scene normal (G-buffer)" };
        ImGui::Combo("Normal source", &normalSource_, normalSources, IM_ARRAYSIZE(normalSources));
        LabeledSlider("Steepness: foam starts", &water_.foamSteepStart, 0.0f, 1.0f, "%.2f");
        LabeledSlider("Steepness: foam full", &water_.foamSteepFull, 0.0f, 1.0f, "%.2f");
        // foam-masks: the mask that breaks up the band, picked like the normal map textures.
        ImGui::TextUnformatted("Foam texture");
        ImGui::SetNextItemWidth(-FLT_MIN);
        water_.foamMap = std::clamp(water_.foamMap, 0, kFoamMapCount - 1);
        if (ImGui::BeginCombo("##foamTexture", kFoamMaps[water_.foamMap].name))
        {
            for (int i = 0; i < kFoamMapCount; ++i)
            {
                if (ImGui::Selectable(kFoamMaps[i].name, i == water_.foamMap))
                {
                    water_.foamMap = i;
                }
            }
            ImGui::EndCombo();
        }
        const FoamMapEntry& foamEntry = kFoamMaps[water_.foamMap];
        ImGui::BeginDisabled(!foamEntry.bubbles);
        LabeledSlider("Bubbles (G: loose bubbles at the band's edge)", &water_.foamBubbles, 0.0f, 1.0f, "%.2f");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!foamEntry.breakup);
        LabeledSlider("Erosion (B: breaks the band into islands)", &water_.foamErosion, 0.0f, 1.0f, "%.2f");
        ImGui::EndDisabled();
        LabeledColor("Foam color (lit by the sun and sky)", &water_.foamColor.x);
    }
}

// The New water's light loss and own color (Light window). The water type buttons set both at once, so they come
// first.
void Graphics::DrawUnderwaterLightSettings()
{
    ImGui::TextDisabled("Water type: light loss + color together");
    for (int type = 0; type < kWaterTypeCount; ++type)
    {
        if (type > 0)
        {
            ImGui::SameLine();
        }
        if (ImGui::Button(kWaterTypeLabels[type]))
        {
            ApplyWaterType(water_, type);
        }
    }

    // rgb-attenuation: read from Transmission = Attenuation on.
    ImGui::SeparatorText("Light loss");
    ImGui::TextDisabled("Per meter of view path through the water.");
    ImGui::TextDisabled("Half distance = where half of that light is left.");
    float* extinction[3] = { &water_.extinction.x, &water_.extinction.y, &water_.extinction.z };
    const char* channelLabels[3] = { "Red fades per meter", "Green fades per meter", "Blue fades per meter" };
    for (int c = 0; c < 3; ++c)
    {
        LabeledSlider(channelLabels[c], extinction[c], 0.0f, 2.0f, "%.3f /m");
        if (*extinction[c] > 0.0f)
        {
            ImGui::TextDisabled("  half left after %.1f m", 0.693f / *extinction[c]);
        }
        else
        {
            ImGui::TextDisabled("  nothing fades");
        }
    }
    if (ImGui::Button("Zero"))
    {
        water_.extinction = {0.0f, 0.0f, 0.0f};
    }
    ImGui::SameLine();
    // Pure water absorption at ~650 / 550 / 450 nm (Pope & Fry 1997), a starting point, not a target.
    if (ImGui::Button("Pure water (ref.)"))
    {
        water_.extinction = {0.35f, 0.06f, 0.01f};
    }

    // water-scatter: what the light loss took is filled with the water's own color (Transmission = Scattering).
    ImGui::SeparatorText("Water color");
    ImGui::TextDisabled("Fills in what the light loss took: deep = this color.");
    const char* colorModes[] = { "Physical (one color)", "Ramp (shallow / mid / deep)" };
    ImGui::Combo("Water color", &water_.waterColorMode, colorModes, IM_ARRAYSIZE(colorModes));
    LabeledSlider("Strength (0 = off, the light loss alone)", &water_.scatterStrength, 0.0f, 3.0f, "%.2f");
    if (water_.waterColorMode == 0)
    {
        LabeledColor("Water color (lit by the sun and sky)", &water_.scatterColor.x);
    }
    else
    {
        LabeledColor("Shallow", &water_.rampShallow.x);
        LabeledColor("Mid", &water_.rampMid.x);
        LabeledColor("Deep", &water_.rampDeep.x);
        LabeledSlider("Mid at (m)", &water_.rampMidDepth, 0.1f, 10.0f, "%.2f m");
        LabeledSlider("Deep at (m)", &water_.rampDeepDepth, 0.2f, 30.0f, "%.2f m");
        bool byPath = water_.rampByPath != 0;
        if (ImGui::Checkbox("Ramp by view path (off = by water depth)", &byPath))
        {
            water_.rampByPath = byPath ? 1 : 0;
        }
        ImGui::TextDisabled(byPath ? "Lower view = deeper color, like the physical mode."
                                   : "Same bed keeps its color from any angle.");
    }
}

// Surface settings both water versions use: ripples, ocean detail, waves.
void Graphics::DrawSurfaceSettings()
{
    // Two textures of small ripples scrolled across the surface; they change the lighting only,
    // the mesh itself is moved by the Waves below.
    if (SectionHeader("Normal Map"))
    {
        ImGui::TextDisabled("Small ripples on the surface (lighting only)");
        LabeledSlider("Ripple strength (bumpiness)", &water_.normalStrength, 0.0f, 3.0f, "%.2f");
        const auto textureCombo = [this](int& index) {
            ImGui::TextUnformatted("Texture");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##texture", normalMapNames_[index].c_str()))
            {
                for (int i = 0; i < static_cast<int>(normalMapNames_.size()); ++i)
                {
                    if (ImGui::Selectable(normalMapNames_[i].c_str(), i == index))
                    {
                        index = i;
                    }
                }
                ImGui::EndCombo();
            }
        };
        if (ImGui::TreeNodeEx("Large ripples (layer A)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID("A");
            textureCombo(normalMapA_);
            ImGui::Checkbox("Align to wind (turn the ripples to run with Wave 1)", &rippleAlignA_);
            LabeledSlider("Normal map scale (repeats per 2 units; higher = smaller)", &water_.normalScale, 0.1f, 5.0f, "%.2f");
            FlowControls(water_.normalScroll1, flowDirectionDeg_[0], cameraRotation_.y);
            ImGui::PopID();
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Small ripples (layer B)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID("B");
            textureCombo(normalMapB_);
            ImGui::Checkbox("Align to wind (turn the ripples to run with Wave 1)", &rippleAlignB_);
            LabeledSlider("Size (times smaller than the large ripples)", &water_.detailScale, 1.0f, 6.0f, "%.1f x");
            FlowControls(water_.normalScroll2, flowDirectionDeg_[1], cameraRotation_.y);
            ImGui::PopID();
            ImGui::TreePop();
        }
    }

    // Near -> far transition of the open ocean (ocean-hero). 0 = off.
    if (SectionHeader("Ocean Detail", false))
    {
        ImGui::TextDisabled("Near -> far blending (0 = off). Debug view 7 shows them.");
        LabeledSlider("Far wave crests (1 = as before; lower = no lattice past the mesh fade)", &water_.farWaveCrests, 0.0f, 1.0f, "%.2f");
        LabeledSlider("Far ripple glitter (ripples too small to see widen the glint)", &water_.rippleRoughness, 0.0f, 2.0f, "%.2f");
        LabeledSlider("Gust patches (rougher / calmer areas)", &water_.gustStrength, 0.0f, 1.0f, "%.2f");
        LabeledSlider("Gust patch size (world units)", &water_.gustScale, 5.0f, 200.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
        LabeledSlider("Horizon haze", &water_.hazeStrength, 0.0f, 1.0f, "%.2f");
        LabeledSlider("Haze distance (world units)", &water_.hazeDistance, 20.0f, 600.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
    }

    // Gerstner waves move the mesh vertices. The presets order them big -> small.
    if (SectionHeader("Waves (geometry)"))
    {
        if (scene_ != NoScene && !labWaves_)
        {
            ImGui::TextColored(kHighlightColor, "Test scene: flat while View > Lab Waves is off.");
        }
        ImGui::TextDisabled("Moving swells that shape the mesh. Arrow = travel\ndirection as seen from the camera (up = away).");
        ImGui::Checkbox("Far waves (lighting only past the mesh fade)", &farWaveNormals_);

        // Simple: six values generate all four waves (WaveMacro.h). Advanced edits stay until one of
        // these moves; whether they still match is recomputed every frame instead of stored.
        if (WavesMatchMacro(waveMacro_, water_.waves))
        {
            ImGui::TextDisabled("4 waves generated from the sliders below.");
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, kHighlightColor);
            ImGui::TextWrapped("Custom - edited in Advanced. Moving any slider here rebuilds all 4 waves.");
            ImGui::PopStyleColor();
        }
        {
            WaveMacro& m = waveMacro_;
            const float windRelativeDeg = ViewRelativeDeg(m.windDeg, cameraRotation_.y);
            ImGui::TextUnformatted("Wind direction (deg, 0 = +X, 90 = +Z)");
            DrawDirectionDial(windRelativeDeg);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", DescribeViewDirection(windRelativeDeg));
            ImGui::SetNextItemWidth(-FLT_MIN);
            bool changed = ImGui::SliderFloat("##windDirection", &m.windDeg, -180.0f, 180.0f, "%.0f");
            changed |= LabeledSlider("Direction spread (deg, 0 = all waves parallel)", &m.spreadDeg, 0.0f, wave_macro::kSpreadMax, "%.0f");
            changed |= LabeledSlider("Wave size (main wave, crest to crest)", &m.size, wave_macro::kSizeMin, wave_macro::kSizeMax, "%.2f");
            changed |= LabeledSlider("Height (main wave; smaller ones follow)", &m.height, 0.0f, wave_macro::kHeightMax, "%.3f");
            changed |= LabeledSlider("Choppiness (0 = round, higher = pointed crests)", &m.chop, 0.0f, wave_macro::kChopMax, "%.2f");
            changed |= LabeledSlider("Speed (x; longer waves already move faster)", &m.speedScale, 0.0f, wave_macro::kSpeedMax, "%.2f");
            if (changed)
            {
                GenerateWaves(m, water_.waves);
            }
        }

        // Advanced: the four waves the shader actually gets, one by one (presets order them big -> small).
        // No extra indent, so the wave rows keep room for the direction text.
        if (ImGui::TreeNodeEx("Advanced - individual waves", ImGuiTreeNodeFlags_NoTreePushOnOpen))
        {
            const char* waveNames[ColorShader::kWaveCount] = { "Wave 1 - Big swell", "Wave 2 - Medium swell", "Wave 3 - Small waves", "Wave 4 - Ripples" };
            for (int i = 0; i < ColorShader::kWaveCount; ++i)
            {
                auto& w = water_.waves[i];
                float angleDeg = DirectX::XMConvertToDegrees(std::atan2(w.direction.y, w.direction.x));
                const float relativeDeg = ViewRelativeDeg(angleDeg, cameraRotation_.y);

                ImGui::PushID(i);
                ImGui::AlignTextToFramePadding();
                const bool open = ImGui::TreeNode("##wave", "%s", waveNames[i]);
                ImGui::SameLine(185.0f);
                DrawDirectionDial(relativeDeg);
                ImGui::SameLine();
                ImGui::TextDisabled("%s", DescribeViewDirection(relativeDeg));
                if (open)
                {
                    if (LabeledSlider("Direction (deg, 0 = +X, 90 = +Z)", &angleDeg, -180.0f, 180.0f, "%.0f"))
                    {
                        const float r = DirectX::XMConvertToRadians(angleDeg);
                        w.direction = { std::cos(r), std::sin(r) };
                    }
                    LabeledSlider("Height", &w.amplitude, 0.0f, 0.3f, "%.3f");
                    LabeledSlider("Length (crest to crest)", &w.wavelength, 0.2f, 8.0f, "%.2f");
                    LabeledSlider("Speed", &w.speed, 0.0f, 3.0f, "%.2f");
                    LabeledSlider("Sharpness (0 = round, 1 = pointed crests)", &w.steepness, 0.0f, 1.0f, "%.2f");
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
    }
}

