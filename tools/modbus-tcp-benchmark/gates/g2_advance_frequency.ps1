param(
    [Parameter(Mandatory = $true)]
    [int]$FromMHz,

    [Parameter(Mandatory = $true)]
    [int]$ToMHz
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " G2 ADVANCE SPI FREQUENCY"
Write-Host "============================================================"

Assert-G2AllowedMHz -MHz $FromMHz
Assert-G2AllowedMHz -MHz $ToMHz
Assert-G2Branch

$allowedTransitions = @{
    20 = 24
    24 = 26
    26 = 30
}

if (-not $allowedTransitions.ContainsKey($FromMHz)) {
    throw "G2_ADVANCE_FROM_FREQUENCY_NOT_SUPPORTED=$FromMHz"
}

if ([int]$allowedTransitions[$FromMHz] -ne $ToMHz) {
    throw "G2_ADVANCE_TRANSITION_NOT_ALLOWED=${FromMHz}_TO_${ToMHz}"
}

$head = Get-G2Head
$currentHz = Get-G2SpiHz
$currentMHz = [int]($currentHz / 1000000)
$dirtyBefore = @(Get-G2TrackedDirtyPaths)

Write-Host "HEAD=$head"
Write-Host "FROM_MHZ=$FromMHz"
Write-Host "TO_MHZ=$ToMHz"
Write-Host "CURRENT_SPI_HZ=$currentHz"
Write-Host "CURRENT_SPI_MHZ=$currentMHz"
Write-Host "TRACKED_DIRTY_BEFORE=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY_BEFORE=$_" }

if ($currentMHz -ne $FromMHz) {
    throw "G2_ADVANCE_CURRENT_FREQUENCY_MISMATCH"
}

if (
    $dirtyBefore.Count -ne 1 -or
    $dirtyBefore[0] -ne $script:G2SpiHeaderRelative
) {
    throw "G2_ADVANCE_UNEXPECTED_DIRTY_STATE"
}

$stagedBefore = @(
    & git -C $script:G2RepoRoot diff --cached --name-only
)

if ($LASTEXITCODE -ne 0) {
    throw "G2_ADVANCE_CACHED_DIFF_FAILED"
}

Write-Host "STAGED_COUNT_BEFORE=$($stagedBefore.Count)"

if ($stagedBefore.Count -ne 0) {
    throw "G2_ADVANCE_INDEX_NOT_CLEAN"
}

$numstatBefore = @(
    & git -C $script:G2RepoRoot diff --numstat -- $script:G2SpiHeaderRelative
)

if ($LASTEXITCODE -ne 0 -or $numstatBefore.Count -ne 1) {
    throw "G2_ADVANCE_NUMSTAT_UNAVAILABLE"
}

Write-Host "FROM_SPI_DIFF_NUMSTAT=$($numstatBefore[0])"
$partsBefore = $numstatBefore[0] -split "`t"

if (
    $partsBefore.Count -lt 3 -or
    $partsBefore[0] -ne "1" -or
    $partsBefore[1] -ne "1"
) {
    throw "G2_ADVANCE_FROM_DIFF_NOT_SINGLE_LINE"
}

Assert-G2ProtectedArtifacts
Assert-G2RawBaselineArtifacts

Write-Host ""
Write-Host "=== RESTORE BASELINE SOURCE ==="
& git -C $script:G2RepoRoot restore --source=HEAD -- $script:G2SpiHeaderRelative

if ($LASTEXITCODE -ne 0) {
    throw "G2_ADVANCE_RESTORE_BASELINE_FAILED"
}

$dirtyRestored = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_AFTER_RESTORE=$($dirtyRestored.Count)"

if ($dirtyRestored.Count -ne 0) {
    $dirtyRestored | ForEach-Object { Write-Host "DIRTY_AFTER_RESTORE=$_" }
    throw "G2_ADVANCE_TREE_NOT_CLEAN_AFTER_RESTORE"
}

$baselineHz = Get-G2SpiHz
Write-Host "BASELINE_SPI_HZ=$baselineHz"

if ($baselineHz -ne 14000000) {
    throw "G2_ADVANCE_HEAD_BASELINE_NOT_14MHZ"
}

Write-Host ""
Write-Host "=== APPLY TARGET FREQUENCY ==="
$targetHz = [int64]$ToMHz * 1000000
Set-G2SpiHz -Hz $targetHz

$effectiveHz = Get-G2SpiHz
Write-Host "TARGET_SPI_HZ=$targetHz"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"

if ($effectiveHz -ne $targetHz) {
    throw "G2_ADVANCE_EFFECTIVE_SPI_HZ_MISMATCH"
}

$dirtyAfter = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_AFTER=$($dirtyAfter.Count)"
$dirtyAfter | ForEach-Object { Write-Host "DIRTY_AFTER=$_" }

if (
    $dirtyAfter.Count -ne 1 -or
    $dirtyAfter[0] -ne $script:G2SpiHeaderRelative
) {
    throw "G2_ADVANCE_FINAL_DIRTY_STATE_INVALID"
}

$numstatAfter = @(
    & git -C $script:G2RepoRoot diff --numstat -- $script:G2SpiHeaderRelative
)

if ($LASTEXITCODE -ne 0 -or $numstatAfter.Count -ne 1) {
    throw "G2_ADVANCE_TARGET_NUMSTAT_UNAVAILABLE"
}

Write-Host "TO_SPI_DIFF_NUMSTAT=$($numstatAfter[0])"
$partsAfter = $numstatAfter[0] -split "`t"

if (
    $partsAfter.Count -lt 3 -or
    $partsAfter[0] -ne "1" -or
    $partsAfter[1] -ne "1"
) {
    throw "G2_ADVANCE_TO_DIFF_NOT_SINGLE_LINE"
}

& git -C $script:G2RepoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw "G2_ADVANCE_DIFF_CHECK_FAILED"
}

Assert-G2ProtectedArtifacts
Assert-G2RawBaselineArtifacts

Write-Host ""
Write-Host "=== STATIC TARGET VALIDATION ==="
& (Join-Path $PSScriptRoot "g2_validate_frequency.ps1") -MHz $ToMHz

Write-Host ""
Write-Host "G2_ADVANCE_FREQUENCY=PASS"
