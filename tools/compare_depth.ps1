# Compares the "meters as color" PNG captures of depth-reconstruct (DepthBands.hlsli, EncodeMeters):
#   R = fraction, G = whole part x 4 of (meters + 0.5), B = marker (128 measured, 64 above water, 255 no surface).
#   The 0.5 m shift keeps whole-meter depths off the R wrap (kEncodeOffset); it is taken off again here.
#
#   -Mode depth : -A = answer key   (--scene-debug 2 --water off), -B = reconstruction (--debug 12)
#                 every measured answer-key pixel must be measured in B too, within max(1 cm, 1 %).
#   -Mode path  : -A = water depth  (--debug 12),                  -B = view path      (--debug 13)
#                 the view path must never be shorter than the water depth (1 cm slack).
#   -Mode transmit : -A = view path (--debug 13), -B = transmittance (--debug 14), -Extinction r,g,b (the capture's),
#                 optional -C = water depth (--debug 12) for the mean transmittance per Deep Stair depth.
#                 every measured pixel must show exp(-extinction x path) per channel (rgb-attenuation).
#   -Mode sum   : -A = attenuation (--transmission 2), -B = water color part (--debug 15), -C = scattering
#                 (--transmission 3), -D = water depth (--debug 12) as the water mask; all with --tonemap none.
#                 A + B = C in linear light on every water pixel (water-scatter).
#   -Mode contrast : -A = water depth (--debug 12), -B = pass-through, -C = scattering, both --tonemap none.
#                 checker contrast kept per Deep Stair depth (water-scatter).
#   -Mode mean  : -A = water depth (--debug 12) as the water mask, -B = a gray debug view (e.g. --debug 17).
#                 mean / min / max of the gray value over the water pixels (surface-reflection).
#   The sum mode also checks reflection part (--debug 18) + underwater part (--debug 19) = final.
#   -Mode foam  : -A = answer key depth (--scene-debug 2), -B = foam amount (--debug 20), -C = water mask (--debug 12),
#                 optional -D = answer key normal (--scene-debug 4), -Width = foam width (m). Foam pixels must sit on a
#                 measured surface no deeper than the width; with -D, counts foam on flat (|n.y| > 0.9) surfaces (contact-foam).
#   -Mode normal: -A = water mask (--debug 12), -B = normal from depth (--debug 22), -C = answer key normal (--scene-debug 4).
#                 angle between them on measured water pixels; -MapDir writes error maps (contact-foam).
#
# Usage (PowerShell 5.1):
#   .\tools\compare_depth.ps1 -A <folder> -B <folder> [-Mode depth|path|transmit] [-Extinction r,g,b] [-C <folder>]
# Pairs the PNG files with the same name in the folders (old test scene shot names count as the new ones,
# tools/shot_names.ps1).
param(
    [Parameter(Mandatory = $true)][string]$A,
    [Parameter(Mandatory = $true)][string]$B,
    [ValidateSet('depth', 'path', 'transmit', 'sum', 'contrast', 'mean', 'foam', 'normal')][string]$Mode = 'depth',
    [string]$Extinction = '0,0,0',
    [string]$C = '',
    [string]$D = '',
    [double]$Offset = 0.5, # kEncodeOffset the captures were made with (0 = captures from before the shift)
    [double]$Width = 0.25, # -Mode foam: the foam width the captures were made with (m)
    [string]$MapDir = ''   # -Mode normal: write an angle error map per image here (black 0 .. white 30 deg)
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
        // Path mode: the mean view path over the pixels that see each Deep Stair depth (pad tops, stair floor).
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
    // With the water depth set (C), also the mean transmittance over the pixels that see each Deep Stair depth.
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

    static double SrgbToLinear(double c)
    {
        return c <= 0.04045 ? c / 12.92 : Math.Pow((c + 0.055) / 1.055, 2.4);
    }

    // Linear value of a byte and how far half an 8-bit step moves it (the rounding of that byte).
    static void Linear(byte b, out double value, out double halfStep)
    {
        double s = b / 255.0;
        value = SrgbToLinear(s);
        halfStep = Math.Abs(SrgbToLinear(Math.Min(s + 0.5 / 255.0, 1.0)) - SrgbToLinear(Math.Max(s - 0.5 / 255.0, 0.0))) * 0.5;
    }

    // Sum mode (water-scatter), all captured with --tonemap none: on every water pixel (mask D: measured or
    // no surface) attenuation (A) + water color part (B) = scattering (C) in linear light, per channel.
    // Saturated C channels are skipped. Also: at no-surface pixels, how much of C the bed part (A) still is.

    // -Mode mean: average of a gray debug view (B, R channel / 255) over the water pixels of the mask A.


    // -Mode foam: where the contact foam (B, --debug 20) shows on the water (C, --debug 12 as the mask), checked
    // against the answer key depth (A, --scene-debug 2) and, if given, the answer key normal (D, --scene-debug 4).
    public static string CompareFoam(string pathA, string pathB, string pathC, string pathD, double width)
    {
        byte[] ma; double[] va; byte[] mc; double[] vc; int w, h;
        Load(pathA, out ma, out va, out w, out h);
        Load(pathC, out mc, out vc, out w, out h);
        byte[] b = LoadRgb(pathB, out w, out h);
        byte[] d = !string.IsNullOrEmpty(pathD) ? LoadRgb(pathD, out w, out h) : null;
        long water = 0, foam = 0, outside = 0, unmeasured = 0, onFlat = 0, onSteep = 0;
        double maxDepth = 0.0;
        for (int i = 0; i < mc.Length; ++i)
        {
            if (mc[i] != 128 && mc[i] != 255) continue;
            ++water;
            if (b[i * 3] == 0) continue;
            ++foam;
            if (ma[i] != 128) { ++unmeasured; continue; }
            maxDepth = Math.Max(maxDepth, va[i]);
            if (va[i] > width + 0.01) ++outside;
            if (d != null)
            {
                double ny = d[i * 3 + 1] / 255.0 * 2.0 - 1.0;
                if (Math.Abs(ny) > 0.9) ++onFlat; else ++onSteep;
            }
        }
        string result = string.Format("water pixels {0}  foam pixels {1} ({2:P1})  deepest foam {3:F3} m  deeper than width {4}  foam without a measured surface {5}",
            water, foam, foam / Math.Max(1.0, water), maxDepth, outside, unmeasured);
        if (d != null) result += string.Format("  on flat (|n.y| > 0.9) {0}  on steep {1}", onFlat, onSteep);
        return result;
    }

    // -Mode normal: angle between the normal reconstructed from depth (B, --debug 22) and the answer key normal
    // (C, --scene-debug 4) on the water pixels with a measured surface (A, --debug 12). Optional error map.
    public static string CompareNormal(string pathA, string pathB, string pathC, string mapPath)
    {
        byte[] ma; double[] va; int w, h;
        Load(pathA, out ma, out va, out w, out h);
        byte[] b = LoadRgb(pathB, out w, out h), c = LoadRgb(pathC, out w, out h);
        var angles = new System.Collections.Generic.List<double>();
        var map = new byte[w * h];
        for (int i = 0; i < ma.Length; ++i)
        {
            if (ma[i] != 128) continue;
            double bx = b[i * 3] / 255.0 * 2 - 1, by = b[i * 3 + 1] / 255.0 * 2 - 1, bz = b[i * 3 + 2] / 255.0 * 2 - 1;
            double cx = c[i * 3] / 255.0 * 2 - 1, cy = c[i * 3 + 1] / 255.0 * 2 - 1, cz = c[i * 3 + 2] / 255.0 * 2 - 1;
            double lb = Math.Sqrt(bx * bx + by * by + bz * bz), lc = Math.Sqrt(cx * cx + cy * cy + cz * cz);
            if (lb < 0.5 || lc < 0.5) continue;
            double cos = (bx * cx + by * cy + bz * cz) / (lb * lc);
            double angle = Math.Acos(Math.Max(-1.0, Math.Min(1.0, cos))) * 180.0 / Math.PI;
            angles.Add(angle);
            map[i] = (byte)Math.Min(255.0, angle / 30.0 * 255.0);
        }
        if (angles.Count == 0) return "no measured water pixels";
        angles.Sort();
        int n = angles.Count;
        long over5 = 0, over20 = 0;
        foreach (double a in angles) { if (a > 5.0) ++over5; if (a > 20.0) ++over20; }
        if (!string.IsNullOrEmpty(mapPath))
        {
            using (var bmp = new Bitmap(w, h, PixelFormat.Format24bppRgb))
            {
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                    {
                        int i = y * w + x;
                        // gray = the frame is not compared here; black..white = 0..30 degrees and more
                        bmp.SetPixel(x, y, ma[i] != 128 ? Color.FromArgb(40, 40, 70) : Color.FromArgb(map[i], map[i], map[i]));
                    }
                bmp.Save(mapPath, ImageFormat.Png);
            }
        }
        return string.Format("measured water pixels {0}  angle median {1:F2} deg  95% {2:F2}  99% {3:F2}  over 5 deg {4:P2}  over 20 deg {5:P2}",
            n, angles[n / 2], angles[(int)(n * 0.95)], angles[(int)(n * 0.99)], over5 / (double)n, over20 / (double)n);
    }

    public static string MeanOverWater(string pathA, string pathB)
    {
        byte[] ma; double[] va; int wa, ha, w, h;
        Load(pathA, out ma, out va, out wa, out ha);
        byte[] b = LoadRgb(pathB, out w, out h);
        long n = 0;
        double sum = 0.0, lo = 1.0, hi = 0.0;
        for (int i = 0; i < ma.Length; ++i)
        {
            if (ma[i] != 128 && ma[i] != 255) continue;
            double v = b[i * 3] / 255.0;
            ++n; sum += v; lo = Math.Min(lo, v); hi = Math.Max(hi, v);
        }
        if (n == 0) return "no water pixels";
        return string.Format("water pixels {0}  mean {1:F3}  min {2:F3}  max {3:F3}", n, sum / n, lo, hi);
    }

    public static string CompareSum(string pathA, string pathB, string pathC, string pathD)
    {
        int w, h;
        byte[] a = LoadRgb(pathA, out w, out h), b = LoadRgb(pathB, out w, out h), c = LoadRgb(pathC, out w, out h);
        byte[] md; double[] vd; int wd, hd;
        Load(pathD, out md, out vd, out wd, out hd);
        long compared = 0, failed = 0, saturated = 0, noSurface = 0;
        double maxRatio = 0.0, bedAtNoSurface = 0.0;
        for (int i = 0; i < md.Length; ++i)
        {
            if (md[i] != 128 && md[i] != 255) continue;
            if (md[i] == 255) ++noSurface;
            ++compared;
            bool bad = false;
            for (int k = 0; k < 3; ++k)
            {
                int o = i * 3 + k;
                if (c[o] == 255) { ++saturated; continue; }
                double va, ea, vb, eb, vc, ec;
                Linear(a[o], out va, out ea); Linear(b[o], out vb, out eb); Linear(c[o], out vc, out ec);
                double tolerance = 1.5 * (ea + eb + ec) + 0.002 * vc + 1e-5; // 8-bit rounding of each + float16
                double ratio = Math.Abs(va + vb - vc) / tolerance;
                maxRatio = Math.Max(maxRatio, ratio);
                if (ratio > 1.0) bad = true;
                if (md[i] == 255 && vc > 1e-4) bedAtNoSurface = Math.Max(bedAtNoSurface, va / vc);
            }
            if (bad) ++failed;
        }
        return string.Format("water pixels {0}  A+B=C over tolerance {1}  worst |A+B-C| / tolerance {2:F2}  saturated channels skipped {3}  no-surface {4}: bed part at most {5:P1} of C",
            compared, failed, maxRatio, saturated, noSurface, bedAtNoSurface);
    }

    // Contrast mode (water-scatter): per Deep Stair depth (pixels selected by the water depth set A), the
    // spread (standard deviation) of luminance in C over that in B (pass-through), both --tonemap none.
    // The water color adds the same amount to light and dark cells, so what is left of the checker contrast
    // follows the transmittance: near 1 in the shallows, near 0 deep down.
    public static string CompareContrast(string pathA, string pathB, string pathC)
    {
        byte[] ma; double[] va; int w, h;
        Load(pathA, out ma, out va, out w, out h);
        byte[] b = LoadRgb(pathB, out w, out h), c = LoadRgb(pathC, out w, out h);
        string result = "contrast kept:";
        foreach (double target in new double[] { 0.2, 1.0, 3.0, 6.0, 8.0 })
        {
            long n = 0; double sb = 0, sbb = 0, sc = 0, scc = 0;
            for (int i = 0; i < ma.Length; ++i)
            {
                if (ma[i] != 128 || Math.Abs(va[i] - target) >= 0.01) continue;
                double lb = 0, lc = 0, v, e;
                double[] weights = { 0.2126, 0.7152, 0.0722 };
                for (int k = 0; k < 3; ++k)
                {
                    Linear(b[i * 3 + k], out v, out e); lb += weights[k] * v;
                    Linear(c[i * 3 + k], out v, out e); lc += weights[k] * v;
                }
                ++n; sb += lb; sbb += lb * lb; sc += lc; scc += lc * lc;
            }
            if (n < 50) { result += string.Format("  [{0} m: -]", target); continue; }
            double stdB = Math.Sqrt(Math.Max(sbb / n - (sb / n) * (sb / n), 0.0));
            double stdC = Math.Sqrt(Math.Max(scc / n - (sc / n) * (sc / n), 0.0));
            result += string.Format("  [{0} m: {1:P0}]", target, stdB > 1e-6 ? stdC / stdB : 0.0);
        }
        return result;
    }
}
'@

