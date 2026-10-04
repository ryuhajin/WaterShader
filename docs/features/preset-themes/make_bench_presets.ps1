# Rewrites only the "bench" lines of assets/shader_presets.txt (v4). Each bench preset starts from the same
# theme's ocean line as saved in the app (sky, lights, colors, normal maps) and overrides what the 2x2 bench
# plane needs: small waves that keep the plane square, finer normal tiling, and a Top View Sun (top-view-sun).
# Ocean lines are copied through unchanged. Waves follow GenerateWaves (src/WaveMacro.h), so presets load as "Simple".
# Usage: powershell -ExecutionPolicy Bypass -File make_bench_presets.ps1
param([string]$File = (Join-Path $PSScriptRoot "../../../assets/shader_presets.txt"))

# Per theme (file order Basic, Sunset, Tropical). Keys: fixed fields by name, extra keys as saved, wave = Simple
# values (missing ones come from the ocean line). Colors are sRGB.
$bench = @(
    @{ # Basic: calm, flowing
        # Top shot: sun right above the plane so its glint faces the camera (the other shots keep the ocean sun)
        topSun = 1; topSunYawDeg = 213.3; topSunElevationDeg = 90; topSunIntensity = 1.6
        normalScale = 1.4; normalStrength = 0.55
        wave = @{ spread = 25; size = 1.0; height = 0.012; chop = 0.20; speed = 0.6 }
    },
    @{ # Sunset: rough. Water color, light and normal maps as saved for the ocean (the top shot reads through
        # its own sun now, so the mauve / strong-sun / fine-normal workaround of step7 is gone).
        topSun = 1; topSunYawDeg = 34.5; topSunElevationDeg = 90; topSunIntensity = 1.0
        wave = @{ size = 0.8; height = 0.025; chop = 0.45 }
    },
    @{ # Tropical: a bit bigger than Basic
        facing = 0.10, 0.62, 0.50; exposureEV = -0.8
        topSun = 1; topSunYawDeg = 35.2; topSunElevationDeg = 90; topSunIntensity = 1.74
        normalScale = 1.5; normalStrength = 1.0
        wave = @{ size = 1.2; height = 0.016; chop = 0.30 }
    }
)

$inv = [Globalization.CultureInfo]::InvariantCulture
function F([double]$v) { $r = [Math]::Round($v, 6); if ([Math]::Abs($r) -lt 1e-9) { $r = 0 }; $r.ToString("G6", $inv) }

# Fixed fields after the mesh name (Graphics.cpp LoadPresets); 3-wide entries are rgb.
$fixedIndex = @{ sunYaw = 0; sunElev = 1; light = 2; lightI = 5; amb = 6; ambI = 9; facing = 10; grazing = 13
                 refl = 16; fresnelPow = 17; normalScale = 18 }
$waveStart = 25; $waveEnd = 49 # 4 waves x (dir.x dir.y amplitude wavelength speed steepness)

$lenR   = 1.0, 0.65625, 0.3875, 0.25625
$ampR   = 1.0, 0.6, (1.0 / 3.0), 0.2
$sprF   = 0.0, (-7.0 / 12.0), (7.0 / 12.0), -1.0
$steepS = 0.0, 0.05, 0.10, 0.15
function Waves($m) {
    $vals = @()
    for ($i = 0; $i -lt 4; $i++) {
        $a = ($m.wind + $sprF[$i] * $m.spread) * [Math]::PI / 180
        $len = $m.size * $lenR[$i]
        $vals += [Math]::Cos($a), [Math]::Sin($a), ($m.height * $ampR[$i]), $len,
                 ($m.speed * 0.55 * [Math]::Sqrt($len / 1.6)), [Math]::Min(1.0, [Math]::Max(0.0, $m.chop + $steepS[$i]))
    }
    $vals
}

$lines = [IO.File]::ReadAllLines((Resolve-Path $File))
if ($lines[0].Trim() -ne "WaterShaderPresets 4") { throw "expected a v4 preset file: $($lines[0])" }
$ocean = @($lines | Where-Object { $_ -like "ocean *" })
if ($ocean.Count -ne 3) { throw "expected 3 ocean lines, found $($ocean.Count)" }

$out = @($lines[0])
for ($t = 0; $t -lt 3; $t++) {
    $tok = $ocean[$t].Split(" ", [StringSplitOptions]::RemoveEmptyEntries)
    $fixed = @($tok[1..$waveStart])            # 25 fixed fields
    $kv = [ordered]@{}
    for ($i = $waveEnd + 1; $i + 1 -lt $tok.Count; $i += 2) { $kv[$tok[$i]] = $tok[$i + 1] }
    $o = $bench[$t]

    foreach ($name in $o.Keys) {
        $v = $o[$name]
        if ($name -eq "wave") { continue }
        if ($fixedIndex.ContainsKey($name)) {
            $vals = @($v); for ($j = 0; $j -lt $vals.Count; $j++) { $fixed[$fixedIndex[$name] + $j] = F $vals[$j] }
        } else {
            $kv[$name] = F $v
        }
    }

    $m = @{ wind = [double]$kv["waveWindDeg"]; spread = [double]$kv["waveSpreadDeg"]; size = [double]$kv["waveSize"]
            height = [double]$kv["waveHeight"]; chop = [double]$kv["waveChop"]; speed = [double]$kv["waveSpeed"] }
    foreach ($k in $o.wave.Keys) { $m[$k] = [double]$o.wave[$k] }
    $kv["waveWindDeg"] = F $m.wind; $kv["waveSpreadDeg"] = F $m.spread; $kv["waveSize"] = F $m.size
    $kv["waveHeight"] = F $m.height; $kv["waveChop"] = F $m.chop; $kv["waveSpeed"] = F $m.speed

    $s = "bench " + ($fixed -join " ") + " " + ((Waves $m | ForEach-Object { F $_ }) -join " ")
    foreach ($k in $kv.Keys) { $s += " $k $($kv[$k])" }
    $out += $s
}
$out += $ocean

[IO.File]::WriteAllText((Resolve-Path $File), ($out -join "`r`n") + "`r`n", (New-Object Text.UTF8Encoding($false)))
$out | ForEach-Object { $_.Substring(0, [Math]::Min(110, $_.Length)) }
