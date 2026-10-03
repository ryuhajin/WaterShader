# Converts an equirectangular (lat-long) panorama into a D3D cube map DDS for CubemapTexture.
#
#   LDR  (.jpg/.png, e.g. Poly Haven "Tonemapped JPG"): sRGB-encoded values -> RGBA8 cube -> BC7_UNORM.
#        The app reads it through an *_SRGB view (decoded to linear by the sampler).
#   HDR  (.hdr, Radiance RGBE): linear scene radiance -> RGBA32F cube -> BC6H_UF16 (float, no sRGB).
#
#   powershell -File tools/equirect_to_cube.ps1 -In sky.hdr -Out assets/textures/env_x.dds
#                                               [-FaceSize 1024] [-AlignSunYaw 34.5 | -RotateYaw <deg>]
#
# -AlignSunYaw rotates the panorama around the vertical axis so the sun lands on that yaw (the fixed
# "sunward" capture shots face yaw ~34.5). -RotateYaw applies a fixed rotation instead, for skies whose
# interesting side is not the sun side (e.g. a beach where the sea is opposite the sun).
#
# For HDR input the tool also measures the light so the scene lights can be calibrated from the sky:
#   sun direction, sun irradiance (radiance integrated over the sun disk), sky irradiance on an
#   upward-facing plane (sun excluded), and a key-based exposure suggestion. Divide irradiance by pi to
#   get the value a Lambert term "albedo * light * N.L" expects.
#
# Direction convention matches the app: yaw = atan2(x, z) (0 = +Z, 90 = +X), elevation = asin(y).
# Face order / orientation is the D3D one (+X, -X, +Y, -Y, +Z, -Z), same as assets/textures/skybox.dds.
param(
    [Parameter(Mandatory = $true)][string]$In,
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$FaceSize = 1024,
    [string]$Format = "",
    [double]$AlignSunYaw = [double]::NaN,
    [double]$RotateYaw = 0.0
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

public static class EquirectToCube
{
    // Source pixels as RGB floats: sRGB-encoded 0..1 for LDR, linear radiance for HDR.
    static float[] src; static int sw, sh; static bool hdr;

    static void LoadLdr(string path)
    {
        using (var bmp = new Bitmap(path))
        {
            sw = bmp.Width; sh = bmp.Height;
            var data = bmp.LockBits(new Rectangle(0, 0, sw, sh), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var row = new byte[sw * 4];
            src = new float[sw * sh * 3];
            for (int y = 0; y < sh; ++y)
            {
                Marshal.Copy(IntPtr.Add(data.Scan0, y * data.Stride), row, 0, sw * 4);
                for (int x = 0; x < sw; ++x)
                {
                    int o = (y * sw + x) * 3;
                    src[o] = row[x * 4 + 2] / 255f; src[o + 1] = row[x * 4 + 1] / 255f; src[o + 2] = row[x * 4] / 255f;
                }
            }
            bmp.UnlockBits(data);
        }
    }

    // Radiance .hdr: text header, blank line, "-Y H +X W", then new-style RLE scanlines (or flat RGBE).
    static void LoadHdr(string path)
    {
        byte[] file = File.ReadAllBytes(path);
        int pos = 0;
        string line;
        do { line = ReadLine(file, ref pos); } while (line.Length > 0);   // header ends at an empty line
        string[] res = ReadLine(file, ref pos).Split(' ');
        if (res.Length != 4 || res[0] != "-Y" || res[2] != "+X") throw new Exception("Unsupported .hdr orientation: " + string.Join(" ", res));
        sh = int.Parse(res[1]); sw = int.Parse(res[3]);
        src = new float[sw * sh * 3];
        var scan = new byte[sw * 4];
        for (int y = 0; y < sh; ++y)
        {
            if (file[pos] == 2 && file[pos + 1] == 2 && ((file[pos + 2] << 8) | file[pos + 3]) == sw)
            {
                pos += 4;
                for (int ch = 0; ch < 4; ++ch)
                {
                    int x = 0;
                    while (x < sw)
                    {
                        int count = file[pos++];
                        if (count > 128) { count -= 128; byte v = file[pos++]; for (int i = 0; i < count; ++i) scan[(x++) * 4 + ch] = v; }
                        else { for (int i = 0; i < count; ++i) scan[(x++) * 4 + ch] = file[pos++]; }
                    }
                }
            }
            else { Array.Copy(file, pos, scan, 0, sw * 4); pos += sw * 4; }
            for (int x = 0; x < sw; ++x)
            {
                int e = scan[x * 4 + 3];
                float f = e == 0 ? 0f : (float)Math.Pow(2.0, e - 136);
                int o = (y * sw + x) * 3;
                src[o] = (scan[x * 4] + 0.5f) * f; src[o + 1] = (scan[x * 4 + 1] + 0.5f) * f; src[o + 2] = (scan[x * 4 + 2] + 0.5f) * f;
                if (e == 0) { src[o] = src[o + 1] = src[o + 2] = 0f; }
            }
        }
    }

    static string ReadLine(byte[] b, ref int pos)
    {
        var sb = new StringBuilder();
        while (pos < b.Length && b[pos] != (byte)'\n') sb.Append((char)b[pos++]);
        pos++;
        return sb.ToString().TrimEnd('\r');
    }

    static double Luminance(int o) { return 0.2126 * src[o] + 0.7152 * src[o + 1] + 0.0722 * src[o + 2]; }

    // Bilinear lookup, u wraps, v clamps.
    static void Sample(double u, double v, double[] rgb)
    {
        double x = u * sw - 0.5, y = v * sh - 0.5;
        int x0 = (int)Math.Floor(x), y0 = (int)Math.Floor(y);
        double fx = x - x0, fy = y - y0;
        rgb[0] = rgb[1] = rgb[2] = 0;
        for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i)
        {
            int px = ((x0 + i) % sw + sw) % sw;
            int py = Math.Min(Math.Max(y0 + j, 0), sh - 1);
            double w = (i == 0 ? 1 - fx : fx) * (j == 0 ? 1 - fy : fy);
            int o = (py * sw + px) * 3;
            rgb[0] += src[o] * w; rgb[1] += src[o + 1] * w; rgb[2] += src[o + 2] * w;
        }
    }

    static void FaceDir(int face, double a, double b, out double dx, out double dy, out double dz)
    {
        // a, b in [-1, 1]: a = texel u, b = texel v (down). D3D cube conventions.
        switch (face)
        {
            case 0: dx = 1;  dy = -b; dz = -a; break; // +X
            case 1: dx = -1; dy = -b; dz = a;  break; // -X
            case 2: dx = a;  dy = 1;  dz = b;  break; // +Y
            case 3: dx = a;  dy = -1; dz = -b; break; // -Y
            case 4: dx = a;  dy = -b; dz = 1;  break; // +Z
            default: dx = -a; dy = -b; dz = -1; break; // -Z
        }
        double len = Math.Sqrt(dx * dx + dy * dy + dz * dz);
        dx /= len; dy /= len; dz /= len;
    }

    public static string Convert(string inPath, string ddsPath, int n, double alignSunYaw, double rotateYaw)
    {
        hdr = inPath.EndsWith(".hdr", StringComparison.OrdinalIgnoreCase);
        if (hdr) LoadHdr(inPath); else LoadLdr(inPath);

        double sunYaw, sunElev;
        string info = hdr ? MeasureHdr(out sunYaw, out sunElev) : FindSunLdr(out sunYaw, out sunElev);
        double offset = double.IsNaN(alignSunYaw) ? rotateYaw : alignSunYaw - sunYaw; // output yaw = source yaw + offset

        var faces = new float[6 * n * n * 4];
        var c = new double[3];
        for (int f = 0; f < 6; ++f)
        for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            double r = 0, g = 0, b = 0;
            for (int sy = 0; sy < 2; ++sy)
            for (int sx = 0; sx < 2; ++sx)
            {
                double a = 2.0 * (x + 0.25 + 0.5 * sx) / n - 1.0;
                double bb = 2.0 * (y + 0.25 + 0.5 * sy) / n - 1.0;
                double dx, dy, dz; FaceDir(f, a, bb, out dx, out dy, out dz);
                double lon = Math.Atan2(dx, dz) - offset * Math.PI / 180.0; // output yaw -> source yaw
                double lat = Math.Asin(Math.Max(-1, Math.Min(1, dy)));
                double u = 0.5 + lon / (2 * Math.PI); u -= Math.Floor(u);
                Sample(u, 0.5 - lat / Math.PI, c);
                r += c[0]; g += c[1]; b += c[2];
            }
            int o = ((f * n + y) * n + x) * 4;
            faces[o] = (float)(r / 4); faces[o + 1] = (float)(g / 4); faces[o + 2] = (float)(b / 4); faces[o + 3] = 1f;
        }

        if (hdr) WriteDdsFloat(ddsPath, faces, n); else WriteDdsRgba8(ddsPath, faces, n);
        return String.Format("{0}{1}rotated {2:F1} deg -> sun yaw {3:F1}, elevation {4:F1}",
            info, Environment.NewLine, offset, sunYaw + offset, sunElev);
    }

    static void WriteDdsRgba8(string path, float[] faces, int n)
    {
        using (var w = new BinaryWriter(File.Create(path)))
        {
            w.Write(0x20534444u); w.Write(124u); w.Write(0x100Fu);   // "DDS ", size, CAPS|HEIGHT|WIDTH|PITCH|PIXELFORMAT
            w.Write((uint)n); w.Write((uint)n); w.Write((uint)(n * 4)); w.Write(0u); w.Write(1u);
            for (int i = 0; i < 11; ++i) w.Write(0u);
            w.Write(32u); w.Write(0x41u); w.Write(0u); w.Write(32u);  // RGB | ALPHAPIXELS, 32 bpp
            w.Write(0x000000FFu); w.Write(0x0000FF00u); w.Write(0x00FF0000u); w.Write(0xFF000000u);
            w.Write(0x1008u); w.Write(0xFE00u);                       // COMPLEX|TEXTURE, CUBEMAP + 6 faces
            w.Write(0u); w.Write(0u); w.Write(0u);
            foreach (float v in faces) w.Write((byte)Math.Round(Math.Min(Math.Max(v, 0f), 1f) * 255));
        }
    }

    // DX10-extended header: R32G32B32A32_FLOAT texture2D array of one cube.
    static void WriteDdsFloat(string path, float[] faces, int n)
    {
        using (var w = new BinaryWriter(File.Create(path)))
        {
            w.Write(0x20534444u); w.Write(124u); w.Write(0x100Fu);
            w.Write((uint)n); w.Write((uint)n); w.Write((uint)(n * 16)); w.Write(0u); w.Write(1u);
            for (int i = 0; i < 11; ++i) w.Write(0u);
            w.Write(32u); w.Write(0x4u); w.Write(0x30315844u); w.Write(0u); // FOURCC "DX10"
            w.Write(0u); w.Write(0u); w.Write(0u); w.Write(0u);
            w.Write(0x1008u); w.Write(0xFE00u);
            w.Write(0u); w.Write(0u); w.Write(0u);
            w.Write(2u);    // DXGI_FORMAT_R32G32B32A32_FLOAT
            w.Write(3u);    // D3D10_RESOURCE_DIMENSION_TEXTURE2D
            w.Write(0x4u);  // DDS_RESOURCE_MISC_TEXTURECUBE
            w.Write(1u);    // array size (cubes)
            w.Write(0u);    // misc flags 2
            foreach (float v in faces) w.Write(v);
        }
    }

    // LDR: sun = weighted centroid of the brightest (often clipped) texels.
    static string FindSunLdr(out double yaw, out double elev)
    {
        double maxL = 0;
        for (int y = 0; y < sh / 2; y += 2) for (int x = 0; x < sw; x += 2) maxL = Math.Max(maxL, Luminance((y * sw + x) * 3));
        double sx = 0, sy = 0, sz = 0;
        for (int y = 0; y < sh / 2; y += 2) for (int x = 0; x < sw; x += 2)
        {
            double l = Luminance((y * sw + x) * 3);
            if (l < maxL * 0.97) continue;
            double w = Math.Pow(l / maxL, 32);
            double lon = ((x + 0.5) / sw - 0.5) * 2 * Math.PI, lat = (0.5 - (y + 0.5) / sh) * Math.PI;
            sx += w * Math.Cos(lat) * Math.Sin(lon); sy += w * Math.Sin(lat); sz += w * Math.Cos(lat) * Math.Cos(lon);
        }
        double len = Math.Sqrt(sx * sx + sy * sy + sz * sz);
        yaw = Math.Atan2(sx, sz) * 180 / Math.PI; elev = Math.Asin(sy / len) * 180 / Math.PI;
        return String.Format("LDR source {0}x{1}, source sun yaw {2:F1} deg; ", sw, sh, yaw);
    }

    // HDR: real radiance, so the sun can be measured instead of guessed.
    static string MeasureHdr(out double yaw, out double elev)
    {
        double dLon = 2 * Math.PI / sw, dLat = Math.PI / sh;
        // 1) sun direction: radiance-weighted centroid of texels above 25% of the peak. A visible sun
        //    disk is thousands of times brighter than the sky, so this isolates it; a sky without a
        //    disk (sun behind clouds) then picks its brightest glow instead of the whole bright cloud
        //    field (a 1% threshold put the "sun" of the_sky_is_on_fire at 77 deg elevation).
        double maxL = 0;
        for (int i = 0; i < sw * sh; ++i) maxL = Math.Max(maxL, Luminance(i * 3));
        double sx = 0, sy = 0, sz = 0;
        for (int y = 0; y < sh / 2; ++y) for (int x = 0; x < sw; ++x)
        {
            double l = Luminance((y * sw + x) * 3);
            if (l < maxL * 0.25) continue;
            double lon = ((x + 0.5) / sw - 0.5) * 2 * Math.PI, lat = (0.5 - (y + 0.5) / sh) * Math.PI;
            sx += l * Math.Cos(lat) * Math.Sin(lon); sy += l * Math.Sin(lat); sz += l * Math.Cos(lat) * Math.Cos(lon);
        }
        double len = Math.Sqrt(sx * sx + sy * sy + sz * sz);
        double ux = sx / len, uy = sy / len, uz = sz / len;
        yaw = Math.Atan2(ux, uz) * 180 / Math.PI; elev = Math.Asin(uy) * 180 / Math.PI;

        // 2) sun core: inside a 2.5 deg cone, texels brighter than 4x the surrounding sky (mean of the
        //    2.5..5 deg ring). Clamping them to that level removes the disk (thousands of times the sky)
        //    but keeps the aureole around it smooth. A first version filled the whole cone with the ring
        //    average and left a visible flat disc in the sky. Sun irradiance = removed excess.
        double cosSun = Math.Cos(2.5 * Math.PI / 180), cosRing = Math.Cos(5.0 * Math.PI / 180);
        double bgL = 0, bgW = 0;
        ForEachTexel(dLon, dLat, (o, dx, dy, dz, dOmega) =>
        {
            double c = dx * ux + dy * uy + dz * uz;
            if (c <= cosSun && c > cosRing) { bgL += Luminance(o) * dOmega; bgW += dOmega; }
        });
        double clampL = 4.0 * bgL / bgW;
        double sunR = 0, sunG = 0, sunB = 0;
        ForEachTexel(dLon, dLat, (o, dx, dy, dz, dOmega) =>
        {
            if (dx * ux + dy * uy + dz * uz <= cosSun) return;
            double l = Luminance(o);
            if (l <= clampL) return;
            double k = 1.0 - clampL / l;   // fraction of this texel that is "sun"
            sunR += src[o] * k * dOmega; sunG += src[o + 1] * k * dOmega; sunB += src[o + 2] * k * dOmega;
        });
        double sunE = 0.2126 * sunR + 0.7152 * sunG + 0.0722 * sunB;

        // 3) Remove a *visible* sun from the sky (the app draws it analytically and lights the water
        //    with it, so leaving it in would reflect the sun twice). Skies whose sun is hidden keep
        //    their glow: below 5% of the sky irradiance there is no disk worth separating.
        double skyBefore = SkyIrradiance(dLon, dLat)[3];
        bool removed = sunE > 0.05 * skyBefore;
        if (removed)
        {
            ForEachTexel(dLon, dLat, (o, dx, dy, dz, dOmega) =>
            {
                if (dx * ux + dy * uy + dz * uz <= cosSun) return;
                double l = Luminance(o);
                if (l <= clampL) return;
                float k = (float)(clampL / l);
                src[o] *= k; src[o + 1] *= k; src[o + 2] *= k;
            });
        }
        else { sunR = sunG = sunB = sunE = 0; }

        // 4) sky irradiance on an upward plane (after removal) and key exposure.
        double[] sky = SkyIrradiance(dLon, dLat);
        double logSum = 0, wSum = 0;
        ForEachTexel(dLon, dLat, (o, dx, dy, dz, dOmega) => { logSum += Math.Log(Luminance(o) + 1e-4) * dOmega; wSum += dOmega; });
        double logAvg = Math.Exp(logSum / wSum);
        double ev = Math.Log(0.18 / logAvg, 2);
        return String.Format(
            "HDR source {0}x{1}, peak luminance {2:F0}" + Environment.NewLine +
            "sun disk {3}: irradiance (normal to sun) RGB = ({4:F4}, {5:F4}, {6:F4}), luminance {7:F4}" + Environment.NewLine +
            "sky irradiance on upward plane RGB = ({8:F4}, {9:F4}, {10:F4}), luminance {11:F4}" + Environment.NewLine +
            "app values (E/pi, linear): sun = ({12:F4}, {13:F4}, {14:F4}), ambient = ({15:F4}, {16:F4}, {17:F4})" + Environment.NewLine +
            "log-average luminance {18:F4} -> key 0.18 exposure EV {19:F2}" + Environment.NewLine,
            sw, sh, maxL, removed ? "REMOVED from sky" : "not visible (kept)", sunR, sunG, sunB, sunE,
            sky[0], sky[1], sky[2], sky[3],
            sunR / Math.PI, sunG / Math.PI, sunB / Math.PI, sky[0] / Math.PI, sky[1] / Math.PI, sky[2] / Math.PI,
            logAvg, ev);
    }

    delegate void TexelFn(int o, double dx, double dy, double dz, double dOmega);

    static void ForEachTexel(double dLon, double dLat, TexelFn fn)
    {
        for (int y = 0; y < sh; ++y)
        {
            double lat = (0.5 - (y + 0.5) / sh) * Math.PI;
            double dOmega = dLon * dLat * Math.Cos(lat);
            for (int x = 0; x < sw; ++x)
            {
                double lon = ((x + 0.5) / sw - 0.5) * 2 * Math.PI;
                fn((y * sw + x) * 3, Math.Cos(lat) * Math.Sin(lon), Math.Sin(lat), Math.Cos(lat) * Math.Cos(lon), dOmega);
            }
        }
    }

    // Irradiance on an upward-facing plane from the upper hemisphere: sum L * cos(theta) * dOmega.
    static double[] SkyIrradiance(double dLon, double dLat)
    {
        double r = 0, g = 0, b = 0;
        ForEachTexel(dLon, dLat, (o, dx, dy, dz, dOmega) =>
        {
            if (dy <= 0) return;
            r += src[o] * dy * dOmega; g += src[o + 1] * dy * dOmega; b += src[o + 2] * dy * dOmega;
        });
        return new double[] { r, g, b, 0.2126 * r + 0.7152 * g + 0.0722 * b };
    }
}
"@

