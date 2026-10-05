param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$gate = Join-Path $PSScriptRoot "alpha12_precompiled_release_activation.ps1"

if (-not (Test-Path -LiteralPath $gate)) {
    throw "A12_P7B_RUNNER_GATE_NOT_FOUND"
}

Write-Host "=============================================================================="
Write-Host " ALPHA12 - P7B SAFE RUNNER"
Write-Host "=============================================================================="

$tokens = $null
$parseErrors = $null

[System.Management.Automation.Language.Parser]::ParseFile(
    $gate,
    [ref]$tokens,
    [ref]$parseErrors
) | Out-Null

Write-Host ("P7B_GATE_POWERSHELL_SYNTAX_ERROR_COUNT=" + [string]$parseErrors.Count)

if ($parseErrors.Count -ne 0) {
    foreach ($parseError in $parseErrors) {
        Write-Host (
            "P7B_GATE_POWERSHELL_SYNTAX_ERROR=" +
            $parseError.Message +
            " @ " +
            [string]$parseError.Extent.StartLineNumber +
            ":" +
            [string]$parseError.Extent.StartColumnNumber
        )
    }

    throw "A12_P7B_RUNNER_GATE_SYNTAX_INVALID"
}

Write-Host "P7B_GATE_POWERSHELL_SYNTAX=PASS"
Write-Host "P7B_RUNNER_PREFLIGHT=PASS"

& $gate

Write-Host ""
Write-Host "P7B_SAFE_RUNNER=PASS"
