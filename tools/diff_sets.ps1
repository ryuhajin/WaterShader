# Where and how much two capture sets differ, per image: differing pixels, how many of them lie on the water
# (optional mask = a --debug 12 capture: blue 128 / 255 = water) and the largest difference in 8-bit levels.
# Complements compare_sets.ps1 (which only says same / different). Identical images are not listed.
#
# Usage (PowerShell 5.1):
#   .\tools\diff_sets.ps1 -A <reference set> -B <set to check> [-Mask <--debug 12 set>] [-List 5]
#   -Mask images are matched by shot: "<preset>_<shot>.png" in A uses "*_<shot>.png" from the mask folder.
#   -List prints the first n differing pixels of each image.
param(
    [Parameter(Mandatory = $true)][string]$A,
    [Parameter(Mandatory = $true)][string]$B,
    [string]$Mask = '',
    [int]$List = 0
)

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public static class SetDiff
{
    static byte[] Load(string path, out int width, out int height)
    {
        using (var bmp = new Bitmap(path))
        {
            width = bmp.Width;
            height = bmp.Height;
            var data = bmp.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var bytes = new byte[data.Stride * height];
            Marshal.Copy(data.Scan0, bytes, 0, bytes.Length);
            bmp.UnlockBits(data);
            return bytes; // BGRA, stride = width * 4 for 32bpp
        }
    }

    public static string Compare(string pathA, string pathB, string pathMask, int list)
    {
        int w, h, wb, hb, wm, hm;
        byte[] a = Load(pathA, out w, out h), b = Load(pathB, out wb, out hb);
        if (w != wb || h != hb) return "different size";
        byte[] m = pathMask != null ? Load(pathMask, out wm, out hm) : null;
        long count = 0, onWater = 0, overOne = 0;
        int maxLevel = 0;
        var sample = new StringBuilder();
        for (int i = 0; i < w * h; ++i)
        {
            int o = i * 4;
            int d = Math.Max(Math.Abs(a[o] - b[o]), Math.Max(Math.Abs(a[o + 1] - b[o + 1]), Math.Abs(a[o + 2] - b[o + 2])));
            if (d == 0) continue;
            ++count;
            if (d > 1) ++overOne;
            maxLevel = Math.Max(maxLevel, d);
            if (m != null && (m[o] == 128 || m[o] == 255)) ++onWater;
            if (count <= list)
                sample.AppendFormat("{0}    ({1},{2}) {3},{4},{5} -> {6},{7},{8}", Environment.NewLine, i % w, i / w,
                    a[o + 2], a[o + 1], a[o], b[o + 2], b[o + 1], b[o]);
        }
        if (count == 0) return null;
        string water = m != null ? string.Format(" (on water {0})", onWater) : "";
        return string.Format("{0} px{1}, over 1 level {2}, max {3} levels", count, water, overOne, maxLevel) + sample;
    }
}
'@

$files = Get-ChildItem -LiteralPath $A -File | Where-Object { $_.Extension -in '.png', '.jpg' } | Sort-Object Name
$differing = 0
foreach ($file in $files) {
    $other = Join-Path $B $file.Name
    if (-not (Test-Path $other)) { Write-Output ("{0,-30} missing in B" -f $file.Name); ++$differing; continue }
    $maskFile = $null
    if ($Mask) {
        $shot = $file.BaseName -replace '^[^_]+_', ''
        $maskFile = Get-ChildItem -LiteralPath $Mask -File -Filter "*_$shot$($file.Extension)" | Select-Object -First 1 -ExpandProperty FullName
    }
    $result = [SetDiff]::Compare($file.FullName, $other, $maskFile, $List)
    if ($result) { Write-Output ("{0,-30} {1}" -f $file.Name, $result); ++$differing }
}
Write-Output ("{0} of {1} images differ" -f $differing, $files.Count)