$inPath = (Resolve-Path $In).Path
$outPath = [IO.Path]::GetFullPath($Out)
$isHdr = $inPath.ToLower().EndsWith(".hdr")
if (-not $Format) { $Format = if ($isHdr) { "BC6H_UF16" } else { "BC7_UNORM" } }
$rawPath = [IO.Path]::ChangeExtension($outPath, ".raw.dds")

$sw = [Diagnostics.Stopwatch]::StartNew()
$info = [EquirectToCube]::Convert($inPath, $rawPath, $FaceSize, $AlignSunYaw, $RotateYaw)
Write-Output "faces written in $([int]$sw.Elapsed.TotalSeconds)s"
Write-Output $info

# Mips + compression. texconv writes <name>.dds next to -o; rename the raw file so the output keeps $Out's name.
$texconv = Join-Path $PSScriptRoot "..\assets\textures\texconv.exe"
$tmpDir = Join-Path ([IO.Path]::GetDirectoryName($outPath)) "_texconv_tmp"
New-Item -ItemType Directory -Force $tmpDir | Out-Null
$named = Join-Path $tmpDir ([IO.Path]::GetFileName($outPath))
Move-Item -Force $rawPath $named
& $texconv -nologo -y -m 0 -f $Format -o $tmpDir $named | Out-Null
Move-Item -Force $named $outPath
Remove-Item -Recurse -Force $tmpDir
Write-Output ("saved {0} ({1:N1} MB, {2}, mips)" -f $outPath, ((Get-Item $outPath).Length / 1MB), $Format)
