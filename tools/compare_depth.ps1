# Compares the "meters as color" PNG captures of depth-reconstruct (DepthBands.hlsli, EncodeMeters):
#   R = fraction of a meter, G = whole meters x 4, B = marker (128 measured, 64 above water, 255 no surface).
#
#   -Mode depth : -A = answer key   (--scene-debug 2 --water off), -B = reconstruction (--debug 12)
#                 every measured answer-key pixel must be measured in B too, within max(1 cm, 1 %).
#   -Mode path  : -A = water depth  (--debug 12),                  -B = view path      (--debug 13)
#                 the view path must never be shorter than the water depth (1 cm slack).
#
# Usage (PowerShell 5.1):
#   .\tools\compare_depth.ps1 -A <folder> -B <folder> [-Mode depth|path]
# Pairs the PNG files with the same name in both folders.
param(
    [Parameter(Mandatory = $true)][string]$A,
    [Parameter(Mandatory = $true)][string]$B,
    [ValidateSet('depth', 'path')][string]$Mode = 'depth'
)

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class MeterImage
{
    // marker[i] = B byte, meters[i] = decoded R/G.
    public static void Load(string path, out byte[] marker, out double[] meters, out int width, out int height)
    {
        using (var bmp = new Bitmap(path))
        {
            width = bmp.Width;
            height = bmp.Height;
            var rect = new Rectangle(0, 0, width, height);
            var data = bmp.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var bytes = new byte[data.Stride * height];
            Marshal.Copy(data.Scan0, bytes, 0, bytes.Length);
            bmp.UnlockBits(data);
            marker = new byte[width * height];
            meters = new double[width * height];
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    int o = y * data.Stride + x * 4; // BGRA
                    int i = y * width + x;
                    marker[i] = bytes[o];
                    meters[i] = Math.Round(bytes[o + 1] / 4.0) + bytes[o + 2] / 255.0;
                }
            }
        }
    }

    public static string Compare(string pathA, string pathB, string mode)
    {
        byte[] ma, mb; double[] va, vb; int w, h, w2, h2;
        Load(pathA, out ma, out va, out w, out h);
        Load(pathB, out mb, out vb, out w2, out h2);
        if (w != w2 || h != h2) return "size differs";

        long compared = 0, notMeasuredInB = 0, failed = 0, noSurfaceB = 0;
        double maxError = 0.0, sumError = 0.0, sumA = 0.0, sumB = 0.0, maxB = 0.0;
        for (int i = 0; i < ma.Length; ++i)
        {
            if (mb[i] == 255) ++noSurfaceB;
            if (ma[i] != 128) continue;
            if (mb[i] != 128) { ++notMeasuredInB; continue; }
            ++compared;
            sumA += va[i];
            sumB += vb[i];
            maxB = Math.Max(maxB, vb[i]);
            if (mode == "depth")
            {
                double error = Math.Abs(va[i] - vb[i]);
                maxError = Math.Max(maxError, error);
                sumError += error;
                if (error > Math.Max(0.01, 0.01 * va[i])) ++failed;
            }
            else if (vb[i] < va[i] - 0.01)
            {
                ++failed;
            }
        }
        double n = Math.Max(compared, 1);
        // Path mode: the mean view path over the pixels that see each Water Lab depth (pad tops, lab floor).
        string perDepth = "";
        if (mode == "path")
        {
            foreach (double target in new double[] { 0.2, 1.0, 3.0, 6.0, 8.0 })
            {
                long count = 0; double sum = 0.0;
                for (int i = 0; i < ma.Length; ++i)
                {
                    if (ma[i] == 128 && mb[i] == 128 && Math.Abs(va[i] - target) < 0.01) { ++count; sum += vb[i]; }
                }
                perDepth += count > 0 ? string.Format("  [{0} m: path {1:F2} m, {2} px]", target, sum / count, count)
                                      : string.Format("  [{0} m: -]", target);
            }
        }
        if (mode == "depth")
            return string.Format("compared {0}  max err {1:F4} m  mean err {2:F4} m  over tolerance {3}  not measured in B {4}  no-surface in B {5}",
                compared, maxError, sumError / n, failed, notMeasuredInB, noSurfaceB);
        return string.Format("compared {0}  mean depth {1:F3} m  mean path {2:F3} m  path/depth {3:F2}  max path {4:F2} m  path < depth {5}  no-surface {6}",
            compared, sumA / n, sumB / n, sumB / Math.Max(sumA, 1e-9), maxB, failed, noSurfaceB) + Environment.NewLine + "    " + perDepth;
    }
}
'@

$files = Get-ChildItem -Path $A -Filter *.png | Sort-Object Name
if (-not $files) { Write-Output "No PNG files in $A"; exit 1 }
foreach ($file in $files) {
    $other = Join-Path $B $file.Name
    if (-not (Test-Path $other)) { Write-Output ("{0,-28} missing in B" -f $file.Name); continue }
    Write-Output ("{0,-28} {1}" -f $file.Name, [MeterImage]::Compare($file.FullName, $other, $Mode))
}
