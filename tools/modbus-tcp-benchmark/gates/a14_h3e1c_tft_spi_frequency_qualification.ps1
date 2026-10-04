param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.1C - TFT SPI 80 VS 40 MHz"
Write-Host "============================================================"
Assert-G2Branch

$baselineHz = 80000000
$targetHz = 40000000
$baselineDisplayAvgUs = 8575.0
$baselineDisplayMaxUs = 9840.0
$baselineRtuHz = 772.962
$baselineTcpAvgUs = 1219.3
$baselineTcpP99Us = 9817.5
$baselineServiceGapUs = 17315.0
$historicalDisplayHash = "2974D42C847C1B7C7AB3A7B74DA42E2F17969FB852B47A8D434F57F70DA924AF"
$spiHeaderRelative = "JWPLC/2.1.0/cores/jwcontrol/peripherals/include/jwplc_spi_bus.h"
$spiHeaderPath = Get-G2Path $spiHeaderRelative
$legPath = Join-Path $PSScriptRoot "a14_h3e1b1_display_linkage_leg.ps1"

function Get-H3E1CTftHz {
    param([string]$Path)
    $text = [IO.File]::ReadAllText($Path)
    $m = @([regex]::Matches($text, '(?m)^[ \t]*#define[ \t]+JWPLC_SPI_TFT_HZ[ \t]+(\d+)UL[ \t]*(?=\r?$)'))
    if ($m.Count -ne 1) { throw "H3E1C_TFT_SPI_DEFINE_COUNT_INVALID" }
    return [int]$m[0].Groups[1].Value
}

