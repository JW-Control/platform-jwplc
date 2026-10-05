param(
    [Parameter(Mandatory = $true)]
    [int]$MHz,

    [string]$SerialPort = "COM14",
    [string]$DutIp = "192.168.0.31",
    [double]$DurationSeconds = 3.0
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

    $value = Get-G2LogValue -Text $Text -Key $Key
    return [int64]::Parse(
        $value,
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
    Write-Host "EXACT_${Key}=$actual"

    if ($actual -ne $Expected) {
        throw "G2_EXACT_${Key}_MISMATCH"
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
    Write-Host "EXACT_${Key}=$value"

    if ($value -ne 0) {
        throw "G2_EXACT_${Key}_NONZERO=$value"
    }
}

Write-Host "============================================================"
Write-Host " G2 PHYSICAL SMOKE - EXACT SNAPSHOTS"
Write-Host "============================================================"

Assert-G2AllowedMHz -MHz $MHz
Assert-G2Branch

if ($DurationSeconds -le 0) {
    throw "G2_DURATION_MUST_BE_POSITIVE"
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
Write-Host "SMOKE_DURATION_S=$DurationSeconds"
Write-Host "TCP_SPI_HOLD_BUDGET_US=$script:G2HoldBudgetUs"
Write-Host "SNAPSHOT_SOURCE=FROZEN_RUNNER_EXACT_FINAL"

if ($effectiveHz -ne $targetHz) {
    throw "G2_PHYSICAL_EFFECTIVE_SPI_HZ_MISMATCH"
}

Write-Host ""
Write-Host "=== STATIC PRECHECK ==="
& (Join-Path $PSScriptRoot "g2_validate_frequency.ps1") -MHz $MHz

Assert-G2ProtectedArtifacts
Assert-G2RawBaselineArtifacts

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"

if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "G2_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue

if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}

if ($null -eq $pythonCommand) {
    throw "G2_PYTHON_NOT_FOUND"
}

$pythonExe = $pythonCommand.Source
$fqbn = "jwplc_local:esp32:jwplcbasic"
$firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$sketchDir = Split-Path -Parent $firmwarePath
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"

if (-not (Test-Path -LiteralPath $bridgePath)) {
    throw "G2_SNAPSHOT_BRIDGE_NOT_FOUND"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_g2_{0}mhz_smoke_exact_{1}" -f $MHz, $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"
$summaryLog = Join-Path $tempRoot "summary.log"

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"
Write-Host "FQBN=$fqbn"
Write-Host "BRIDGE=$bridgePath"
Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "BUILD_PATH=$buildPath"

Write-Host ""
Write-Host "=== COMPILE RAW FIRMWARE ==="

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $sketchDir
)

$compileExit = Invoke-G2NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog
Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Get-Content -LiteralPath $compileLog -Tail 80 | ForEach-Object { Write-Host $_ }
    throw "G2_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$ethernetLibraryUsed = (
    $compileText.IndexOf(
        $expectedEthernetLibrary,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0
)

Write-Host "REPO_ETHERNET_LIBRARY_USED=$ethernetLibraryUsed"

if (-not $ethernetLibraryUsed) {
    throw "G2_COMPILE_DID_NOT_USE_REPO_ETHERNET_LIBRARY"
}

$binCount = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin"
).Count

Write-Host "BIN_COUNT=$binCount"

if ($binCount -lt 1) {
    throw "G2_COMPILE_NO_BIN_OUTPUT"
}

Assert-G2ProtectedArtifacts
Assert-G2RawBaselineArtifacts

Write-Host ""
Write-Host "=== UPLOAD RAW FIRMWARE ==="

$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildPath,
    $sketchDir
)

$uploadExit = Invoke-G2NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog
Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"

if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 80 | ForEach-Object { Write-Host $_ }
    throw "G2_UPLOAD_FAILED"
}

