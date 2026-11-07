# Summarizes --bench runs (timings.csv) per shot: the water section and the whole GPU frame, as the mean of the
# runs' medians with the [min - max] over runs, one column per case.
#
# Usage (PowerShell 5.1):
#   .\tools\summarize_bench.ps1 -Root C:\Users\da171\ws-capture\contact-foam\captures -Cases step6,foam0,foam -Prefix bench_
# reads <Root>\<Prefix><case>_run*\timings.csv for every case.
param(
    [Parameter(Mandatory = $true)][string]$Root,
    [Parameter(Mandatory = $true)][string[]]$Cases,
    [string]$Prefix = 'bench_',
    [ValidateSet('water', 'opaque', 'post', 'gpu')][string]$Section = 'water'
)

$column = @{ gpu = 3; opaque = 6; water = 7; post = 9 }[$Section]

function Read-Case([string]$case) {
    $rows = @{}
    foreach ($dir in Get-ChildItem $Root -Directory -Filter "$Prefix${case}_run*") {
        foreach ($line in Get-Content (Join-Path $dir.FullName 'timings.csv')) {
            if ($line -match '^#|^preset') { continue }
            $f = $line -split ','
            if (-not $rows.ContainsKey($f[1])) { $rows[$f[1]] = @{ part = @(); gpu = @() } }
            $rows[$f[1]].part += [double]$f[$column]
            $rows[$f[1]].gpu += [double]$f[3]
        }
    }
    return $rows
}

$data = [ordered]@{}
foreach ($case in $Cases) { $data[$case] = Read-Case $case }
$shots = @($data[$Cases[0]].Keys | Sort-Object)
Write-Output ("{0} section median ms (mean of runs) [min - max over runs] / whole GPU frame ms" -f $Section)
Write-Output (("{0,-16}" -f 'shot') + (($Cases | ForEach-Object { "{0,32}" -f $_ }) -join ''))
foreach ($shot in $shots) {
    $cells = foreach ($case in $Cases) {
        $row = $data[$case][$shot]
        if ($null -eq $row) { "{0,32}" -f '-'; continue }
        $p = $row.part | Measure-Object -Average -Minimum -Maximum
        $g = $row.gpu | Measure-Object -Average
        "{0,32}" -f ("{0:F4} [{1:F4}-{2:F4}] / {3:F3}" -f $p.Average, $p.Minimum, $p.Maximum, $g.Average)
    }
    Write-Output (("{0,-16}" -f $shot) + ($cells -join ''))
}
