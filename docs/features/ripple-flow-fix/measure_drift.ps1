# Measures how far a pattern moved between two captures of the same shot (best integer shift by
# sum of absolute differences over a centre crop). Use with --debug 1 (normal map layer A only).
# Usage: powershell -ExecutionPolicy Bypass -File measure_drift.ps1 <before.jpg> <after.jpg> [-Range 48]
param([Parameter(Mandatory)][string]$A, [Parameter(Mandatory)][string]$B, [int]$Range = 48, [int]$Crop = 240)

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System; using System.Drawing;
public static class Drift {
    static float[,] Gray(Bitmap b) {
        var g = new float[b.Width, b.Height];
        for (int y = 0; y < b.Height; y++) for (int x = 0; x < b.Width; x++) {
            var c = b.GetPixel(x, y); g[x, y] = c.R + c.G;  // the slope channels
        }
        return g;
    }
    // Returns {dx, dy}: content at (x, y) in A is found at (x + dx, y + dy) in B.
    public static int[] Find(Bitmap a, Bitmap b, int range, int crop) {
        var ga = Gray(a); var gb = Gray(b);
        int cx = a.Width / 2, cy = a.Height / 2;
        double best = double.MaxValue; int bx = 0, by = 0;
        for (int dy = -range; dy <= range; dy++) for (int dx = -range; dx <= range; dx++) {
            double s = 0;
            for (int y = cy - crop / 2; y < cy + crop / 2; y += 2) for (int x = cx - crop / 2; x < cx + crop / 2; x += 2)
                s += Math.Abs(ga[x, y] - gb[x + dx, y + dy]);
            if (s < best) { best = s; bx = dx; by = dy; }
        }
        return new[] { bx, by };
    }
}
"@
$ba = [Drawing.Bitmap]::FromFile((Resolve-Path $A)); $bb = [Drawing.Bitmap]::FromFile((Resolve-Path $B))
$d = [Drift]::Find($ba, $bb, $Range, $Crop)
$ba.Dispose(); $bb.Dispose()
# "top" shot looks straight down with yaw 0: screen right = world +X, screen down = world -Z.
$deg = [Math]::Atan2(-$d[1], $d[0]) * 180 / [Math]::PI
"shift dx={0} dy={1} px  -> world direction {2:N1} deg (0 = +X, 90 = +Z)" -f $d[0], $d[1], $deg
