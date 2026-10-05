param(
    [Parameter(Mandatory = $true)]
    [int]$MHz,

    [string]$SerialPort = "COM14",
    [string]$DutIp = "192.168.0.31",
    [double]$DurationSeconds = 3.0,
    [int]$Runs = 12
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

Write-Host "============================================================"
Write-Host " G2 TCP_RX EXACT HOLD DIAGNOSTIC"
Write-Host "============================================================"

Assert-G2AllowedMHz -MHz $MHz
Assert-G2Branch

if ($DurationSeconds -le 0) {
    throw "G2_DURATION_MUST_BE_POSITIVE"
}

if ($Runs -lt 1) {
    throw "G2_RUNS_MUST_BE_POSITIVE"
}

$targetHz = [int64]$MHz * 1000000
$effectiveHz = Get-G2SpiHz
$head = Get-G2Head

Write-Host "HEAD=$head"
Write-Host "TARGET_MHZ=$MHz"
Write-Host "TARGET_SPI_HZ=$targetHz"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "DUT_IP=$DutIp"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "RUNS=$Runs"
Write-Host "TCP_SPI_HOLD_BUDGET_US=$script:G2HoldBudgetUs"

if ($effectiveHz -ne $targetHz) {
    throw "G2_DIAG_EFFECTIVE_SPI_HZ_MISMATCH"
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
    throw "G2_SNAPSHOT_BRIDGE_NOT_FOUND"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_g2_{0}mhz_tcp_rx_hold_diag_{1}" -f $MHz, $timestamp)
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$durationText = $DurationSeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$rows = New-Object System.Collections.Generic.List[object]
$overBudgetCount = 0

Write-Host "PYTHON=$pythonExe"
Write-Host "BRIDGE=$bridgePath"
Write-Host "TEMP_ROOT=$tempRoot"
Write-Host ""
Write-Host "=== EXACT TCP_RX RUNS ==="

for ($i = 1; $i -le $Runs; $i++) {
    $logPath = Join-Path $tempRoot ("tcp_rx_{0:D2}.log" -f $i)

    $args = @(
        $bridgePath,
        "--host", $DutIp,
        "--serial", $SerialPort,
        "--duration", $durationText,
        "--mode", "tcp-rx",
        "--tcp-chunk", "4096",
        "--udp-payload", "1472"
    )

    $exitCode = Invoke-G2NativeToLog -FilePath $pythonExe -Arguments $args -LogPath $logPath

    if ($exitCode -ne 0) {
        Get-Content -LiteralPath $logPath -Tail 120 | ForEach-Object { Write-Host $_ }
        throw "G2_TCP_RX_DIAG_RUNNER_FAILED_RUN=$i EXIT=$exitCode"
    }

    $text = [System.IO.File]::ReadAllText($logPath)

    $snapshotPresent = Get-G2LogValue -Text $text -Key "G2_FINAL_SNAPSHOT_PRESENT"
    $functional = Get-G2LogValue -Text $text -Key "RAW_BENCH_FUNCTIONAL_PASS"
    $ready = Get-G2LogValue -Text $text -Key "G2_FINAL_RAW_SERVER_READY"
    $ethReady = Get-G2LogValue -Text $text -Key "G2_FINAL_ETH_READY"
    $link = Get-G2LogValue -Text $text -Key "G2_FINAL_ETH_LINK"
    $ip = Get-G2LogValue -Text $text -Key "G2_FINAL_IP"
    $pcMbps = Get-G2LogValue -Text $text -Key "SUMMARY_TCP_RX_PC_MBPS"
    $dutMbps = Get-G2LogValue -Text $text -Key "SUMMARY_TCP_RX_DUT_MBPS"

    if ($snapshotPresent -ne "YES") {
        throw "G2_TCP_RX_DIAG_NO_FINAL_SNAPSHOT_RUN=$i"
    }

    if ($functional -ne "YES") {
        throw "G2_TCP_RX_DIAG_FUNCTIONAL_FAIL_RUN=$i"
    }

    if ($ready -ne "YES" -or $ethReady -ne "YES" -or $link -ne "UP" -or $ip -ne $DutIp) {
        throw "G2_TCP_RX_DIAG_READY_FAIL_RUN=$i"
    }

    $transportErrors = Get-G2LogInt64 -Text $text -Key "G2_FINAL_TRANSPORT_ERRORS"
    $tcpLockErrors = Get-G2LogInt64 -Text $text -Key "G2_FINAL_TCP_SPI_LOCK_ERRORS"
    $udpLockErrors = Get-G2LogInt64 -Text $text -Key "G2_FINAL_UDP_SPI_LOCK_ERRORS"
    $holdCount = Get-G2LogInt64 -Text $text -Key "G2_FINAL_TCP_SPI_HOLD_COUNT"
    $holdAvgUs = Get-G2LogInt64 -Text $text -Key "G2_FINAL_TCP_SPI_HOLD_AVG_US"
    $holdMaxUs = Get-G2LogInt64 -Text $text -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-G2LogInt64 -Text $text -Key "G2_FINAL_LOOP_GAP_MAX_US"

    if ($transportErrors -ne 0) {
        throw "G2_TCP_RX_DIAG_TRANSPORT_ERRORS_RUN=$i VALUE=$transportErrors"
    }

    if ($tcpLockErrors -ne 0) {
        throw "G2_TCP_RX_DIAG_TCP_LOCK_ERRORS_RUN=$i VALUE=$tcpLockErrors"
    }

    if ($udpLockErrors -ne 0) {
        throw "G2_TCP_RX_DIAG_UDP_LOCK_ERRORS_RUN=$i VALUE=$udpLockErrors"
    }

    $overBudget = $holdMaxUs -gt $script:G2HoldBudgetUs
    if ($overBudget) {
        ++$overBudgetCount
    }

    Write-Host (
        "RUN={0} PC_MBPS={1} DUT_MBPS={2} HOLD_COUNT={3} HOLD_AVG_US={4} HOLD_MAX_US={5} LOOP_GAP_MAX_US={6} OVER_BUDGET={7}" -f
        $i,
        $pcMbps,
        $dutMbps,
        $holdCount,
        $holdAvgUs,
        $holdMaxUs,
        $loopGapMaxUs,
        ($(if ($overBudget) { "YES" } else { "NO" }))
    )

    $rows.Add([pscustomobject]@{
        Run = $i
        PcMbps = [double]::Parse($pcMbps, [System.Globalization.CultureInfo]::InvariantCulture)
        DutMbps = [double]::Parse($dutMbps, [System.Globalization.CultureInfo]::InvariantCulture)
        HoldCount = $holdCount
        HoldAvgUs = $holdAvgUs
        HoldMaxUs = $holdMaxUs
        LoopGapMaxUs = $loopGapMaxUs
        OverBudget = $overBudget
        Log = $logPath
    })
}

$maxHold = ($rows | Measure-Object -Property HoldMaxUs -Maximum).Maximum
$avgOfHoldAvg = ($rows | Measure-Object -Property HoldAvgUs -Average).Average
$minPc = ($rows | Measure-Object -Property PcMbps -Minimum).Minimum
$maxPc = ($rows | Measure-Object -Property PcMbps -Maximum).Maximum
$avgPc = ($rows | Measure-Object -Property PcMbps -Average).Average
$maxLoopGap = ($rows | Measure-Object -Property LoopGapMaxUs -Maximum).Maximum

Write-Host ""
Write-Host "=== DIAGNOSTIC SUMMARY ==="
Write-Host "RUNS_COMPLETED=$($rows.Count)"
Write-Host "HOLD_OVER_BUDGET_COUNT=$overBudgetCount"
Write-Host "HOLD_MAX_US_OVERALL=$maxHold"
Write-Host ("HOLD_AVG_US_MEAN={0:F2}" -f $avgOfHoldAvg)
Write-Host ("TCP_RX_PC_MBPS_MIN={0:F6}" -f $minPc)
Write-Host ("TCP_RX_PC_MBPS_AVG={0:F6}" -f $avgPc)
Write-Host ("TCP_RX_PC_MBPS_MAX={0:F6}" -f $maxPc)
Write-Host "LOOP_GAP_MAX_US_OVERALL=$maxLoopGap"
Write-Host "TCP_SPI_HOLD_BUDGET_US=$script:G2HoldBudgetUs"

Assert-G2ProtectedArtifacts
Assert-G2RawBaselineArtifacts

$dirtyAfter = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyAfter.Count)"
$dirtyAfter | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }

if ($MHz -eq 14) {
    if ($dirtyAfter.Count -ne 0) {
        throw "G2_DIAG_FINAL_DIRTY_STATE_INVALID_BASELINE"
    }
}
else {
    if ($dirtyAfter.Count -ne 1 -or $dirtyAfter[0] -ne $script:G2SpiHeaderRelative) {
        throw "G2_DIAG_FINAL_DIRTY_STATE_INVALID"
    }
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante las corridas."
$visualAnswer = ""
while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante el diagnostico? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = $(if ($visualAnswer -eq "S") { 1 } else { 0 })
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

if ($visualEvents -ne 0) {
    Write-Host "G2_TCP_RX_HOLD_DIAG=VISUAL_SPI_REPRODUCED"
    exit 3
}

if ($overBudgetCount -gt 0) {
    Write-Host "G2_TCP_RX_HOLD_DIAG=REPRODUCED_OVER_BUDGET"
    exit 2
}

Write-Host "G2_TCP_RX_HOLD_DIAG=NO_REPRO_IN_EXACT_SNAPSHOTS"
exit 0
