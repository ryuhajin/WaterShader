# Builds side-by-side comparison sheets from capture sets.
#   rows = presets, columns = capture steps (in the order given)
# Usage: powershell -File make_compare.ps1 before step1_bugfix step2_sun_glint
#        powershell -File make_compare.ps1 -Dir ../bench-tools before env final   (another feature folder)
#        ... -Out compare_operators ...                                          (output folder name)
[CmdletBinding(PositionalBinding = $false)] # -Dir must be named; bare words are step names
param(
    [string]$Dir = $PSScriptRoot,
    [string]$Out = "compare",
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$Steps
)

Add-Type -AssemblyName System.Drawing

if (-not [IO.Path]::IsPathRooted($Dir)) { $Dir = Join-Path $PSScriptRoot $Dir }
$Dir = (Resolve-Path $Dir).Path
$root = Join-Path $Dir "captures"
$outDir = Join-Path $Dir $Out
New-Item -ItemType Directory -Force $outDir | Out-Null

$presets = @("basic", "sunset", "tropical")
# A feature that only touches one preset keeps only that preset's captures; drop the empty rows.
$presets = @($presets | Where-Object { $p = $_; $Steps | Where-Object { Test-Path (Join-Path $root "$_\$($p)_oblique.jpg") } })
$shots = @("oblique", "top", "sunward", "ocean_sunward", "ocean_wide")
$cellW = 480; $cellH = 270; $labelH = 28

$jpeg = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq "image/jpeg" }
$encParams = New-Object System.Drawing.Imaging.EncoderParameters(1)
$encParams.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, [long]90)

foreach ($shot in $shots) {
    # Only keep steps that captured this shot (ocean_* shots exist from step 6 on).
    $shotSteps = @($Steps | Where-Object { Test-Path (Join-Path $root "$_\$($presets[0])_$shot.jpg") })
    if ($shotSteps.Count -eq 0) { continue }

    $sheet = New-Object System.Drawing.Bitmap ($cellW * $shotSteps.Count), ($labelH + $cellH * $presets.Count)
    $g = [System.Drawing.Graphics]::FromImage($sheet)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.Clear([System.Drawing.Color]::FromArgb(24, 24, 24))
    $font = New-Object System.Drawing.Font("Segoe UI", 12, [System.Drawing.FontStyle]::Bold)

    for ($c = 0; $c -lt $shotSteps.Count; $c++) {
        $g.DrawString($shotSteps[$c], $font, [System.Drawing.Brushes]::White, ($c * $cellW + 8), 4)
        for ($r = 0; $r -lt $presets.Count; $r++) {
            $path = Join-Path $root "$($shotSteps[$c])\$($presets[$r])_$shot.jpg"
            if (-not (Test-Path $path)) { continue }
            $img = [System.Drawing.Image]::FromFile($path)
            $g.DrawImage($img, ($c * $cellW), ($labelH + $r * $cellH), $cellW, $cellH)
            $img.Dispose()
        }
    }

    $out = Join-Path $outDir "$shot.jpg"
    $sheet.Save($out, $jpeg, $encParams)
    $g.Dispose(); $sheet.Dispose()
    Write-Output "saved $out"
}
