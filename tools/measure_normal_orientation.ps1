# Dominant ripple axis of tangent-space normal maps, for rotating them to follow the wind.
#
# Usage: powershell -File tools/measure_normal_orientation.ps1 assets/textures/water_normal.dds ...
#   .dds files are converted to PNG first with assets/textures/texconv.exe (what the renderer loads),
#   other formats are read directly. Images are box-downsampled to 512^2 for speed.
#
# Two estimates, both as an axis angle in world terms (0 = +X, 90 = +Z, mod 180) for the water mesh
# mapping (bench plane and ocean grid both run u = +X, v = +Z; PixelShader TBN: normal.x -> +X,
# normal.y -> +Z):
#   slope   - structure tensor of the stored slopes (n.xy): the axis the lighting tilts along.
#   pattern - structure tensor of the spatial gradients of n.x / n.y: the axis across the visible stripes.
# For a consistent normal map the two agree; that axis is the ripples' travel axis.
# Anisotropy 0 = no preferred direction (rotation makes no visible difference), 1 = pure stripes.

param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Files)

Add-Type -AssemblyName System.Drawing
$repo = Split-Path $PSScriptRoot
$texconv = Join-Path $repo "assets\textures\texconv.exe"
$tmp = Join-Path ([IO.Path]::GetTempPath()) "measure_normal_orientation"
New-Item -ItemType Directory -Force $tmp | Out-Null

function AxisOf($xx, $xy, $yy)
{
    $angle = 0.5 * [Math]::Atan2(2.0 * $xy, $xx - $yy) * 180.0 / [Math]::PI
    $anisotropy = [Math]::Sqrt(($xx - $yy) * ($xx - $yy) + 4.0 * $xy * $xy) / [Math]::Max($xx + $yy, 1e-12)
    return @{ Angle = $angle; Anisotropy = $anisotropy }
}

foreach ($file in $Files)
{
    $path = (Resolve-Path $file).Path
    if ([IO.Path]::GetExtension($path) -eq ".dds")
    {
        & $texconv -nologo -y -ft png -f R8G8B8A8_UNORM -o $tmp $path | Out-Null
        $path = Join-Path $tmp ([IO.Path]::GetFileNameWithoutExtension($path) + ".png")
    }

    $src = [System.Drawing.Image]::FromFile($path)
    $n = 512
    $bmp = New-Object System.Drawing.Bitmap $n, $n, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBilinear
    $g.DrawImage($src, 0, 0, $n, $n)
    $g.Dispose(); $src.Dispose()

    $rect = New-Object System.Drawing.Rectangle 0, 0, $n, $n
    $data = $bmp.LockBits($rect, 'ReadOnly', 'Format24bppRgb')
    $bytes = New-Object byte[] ($data.Stride * $n)
    [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
    $stride = $data.Stride
    $bmp.UnlockBits($data); $bmp.Dispose()

    # Decoded slopes in world terms: x -> +X, y -> +Z (BGR byte order).
    $sx = New-Object double[] ($n * $n)
    $sz = New-Object double[] ($n * $n)
    for ($y = 0; $y -lt $n; $y++)
    {
        for ($x = 0; $x -lt $n; $x++)
        {
            $o = $y * $stride + $x * 3
            $sx[$y * $n + $x] = $bytes[$o + 2] / 127.5 - 1.0
            $sz[$y * $n + $x] = $bytes[$o + 1] / 127.5 - 1.0
        }
    }

    $mx = ($sx | Measure-Object -Average).Average
    $mz = ($sz | Measure-Object -Average).Average
    $sxx = 0.0; $sxz = 0.0; $szz = 0.0   # slope tensor (mean removed)
    $pxx = 0.0; $pxz = 0.0; $pzz = 0.0   # pattern tensor (spatial gradients, world X/Z)
    for ($y = 0; $y -lt $n; $y++)
    {
        for ($x = 0; $x -lt $n; $x++)
        {
            $i = $y * $n + $x
            $a = $sx[$i] - $mx; $b = $sz[$i] - $mz
            $sxx += $a * $a; $sxz += $a * $b; $szz += $b * $b

            # Central differences, wrapping (the maps tile). Image +x = +X, image +y (down) = +v = +Z.
            $xr = ($x + 1) % $n; $xl = ($x + $n - 1) % $n; $yd = ($y + 1) % $n; $yu = ($y + $n - 1) % $n
            foreach ($ch in @($sx, $sz))
            {
                $gX = 0.5 * ($ch[$y * $n + $xr] - $ch[$y * $n + $xl])
                $gZ = 0.5 * ($ch[$yd * $n + $x] - $ch[$yu * $n + $x])
                $pxx += $gX * $gX; $pxz += $gX * $gZ; $pzz += $gZ * $gZ
            }
        }
    }

    $slope = AxisOf $sxx $sxz $szz
    $pattern = AxisOf $pxx $pxz $pzz
    "{0,-22} slope axis {1,7:N1} deg (aniso {2:N2})   pattern axis {3,7:N1} deg (aniso {4:N2})" -f `
        [IO.Path]::GetFileName($file), $slope.Angle, $slope.Anisotropy, $pattern.Angle, $pattern.Anisotropy
}
