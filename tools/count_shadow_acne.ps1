# Counts shadow acne in Scene Debug 5 captures (sun-shadow): the view paints the sun share as gray (white = lit,
# black = shadow, mid gray 128 = turned away from the sun), the sky / grass stay in color.
# Per image: pixels that are partly shadowed (gray between 8 and 247, except the turned-away 128 +- 2), and of the
# lit-or-shadowed surface pixels the share that is partial. Acne turns whole lit faces into stripes, so the share
# jumps; a clean map keeps only the thin soft edges of real shadows.
#
# Usage (PowerShell 5.1):
#   .\tools\count_shadow_acne.ps1 -Dir C:\Users\da171\ws-capture\sun-shadow\captures\bias_none
param(
    [Parameter(Mandatory = $true)][string]$Dir
)

Add-Type -AssemblyName System.Drawing
foreach ($file in Get-ChildItem -LiteralPath $Dir -Filter *.png) {
    $bitmap = [System.Drawing.Bitmap]::FromFile($file.FullName)
    $rect = New-Object System.Drawing.Rectangle(0, 0, $bitmap.Width, $bitmap.Height)
    $data = $bitmap.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $bytes = New-Object byte[] ($data.Stride * $bitmap.Height)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
    $bitmap.UnlockBits($data)
    $surface = 0
    $partial = 0
    for ($y = 0; $y -lt $bitmap.Height; $y++) {
        $row = $y * $data.Stride
        for ($x = 0; $x -lt $bitmap.Width; $x++) {
            $i = $row + $x * 3
            $b = $bytes[$i]; $g = $bytes[$i + 1]; $r = $bytes[$i + 2]
            if ($r -ne $g -or $g -ne $b) { continue }   # sky / grass: not a debug pixel
            if ($r -ge 126 -and $r -le 130) { continue } # turned away from the sun
            $surface++
            if ($r -gt 7 -and $r -lt 248) { $partial++ }
        }
    }
    $bitmap.Dispose()
    $share = if ($surface -gt 0) { 100.0 * $partial / $surface } else { 0.0 }
    '{0,-28} partial {1,8} px of {2,8} ({3,5:N1} %)' -f $file.Name, $partial, $surface, $share
}
