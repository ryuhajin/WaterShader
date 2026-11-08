# Bakes the contact foam masks (foam-masks) and turns them into the textures the app loads.
#   1. WaterShader.exe --bake-foam-masks <assets/textures>: a compute shader (shaders/foam_bake.hlsl) writes
#      foam_cells / foam_bubbles / foam_packed / foam_streaks .png (512 x 512, RGBA8, same seed = same bytes)
#   2. texconv: each PNG -> .dds with a full mip chain, BC7 (what the app reads, like the normal maps)
#   3. seam check: how much the last column / row differs from the first (the texture wrapping around),
#      ranked among the neighbouring column / row pairs inside. A seam sticks out above every inside pair;
#      thin cell borders make single pairs vary a lot, so a high rank alone is not a seam.
#      -PreviewDir also writes each mask tiled 2 x 2 (R, G, B side by side) to look at.
#
# Usage (PowerShell 5.1), from the repository root:
#   .\tools\bake_foam_masks.ps1 [-Exe .\build\vs2022\Release\WaterShader.exe] [-PreviewDir <folder>]
param(
    [string]$Exe = '',
    [string]$PreviewDir = ''
)

$repo = Split-Path -Parent $PSScriptRoot
if (-not $Exe) { $Exe = Join-Path $repo 'build\vs2022\Release\WaterShader.exe' }
$textures = Join-Path $repo 'assets\textures'
$texconv = Join-Path $textures 'texconv.exe'
$names = 'foam_cells', 'foam_bubbles', 'foam_packed', 'foam_streaks'

# 1. Bake (the app closes by itself once the PNGs are written).
$process = Start-Process -FilePath $Exe -ArgumentList @('--bake-foam-masks', "`"$textures`"") -PassThru -Wait
$log = Join-Path $textures 'bake_log.txt'
$logText = if (Test-Path $log) { Get-Content $log -Raw } else { 'no bake_log.txt' }
Remove-Item $log -ErrorAction SilentlyContinue
if ($logText -notmatch '^baked') { Write-Output "Bake failed: $logText"; exit 1 }
Write-Output ("Baked with {0} (exit {1})" -f $Exe, $process.ExitCode)

# 2. PNG -> DDS: BC7 with mips; --ignore-srgb keeps the stored values as they are (masks are data, not colors).
foreach ($name in $names) {
    & $texconv -nologo -y -m 0 -f BC7_UNORM --ignore-srgb -o $textures (Join-Path $textures "$name.png") | Out-Null
    if (-not (Test-Path (Join-Path $textures "$name.dds"))) { Write-Output "texconv failed for $name"; exit 1 }
}

# 3. Seam check (and optional 2 x 2 previews).
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class FoamSeam
{
    static byte[] Load(string path, out int w, out int h)
    {
        using (var bmp = new Bitmap(path))
        {
            w = bmp.Width; h = bmp.Height;
            var d = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var bytes = new byte[d.Stride * h];
            Marshal.Copy(d.Scan0, bytes, 0, bytes.Length);
            bmp.UnlockBits(d);
            return bytes; // BGRA
        }
    }

    // Mean difference of each pair of neighbouring columns (or rows); the last pair wraps around to the first.
    static double[] PairMeans(byte[] p, int w, int h, int channel, bool columns)
    {
        int n = columns ? w : h, along = columns ? h : w;
        var means = new double[n];
        for (int i = 0; i < n; ++i)
        {
            int j = (i + 1) % n;
            double sum = 0;
            for (int k = 0; k < along; ++k)
            {
                int a = columns ? p[(k * w + i) * 4 + channel] : p[(i * w + k) * 4 + channel];
                int b = columns ? p[(k * w + j) * 4 + channel] : p[(j * w + k) * 4 + channel];
                sum += Math.Abs(a - b);
            }
            means[i] = sum / along;
        }
        return means;
    }

    // The wrap pair against the inside pairs: a seam would stick out above all of them.
    static string Rank(double[] means)
    {
        int n = means.Length;
        double wrap = means[n - 1];
        var inside = new double[n - 1];
        Array.Copy(means, inside, n - 1);
        Array.Sort(inside);
        int below = 0;
        foreach (double v in inside) if (v < wrap) ++below;
        string verdict = wrap > inside[n - 2] ? "SEAM?" : "ok";
        return string.Format("wrap {0,5:F2} (median {1,5:F2}, max {2,5:F2}, above {3,3:P0}) {4}",
            wrap, inside[(n - 1) / 2], inside[n - 2], below / (double)(n - 1), verdict);
    }

    // channel: 2 = R, 1 = G, 0 = B (BGRA order)
    public static string Check(string path, int channel)
    {
        int w, h;
        byte[] p = Load(path, out w, out h);
        return "columns " + Rank(PairMeans(p, w, h, channel, true)) + "   rows " + Rank(PairMeans(p, w, h, channel, false));
    }

    public static void Preview(string path, string outPath)
    {
        int w, h;
        byte[] p = Load(path, out w, out h);
        using (var bmp = new Bitmap(w * 3, h, PixelFormat.Format24bppRgb))
        {
            for (int c = 0; c < 3; ++c)
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                    {
                        int sx = (x * 2) % w, sy = (y * 2) % h;
                        int v = p[(sy * w + sx) * 4 + (2 - c)];
                        bmp.SetPixel(c * w + x, y, Color.FromArgb(v, v, v));
                    }
            bmp.Save(outPath, ImageFormat.Png);
        }
    }
}
'@

if ($PreviewDir) { New-Item -ItemType Directory -Force $PreviewDir | Out-Null }
Write-Output 'Seam check (mean 8-bit difference of the wrap pair, among the inside pairs)'
foreach ($name in $names) {
    $png = Join-Path $textures "$name.png"
    foreach ($channel in @(@(2, 'R'), @(1, 'G'), @(0, 'B'))) {
        Write-Output ("{0,-14} {1}  {2}" -f $name, $channel[1], [FoamSeam]::Check($png, $channel[0]))
    }
    if ($PreviewDir) { [FoamSeam]::Preview($png, (Join-Path $PreviewDir "${name}_tiled_RGB.png")) }
}