function Get-H3E1CMetric {
    param([string]$Text, [string]$Pattern, [string]$Label)
    $m = [regex]::Match($Text, $Pattern)
    if (-not $m.Success) { throw "H3E1C_METRIC_MISSING=$Label" }
    return [double]::Parse($m.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
}

if (-not (Test-Path -LiteralPath $spiHeaderPath)) { throw "H3E1C_SPI_HEADER_MISSING" }
if (-not (Test-Path -LiteralPath $legPath)) { throw "H3E1C_LEG_MISSING" }
$entryHz = Get-H3E1CTftHz -Path $spiHeaderPath
if ($entryHz -ne $baselineHz) { throw "H3E1C_ENTRY_TFT_SPI_HZ_NOT_80MHZ" }

[string[]]$entryDirty = @(Get-G2TrackedDirtyPaths)
[string[]]$entryStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
[string[]]$normalizedEntryDirty = @($entryDirty | ForEach-Object { $_.Replace("\", "/") } | Sort-Object)
$expectedCoreDirty = $script:G2CoreRelative.Replace("\", "/")
if ($normalizedEntryDirty.Count -ne 1 -or $normalizedEntryDirty[0] -ne $expectedCoreDirty) {
    $normalizedEntryDirty | ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }
    throw "H3E1C_ENTRY_DIRTY_SCOPE_INVALID"
}
if ($entryStaged.Count -ne 0) { throw "H3E1C_ENTRY_INDEX_NOT_CLEAN" }

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E1C_BASELINE_TFT_SPI_HZ=$baselineHz"
Write-Host "H3E1C_TARGET_TFT_SPI_HZ=$targetHz"
Write-Host "H3E1C_DISPLAY_LINKAGE=SOURCE"
Write-Host "H3E1C_ONE_PRIMARY_VARIABLE=TFT_SPI_FREQUENCY"
Write-Host "H3E1C_BASELINE_DISPLAY_AVG_US=$baselineDisplayAvgUs"
Write-Host "H3E1C_BASELINE_DISPLAY_MAX_US=$baselineDisplayMaxUs"

if ($PreflightOnly) {
    $preflightArgs = @{
        MasterPort = $MasterPort
        SlavePort = $SlavePort
        DisplayLinkage = "SOURCE"
        ExpectedDisplayArchiveHash = $historicalDisplayHash
        TftSpiHz = $baselineHz
        PreflightOnly = $true
    }
    [object[]]$out = @(& $legPath @preflightArgs *>&1)
    $out | ForEach-Object { Write-Host $_ }
    $txt = ($out | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
    if (-not $txt.Contains("A14_H3E1B1_LINKAGE_LEG_PREFLIGHT_ONLY=PASS")) { throw "H3E1C_CHILD_PREFLIGHT_FAILED" }
    if (-not $txt.Contains("TFT_SPI_HZ=80000000")) { throw "H3E1C_PREFLIGHT_FREQ_MISMATCH" }
    Write-Host "H3E1C_PREFLIGHT_PATCHES_HEADER=NO"
    Write-Host "H3E1C_PREFLIGHT_COMPILES=NO"
    Write-Host "H3E1C_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E1C_PREFLIGHT_RUNS_300S=NO"
    Write-Host "A14_H3E1C_TFT_SPI_FREQUENCY_PREFLIGHT_ONLY=PASS"
    return
}

$originalBytes = [IO.File]::ReadAllBytes($spiHeaderPath)
$originalHash = Get-G2Sha256Path -Path $spiHeaderPath
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h3e1c_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
$runLog = Join-Path $tempRoot "h3e1c_40mhz.log"

try {
    $headerText = [IO.File]::ReadAllText($spiHeaderPath)
    $anchor = "#define JWPLC_SPI_TFT_HZ   80000000UL"
    $replacement = "#define JWPLC_SPI_TFT_HZ   40000000UL"
    $anchorCount = [regex]::Matches($headerText, [regex]::Escape($anchor)).Count
    Write-Host "H3E1C_PATCH_ANCHOR_COUNT=$anchorCount"
    if ($anchorCount -ne 1) { throw "H3E1C_PATCH_ANCHOR_COUNT_INVALID" }
    $patchedText = $headerText.Replace($anchor, $replacement)
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($spiHeaderPath, $patchedText, $utf8)
    if ((Get-H3E1CTftHz -Path $spiHeaderPath) -ne $targetHz) { throw "H3E1C_PATCH_FAILED" }
    Write-Host "H3E1C_TEMP_HEADER_PATCH=PASS"
    Write-Host "H3E1C_EFFECTIVE_TFT_SPI_HZ=$targetHz"

    $legArgs = @{
        MasterPort = $MasterPort
        SlavePort = $SlavePort
        DisplayLinkage = "SOURCE"
        ExpectedDisplayArchiveHash = $historicalDisplayHash
        AdditionalAllowedDirtyPaths = @($spiHeaderRelative)
        TftSpiHz = $targetHz
    }
    & $legPath @legArgs *>&1 | Tee-Object -FilePath $runLog
    $runText = [IO.File]::ReadAllText($runLog)
    if (-not $runText.Contains("A14_H3E1B1_DISPLAY_LINKAGE_LEG_GATE=PASS")) { throw "H3E1C_CHILD_PASS_MISSING" }
    if (-not $runText.Contains("RTUH3E0B_RUNTIME_CLEAN=YES")) { throw "H3E1C_RUNTIME_NOT_CLEAN" }

    $stage = [regex]::Match($runText, '(?m)^RTUH3E0B_STAGE NAME=SYS_DISPLAY CALLS=\d+ TOTAL_US=\d+ AVG_US=([0-9.]+) MAX_US=([0-9.]+)\r?$')
    if (-not $stage.Success) { throw "H3E1C_DISPLAY_STAGE_MISSING" }
    $avg40 = [double]::Parse($stage.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $max40 = [double]::Parse($stage.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    $rtu40 = Get-H3E1CMetric -Text $runText -Pattern '(?m)^RTUH3E0B_RTU_HZ=([0-9.]+)\r?$' -Label 'RTU_HZ'
    $tcpAvg40 = Get-H3E1CMetric -Text $runText -Pattern '(?m)^RTUH3E0B_TCP_AVG_US=([0-9.]+)\r?$' -Label 'TCP_AVG_US'
    $tcpP9940 = Get-H3E1CMetric -Text $runText -Pattern '(?m)^RTUH3E0B_TCP_P99_US=([0-9.]+)\r?$' -Label 'TCP_P99_US'
    $gap40 = Get-H3E1CMetric -Text $runText -Pattern '(?m)^RTUH3E0B_RTU_SERVICE_GAP_MAX_US=([0-9.]+)\r?$' -Label 'SERVICE_GAP_MAX_US'

    $ratio = $avg40 / $baselineDisplayAvgUs
    $deltaPct = (($avg40 - $baselineDisplayAvgUs) / $baselineDisplayAvgUs) * 100.0
    $spiUs80 = $avg40 - $baselineDisplayAvgUs
    $spiPct80 = ($spiUs80 / $baselineDisplayAvgUs) * 100.0
    $softwareFloorUs = (2.0 * $baselineDisplayAvgUs) - $avg40
    $class = if ($ratio -ge 1.70) { "STRONG_SPI_SENSITIVITY" } elseif ($ratio -ge 1.30) { "MIXED_SPI_AND_RENDER" } else { "WEAK_SPI_SENSITIVITY" }

    Write-Host "============================================================"
    Write-Host " H3E.1C 80 VS 40 MHz SUMMARY"
    Write-Host "============================================================"
    Write-Host ("H3E1C_80MHZ_DISPLAY_AVG_US={0:F1}" -f $baselineDisplayAvgUs)
    Write-Host ("H3E1C_40MHZ_DISPLAY_AVG_US={0:F1}" -f $avg40)
    Write-Host ("H3E1C_80MHZ_DISPLAY_MAX_US={0:F1}" -f $baselineDisplayMaxUs)
    Write-Host ("H3E1C_40MHZ_DISPLAY_MAX_US={0:F1}" -f $max40)
    Write-Host ("H3E1C_DISPLAY_AVG_RATIO_40_OVER_80={0:F4}" -f $ratio)
    Write-Host ("H3E1C_DISPLAY_AVG_DELTA_PCT={0:F3}" -f $deltaPct)
    Write-Host ("H3E1C_EST_SPI_SENSITIVE_US_AT_80={0:F1}" -f $spiUs80)
    Write-Host ("H3E1C_EST_SPI_SENSITIVE_PCT_AT_80={0:F2}" -f $spiPct80)
    Write-Host ("H3E1C_EST_SOFTWARE_FLOOR_US={0:F1}" -f $softwareFloorUs)
    Write-Host "H3E1C_SPI_SENSITIVITY_CLASS=$class"
    Write-Host ("H3E1C_80MHZ_RTU_HZ={0:F3}" -f $baselineRtuHz)
    Write-Host ("H3E1C_40MHZ_RTU_HZ={0:F3}" -f $rtu40)
    Write-Host ("H3E1C_80MHZ_TCP_AVG_US={0:F1}" -f $baselineTcpAvgUs)
    Write-Host ("H3E1C_40MHZ_TCP_AVG_US={0:F1}" -f $tcpAvg40)
    Write-Host ("H3E1C_80MHZ_TCP_P99_US={0:F1}" -f $baselineTcpP99Us)
    Write-Host ("H3E1C_40MHZ_TCP_P99_US={0:F1}" -f $tcpP9940)
    Write-Host ("H3E1C_80MHZ_SERVICE_GAP_MAX_US={0:F1}" -f $baselineServiceGapUs)
    Write-Host ("H3E1C_40MHZ_SERVICE_GAP_MAX_US={0:F1}" -f $gap40)
    Write-Host "H3E1C_RUNTIME_CLEAN=YES"
    Write-Host "H3E1C_DIAGNOSTIC_CAPTURE_VALID=YES"
}
finally {
    [IO.File]::WriteAllBytes($spiHeaderPath, $originalBytes)
    $restored = ((Get-G2Sha256Path -Path $spiHeaderPath) -eq $originalHash)
    Write-Host "H3E1C_SPI_HEADER_RESTORED=$(if ($restored) { "YES" } else { "NO" })"
    if (-not $restored) { throw "H3E1C_SPI_HEADER_RESTORE_FAILED" }
}

[string[]]$finalDirty = @(Get-G2TrackedDirtyPaths)
[string[]]$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
[string[]]$normalizedFinalDirty = @($finalDirty | ForEach-Object { $_.Replace("\", "/") } | Sort-Object)
if ($normalizedFinalDirty.Count -ne 1 -or $normalizedFinalDirty[0] -ne $expectedCoreDirty) { throw "H3E1C_FINAL_DIRTY_SCOPE_INVALID" }
if ($finalStaged.Count -ne 0) { throw "H3E1C_FINAL_INDEX_NOT_CLEAN" }
Write-Host "A14_H3E1C_TFT_SPI_FREQUENCY_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_JW_TFT_DIRECTION_DECISION"
