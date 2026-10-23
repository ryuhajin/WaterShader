# Byte-for-byte comparison of two capture sets (regression and identity checks).
#   -A          reference set (e.g. ws-capture\waterfront\captures\baseline, or the "water off" set)
#   -B          set to check   (captured into a temporary --capture-root)
#   -KeepDiffTo where to keep the images that differ: <name>.A.<ext> and <name>.B.<ext> side by side.
#               Nothing is written when every image matches.
# Pairs the .png / .jpg files with the same name. Prints one summary line for NOTES and returns
# exit code 0 when all match, 1 otherwise (missing files count as differences).
#
# Usage (PowerShell 5.1), CONVENTIONS.md "회귀·동일성 캡처":
#   $tmp = Join-Path $env:TEMP 'ws-check'
#   .\build\vs2022\Release\WaterShader.exe --capture regress --capture-feature <f> --capture-root $tmp --capture-shots <shots>
#   .\tools\compare_sets.ps1 -A C:\Users\da171\ws-capture\waterfront\captures\baseline -B "$tmp\<f>\captures\regress" -KeepDiffTo C:\Users\da171\ws-capture\<f>\captures\regress_diff
#   Remove-Item -Recurse -Force $tmp
param(
    [Parameter(Mandatory = $true)][string]$A,
    [Parameter(Mandatory = $true)][string]$B,
    [string]$KeepDiffTo = ''
)

$images = { param($dir) Get-ChildItem -LiteralPath $dir -File | Where-Object { $_.Extension -in '.png', '.jpg' } }
$filesA = @(& $images $A)
$filesB = @{}
foreach ($file in & $images $B) { $filesB[$file.Name] = $file }

$same = 0
$diff = @()
foreach ($fileA in $filesA) {
    $fileB = $filesB[$fileA.Name]
    if ($null -eq $fileB) {
        $diff += "$($fileA.Name) (missing in B)"
        continue
    }
    $filesB.Remove($fileA.Name)
    $hashA = (Get-FileHash -LiteralPath $fileA.FullName -Algorithm SHA256).Hash
    $hashB = (Get-FileHash -LiteralPath $fileB.FullName -Algorithm SHA256).Hash
    if ($hashA -eq $hashB) {
        $same++
        continue
    }
    $diff += $fileA.Name
    if ($KeepDiffTo) {
        New-Item -ItemType Directory -Force -Path $KeepDiffTo | Out-Null
        $stem = [IO.Path]::GetFileNameWithoutExtension($fileA.Name)
        Copy-Item -LiteralPath $fileA.FullName -Destination (Join-Path $KeepDiffTo "$stem.A$($fileA.Extension)")
        Copy-Item -LiteralPath $fileB.FullName -Destination (Join-Path $KeepDiffTo "$stem.B$($fileB.Extension)")
    }
}
foreach ($name in $filesB.Keys) { $diff += "$name (missing in A)" }

$total = $same + $diff.Count
if ($diff.Count -eq 0) {
    Write-Output "$total images byte-identical ($same / $total)"
    exit 0
}
Write-Output "$($diff.Count) of $total images differ:"
$diff | ForEach-Object { Write-Output "  $_" }
if ($KeepDiffTo) { Write-Output "Kept the differing pairs in $KeepDiffTo" }
exit 1
