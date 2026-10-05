param(
    [Parameter(Mandatory = $true)]
    [int]$MHz,

    [string]$SerialPort = "COM14",
    [string]$DutIp = "192.168.0.31",
    [double]$DurationSeconds = 10.0,
    [int]$RunsPerMode = 3
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-G2NativeToLog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [Parameter(Mandatory = $true)]
        [string]$LogPath
    )

    & $FilePath @Arguments *> $LogPath
    return [int]$LASTEXITCODE
}

function Get-G2LogValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,

        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))

    if ($matches.Count -ne 1) {
        throw "G2_LOG_KEY_COUNT_${Key}=$($matches.Count)"
    }

    return $matches[0].Groups[1].Value.Trim()
}

function Get-G2LogInt64 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,

        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    return [int64]::Parse(
        (Get-G2LogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Get-G2LogDouble {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,

        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    return [double]::Parse(
        (Get-G2LogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Assert-G2ExactValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,

        [Parameter(Mandatory = $true)]
        [string]$Key,

        [Parameter(Mandatory = $true)]
        [string]$Expected
    )

    $actual = Get-G2LogValue -Text $Text -Key ("G2_FINAL_{0}" -f $Key)

    if ($actual -ne $Expected) {
        throw "G2_EXACT_${Key}_MISMATCH=$actual"
    }
}

function Assert-G2ExactZero {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,

        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    $value = Get-G2LogInt64 -Text $Text -Key ("G2_FINAL_{0}" -f $Key)

    if ($value -ne 0) {
        throw "G2_EXACT_${Key}_NONZERO=$value"
    }
}

function Get-G2Stats {
    param(
        [Parameter(Mandatory = $true)]
        [double[]]$Values
    )

    if ($Values.Count -lt 1) {
        throw "G2_STATS_EMPTY"
    }

    $min = [double]::PositiveInfinity
    $max = [double]::NegativeInfinity
    $sum = 0.0

    foreach ($value in $Values) {
        if ($value -lt $min) {
            $min = $value
        }

        if ($value -gt $max) {
            $max = $value
        }

        $sum += $value
    }

    return [pscustomobject]@{
        Min = $min
        Avg = ($sum / $Values.Count)
        Max = $max
    }
}

Write-Host "============================================================"
Write-Host " G2 FREQUENCY QUALIFICATION"
Write-Host "============================================================"

Assert-G2AllowedMHz -MHz $MHz
Assert-G2Branch

if ($DurationSeconds -le 0) {
    throw "G2_DURATION_MUST_BE_POSITIVE"
}

if ($RunsPerMode -lt 1) {
    throw "G2_RUNS_PER_MODE_MUST_BE_POSITIVE"
}

$head = Get-G2Head
$targetHz = [int64]$MHz * 1000000
$effectiveHz = Get-G2SpiHz

Write-Host "HEAD=$head"
Write-Host "TARGET_MHZ=$MHz"
Write-Host "TARGET_SPI_HZ=$targetHz"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "DUT_IP=$DutIp"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "RUNS_PER_MODE=$RunsPerMode"
Write-Host "TOTAL_RUNS=$($RunsPerMode * 4)"
Write-Host "TCP_SPI_HOLD_BUDGET_US=$script:G2HoldBudgetUs"
Write-Host "SNAPSHOT_SOURCE=FROZEN_RUNNER_EXACT_FINAL"
Write-Host "NO_BUILD=YES"
Write-Host "NO_UPLOAD=YES"

if ($effectiveHz -ne $targetHz) {
    throw "G2_QUAL_EFFECTIVE_SPI_HZ_MISMATCH"
}

Write-Host ""
Write-Host "=== STATIC PRECHECK ==="
& (Join-Path $PSScriptRoot "g2_validate_frequency.ps1") -MHz $MHz

Assert-G2ProtectedArtifacts
Assert-G2RawBaselineArtifacts

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue

if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}

if ($null -eq $pythonCommand) {
    throw "G2_PYTHON_NOT_FOUND"
}

$pythonExe = $pythonCommand.Source
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"

if (-not (Test-Path -LiteralPath $bridgePath)) {
    throw "G2_RUNNER_SNAPSHOT_BRIDGE_NOT_FOUND"
}

$durationText = $DurationSeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_g2_{0}mhz_qualification_{1}" -f $MHz, $timestamp)
$summaryLog = Join-Path $tempRoot "qualification_summary.txt"
$csvLog = Join-Path $tempRoot "qualification_runs.csv"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

Write-Host "PYTHON=$pythonExe"
Write-Host "BRIDGE=$bridgePath"
Write-Host "TEMP_ROOT=$tempRoot"

# Frozen D40-R2 14 MHz primary-throughput averages.
# UDP_RX uses DUT throughput because PC_MBPS is offered flood, not received throughput.
$modeMap = @(
    [pscustomobject]@{
        Cli = "tcp-rx"
        Key = "TCP_RX"
        PrimaryField = "PC"
        Baseline14 = 9.755605
    },
    [pscustomobject]@{
        Cli = "tcp-tx"
        Key = "TCP_TX"
        PrimaryField = "PC"
        Baseline14 = 3.706707
    },
    [pscustomobject]@{
        Cli = "udp-rx"
        Key = "UDP_RX"
        PrimaryField = "DUT"
        Baseline14 = 8.132316
    },
    [pscustomobject]@{
        Cli = "udp-tx"
        Key = "UDP_TX"
        PrimaryField = "PC"
        Baseline14 = 4.163015
    }
)

$results = New-Object System.Collections.Generic.List[object]

Write-Host ""
Write-Host "=== QUALIFICATION RUNS ==="

for ($cycle = 1; $cycle -le $RunsPerMode; ++$cycle) {
    Write-Host ""
    Write-Host ("--- CYCLE {0}/{1} ---" -f $cycle, $RunsPerMode)

    foreach ($mode in $modeMap) {
        $runLog = Join-Path $tempRoot ("cycle{0}_{1}.log" -f $cycle, $mode.Key.ToLowerInvariant())

        $runnerArgs = @(
            $bridgePath,
            "--host", $DutIp,
            "--serial", $SerialPort,
            "--duration", $durationText,
            "--mode", $mode.Cli,
            "--tcp-chunk", "4096",
            "--udp-payload", "1472"
        )

        $runExit = Invoke-G2NativeToLog -FilePath $pythonExe -Arguments $runnerArgs -LogPath $runLog

        if ($runExit -ne 0) {
            Get-Content -LiteralPath $runLog -Tail 120 | ForEach-Object { Write-Host $_ }
            throw "G2_QUAL_RUNNER_FAILED_$($mode.Key)_CYCLE_$cycle"
        }

        $runText = [System.IO.File]::ReadAllText($runLog)

        if ((Get-G2LogValue -Text $runText -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
            throw "G2_QUAL_FUNCTIONAL_FAIL_$($mode.Key)_CYCLE_$cycle"
        }

        if ((Get-G2LogValue -Text $runText -Key "G2_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
            throw "G2_QUAL_EXACT_SNAPSHOT_MISSING_$($mode.Key)_CYCLE_$cycle"
        }

        Assert-G2ExactValue -Text $runText -Key "RAW_SERVER_READY" -Expected "YES"
        Assert-G2ExactValue -Text $runText -Key "ETH_READY" -Expected "YES"
        Assert-G2ExactValue -Text $runText -Key "ETH_LINK" -Expected "UP"
        Assert-G2ExactValue -Text $runText -Key "IP" -Expected $DutIp

        @(
            "TRANSPORT_ERRORS",
            "UDP_BEGIN_PACKET_ERRORS",
            "UDP_WRITE_ERRORS",
            "UDP_END_PACKET_ERRORS",
            "UDP_SPI_LOCK_ERRORS",
            "TCP_SPI_LOCK_ERRORS",
            "UDP_LAST_SHORT_WRITE_BYTES"
        ) | ForEach-Object {
            Assert-G2ExactZero -Text $runText -Key $_
        }

        $pcMbps = Get-G2LogDouble -Text $runText -Key ("SUMMARY_{0}_PC_MBPS" -f $mode.Key)
        $dutMbps = Get-G2LogDouble -Text $runText -Key ("SUMMARY_{0}_DUT_MBPS" -f $mode.Key)
        $lossPct = Get-G2LogDouble -Text $runText -Key ("SUMMARY_{0}_LOSS_PERCENT" -f $mode.Key)
        $holdAvgUs = Get-G2LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_AVG_US"
        $holdMaxUs = Get-G2LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
        $loopGapMaxUs = Get-G2LogInt64 -Text $runText -Key "G2_FINAL_LOOP_GAP_MAX_US"

        if ($holdMaxUs -gt $script:G2HoldBudgetUs) {
            throw "G2_QUAL_HOLD_BUDGET_EXCEEDED_$($mode.Key)_CYCLE_${cycle}=$holdMaxUs"
        }

        if ($mode.Key -eq "UDP_TX") {
            @(
                "UDP_TX_SEQUENCE_DECODE_ERRORS",
                "UDP_TX_SEQUENCE_TOTAL_DUPLICATES",
                "UDP_TX_SEQUENCE_REORDERS",
                "UDP_TX_SEQUENCE_TOTAL_RANGE_MISSING",
                "UDP_TX_UNEXPECTED_SOURCE_PACKETS",
                "UDP_TX_WRONG_SIZE_FROM_DUT"
            ) | ForEach-Object {
                $integrityValue = Get-G2LogInt64 -Text $runText -Key $_

                if ($integrityValue -ne 0) {
                    throw "G2_QUAL_UDP_TX_INTEGRITY_FAIL_${_}=$integrityValue"
                }
            }

            $sequenceCount = Get-G2LogInt64 -Text $runText -Key "UDP_TX_SEQUENCE_TOTAL_COUNT"

            if ($sequenceCount -le 0) {
                throw "G2_QUAL_UDP_TX_SEQUENCE_EMPTY"
            }
        }

        $primaryMbps = $pcMbps

        if ($mode.PrimaryField -eq "DUT") {
            $primaryMbps = $dutMbps
        }

        $results.Add([pscustomobject]@{
            Cycle = $cycle
            Mode = $mode.Key
            PcMbps = $pcMbps
            DutMbps = $dutMbps
            PrimaryMbps = $primaryMbps
            LossPct = $lossPct
            HoldAvgUs = $holdAvgUs
            HoldMaxUs = $holdMaxUs
            LoopGapMaxUs = $loopGapMaxUs
        })

        if ($mode.Key -eq "UDP_RX") {
            Write-Host (
                "RUN CYCLE={0} MODE={1} PC_OFFERED_MBPS={2:F6} DUT_MBPS={3:F6} LOSS_PCT={4:F3} HOLD_AVG_US={5} HOLD_MAX_US={6} LOOP_GAP_MAX_US={7} PASS=YES" -f
                $cycle,
                $mode.Key,
                $pcMbps,
                $dutMbps,
                $lossPct,
                $holdAvgUs,
                $holdMaxUs,
                $loopGapMaxUs
            )
        }
        else {
            Write-Host (
                "RUN CYCLE={0} MODE={1} PC_MBPS={2:F6} DUT_MBPS={3:F6} LOSS_PCT={4:F3} HOLD_AVG_US={5} HOLD_MAX_US={6} LOOP_GAP_MAX_US={7} PASS=YES" -f
                $cycle,
                $mode.Key,
                $pcMbps,
                $dutMbps,
                $lossPct,
                $holdAvgUs,
                $holdMaxUs,
                $loopGapMaxUs
            )
        }

        Start-Sleep -Milliseconds 500
    }
}

if ($results.Count -ne ($RunsPerMode * 4)) {
    throw "G2_QUAL_RESULT_COUNT_MISMATCH=$($results.Count)"
}

Write-Host ""
Write-Host "=== FINAL INVARIANTS ==="
Assert-G2ProtectedArtifacts
Assert-G2RawBaselineArtifacts

$dirtyAfter = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyAfter.Count)"
$dirtyAfter | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }

if ($MHz -eq 14) {
    if ($dirtyAfter.Count -ne 0) {
        throw "G2_QUAL_FINAL_DIRTY_STATE_INVALID_BASELINE"
    }
}
else {
    if (
        $dirtyAfter.Count -ne 1 -or
        $dirtyAfter[0] -ne $script:G2SpiHeaderRelative
    ) {
        throw "G2_QUAL_FINAL_DIRTY_STATE_INVALID"
    }
}

$stagedAfter = @(
    & git -C $script:G2RepoRoot diff --cached --name-only
)

if ($LASTEXITCODE -ne 0) {
    throw "G2_QUAL_FINAL_CACHED_DIFF_FAILED"
}

Write-Host "STAGED_COUNT_FINAL=$($stagedAfter.Count)"

if ($stagedAfter.Count -ne 0) {
    throw "G2_QUAL_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante las 12 corridas."
$visualAnswer = ""

while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante la calificacion? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = 0

if ($visualAnswer -eq "S") {
    $visualEvents = 1
}

Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

$summaryRows = New-Object System.Collections.Generic.List[object]
$reviewRequired = $false

foreach ($mode in $modeMap) {
    $modeResults = @($results | Where-Object { $_.Mode -eq $mode.Key })

    if ($modeResults.Count -ne $RunsPerMode) {
        throw "G2_QUAL_MODE_RESULT_COUNT_$($mode.Key)=$($modeResults.Count)"
    }

    $primaryValues = @($modeResults | ForEach-Object { [double]$_.PrimaryMbps })
    $pcValues = @($modeResults | ForEach-Object { [double]$_.PcMbps })
    $dutValues = @($modeResults | ForEach-Object { [double]$_.DutMbps })
    $primaryStats = Get-G2Stats -Values $primaryValues
    $pcStats = Get-G2Stats -Values $pcValues
    $dutStats = Get-G2Stats -Values $dutValues

    $holdMaxOverall = 0
    $loopGapMaxOverall = 0

    foreach ($item in $modeResults) {
        if ([int64]$item.HoldMaxUs -gt $holdMaxOverall) {
            $holdMaxOverall = [int64]$item.HoldMaxUs
        }

        if ([int64]$item.LoopGapMaxUs -gt $loopGapMaxOverall) {
            $loopGapMaxOverall = [int64]$item.LoopGapMaxUs
        }
    }

    $gainPct = (($primaryStats.Avg / [double]$mode.Baseline14) - 1.0) * 100.0
    $perfClass = "UP"

    if ($gainPct -lt 0.0) {
        $perfClass = "DOWN_REVIEW"
        $reviewRequired = $true
    }

    $summaryRows.Add([pscustomobject]@{
        Mode = $mode.Key
        Primary = $mode.PrimaryField
        Baseline14 = [double]$mode.Baseline14
        Min = [double]$primaryStats.Min
        Avg = [double]$primaryStats.Avg
        Max = [double]$primaryStats.Max
        GainPct = $gainPct
        HoldMaxUs = $holdMaxOverall
        LoopGapMaxUs = $loopGapMaxOverall
        Perf = $perfClass
        PcAvg = [double]$pcStats.Avg
        DutAvg = [double]$dutStats.Avg
    })
}

Write-Host ""
Write-Host "============================================================"
Write-Host (" G2 QUALIFICATION TABLE - {0} MHz" -f $MHz)
Write-Host "============================================================"
Write-Host "MODE     METRIC  BASE14    MIN       AVG       MAX       GAIN_AVG   HOLD_MAX  RESULT"
Write-Host "-------- ------- --------- --------- --------- --------- ---------- --------- -----------"

foreach ($row in $summaryRows) {
    Write-Host (
        "{0,-8} {1,-7} {2,9:F3} {3,9:F3} {4,9:F3} {5,9:F3} {6,9:F1}% {7,9} {8,-11}" -f
        $row.Mode,
        $row.Primary,
        $row.Baseline14,
        $row.Min,
        $row.Avg,
        $row.Max,
        $row.GainPct,
        $row.HoldMaxUs,
        $row.Perf
    )
}

Write-Host ""
Write-Host "NOTA_UDP_RX=PC_MBPS_ES_OFRECIDO; LA METRICA_PRIMARIA_ES_DUT_MBPS"
Write-Host "TCP_SPI_HOLD_BUDGET_US=$script:G2HoldBudgetUs"
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

$csvLines = New-Object System.Collections.Generic.List[string]
$csvLines.Add("cycle,mode,pc_mbps,dut_mbps,primary_mbps,loss_pct,hold_avg_us,hold_max_us,loop_gap_max_us")

foreach ($item in $results) {
    $csvLines.Add((
        "{0},{1},{2:F6},{3:F6},{4:F6},{5:F6},{6},{7},{8}" -f
        $item.Cycle,
        $item.Mode,
        $item.PcMbps,
        $item.DutMbps,
        $item.PrimaryMbps,
        $item.LossPct,
        $item.HoldAvgUs,
        $item.HoldMaxUs,
        $item.LoopGapMaxUs
    ))
}

$csvLines | Set-Content -LiteralPath $csvLog -Encoding UTF8

$summaryLines = New-Object System.Collections.Generic.List[string]
$summaryLines.Add("G2_QUALIFICATION_MHZ=$MHz")
$summaryLines.Add("HEAD=$head")
$summaryLines.Add("RUNS_PER_MODE=$RunsPerMode")
$summaryLines.Add("DURATION_S=$durationText")
$summaryLines.Add("TCP_SPI_HOLD_BUDGET_US=$script:G2HoldBudgetUs")
$summaryLines.Add("VISUAL_SPI_EVENTS=$visualEvents")

foreach ($row in $summaryRows) {
    $summaryLines.Add(("{0}_PRIMARY={1}" -f $row.Mode, $row.Primary))
    $summaryLines.Add(("{0}_BASELINE14_MBPS={1:F6}" -f $row.Mode, $row.Baseline14))
    $summaryLines.Add(("{0}_MIN_MBPS={1:F6}" -f $row.Mode, $row.Min))
    $summaryLines.Add(("{0}_AVG_MBPS={1:F6}" -f $row.Mode, $row.Avg))
    $summaryLines.Add(("{0}_MAX_MBPS={1:F6}" -f $row.Mode, $row.Max))
    $summaryLines.Add(("{0}_GAIN_AVG_PCT={1:F3}" -f $row.Mode, $row.GainPct))
    $summaryLines.Add(("{0}_HOLD_MAX_US={1}" -f $row.Mode, $row.HoldMaxUs))
    $summaryLines.Add(("{0}_LOOP_GAP_MAX_US={1}" -f $row.Mode, $row.LoopGapMaxUs))
    $summaryLines.Add(("{0}_PERF={1}" -f $row.Mode, $row.Perf))
}

$qualificationResult = "PASS"

if ($visualEvents -ne 0) {
    $qualificationResult = "FAIL"
}
elseif ($reviewRequired) {
    $qualificationResult = "REVIEW"
}

$summaryLines.Add("G2_FREQUENCY_QUALIFICATION=$qualificationResult")
$summaryLines.Add("RUNS_CSV=$csvLog")
$summaryLines | Set-Content -LiteralPath $summaryLog -Encoding UTF8

Write-Host "RUNS_CSV=$csvLog"
Write-Host "SUMMARY_LOG=$summaryLog"
Write-Host "G2_FREQUENCY_QUALIFICATION=$qualificationResult"

if ($qualificationResult -eq "FAIL") {
    exit 2
}

if ($qualificationResult -eq "REVIEW") {
    exit 3
}
