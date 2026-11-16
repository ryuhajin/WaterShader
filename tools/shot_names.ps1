# Old -> new names of the test scene shots (2026-11-17: Water Lab -> Deep Stair, Waterfront -> Shallow Rail).
# Capture sets from before the rename hold <preset>_lab_top.png etc.; the compare scripts pair them with
# <preset>_stair_top.png through these functions. Same table as kShotAliases in src/Graphics.cpp.
# Dot-source it: . (Join-Path $PSScriptRoot 'shot_names.ps1')

$ShotAliases = @{
    'lab_top'         = 'stair_top'
    'lab_stairs'      = 'stair_angle'
    'lab_low'         = 'stair_low'
    'lab_pillar'      = 'stair_pillar'
    'lab_pillar_near' = 'stair_pillar_near'
    'front_rails'     = 'rail_line'
}

# <preset>_<shot>.<ext> with an old shot name -> the same file name with the new shot name. Other names
# (slots, ocean shots, anything else) come back unchanged. Preset names have no underscore.
function ConvertTo-CurrentShotName([string]$FileName) {
    if ($FileName -match '^(?<preset>[^_]+)_(?<shot>.+?)(?<ext>\.[^.]+)$' -and $ShotAliases.ContainsKey($Matches.shot)) {
        return $Matches.preset + '_' + $ShotAliases[$Matches.shot] + $Matches.ext
    }
    return $FileName
}

# Path of the file in $Dir that is the same capture as $FileName, under either the old or the new shot name.
# Falls back to Join-Path $Dir $FileName (the plain same-name pairing) when there is no such file.
function Find-ShotFile([string]$Dir, [string]$FileName) {
    $plain = Join-Path $Dir $FileName
    if (-not $Dir -or (Test-Path -LiteralPath $plain)) { return $plain }
    $key = ConvertTo-CurrentShotName $FileName
    $match = Get-ChildItem -LiteralPath $Dir -File -ErrorAction SilentlyContinue |
        Where-Object { (ConvertTo-CurrentShotName $_.Name) -eq $key } | Select-Object -First 1
    if ($match) { return $match.FullName }
    return $plain
}
