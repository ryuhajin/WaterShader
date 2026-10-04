# Writes assets/shader_presets.txt (v3) from the theme tables below. Waves follow GenerateWaves (src/WaveMacro.h),
# so every preset loads as "Simple" (not Custom). Usage: powershell -ExecutionPolicy Bypass -File make_presets.ps1
param([string]$Out = (Join-Path $PSScriptRoot "../../../assets/shader_presets.txt"))

# Theme values (colors sRGB). flow = (UI direction deg, speed)
$basic = @{
    sunYaw = 40; sunElev = 30; light = 1, 0.97, 0.91; lightI = 1.6; amb = 0.78, 0.88, 1; ambI = 0.95
    facing = 0.18, 0.40, 0.50; grazing = 0.05, 0.15, 0.24; refl = 0.7; fresnelPow = 5; f0 = 0.04
    glint = 1200, 1.2, 0.8; env = 6; ev = -0.4
    normalScale = 0.8; detail = 2.5; normalStrength = 0.45; maps = 3, 1; align = 1, 0
    flowA = -105, 0.020; flowB = -75, 0.012
    wave = @{ wind = -105; spread = 25; size = 3.6; height = 0.05; chop = 0.30; speed = 0.6 }
}
$sunset = @{
    sunYaw = 34.5; sunElev = 4; light = 1, 0.50, 0.22; lightI = 1.0; amb = 0.93, 0.85, 1; ambI = 0.9
    facing = 0.10, 0.12, 0.18; grazing = 0.04, 0.05, 0.09; refl = 1.0; fresnelPow = 5; f0 = 0.02
    glint = 300, 1.0, 1.6; env = 4; ev = -1.0
    normalScale = 1.3; detail = 3.2; normalStrength = 0.9; maps = 0, 4; align = 1, 0
    flowA = -125, 0.050; flowB = -100, 0.035
    wave = @{ wind = -125; spread = 40; size = 3.0; height = 0.12; chop = 0.75; speed = 1.35 }
}
$tropical = @{
    sunYaw = 236.4; sunElev = 25.2; light = 0.98, 1, 0.92; lightI = 1.74; amb = 0.586, 0.753, 1; ambI = 0.60
    facing = 0.16, 0.82, 0.64; grazing = 0.03, 0.50, 0.50; refl = 0.8; fresnelPow = 5; f0 = 0.02
    glint = 900, 1.0, 1.0; env = 5; ev = -0.5
    normalScale = 1.0; detail = 3.0; normalStrength = 0.7; maps = 2, 4; align = 0, 0
    flowA = -80, 0.025; flowB = -50, 0.018
    wave = @{ wind = -80; spread = 35; size = 4.8; height = 0.10; chop = 0.50; speed = 0.85 }
}

$inv = [Globalization.CultureInfo]::InvariantCulture
function F([double]$v) { $r = [Math]::Round($v, 6); if ([Math]::Abs($r) -lt 1e-9) { $r = 0 }; $r.ToString("G6", $inv) }

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

# UI flow direction (deg) + speed -> stored UV velocity (Graphics.cpp FlowControls)
function Flow([double]$deg, [double]$speed) {
    $r = $deg * [Math]::PI / 180
    @((-[Math]::Cos($r) * $speed), (-[Math]::Sin($r) * $speed))
}

function Line($p) {
    $fixed = @($p.sunYaw, $p.sunElev) + $p.light + @($p.lightI) + $p.amb + @($p.ambI) + $p.facing + $p.grazing +
             @($p.refl, $p.fresnelPow, $p.normalScale, 0, 128) + (Flow $p.flowA[0] $p.flowA[1]) + (Flow $p.flowB[0] $p.flowB[1])
    $m = $p.wave
    $all = $fixed + (Waves $m)
    $s = ($all | ForEach-Object { F $_ }) -join " "
    $kv = [ordered]@{
        sunGlintPower = $p.glint[0]; sunGlintIntensity = $p.glint[1]; farGlintSpread = $p.glint[2]
        fresnelF0 = $p.f0; normalStrength = $p.normalStrength; detailScale = $p.detail
        environment = $p.env; exposureEV = $p.ev
        normalMapA = $p.maps[0]; normalMapB = $p.maps[1]; rippleAlignA = $p.align[0]; rippleAlignB = $p.align[1]
        waveWindDeg = $m.wind; waveSpreadDeg = $m.spread; waveSize = $m.size; waveHeight = $m.height; waveChop = $m.chop; waveSpeed = $m.speed
    }
    foreach ($k in $kv.Keys) { $s += " $k $(F $kv[$k])" }
    $s
}


$lines = @("WaterShaderPresets 3", (Line $basic), (Line $sunset), (Line $tropical))
[IO.File]::WriteAllText($Out, ($lines -join "`r`n") + "`r`n", (New-Object Text.UTF8Encoding($false)))
Get-Content $Out
