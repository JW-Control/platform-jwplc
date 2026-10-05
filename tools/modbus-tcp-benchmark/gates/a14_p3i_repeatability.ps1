param(
    [string]$SerialPort = "COM14",
    [double]$DurationSeconds = 5.0,
    [int]$Runs = 3
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$block = Join-Path $PSScriptRoot "a14_p3i_single_ab_1016.ps1"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p3i_repeatability_{0}" -f $timestamp)
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

    Write-Host "P3I_BLOCK=$Index EXIT=$exit LOG=$log"

    if ($exit -ne 0) {
        Get-Content -LiteralPath $log -Tail 260 | ForEach-Object { Write-Host $_ }
        throw "P3I_BLOCK_FAILED_$Index"
    }

    return $log
}

function Get-Results {
    param([string]$LogPath, [string]$Variant)

    $text = [System.IO.File]::ReadAllText($LogPath)

    $pattern = (
        "P3I_RESULT VARIANT=" +
        [regex]::Escape($Variant) +
        " PAYLOAD=1016 RUN=\d+ DUT_MBPS=([0-9.]+)"
    )

    $matches = @([regex]::Matches($text, $pattern))

    if ($matches.Count -ne $Runs) {
        throw (
            "P3I_RESULT_COUNT_{0}_{1}={2}" -f
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
        throw "P3I_MEDIAN_EMPTY"
    }

    if (($n % 2) -eq 1) {
        return [double]$sorted[[int][math]::Floor($n / 2)]
    }

    return (
        [double]$sorted[($n / 2) - 1] +
        [double]$sorted[$n / 2]
    ) / 2.0
}

Write-Host "============================================================"
Write-Host " A14 P3I - REPEATABILITY SINGLE-CS BURST VS FUSED @ 1016 B"
Write-Host "============================================================"
Write-Host "SEQUENCE=BURST,FUSED,BURST,FUSED"
Write-Host "BLOCKS=2"
Write-Host "RUNS_PER_VARIANT_PER_BLOCK=$Runs"
Write-Host "TOTAL_RUNS_PER_VARIANT=$($Runs * 2)"
Write-Host "PAIRED_GAIN_IS_PRIMARY_METRIC=YES"
Write-Host "ABSOLUTE_DRIFT_IS_DIAGNOSTIC_ONLY=YES"
Write-Host "SPI_HZ=26000000"
Write-Host "SOCKET_TOPOLOGY=8x2KB"
Write-Host "BATCH=2"
Write-Host "INT_ETH=GPIO15"
Write-Host "FUSED_FAST_PATH=FROZEN"

$log1 = Invoke-Block -Index 1
$log2 = Invoke-Block -Index 2

$burst1 = @(Get-Results -LogPath $log1 -Variant "INT_BURST")
$fused1 = @(Get-Results -LogPath $log1 -Variant "INT_FUSED")
$burst2 = @(Get-Results -LogPath $log2 -Variant "INT_BURST")
$fused2 = @(Get-Results -LogPath $log2 -Variant "INT_FUSED")

$burstAll = @($burst1 + $burst2)
$fusedAll = @($fused1 + $fused2)

$burstBlock1 = Get-Median -Values $burst1
$burstBlock2 = Get-Median -Values $burst2
$fusedBlock1 = Get-Median -Values $fused1
$fusedBlock2 = Get-Median -Values $fused2

$gainBlock1 = (($burstBlock1 / $fusedBlock1) - 1.0) * 100.0
$gainBlock2 = (($burstBlock2 / $fusedBlock2) - 1.0) * 100.0
$gainSpreadPp = [math]::Abs($gainBlock2 - $gainBlock1)

$burstMedian = Get-Median -Values $burstAll
$fusedMedian = Get-Median -Values $fusedAll
$aggregateGainPct = (($burstMedian / $fusedMedian) - 1.0) * 100.0

$burstDriftPct = (($burstBlock2 / $burstBlock1) - 1.0) * 100.0
$fusedDriftPct = (($fusedBlock2 / $fusedBlock1) - 1.0) * 100.0
$commonModeDriftMismatchPp = [math]::Abs($burstDriftPct - $fusedDriftPct)

$tcpRxReference = 13.798000
$burstAsTcpPct = ($burstMedian / $tcpRxReference) * 100.0
$tcpGapPct = (1.0 - ($burstMedian / $tcpRxReference)) * 100.0

$interpretation = "INCONCLUSIVE"

if (
    $gainBlock1 -ge 0.5 -and
    $gainBlock2 -ge 0.5 -and
    $aggregateGainPct -ge 0.5 -and
    $gainSpreadPp -le 1.0
) {
    $interpretation = "BURST_REPEATABLE_PAIRED_GAIN"
}
elseif (
    $gainBlock1 -lt 0.0 -and
    $gainBlock2 -lt 0.0
) {
    $interpretation = "BURST_REPEATABLE_REGRESSION"
}
elseif ($gainSpreadPp -gt 1.0) {
    $interpretation = "BURST_GAIN_NOT_STABLE"
}
elseif ($aggregateGainPct -lt 0.5) {
    $interpretation = "BURST_GAIN_TOO_SMALL"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " P3I REPEATABILITY SUMMARY"
Write-Host "============================================================"
Write-Host ("P3I_BURST_BLOCK1_MEDIAN_MBPS={0:F6}" -f $burstBlock1)
Write-Host ("P3I_FUSED_BLOCK1_MEDIAN_MBPS={0:F6}" -f $fusedBlock1)
Write-Host ("P3I_BLOCK1_PAIRED_GAIN_PCT={0:F2}" -f $gainBlock1)
Write-Host ("P3I_BURST_BLOCK2_MEDIAN_MBPS={0:F6}" -f $burstBlock2)
Write-Host ("P3I_FUSED_BLOCK2_MEDIAN_MBPS={0:F6}" -f $fusedBlock2)
Write-Host ("P3I_BLOCK2_PAIRED_GAIN_PCT={0:F2}" -f $gainBlock2)
Write-Host ("P3I_PAIRED_GAIN_SPREAD_PP={0:F2}" -f $gainSpreadPp)
Write-Host ("P3I_BURST_MEDIAN_MBPS={0:F6}" -f $burstMedian)
Write-Host ("P3I_FUSED_MEDIAN_MBPS={0:F6}" -f $fusedMedian)
Write-Host ("P3I_AGGREGATE_GAIN_PCT={0:F2}" -f $aggregateGainPct)
Write-Host ("P3I_BURST_ABSOLUTE_DRIFT_PCT={0:F2}" -f $burstDriftPct)
Write-Host ("P3I_FUSED_ABSOLUTE_DRIFT_PCT={0:F2}" -f $fusedDriftPct)
Write-Host ("P3I_COMMON_MODE_DRIFT_MISMATCH_PP={0:F2}" -f $commonModeDriftMismatchPp)
Write-Host ("P3I_BURST_AS_TCP_REFERENCE_PCT={0:F2}" -f $burstAsTcpPct)
Write-Host ("P3I_TCP_REFERENCE_GAP_PCT={0:F2}" -f $tcpGapPct)
Write-Host "P3I_INTERPRETATION=$interpretation"
Write-Host "A14_P3I_REPEATABILITY=PASS"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT_FOR_P3I_DECISION"
