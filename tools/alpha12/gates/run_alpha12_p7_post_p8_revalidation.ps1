param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"
$auditGate = Join-Path $PSScriptRoot "alpha12_precompiled_global_audit.ps1"
$activationGate = Join-Path $PSScriptRoot "alpha12_precompiled_release_activation.ps1"

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

Write-Host "=============================================================================="
Write-Host " ALPHA12 - P7 POST-P8 RELEASE-LIKE REVALIDATION"
Write-Host "=============================================================================="
Write-Host ("BRANCH=" + $branch)
Write-Host ("HEAD=" + $head)

if ($branch -ne $expectedBranch) {
    throw ("A12_P7_REVALIDATION_BRANCH_MISMATCH expected=" + $expectedBranch + " actual=" + $branch)
}

[string[]]$dirty = @(& git -C $repo status --short)
Write-Host ("ENTRY_DIRTY_COUNT=" + [string]$dirty.Count)

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host ("ENTRY_DIRTY=" + $_) }
    throw "A12_P7_REVALIDATION_TREE_NOT_CLEAN"
}

foreach ($gate in @($auditGate, $activationGate)) {
    if (-not (Test-Path -LiteralPath $gate)) {
        throw ("A12_P7_REVALIDATION_GATE_NOT_FOUND=" + $gate)
    }

    $tokens = $null
    $parseErrors = $null

    [System.Management.Automation.Language.Parser]::ParseFile(
        $gate,
        [ref]$tokens,
        [ref]$parseErrors
    ) | Out-Null

    Write-Host (
        "POWERSHELL_PARSE_" +
        [IO.Path]::GetFileName($gate) +
        "=" +
        $(if ($parseErrors.Count -eq 0) { "PASS" } else { "FAIL" })
    )

    if ($parseErrors.Count -ne 0) {
        $parseErrors | ForEach-Object { Write-Host $_.Message }
        throw "A12_P7_REVALIDATION_GATE_SYNTAX_INVALID"
    }
}

Write-Host ""
Write-Host "=== P7A POST-FREEZE GLOBAL AUDIT ==="
& $auditGate -ExpectedActivationState Active
if ($LASTEXITCODE -ne 0) {
    throw "A12_P7A_POST_FREEZE_FAILED"
}

Write-Host ""
Write-Host "=== P7B RELEASE-LIKE LINK/AUTOLOAD ==="
& $activationGate
if ($LASTEXITCODE -ne 0) {
    throw "A12_P7B_POST_FREEZE_FAILED"
}

[string[]]$finalDirty = @(& git -C $repo status --short)
Write-Host ""
Write-Host "=============================================================================="
Write-Host " SUMMARY"
Write-Host "=============================================================================="
Write-Host "P7A_POST_FREEZE_GLOBAL_AUDIT=PASS"
Write-Host "P7B_RELEASE_LIKE_ACTIVATION=PASS"
Write-Host "PRECOMPILED_ARCHIVE_IDENTITIES=PASS"
Write-Host "PRECOMPILED_POLICY_ACTIVE=PASS"
Write-Host "NORMAL_AUTOLOAD_COMPLETE=PASS"
Write-Host ("FINAL_DIRTY_COUNT=" + [string]$finalDirty.Count)

if ($finalDirty.Count -ne 0) {
    $finalDirty | ForEach-Object { Write-Host ("FINAL_DIRTY=" + $_) }
    throw "A12_P7_REVALIDATION_REPOSITORY_MUTATED"
}

Write-Host "ALPHA12_P7_POST_P8_REVALIDATION=PASS"
