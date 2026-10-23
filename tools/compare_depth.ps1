# Compares the "meters as color" PNG captures of depth-reconstruct (DepthBands.hlsli, EncodeMeters):
#   R = fraction, G = whole part x 4 of (meters + 0.5), B = marker (128 measured, 64 above water, 255 no surface).
#   The 0.5 m shift keeps whole-meter depths off the R wrap (kEncodeOffset); it is taken off again here.
#
#   -Mode depth : -A = answer key   (--scene-debug 2 --water off), -B = reconstruction (--debug 12)
#                 every measured answer-key pixel must be measured in B too, within max(1 cm, 1 %).
#   -Mode path  : -A = water depth  (--debug 12),                  -B = view path      (--debug 13)
#                 the view path must never be shorter than the water depth (1 cm slack).
#   -Mode transmit : -A = view path (--debug 13), -B = transmittance (--debug 14), -Extinction r,g,b (the capture's),
#                 optional -C = water depth (--debug 12) for the mean transmittance per Water Lab depth.
#                 every measured pixel must show exp(-extinction x path) per channel (rgb-attenuation).
#
# Usage (PowerShell 5.1):
#   .\tools\compare_depth.ps1 -A <folder> -B <folder> [-Mode depth|path|transmit] [-Extinction r,g,b] [-C <folder>]
# Pairs the PNG files with the same name in the folders.
param(
    [Parameter(Mandatory = $true)][string]$A,
    [Parameter(Mandatory = $true)][string]$B,
    [ValidateSet('depth', 'path', 'transmit')][string]$Mode = 'depth',
    [string]$Extinction = '0,0,0',
    [string]$C = '',
    [double]$Offset = 0.5 # kEncodeOffset the captures were made with (0 = captures from before the shift)
)

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class MeterImage
{
    public static double Offset = 0.5; // kEncodeOffset

    // marker[i] = B byte, meters[i] = decoded R/G.
    public static long LastMalformed; // pixels marked 1 by the last Load

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
                    // A real measurement always has G on a multiple of 4; anything else is not one (a lit pixel
                    // or the sky whose blue happens to be 128). Marked 1 so it is never compared.
                    marker[i] = (bytes[o] == 128 && bytes[o + 1] % 4 != 0) ? (byte)1 : bytes[o];
                    if (marker[i] == 1) ++LastMalformed;
                    meters[i] = Math.Round(bytes[o + 1] / 4.0) + bytes[o + 2] / 255.0 - Offset;
                }
            }
        }
    }

    public static string Compare(string pathA, string pathB, string mode)
    {
        byte[] ma, mb; double[] va, vb; int w, h, w2, h2;
        LastMalformed = 0;
        Load(pathA, out ma, out va, out w, out h);
        long malformedA = LastMalformed;
        LastMalformed = 0;
        Load(pathB, out mb, out vb, out w2, out h2);
        long malformedB = LastMalformed;
        if (w != w2 || h != h2) return "size differs";

        long compared = 0, notMeasuredInB = 0, failed = 0, noSurfaceB = 0;
        string where = ""; // first pixels measured in A but not in B
        double maxError = 0.0, sumError = 0.0, sumA = 0.0, sumB = 0.0, maxB = 0.0;
        for (int i = 0; i < ma.Length; ++i)
        {
            if (mb[i] == 255) ++noSurfaceB;
            if (ma[i] != 128) continue;
            if (mb[i] != 128)
            {
                if (notMeasuredInB < 3) where += string.Format(" ({0},{1})", i % w, i / w);
                ++notMeasuredInB;
                continue;
            }
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
                compared, maxError, sumError / n, failed, notMeasuredInB, noSurfaceB) + string.Format("  malformed A {0} B {1}", malformedA, malformedB) + (where == "" ? "" : "  first unmatched at" + where);
        string malformed = string.Format("  malformed A {0} B {1}", malformedA, malformedB) + (where == "" ? "" : "  first unmatched at" + where);
        return string.Format("compared {0}  mean depth {1:F3} m  mean path {2:F3} m  path/depth {3:F2}  max path {4:F2} m  path < depth {5}  no-surface {6}",
            compared, sumA / n, sumB / n, sumB / Math.Max(sumA, 1e-9), maxB, failed, noSurfaceB) + malformed + Environment.NewLine + "    " + perDepth;
    }

    static byte[] LoadRgb(string path, out int width, out int height)
    {
        using (var bmp = new Bitmap(path))
        {
            width = bmp.Width;
            height = bmp.Height;
            var data = bmp.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var bytes = new byte[data.Stride * height];
            Marshal.Copy(data.Scan0, bytes, 0, bytes.Length);
            bmp.UnlockBits(data);
            var rgb = new byte[width * height * 3]; // R, G, B per pixel
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    int o = y * data.Stride + x * 4;
                    int i = (y * width + x) * 3;
                    rgb[i] = bytes[o + 2];
                    rgb[i + 1] = bytes[o + 1];
                    rgb[i + 2] = bytes[o];
                }
            }
            return rgb;
        }
    }

    // Transmit mode: every measured pixel of the view path set (A) must show exp(-k x path) in the transmittance
    // set (B), per channel, within 8-bit rounding (1.5 levels) plus what the path's own rounding allows.
    // With the water depth set (C), also the mean transmittance over the pixels that see each Water Lab depth.
    public static string CompareTransmit(string pathA, string pathB, string pathC, double kr, double kg, double kb)
    {
        byte[] ma, mc = null; double[] va, vc = null; int w, h, w2, h2;
        LastMalformed = 0;
        Load(pathA, out ma, out va, out w, out h);
        long malformedA = LastMalformed;
        byte[] t = LoadRgb(pathB, out w2, out h2);
        if (w != w2 || h != h2) return "size differs";
        if (pathC != null) { int w3, h3; Load(pathC, out mc, out vc, out w3, out h3); }

        double[] k = { kr, kg, kb };
        long compared = 0, failed = 0;
        double[] maxLevels = new double[3];
        double[] sumT = new double[3];
        for (int i = 0; i < ma.Length; ++i)
        {
            if (ma[i] != 128) continue;
            ++compared;
            bool bad = false;
            for (int c = 0; c < 3; ++c)
            {
                double expected = Math.Exp(-k[c] * va[i]);
                double levels = Math.Abs(t[i * 3 + c] - expected * 255.0);
                double tolerance = 1.5 + 255.0 * k[c] * expected * 0.004; // path decoded to ~4 mm
                maxLevels[c] = Math.Max(maxLevels[c], levels);
                sumT[c] += t[i * 3 + c] / 255.0;
                if (levels > tolerance) bad = true;
            }
            if (bad) ++failed;
        }
        double n = Math.Max(compared, 1);
        string result = string.Format("compared {0}  max err (8-bit levels) R {1:F1} G {2:F1} B {3:F1}  over tolerance {4}  mean T ({5:F3}, {6:F3}, {7:F3})  malformed A {8}",
            compared, maxLevels[0], maxLevels[1], maxLevels[2], failed, sumT[0] / n, sumT[1] / n, sumT[2] / n, malformedA);
        if (mc != null)
        {
            result += Environment.NewLine + "   ";
            foreach (double target in new double[] { 0.2, 1.0, 3.0, 6.0, 8.0 })
            {
                long count = 0; double r = 0.0, g = 0.0, b = 0.0;
                for (int i = 0; i < ma.Length; ++i)
                {
                    if (ma[i] == 128 && mc[i] == 128 && Math.Abs(vc[i] - target) < 0.01)
                    {
                        ++count; r += t[i * 3]; g += t[i * 3 + 1]; b += t[i * 3 + 2];
                    }
                }
                result += count > 0 ? string.Format("  [{0} m: T ({1:F2}, {2:F2}, {3:F2})]", target, r / count / 255.0, g / count / 255.0, b / count / 255.0)
                                    : string.Format("  [{0} m: -]", target);
            }
        }
        return result;
    }
}
'@

[MeterImage]::Offset = $Offset
$files = Get-ChildItem -Path $A -Filter *.png | Sort-Object Name
if (-not $files) { Write-Output "No PNG files in $A"; exit 1 }
$k = @($Extinction -split ',' | ForEach-Object { [double]$_ })
foreach ($file in $files) {
    $other = Join-Path $B $file.Name
    if (-not (Test-Path $other)) { Write-Output ("{0,-28} missing in B" -f $file.Name); continue }
    if ($Mode -eq 'transmit') {
        $depthFile = if ($C) { Join-Path $C $file.Name } else { $null }
        if ($depthFile -and -not (Test-Path $depthFile)) { $depthFile = $null }
        Write-Output ("{0,-28} {1}" -f $file.Name, [MeterImage]::CompareTransmit($file.FullName, $other, $depthFile, $k[0], $k[1], $k[2]))
    }
    else {
        Write-Output ("{0,-28} {1}" -f $file.Name, [MeterImage]::Compare($file.FullName, $other, $Mode))
    }
}
