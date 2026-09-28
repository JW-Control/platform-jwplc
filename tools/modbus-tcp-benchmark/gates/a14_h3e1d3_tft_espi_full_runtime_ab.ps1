param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [switch]$PreflightOnly,
    [double]$DurationS = 300.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.1D.3 - FULL RUNTIME A/B TFT_eSPI"
Write-Host "============================================================"

Assert-G2Branch

$masterDirRelative = "tools/modbus-tcp-benchmark/firmware/a14_h3e1d3_tft_espi_full_runtime_master"
$masterSketchName = "a14_h3e1d3_tft_espi_full_runtime_master.ino"
$masterDir = Get-G2Path $masterDirRelative
$masterSketch = Join-Path $masterDir $masterSketchName
$tftSetup = Join-Path $masterDir "tft_setup.h"
$leg = Join-Path $PSScriptRoot "a14_h3e1b1_display_linkage_leg.ps1"
$d0 = Join-Path $PSScriptRoot "a14_h3e1d0_tft_espi_env_preflight.ps1"

$displayArchiveHash = "2974D42C847C1B7C7AB3A7B74DA42E2F17969FB852B47A8D434F57F70DA924AF"
$diagDefine = "compiler.cpp.extra_flags=-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1"
[string[]]$masterBuildProperties = @($diagDefine)

$baselineDisplayAvgUs = 8575.0
$baselineDisplayMaxUs = 9840.0
$baselineRtuHz = 772.962
$baselineTcpAvgUs = 1219.3
$baselineTcpP99Us = 9817.5
$baselineServiceGapUs = 17315.0

foreach ($required in @($masterSketch, $tftSetup, $leg, $d0)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E1D3_REQUIRED_PATH_MISSING=$required"
    }
}

$masterText = [IO.File]::ReadAllText($masterSketch)
$setupText = [IO.File]::ReadAllText($tftSetup)

$contracts = @(
    [PSCustomObject]@{ Label="BACKEND_INCLUDE"; Pass=$masterText.Contains("#include <TFT_eSPI.h>") },
    [PSCustomObject]@{ Label="NO_JWPLC_DISPLAY_INCLUDE"; Pass=(-not $masterText.Contains("#include <JWPLC_Display.h>")) },
    [PSCustomObject]@{ Label="DISPLAY_BEGIN_HOOK"; Pass=$masterText.Contains("jwplcDisplayBeginCallback") },
    [PSCustomObject]@{ Label="DISPLAY_REFRESH_HOOK"; Pass=$masterText.Contains("jwplcDisplayRefreshCallback") },
    [PSCustomObject]@{ Label="BATCH_STARTWRITE"; Pass=$masterText.Contains("h3e1d3Tft.startWrite()") },
    [PSCustomObject]@{ Label="BATCH_ENDWRITE"; Pass=$masterText.Contains("h3e1d3Tft.endWrite()") },
    [PSCustomObject]@{ Label="PROFILE_H3E0B"; Pass=$masterText.Contains("H3E0B_PROFILER=ENABLED") },
    [PSCustomObject]@{ Label="ROTATION_1"; Pass=$masterText.Contains("h3e1d3Tft.setRotation(1)") },
    [PSCustomObject]@{ Label="TFT_BGR"; Pass=$setupText.Contains("#define TFT_RGB_ORDER TFT_BGR") },
    [PSCustomObject]@{ Label="TFT_80MHZ"; Pass=$setupText.Contains("#define SPI_FREQUENCY 80000000") },
    [PSCustomObject]@{ Label="TFT_MODE0"; Pass=$setupText.Contains("#define TFT_SPI_MODE SPI_MODE0") },
    [PSCustomObject]@{ Label="TRANSACTIONS"; Pass=$setupText.Contains("#define SUPPORT_TRANSACTIONS") }
)

foreach ($contract in $contracts) {
    Write-Host "H3E1D3_CONTRACT_$($contract.Label)=$($contract.Pass)"
    if (-not $contract.Pass) {
        throw "H3E1D3_CONTRACT_FAILED_$($contract.Label)"
    }
}

