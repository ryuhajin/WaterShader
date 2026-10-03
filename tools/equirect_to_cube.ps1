# Converts an equirectangular (lat-long) LDR panorama (e.g. Poly Haven "Tonemapped JPG") into a
# D3D cube map DDS usable by CubemapTexture, and reports where the sun is.
#
#   powershell -File tools/equirect_to_cube.ps1 -In sky.jpg -Out assets/textures/env_x.dds [-FaceSize 1024] [-AlignSunYaw 34.5]
#
# -AlignSunYaw rotates the panorama around the vertical axis so the sun lands on that yaw. Every
# environment then shares the sun yaw of skybox.dds, and the fixed "sunward" capture shots face the
# sun whichever sky a preset uses. -RotateYaw <deg> instead applies a fixed rotation, for skies whose
# interesting side is not the sun side (e.g. a beach where the sea is opposite the sun).
#
# Steps: equirect -> 6 faces (2x2 supersampled bilinear) -> RGBA8 cube DDS -> texconv (mips + BC7).
# Direction convention matches the app: yaw = atan2(x, z) (0 = +Z, 90 = +X), elevation = asin(y).
# Face order / orientation is the D3D one (+X, -X, +Y, -Y, +Z, -Z), same as assets/textures/skybox.dds.
param(
    [Parameter(Mandatory = $true)][string]$In,
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$FaceSize = 1024,
    [string]$Format = "BC7_UNORM",
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

public static class EquirectToCube
{
    static byte[] src; static int sw, sh;

    static void Load(string path)
    {
        using (var bmp = new Bitmap(path))
        {
            sw = bmp.Width; sh = bmp.Height;
            var data = bmp.LockBits(new Rectangle(0, 0, sw, sh), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            src = new byte[sw * sh * 4];
            for (int y = 0; y < sh; ++y)
                Marshal.Copy(IntPtr.Add(data.Scan0, y * data.Stride), src, y * sw * 4, sw * 4);
            bmp.UnlockBits(data);
        }
    }

    // Bilinear lookup, u wraps, v clamps. Returns B,G,R (GDI byte order) as doubles.
    static void Sample(double u, double v, double[] bgr)
    {
        double x = u * sw - 0.5, y = v * sh - 0.5;
        int x0 = (int)Math.Floor(x), y0 = (int)Math.Floor(y);
        double fx = x - x0, fy = y - y0;
        bgr[0] = bgr[1] = bgr[2] = 0;
        for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i)
        {
            int px = ((x0 + i) % sw + sw) % sw;
            int py = Math.Min(Math.Max(y0 + j, 0), sh - 1);
            double w = (i == 0 ? 1 - fx : fx) * (j == 0 ? 1 - fy : fy);
            int o = (py * sw + px) * 4;
            bgr[0] += src[o] * w; bgr[1] += src[o + 1] * w; bgr[2] += src[o + 2] * w;
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
        Load(inPath);
        double sunYaw, sunElev;
        string info = FindSun(out sunYaw, out sunElev);
        // Output yaw = source yaw + offset.
        double offset = double.IsNaN(alignSunYaw) ? rotateYaw : alignSunYaw - sunYaw;
        var faces = new byte[6 * n * n * 4];
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
                double lat = Math.Asin(Math.Max(-1, Math.Min(1, dy))); // elevation
                double u = 0.5 + lon / (2 * Math.PI); u -= Math.Floor(u);
                Sample(u, 0.5 - lat / Math.PI, c);
                b += c[0]; g += c[1]; r += c[2];
            }
            int o = ((f * n + y) * n + x) * 4;
            faces[o] = (byte)Math.Round(r / 4); faces[o + 1] = (byte)Math.Round(g / 4);
            faces[o + 2] = (byte)Math.Round(b / 4); faces[o + 3] = 255;
        }
        WriteDds(ddsPath, faces, n);
        return String.Format("{0}; rotated {1:F1} deg -> sun yaw {2:F1}, elevation {3:F1}",
            info, offset, sunYaw + offset, sunElev);
    }

    static void WriteDds(string path, byte[] faces, int n)
    {
        using (var w = new BinaryWriter(File.Create(path)))
        {
            w.Write(0x20534444u);                  // "DDS "
            w.Write(124u);                         // header size
            w.Write(0x100Fu);                      // CAPS | HEIGHT | WIDTH | PITCH | PIXELFORMAT
            w.Write((uint)n); w.Write((uint)n);    // height, width
            w.Write((uint)(n * 4));                // pitch
            w.Write(0u); w.Write(1u);              // depth, mip count
            for (int i = 0; i < 11; ++i) w.Write(0u);
            w.Write(32u); w.Write(0x41u);          // pixel format: size, RGB | ALPHAPIXELS
            w.Write(0u); w.Write(32u);             // fourCC, bit count
            w.Write(0x000000FFu); w.Write(0x0000FF00u); w.Write(0x00FF0000u); w.Write(0xFF000000u); // RGBA
            w.Write(0x1008u);                      // caps: COMPLEX | TEXTURE
            w.Write(0xFE00u);                      // caps2: CUBEMAP + all six faces
            w.Write(0u); w.Write(0u); w.Write(0u);
            w.Write(faces);
        }
    }

    // Sun = weighted centroid of the brightest texels in the source panorama.
    static string FindSun(out double yaw, out double elev)
    {
        int step = Math.Max(1, sw / 2048);
        double maxL = 0;
        for (int y = 0; y < sh / 2; y += step) for (int x = 0; x < sw; x += step)
        { int o = (y * sw + x) * 4; maxL = Math.Max(maxL, src[o] + src[o + 1] + src[o + 2]); }
        double sx = 0, sy = 0, sz = 0;
        for (int y = 0; y < sh / 2; y += step) for (int x = 0; x < sw; x += step)
        {
            int o = (y * sw + x) * 4; double l = src[o] + src[o + 1] + src[o + 2];
            if (l < maxL * 0.97) continue;
            double w = Math.Pow(l / maxL, 32);
            double lon = ((x + 0.5) / sw - 0.5) * 2 * Math.PI, lat = (0.5 - (y + 0.5) / sh) * Math.PI;
            sx += w * Math.Cos(lat) * Math.Sin(lon); sy += w * Math.Sin(lat); sz += w * Math.Cos(lat) * Math.Cos(lon);
        }
        double len = Math.Sqrt(sx * sx + sy * sy + sz * sz);
        yaw = Math.Atan2(sx, sz) * 180 / Math.PI; elev = Math.Asin(sy / len) * 180 / Math.PI;
        return String.Format("source {0}x{1}, brightest {2:F0}/765, source sun yaw {3:F1} deg", sw, sh, maxL, yaw);
    }
}
"@

$inPath = (Resolve-Path $In).Path
$outPath = [IO.Path]::GetFullPath($Out)
$rawPath = [IO.Path]::ChangeExtension($outPath, ".raw.dds")

$sw = [Diagnostics.Stopwatch]::StartNew()
$info = [EquirectToCube]::Convert($inPath, $rawPath, $FaceSize, $AlignSunYaw, $RotateYaw)
Write-Output "faces written in $([int]$sw.Elapsed.TotalSeconds)s; $info"

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