. (Join-Path $PSScriptRoot 'shot_names.ps1')
[MeterImage]::Offset = $Offset
$files = Get-ChildItem -Path $A -Filter *.png | Sort-Object Name
if (-not $files) { Write-Output "No PNG files in $A"; exit 1 }
$k = @($Extinction -split ',' | ForEach-Object { [double]$_ })
foreach ($file in $files) {
    $other = Find-ShotFile $B $file.Name
    if (-not (Test-Path $other)) { Write-Output ("{0,-28} missing in B" -f $file.Name); continue }
    if ($Mode -eq 'sum') {
        Write-Output ("{0,-28} {1}" -f $file.Name, [MeterImage]::CompareSum($file.FullName, $other, (Find-ShotFile $C $file.Name), (Find-ShotFile $D $file.Name)))
    }
    elseif ($Mode -eq 'mean') {
        Write-Output ("{0,-28} {1}" -f $file.Name, [MeterImage]::MeanOverWater($file.FullName, $other))
    }
    elseif ($Mode -eq 'foam') {
        $normalFile = if ($D) { Find-ShotFile $D $file.Name } else { $null }
        Write-Output ("{0,-28} {1}" -f $file.Name, [MeterImage]::CompareFoam($file.FullName, $other, (Find-ShotFile $C $file.Name), $normalFile, $Width))
    }
    elseif ($Mode -eq 'normal') {
        $mapFile = $null
        if ($MapDir) { New-Item -ItemType Directory -Force $MapDir | Out-Null; $mapFile = Join-Path $MapDir $file.Name }
        Write-Output ("{0,-28} {1}" -f $file.Name, [MeterImage]::CompareNormal($file.FullName, $other, (Find-ShotFile $C $file.Name), $mapFile))
    }
    elseif ($Mode -eq 'contrast') {
        Write-Output ("{0,-28} {1}" -f $file.Name, [MeterImage]::CompareContrast($file.FullName, $other, (Find-ShotFile $C $file.Name)))
    }
    elseif ($Mode -eq 'transmit') {
        $depthFile = if ($C) { Find-ShotFile $C $file.Name } else { $null }
        if ($depthFile -and -not (Test-Path $depthFile)) { $depthFile = $null }
        Write-Output ("{0,-28} {1}" -f $file.Name, [MeterImage]::CompareTransmit($file.FullName, $other, $depthFile, $k[0], $k[1], $k[2]))
    }
    else {
        Write-Output ("{0,-28} {1}" -f $file.Name, [MeterImage]::Compare($file.FullName, $other, $Mode))
    }
}
