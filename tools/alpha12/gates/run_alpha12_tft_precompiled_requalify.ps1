param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$gate = Join-Path $PSScriptRoot "alpha12_tft_precompiled_requalify.ps1"

if (-not (Test-Path -LiteralPath $gate)) {
    throw "A12_TFT_RUNNER_GATE_NOT_FOUND"
}

Write-Host "=============================================================================="
Write-Host " ALPHA12 - P6 JWPLC_TFT SAFE RUNNER"
Write-Host "=============================================================================="

$tokens = $null
$parseErrors = $null

[System.Management.Automation.Language.Parser]::ParseFile(
    $gate,
    [ref]$tokens,
    [ref]$parseErrors
) | Out-Null

Write-Host ("P6_GATE_POWERSHELL_SYNTAX_ERROR_COUNT=" + [string]$parseErrors.Count)

if ($parseErrors.Count -ne 0) {
    foreach ($parseError in $parseErrors) {
        Write-Host (
            "P6_GATE_POWERSHELL_SYNTAX_ERROR=" +
            $parseError.Message +
            " @ " +
            [string]$parseError.Extent.StartLineNumber +
            ":" +
            [string]$parseError.Extent.StartColumnNumber
        )
    }

    throw "A12_TFT_RUNNER_GATE_SYNTAX_INVALID"
}

Write-Host "P6_GATE_POWERSHELL_SYNTAX=PASS"

$toolDir = Join-Path $env:LOCALAPPDATA "Arduino15\packages\jwplc_local\tools\esp-x32\2601\bin"
$archiver = Join-Path $toolDir "xtensa-esp32-elf-gcc-ar.exe"
$nm = Join-Path $toolDir "xtensa-esp32-elf-nm.exe"

$archiverExists = Test-Path -LiteralPath $archiver
$nmExists = Test-Path -LiteralPath $nm

Write-Host ("P6_PREFLIGHT_TOOL_DIR=" + $toolDir)
Write-Host ("P6_PREFLIGHT_GCC_AR_EXISTS=" + [string]$archiverExists)
Write-Host ("P6_PREFLIGHT_NM_EXISTS=" + [string]$nmExists)

if (-not $archiverExists) {
    throw "A12_TFT_RUNNER_GCC_AR_NOT_FOUND"
}

if (-not $nmExists) {
    throw "A12_TFT_RUNNER_NM_NOT_FOUND"
}

Write-Host ("P6_PREFLIGHT_GCC_AR=" + (Resolve-Path -LiteralPath $archiver).Path)
Write-Host ("P6_PREFLIGHT_NM=" + (Resolve-Path -LiteralPath $nm).Path)
Write-Host "P6_RUNNER_PREFLIGHT=PASS"

Write-Host ""
Write-Host "=== P6 PRODUCT GATE ==="

& $gate

Write-Host ""
Write-Host "P6_SAFE_RUNNER=PASS"
