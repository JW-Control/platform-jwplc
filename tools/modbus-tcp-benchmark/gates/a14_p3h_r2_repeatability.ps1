param(
    [string]$SerialPort = "COM14",
    [double]$DurationSeconds = 5.0,
    [int]$Runs = 3
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$block = Join-Path $PSScriptRoot "a14_p3h_r2_single_ab_1016.ps1"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p3h_r2_repeatability_{0}" -f $timestamp)
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

function Invoke-Block {
    param([int]$Index)

    $log = Join-Path $tempRoot ("block_{0}.log" -f $Index)

    $args = @(
        "-NoLogo",
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $block,
        "-SerialPort", $SerialPort,
        "-DurationSeconds", ([string]$DurationSeconds),
        "-Runs", ([string]$Runs)
    )

    & powershell.exe @args *> $log
    $exit = [int]$LASTEXITCODE

    Write-Host "P3H_R2_BLOCK=$Index EXIT=$exit LOG=$log"

    if ($exit -ne 0) {
        Get-Content -LiteralPath $log -Tail 240 | ForEach-Object { Write-Host $_ }
        throw "P3H_R2_BLOCK_FAILED_$Index"
    }

    return $log
}

function Get-Results {
    param([string]$LogPath, [string]$Variant)

    $text = [System.IO.File]::ReadAllText($LogPath)

    $pattern = (
        "P3H_RESULT VARIANT=" +
        [regex]::Escape($Variant) +
        " PAYLOAD=1016 RUN=\d+ DUT_MBPS=([0-9.]+)"
    )

    $matches = @([regex]::Matches($text, $pattern))

    if ($matches.Count -ne $Runs) {
        throw (
            "P3H_R2_RESULT_COUNT_{0}_{1}={2}" -f
            (Split-Path $LogPath -Leaf),
            $Variant,
            $matches.Count
        )
    }

    $values = New-Object System.Collections.Generic.List[double]

    foreach ($m in $matches) {
        $values.Add(
            [double]::Parse(
                $m.Groups[1].Value,
                [System.Globalization.CultureInfo]::InvariantCulture
            )
        )
    }

    return $values.ToArray()
}

function Get-Median {
    param([double[]]$Values)

    $sorted = @($Values | Sort-Object)
    $n = $sorted.Count

    if ($n -eq 0) {
        throw "P3H_R2_MEDIAN_EMPTY"
    }

    if (($n % 2) -eq 1) {
        return [double]$sorted[[int][math]::Floor($n / 2)]
    }

    $a = [double]$sorted[($n / 2) - 1]
    $b = [double]$sorted[$n / 2]
    return ($a + $b) / 2.0
}

Write-Host "============================================================"
Write-Host " A14 P3H-R2 - REPEATABILITY FUSED VS LEGACY @ 1016 B"
Write-Host "============================================================"
Write-Host "SEQUENCE=FUSED,LEGACY,FUSED,LEGACY"
Write-Host "BLOCKS=2"
Write-Host "RUNS_PER_VARIANT_PER_BLOCK=$Runs"
Write-Host "TOTAL_RUNS_PER_VARIANT=$($Runs * 2)"
Write-Host "SERIAL_RESET_GUARD=REQUIRED"
Write-Host "SPI_HZ=26000000"
Write-Host "SOCKET_TOPOLOGY=8x2KB"
Write-Host "BATCH=2"
Write-Host "INT_ETH=GPIO15"

$log1 = Invoke-Block -Index 1
$log2 = Invoke-Block -Index 2

$fused1 = @(Get-Results -LogPath $log1 -Variant "INT_FUSED")
$legacy1 = @(Get-Results -LogPath $log1 -Variant "INT_LEGACY")
$fused2 = @(Get-Results -LogPath $log2 -Variant "INT_FUSED")
$legacy2 = @(Get-Results -LogPath $log2 -Variant "INT_LEGACY")

$fusedAll = @($fused1 + $fused2)
$legacyAll = @($legacy1 + $legacy2)

$fusedBlock1 = Get-Median -Values $fused1
$fusedBlock2 = Get-Median -Values $fused2
$legacyBlock1 = Get-Median -Values $legacy1
$legacyBlock2 = Get-Median -Values $legacy2
$fusedMedian = Get-Median -Values $fusedAll
$legacyMedian = Get-Median -Values $legacyAll

$fusedGainPct = (($fusedMedian / $legacyMedian) - 1.0) * 100.0
$fusedDriftPct = ([math]::Abs($fusedBlock2 - $fusedBlock1) / $fusedMedian) * 100.0
$legacyDriftPct = ([math]::Abs($legacyBlock2 - $legacyBlock1) / $legacyMedian) * 100.0

$p3gIntReference = 12.586247
$tcpRxReference = 13.798000

$fusedVsP3gPct = (($fusedMedian / $p3gIntReference) - 1.0) * 100.0
$fusedVsTcpPct = ($fusedMedian / $tcpRxReference) * 100.0
$tcpGapPct = (1.0 - ($fusedMedian / $tcpRxReference)) * 100.0

Write-Host ""
Write-Host "============================================================"
Write-Host " P3H-R2 SUMMARY"
Write-Host "============================================================"
Write-Host ("P3H_R2_FUSED_BLOCK1_MEDIAN_MBPS={0:F6}" -f $fusedBlock1)
Write-Host ("P3H_R2_FUSED_BLOCK2_MEDIAN_MBPS={0:F6}" -f $fusedBlock2)
Write-Host ("P3H_R2_LEGACY_BLOCK1_MEDIAN_MBPS={0:F6}" -f $legacyBlock1)
Write-Host ("P3H_R2_LEGACY_BLOCK2_MEDIAN_MBPS={0:F6}" -f $legacyBlock2)
Write-Host ("P3H_R2_FUSED_MEDIAN_MBPS={0:F6}" -f $fusedMedian)
Write-Host ("P3H_R2_LEGACY_MEDIAN_MBPS={0:F6}" -f $legacyMedian)
Write-Host ("P3H_R2_FUSED_GAIN_VS_LEGACY_PCT={0:F2}" -f $fusedGainPct)
Write-Host ("P3H_R2_FUSED_BLOCK_DRIFT_PCT={0:F2}" -f $fusedDriftPct)
Write-Host ("P3H_R2_LEGACY_BLOCK_DRIFT_PCT={0:F2}" -f $legacyDriftPct)
Write-Host ("P3H_R2_FUSED_VS_P3G_INT_REFERENCE_PCT={0:F2}" -f $fusedVsP3gPct)
Write-Host ("P3H_R2_FUSED_AS_TCP_REFERENCE_PCT={0:F2}" -f $fusedVsTcpPct)
Write-Host ("P3H_R2_TCP_REFERENCE_GAP_PCT={0:F2}" -f $tcpGapPct)

$interpretation = "INCONCLUSIVE"

if (
    $fusedGainPct -ge 2.0 -and
    $fusedDriftPct -le 3.0 -and
    $legacyDriftPct -le 5.0
) {
    $interpretation = "FUSED_REPEATABLE_GAIN"
}
elseif (
    $fusedDriftPct -gt 3.0 -or
    $legacyDriftPct -gt 5.0
) {
    $interpretation = "BASELINE_OR_FUSED_DRIFT_TOO_HIGH"
}
elseif ($fusedGainPct -lt 2.0) {
    $interpretation = "FUSED_GAIN_BELOW_2PCT"
}

Write-Host "P3H_R2_INTERPRETATION=$interpretation"
Write-Host "A14_P3H_R2_REPEATABILITY=PASS"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT_FOR_P3H_PRODUCTIZATION_DECISION"