[object[]]$d0Output = @(& $d0 *>&1)
$d0Output | ForEach-Object { Write-Host $_ }
$d0Text = ($d0Output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
if (-not $d0Text.Contains("A14_H3E1D0_TFT_ESPI_ENV_PREFLIGHT=PASS")) {
    throw "H3E1D3_D0_PREFLIGHT_FAILED"
}

Write-Host "H3E1D3_BASELINE_BACKEND=ADAFRUIT_SOURCE_80MHZ"
Write-Host "H3E1D3_CANDIDATE_BACKEND=TFT_eSPI_2.5.43"
Write-Host "H3E1D3_BASELINE_DISPLAY_AVG_US=$baselineDisplayAvgUs"
Write-Host "H3E1D3_BASELINE_DISPLAY_MAX_US=$baselineDisplayMaxUs"
Write-Host "H3E1D3_BASELINE_RTU_HZ=$baselineRtuHz"
Write-Host "H3E1D3_BASELINE_TCP_AVG_US=$baselineTcpAvgUs"
Write-Host "H3E1D3_BASELINE_TCP_P99_US=$baselineTcpP99Us"
Write-Host "H3E1D3_BASELINE_SERVICE_GAP_MAX_US=$baselineServiceGapUs"
Write-Host "H3E1D3_ONE_PRIMARY_VARIABLE=DISPLAY_BACKEND_RENDERER"
Write-Host "H3E1D3_MASTER_BUILD_PROPERTY=$diagDefine"

$commonArgs = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    DisplayLinkage = "ARCHIVE"
    ExpectedDisplayArchiveHash = $displayArchiveHash
    TftSpiHz = 80000000
    MasterDirRelative = $masterDirRelative
    MasterSketchName = $masterSketchName
    MasterBuildProperties = $masterBuildProperties
}

