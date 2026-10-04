# Renders the ocean-hero comparison: wave variants (columns of make_compare) x shots.
#   Each variant is a copy of assets/shader_presets.txt whose ocean Basic line has the variant's changes;
#   the other presets and the bench lines are left as they are.
#   Captures go to captures/<variant>/basic_<shot>.jpg, comparison sheets (one per shot) to compare/.
#   -Cameras <file> + -Shots slot1,... renders camera slots of that file instead (framing candidates).
# Usage: powershell -File make_samples.ps1                 (all variants)
#        powershell -File make_samples.ps1 -Only w2,w3      (some of them)
#        powershell -File make_samples.ps1 -Exe ..\..\..\build\vs2022\Debug\WaterShader.exe
[CmdletBinding()]
param(
    [string[]]$Only = @(),
    [string]$Exe = "",     # default: build\vs2022\Debug\WaterShader.exe (Debug reads the source shaders)
    [string]$Cameras = "", # camera slot file for slot1..slot4 shots (e.g. cameras_candidates.txt)
    [string]$Shots = "ocean_aerial,ocean_hero",
    [string[]]$Extra = @() # more WaterShader arguments, e.g. -Extra --render-size,2560x1440,--capture-format,png
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
# Windows PowerShell 5.1 has no $PSScriptRoot yet while the param defaults are evaluated.
if (-not $Exe) { $Exe = Join-Path $root "build\vs2022\Debug\WaterShader.exe" }
$inv = [Globalization.CultureInfo]::InvariantCulture

# ---- Variants: changes on top of the ocean Basic preset -------------------------------------------------
# macro = Simple wave controls (WaveMacro.h); the 4 waves are regenerated from them like GenerateWaves.
# scrollA / scrollB = normal map drift: direction the pattern moves (deg, 0 = +X, 90 = +Z) and speed.
# keys = preset "key value" fields (normal maps, ripple strength, ocean detail, ...).
$variants = [ordered]@{
    # Current Basic: the wind runs along the view of the aerial framings, so the crests lie flat across the
    # frame as regular dark dashes, and the far field turns into a sharp mirror.
    w0 = @{ }
    # Basic + ocean detail only (shader): far ripple glitter, gust patches, horizon haze.
    w1 = @{
        keys = @{ rippleRoughness = 0.3; gustStrength = 0.4; gustScale = 15; hazeStrength = 0.35; hazeDistance = 150 }
    }
    # w1 + wind turned 20 deg off the view, a little less chop, and fewer far crests: the rows run at an
    # angle, and the lattice the small waves form past the mesh fade (radial streaks at this angle) fades
    # into glint roughness. The wave height (and so the crisp near glint) stays Basic's.
    w2 = @{
        macro   = @{ windDeg = -125; chop = 0.2 }
        scrollA = @(-125, 0.02); scrollB = @(-95, 0.012)
        keys    = @{ rippleRoughness = 0.3; gustStrength = 0.4; gustScale = 15; hazeStrength = 0.35; hazeDistance = 150; farWaveCrests = 0.6 }
    }
    # w2, calmer far field: even fewer far crests with a narrower far glint (so it does not bloom), a little
    # more spread and longer swell, stronger gusts / haze.
    # (Spread past ~35 deg turns the four waves into a diamond lattice - wave-far-normals NOTES.)
    w3 = @{
        macro   = @{ windDeg = -125; spreadDeg = 32; size = 4.2; chop = 0.2 }
        scrollA = @(-125, 0.02); scrollB = @(-95, 0.012)
        keys    = @{ rippleRoughness = 0.3; gustStrength = 0.55; gustScale = 15; hazeStrength = 0.45; hazeDistance = 150; farWaveCrests = 0.35; farGlintSpread = 0.35 }
    }
}

# ---- WaveMacro.h ------------------------------------------------------------------------------------------
$lengthRatio = @(1.0, 0.65625, 0.3875, 0.25625)
$heightRatio = @(1.0, 0.6, (1.0 / 3.0), 0.2)
$spreadFactor = @(0.0, (-7.0 / 12.0), (7.0 / 12.0), -1.0)
$steepnessStep = @(0.0, 0.05, 0.10, 0.15)
function Get-Waves($m) {
    $waves = @()
    for ($i = 0; $i -lt 4; $i++) {
        $a = ($m.windDeg + $spreadFactor[$i] * $m.spreadDeg) * [Math]::PI / 180.0
        $wl = $m.size * $lengthRatio[$i]
        $waves += [Math]::Cos($a), [Math]::Sin($a), ($m.height * $heightRatio[$i]), $wl,
                  ($m.speedScale * 0.55 * [Math]::Sqrt($wl / 1.6)), [Math]::Min([Math]::Max($m.chop + $steepnessStep[$i], 0.0), 1.0)
    }
    return $waves
}

function Fmt($v) { return ([double]$v).ToString("0.######", $inv) }

# ---- Build one preset file per variant and capture it ------------------------------------------------------
$lines = Get-Content (Join-Path $root "assets\shader_presets.txt")
$bench = @($lines | Where-Object { $_ -like "bench *" })
$ocean = @($lines | Where-Object { $_ -like "ocean *" })
$basic = $ocean[0] -split ' '
$kvStart = 50 # mesh name + 25 fixed fields + 4 waves x 6

$presetDir = Join-Path $PSScriptRoot "samples"
New-Item -ItemType Directory -Force $presetDir | Out-Null

foreach ($name in $variants.Keys) {
    if ($Only.Count -gt 0 -and $Only -notcontains $name) { continue }
    $v = $variants[$name]
    $t = [string[]]$basic.Clone()

    # Key/value tail as an ordered map so changed keys keep their place and new ones are appended.
    $kv = [ordered]@{}
    for ($i = $kvStart; $i + 1 -lt $t.Count; $i += 2) { $kv[$t[$i]] = $t[$i + 1] }

    $macro = @{ windDeg = [double]$kv.waveWindDeg; spreadDeg = [double]$kv.waveSpreadDeg; size = [double]$kv.waveSize
                height = [double]$kv.waveHeight; chop = [double]$kv.waveChop; speedScale = [double]$kv.waveSpeed }
    if ($v.macro) {
        foreach ($k in $v.macro.Keys) { $macro[$k] = [double]$v.macro[$k] }
        $waves = Get-Waves $macro
        for ($i = 0; $i -lt 24; $i++) { $t[26 + $i] = Fmt $waves[$i] }
        $kv.waveWindDeg = Fmt $macro.windDeg; $kv.waveSpreadDeg = Fmt $macro.spreadDeg; $kv.waveSize = Fmt $macro.size
        $kv.waveHeight = Fmt $macro.height; $kv.waveChop = Fmt $macro.chop; $kv.waveSpeed = Fmt $macro.speedScale
    }
    # Scroll is in plane uv; the pattern drifts along -scroll (see FlowControls), so scroll = -speed * dir.
    foreach ($layer in @(@("scrollA", 22), @("scrollB", 24))) {
        $s = $v[$layer[0]]
        if (-not $s) { continue }
        $r = $s[0] * [Math]::PI / 180.0
        $t[$layer[1]] = Fmt (-$s[1] * [Math]::Cos($r)); $t[$layer[1] + 1] = Fmt (-$s[1] * [Math]::Sin($r))
    }
    if ($v.keys) { foreach ($k in $v.keys.Keys) { $kv[$k] = Fmt $v.keys[$k] } }

    $variantLine = ($t[0..($kvStart - 1)] -join ' ') + ' ' + (($kv.Keys | ForEach-Object { "$_ $($kv[$_])" }) -join ' ')
    $file = Join-Path $presetDir "$name.txt"
    $content = @($lines[0]) + $bench + $variantLine + $ocean[1..($ocean.Count - 1)]
    [IO.File]::WriteAllLines($file, [string[]]$content)

    $exePath = (Resolve-Path $Exe).Path
    $arguments = @("--preset-file", "`"$file`"", "--capture", $name, "--capture-feature", "ocean-hero",
                   "--capture-presets", "basic", "--capture-shots", $Shots) + $Extra
    if ($Cameras) { $arguments += @("--camera-file", "`"$((Resolve-Path $Cameras).Path)`"") }
    $proc = Start-Process $exePath -ArgumentList $arguments -WorkingDirectory (Split-Path $exePath) -PassThru
    if (-not $proc.WaitForExit(240000)) { $proc.Kill(); throw "capture $name timed out" }
    Write-Output "captured $name"
}

$steps = [string[]]@($variants.Keys)
& (Join-Path $root "docs\features\water-polish\make_compare.ps1") -Dir $PSScriptRoot @steps