$modeMap = @(
    [pscustomobject]@{ Cli = "tcp-rx"; Key = "TCP_RX" },
    [pscustomobject]@{ Cli = "tcp-tx"; Key = "TCP_TX" },
    [pscustomobject]@{ Cli = "udp-rx"; Key = "UDP_RX" },
    [pscustomobject]@{ Cli = "udp-tx"; Key = "UDP_TX" }
)

$durationText = $DurationSeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$summaryLines = New-Object System.Collections.Generic.List[string]
$summaryLines.Add("G2_PHYSICAL_SMOKE_MHZ=$MHz")
$summaryLines.Add("HEAD=$head")
$summaryLines.Add("SERIAL_PORT=$SerialPort")
$summaryLines.Add("DUT_IP=$DutIp")
$summaryLines.Add("DURATION_S=$durationText")
$summaryLines.Add("SNAPSHOT_SOURCE=FROZEN_RUNNER_EXACT_FINAL")

Write-Host ""
Write-Host "=== FOUR-MODE EXACT SMOKE ==="

foreach ($mode in $modeMap) {
    $runLog = Join-Path $tempRoot ("{0}.log" -f $mode.Key.ToLowerInvariant())

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
    Write-Host "MODE=$($mode.Key) RUNNER_EXIT=$runExit LOG=$runLog"

    if ($runExit -ne 0) {
        Get-Content -LiteralPath $runLog -Tail 120 | ForEach-Object { Write-Host $_ }
        throw "G2_RUNNER_FAILED_$($mode.Key)"
    }

    $runText = [System.IO.File]::ReadAllText($runLog)
    $functionalPass = Get-G2LogValue -Text $runText -Key "RAW_BENCH_FUNCTIONAL_PASS"
    $dutReady = Get-G2LogValue -Text $runText -Key "DUT_READY"
    $snapshotPresent = Get-G2LogValue -Text $runText -Key "G2_FINAL_SNAPSHOT_PRESENT"
    $pcMbps = Get-G2LogValue -Text $runText -Key ("SUMMARY_{0}_PC_MBPS" -f $mode.Key)
    $dutMbps = Get-G2LogValue -Text $runText -Key ("SUMMARY_{0}_DUT_MBPS" -f $mode.Key)

    if ($functionalPass -ne "YES") {
        throw "G2_FUNCTIONAL_FAIL_$($mode.Key)"
    }

    if ($dutReady -ne "YES") {
        throw "G2_DUT_NOT_READY_$($mode.Key)"
    }

    if ($snapshotPresent -ne "YES") {
        throw "G2_EXACT_SNAPSHOT_MISSING_$($mode.Key)"
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

    $holdCount = Get-G2LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_COUNT"
    $holdAvgUs = Get-G2LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_AVG_US"
    $holdMaxUs = Get-G2LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-G2LogInt64 -Text $runText -Key "G2_FINAL_LOOP_GAP_MAX_US"

    Write-Host "EXACT_TCP_SPI_HOLD_COUNT=$holdCount"
    Write-Host "EXACT_TCP_SPI_HOLD_AVG_US=$holdAvgUs"
    Write-Host "EXACT_TCP_SPI_HOLD_MAX_US=$holdMaxUs"
    Write-Host "EXACT_LOOP_GAP_MAX_US=$loopGapMaxUs"

    if ($holdMaxUs -gt $script:G2HoldBudgetUs) {
        throw "G2_TCP_SPI_HOLD_BUDGET_EXCEEDED_$($mode.Key)=$holdMaxUs"
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
            $sequenceValue = Get-G2LogInt64 -Text $runText -Key $_
            Write-Host "$_=$sequenceValue"

            if ($sequenceValue -ne 0) {
                throw "G2_UDP_TX_INTEGRITY_FAIL_$_=$sequenceValue"
            }
        }

        $sequenceCount = Get-G2LogInt64 -Text $runText -Key "UDP_TX_SEQUENCE_TOTAL_COUNT"
        Write-Host "UDP_TX_SEQUENCE_TOTAL_COUNT=$sequenceCount"

        if ($sequenceCount -le 0) {
            throw "G2_UDP_TX_SEQUENCE_EMPTY"
        }
    }

    if ($mode.Key -eq "UDP_RX") {
        Write-Host ("RUN {0} PASS PC_OFFERED_MBPS={1} DUT_MBPS={2} HOLD_MAX_US={3}" -f $mode.Key, $pcMbps, $dutMbps, $holdMaxUs)
    }
    else {
        Write-Host ("RUN {0} PASS PC_MBPS={1} DUT_MBPS={2} HOLD_MAX_US={3}" -f $mode.Key, $pcMbps, $dutMbps, $holdMaxUs)
    }

    $summaryLines.Add(("{0}_PC_MBPS={1}" -f $mode.Key, $pcMbps))
    $summaryLines.Add(("{0}_DUT_MBPS={1}" -f $mode.Key, $dutMbps))
    $summaryLines.Add(("{0}_TCP_SPI_HOLD_AVG_US={1}" -f $mode.Key, $holdAvgUs))
    $summaryLines.Add(("{0}_TCP_SPI_HOLD_MAX_US={1}" -f $mode.Key, $holdMaxUs))
    $summaryLines.Add(("{0}_LOOP_GAP_MAX_US={1}" -f $mode.Key, $loopGapMaxUs))
    $summaryLines.Add(("{0}_PASS=YES" -f $mode.Key))
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
        throw "G2_FINAL_DIRTY_STATE_INVALID_BASELINE"
    }
}
else {
    if (
        $dirtyAfter.Count -ne 1 -or
        $dirtyAfter[0] -ne $script:G2SpiHeaderRelative
    ) {
        throw "G2_FINAL_DIRTY_STATE_INVALID"
    }
}