if ($PreflightOnly) {
    [object[]]$preflight = @(
        & $leg @commonArgs -PreflightOnly *>&1
    )

    $preflight | ForEach-Object { Write-Host $_ }
    $preflightText =
        ($preflight | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

    if (-not $preflightText.Contains("A14_H3E1B1_LINKAGE_LEG_PREFLIGHT_ONLY=PASS")) {
        throw "H3E1D3_LEG_PREFLIGHT_FAILED"
    }
    if (-not $preflightText.Contains("MASTER_DIR_RELATIVE=$masterDirRelative")) {
        throw "H3E1D3_MASTER_PATH_NOT_PROVEN"
    }
    if (-not $preflightText.Contains("MASTER_BUILD_PROPERTY_COUNT=1")) {
        throw "H3E1D3_BUILD_PROPERTY_NOT_PROVEN"
    }

    Write-Host "H3E1D3_PREFLIGHT_COMPILES=NO"
    Write-Host "H3E1D3_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E1D3_PREFLIGHT_RUNS_300S=NO"
    Write-Host "A14_H3E1D3_TFT_ESPI_FULL_RUNTIME_PREFLIGHT=PASS"
    return
}

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h3e1d3_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
$legLog = Join-Path $tempRoot "h3e1d3_leg.log"

& $leg @commonArgs -DurationS $DurationS *>&1 |
    Tee-Object -FilePath $legLog

$legText = [IO.File]::ReadAllText($legLog)

if (-not $legText.Contains("A14_H3E1B1_DISPLAY_LINKAGE_LEG_GATE=PASS")) {
    throw "H3E1D3_FULL_RUNTIME_LEG_FAILED"
}
if (-not $legText.Contains("RTUH3E0B_RUNTIME_CLEAN=YES")) {
    throw "H3E1D3_RUNTIME_NOT_CLEAN"
}

$stage = [regex]::Match(
    $legText,
    '(?m)^RTUH3E0B_STAGE NAME=SYS_DISPLAY CALLS=\d+ TOTAL_US=\d+ AVG_US=([0-9.]+) MAX_US=([0-9.]+)\r?$'
)
if (-not $stage.Success) {
    throw "H3E1D3_SYS_DISPLAY_STAGE_MISSING"
}

$candidateDisplayAvgUs = [double]::Parse(
    $stage.Groups[1].Value,
    [Globalization.CultureInfo]::InvariantCulture)
$candidateDisplayMaxUs = [double]::Parse(
    $stage.Groups[2].Value,
    [Globalization.CultureInfo]::InvariantCulture)

function Get-H3E1D3Metric {
    param([string]$Pattern, [string]$Label)
    $m = [regex]::Match($legText, $Pattern)
    if (-not $m.Success) {
        throw "H3E1D3_METRIC_MISSING=$Label"
    }
    return [double]::Parse(
        $m.Groups[1].Value,
        [Globalization.CultureInfo]::InvariantCulture)
}

$candidateRtuHz = Get-H3E1D3Metric '(?m)^RTUH3E0B_RTU_HZ=([0-9.]+)\r?$' "RTU_HZ"
$candidateTcpAvgUs = Get-H3E1D3Metric '(?m)^RTUH3E0B_TCP_AVG_US=([0-9.]+)\r?$' "TCP_AVG_US"
$candidateTcpP99Us = Get-H3E1D3Metric '(?m)^RTUH3E0B_TCP_P99_US=([0-9.]+)\r?$' "TCP_P99_US"
$candidateGapUs = Get-H3E1D3Metric '(?m)^RTUH3E0B_RTU_SERVICE_GAP_MAX_US=([0-9.]+)\r?$' "SERVICE_GAP_MAX_US"

$avgDeltaPct =
    (($candidateDisplayAvgUs - $baselineDisplayAvgUs) /
        $baselineDisplayAvgUs) * 100.0
$maxDeltaPct =
    (($candidateDisplayMaxUs - $baselineDisplayMaxUs) /
        $baselineDisplayMaxUs) * 100.0
$speedup =
    if ($candidateDisplayAvgUs -gt 0.0) {
        $baselineDisplayAvgUs / $candidateDisplayAvgUs
    }
    else {
        0.0
    }

$direction =
    if ($candidateDisplayAvgUs -le ($baselineDisplayAvgUs * 0.70)) {
        "STRONG_TFT_ESPI_GAIN"
    }
    elseif ($candidateDisplayAvgUs -le ($baselineDisplayAvgUs * 0.90)) {
        "MODERATE_TFT_ESPI_GAIN"
    }
    else {
        "NO_DECISIVE_GAIN"
    }

Write-Host ""
Write-Host "============================================================"
Write-Host " H3E.1D.3 A/B SUMMARY"
Write-Host "============================================================"
Write-Host ("H3E1D3_ADAFRUIT_DISPLAY_AVG_US={0:F1}" -f $baselineDisplayAvgUs)
Write-Host ("H3E1D3_TFT_ESPI_DISPLAY_AVG_US={0:F1}" -f $candidateDisplayAvgUs)
Write-Host ("H3E1D3_ADAFRUIT_DISPLAY_MAX_US={0:F1}" -f $baselineDisplayMaxUs)
Write-Host ("H3E1D3_TFT_ESPI_DISPLAY_MAX_US={0:F1}" -f $candidateDisplayMaxUs)
Write-Host ("H3E1D3_DISPLAY_AVG_DELTA_PCT={0:F3}" -f $avgDeltaPct)
Write-Host ("H3E1D3_DISPLAY_MAX_DELTA_PCT={0:F3}" -f $maxDeltaPct)
Write-Host ("H3E1D3_DISPLAY_SPEEDUP_X={0:F3}" -f $speedup)
Write-Host "H3E1D3_DIRECTION=$direction"
Write-Host ("H3E1D3_ADAFRUIT_RTU_HZ={0:F3}" -f $baselineRtuHz)
Write-Host ("H3E1D3_TFT_ESPI_RTU_HZ={0:F3}" -f $candidateRtuHz)
Write-Host ("H3E1D3_ADAFRUIT_TCP_AVG_US={0:F1}" -f $baselineTcpAvgUs)
Write-Host ("H3E1D3_TFT_ESPI_TCP_AVG_US={0:F1}" -f $candidateTcpAvgUs)
Write-Host ("H3E1D3_ADAFRUIT_TCP_P99_US={0:F1}" -f $baselineTcpP99Us)
Write-Host ("H3E1D3_TFT_ESPI_TCP_P99_US={0:F1}" -f $candidateTcpP99Us)
Write-Host ("H3E1D3_ADAFRUIT_SERVICE_GAP_MAX_US={0:F1}" -f $baselineServiceGapUs)
Write-Host ("H3E1D3_TFT_ESPI_SERVICE_GAP_MAX_US={0:F1}" -f $candidateGapUs)
Write-Host "H3E1D3_RUNTIME_CLEAN=YES"
Write-Host "A14_H3E1D3_TFT_ESPI_FULL_RUNTIME_AB_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_JW_TFT_MIGRATION_DECISION"
