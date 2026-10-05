param(
    [Parameter(Mandatory = $true)]
    [int]$MHz
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " G2 VALIDATE STATIC FREQUENCY"
Write-Host "============================================================"

Assert-G2AllowedMHz -MHz $MHz
Assert-G2Branch

$head = Get-G2Head
$targetHz = [int64]$MHz * 1000000
$effectiveHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)

Write-Host "HEAD=$head"
Write-Host "TARGET_MHZ=$MHz"
Write-Host "TARGET_SPI_HZ=$targetHz"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }

if ($effectiveHz -ne $targetHz) {
    throw "G2_STATIC_EFFECTIVE_SPI_HZ_MISMATCH"
}

if ($dirty.Count -gt 1) {
    throw "G2_STATIC_TOO_MANY_TRACKED_CHANGES"
}

if ($dirty.Count -eq 1 -and $dirty[0] -ne $script:G2SpiHeaderRelative) {
    throw "G2_STATIC_UNEXPECTED_TRACKED_CHANGE"
}

$staged = @(
    & git -C $script:G2RepoRoot diff --cached --name-only
)

if ($LASTEXITCODE -ne 0) {
    throw "G2_STATIC_CACHED_DIFF_FAILED"
}

Write-Host "STAGED_COUNT=$($staged.Count)"

if ($staged.Count -ne 0) {
    throw "G2_STATIC_INDEX_NOT_CLEAN"
}

& git -C $script:G2RepoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw "G2_STATIC_DIFF_CHECK_FAILED"
}

if ($dirty.Count -eq 1) {
    $numstat = @(
        & git -C $script:G2RepoRoot diff --numstat -- $script:G2SpiHeaderRelative
    )

    if ($LASTEXITCODE -ne 0 -or $numstat.Count -ne 1) {
        throw "G2_STATIC_NUMSTAT_UNAVAILABLE"
    }

    Write-Host "SPI_DIFF_NUMSTAT=$($numstat[0])"

    $parts = $numstat[0] -split "`t"
    if ($parts.Count -lt 3 -or $parts[0] -ne "1" -or $parts[1] -ne "1") {
        throw "G2_STATIC_SPI_DIFF_NOT_SINGLE_LINE"
    }
}
else {
    if ($MHz -ne 14) {
        throw "G2_STATIC_EXPECTED_SPI_DIFF_MISSING"
    }

    Write-Host "SPI_DIFF_NUMSTAT=NONE_BASELINE"
}

Assert-G2ProtectedArtifacts
Assert-G2RawBaselineArtifacts

Write-Host "TCP_SPI_HOLD_BUDGET_US=$script:G2HoldBudgetUs"
Write-Host "NO_BUILD=YES"
Write-Host "NO_UPLOAD=YES"
Write-Host "NO_BENCHMARK=YES"
Write-Host "G2_VALIDATE_STATIC=PASS"
