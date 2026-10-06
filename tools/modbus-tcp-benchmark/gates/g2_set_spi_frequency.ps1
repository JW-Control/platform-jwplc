param(
    [Parameter(Mandatory = $true)]
    [int]$MHz
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " G2 SET SPI FREQUENCY"
Write-Host "============================================================"

Assert-G2AllowedMHz -MHz $MHz
Assert-G2Branch

$head = Get-G2Head
$dirtyBefore = @(Get-G2TrackedDirtyPaths)

Write-Host "HEAD=$head"
Write-Host "TARGET_MHZ=$MHz"
Write-Host "TRACKED_DIRTY_BEFORE=$($dirtyBefore.Count)"

if ($dirtyBefore.Count -ne 0) {
    $dirtyBefore | ForEach-Object { Write-Host "DIRTY_BEFORE=$_" }
    throw "G2_TRACKED_TREE_NOT_CLEAN_BEFORE_SET"
}

Assert-G2ProtectedArtifacts
Assert-G2RawBaselineArtifacts

$currentHz = Get-G2SpiHz
$currentMHz = [int]($currentHz / 1000000)
$targetHz = [int64]$MHz * 1000000

Write-Host "CURRENT_SPI_HZ=$currentHz"
Write-Host "CURRENT_SPI_MHZ=$currentMHz"
Write-Host "TARGET_SPI_HZ=$targetHz"

if ($currentHz -eq $targetHz) {
    Write-Host "SPI_CHANGE=NO_CHANGE"
}
else {
    Set-G2SpiHz -Hz $targetHz
    Write-Host "SPI_CHANGE=APPLIED"
}

$effectiveHz = Get-G2SpiHz
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"

if ($effectiveHz -ne $targetHz) {
    throw "G2_EFFECTIVE_SPI_HZ_MISMATCH"
}

$dirtyAfter = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_AFTER=$($dirtyAfter.Count)"
$dirtyAfter | ForEach-Object { Write-Host "DIRTY_AFTER=$_" }

if ($dirtyAfter.Count -gt 1) {
    throw "G2_TOO_MANY_TRACKED_CHANGES"
}

if ($dirtyAfter.Count -eq 1 -and $dirtyAfter[0] -ne $script:G2SpiHeaderRelative) {
    throw "G2_UNEXPECTED_TRACKED_CHANGE"
}

& git -C $script:G2RepoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw "G2_DIFF_CHECK_FAILED"
}

Assert-G2ProtectedArtifacts
Assert-G2RawBaselineArtifacts

Write-Host "G2_SET_SPI_FREQUENCY=PASS"