$stagedAfter = @(
    & git -C $script:G2RepoRoot diff --cached --name-only
)

if ($LASTEXITCODE -ne 0) {
    throw "G2_FINAL_CACHED_DIFF_FAILED"
}

Write-Host "STAGED_COUNT_FINAL=$($stagedAfter.Count)"

if ($stagedAfter.Count -ne 0) {
    throw "G2_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT."
$visualAnswer = ""

while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante el gate? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = 0
if ($visualAnswer -eq "S") {
    $visualEvents = 1
}

Write-Host "VISUAL_SPI_EVENTS=$visualEvents"
$summaryLines.Add("VISUAL_SPI_EVENTS=$visualEvents")
$summaryLines.Add("COMPILE_EXIT=$compileExit")
$summaryLines.Add("UPLOAD_EXIT=$uploadExit")
$summaryLines.Add("TEMP_ROOT=$tempRoot")

if ($visualEvents -ne 0) {
    $summaryLines.Add("G2_PHYSICAL_SMOKE_EXACT=FAIL")
    $summaryLines | Set-Content -LiteralPath $summaryLog -Encoding UTF8
    Write-Host "SUMMARY_LOG=$summaryLog"
    Write-Host "G2_PHYSICAL_SMOKE_EXACT=FAIL"
    exit 2
}

$summaryLines.Add("G2_PHYSICAL_SMOKE_EXACT=PASS")
$summaryLines | Set-Content -LiteralPath $summaryLog -Encoding UTF8

Write-Host ""
Write-Host "============================================================"
Write-Host " G2 PHYSICAL SMOKE EXACT SUMMARY"
Write-Host "============================================================"
Write-Host "MHZ=$MHz"
Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "RUNTIME_READY=YES"
Write-Host "FOUR_MODES_PASS=YES"
Write-Host "SNAPSHOT_SOURCE=FROZEN_RUNNER_EXACT_FINAL"
Write-Host "VISUAL_SPI_EVENTS=0"
Write-Host "SUMMARY_LOG=$summaryLog"
Write-Host "G2_PHYSICAL_SMOKE_EXACT=PASS"
