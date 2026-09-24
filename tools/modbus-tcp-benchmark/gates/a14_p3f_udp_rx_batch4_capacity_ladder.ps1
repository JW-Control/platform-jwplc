param(
    [string]$SerialPort = "COM14",
    [double]$DurationSeconds = 5.0,
    [int]$Runs = 3
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$expectedRawHash = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
$expectedW5100CppHash = "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F"
$expectedW5100HHash = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
$p3eReference1016Mbps = [double]12.629949
$udpRxBufferBytes = [int64]2048
$udpInternalHeaderBytes = [int64]8
$payloads = @(1016, 505, 504, 500)

function Invoke-NativeToLog {
    param([string]$FilePath, [string[]]$Arguments, [string]$LogPath)

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $FilePath @Arguments *> $LogPath
        return [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

function Get-LogValue {
    param([string]$Text, [string]$Key)

    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))

    if ($matches.Count -ne 1) {
        throw ("P3_LOG_KEY_COUNT_{0}={1}" -f $Key, $matches.Count)
    }

    return $matches[0].Groups[1].Value.Trim()
}

function Get-LogInt64 {
    param([string]$Text, [string]$Key)

    return [int64]::Parse(
        (Get-LogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Get-LogDouble {
    param([string]$Text, [string]$Key)

    return [double]::Parse(
        (Get-LogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Get-Median {
    param([object[]]$Values)

    $sorted = @($Values | Sort-Object)
    if ($sorted.Count -eq 0) {
        throw "P3_MEDIAN_EMPTY"
    }

    $middle = [int][math]::Floor($sorted.Count / 2)
    if (($sorted.Count % 2) -eq 1) {
        return $sorted[$middle]
    }

    return ($sorted[$middle - 1] + $sorted[$middle]) / 2
}

Write-Host "============================================================"
Write-Host " A14 P3F - UDP RX BATCH4 / 2KB CAPACITY LADDER @ 26 MHz"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationSeconds -le 0) { throw "P3_DURATION_INVALID" }
if ($Runs -lt 1) { throw "P3_RUNS_INVALID" }

$head = Get-G2Head
$effectiveHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($LASTEXITCODE -ne 0) { throw "P3_CACHED_DIFF_FAILED" }

Write-Host "HEAD=$head"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "RUNS=$Runs"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "ALGORITHM=UDP_RX_BATCH4_DIAGNOSTIC"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"
Write-Host "DIAGNOSTIC_COPY_ONLY=YES"
Write-Host "SOCKET_TOPOLOGY=8x2KB_UNCHANGED"
Write-Host "UDP_RX_BUFFER_BYTES=$udpRxBufferBytes"
Write-Host "UDP_INTERNAL_HEADER_BYTES=$udpInternalHeaderBytes"
Write-Host "PAYLOADS=$($payloads -join ',')"
Write-Host "P3E_REFERENCE_1016_DUT_MBPS=$p3eReference1016Mbps"

if ($effectiveHz -ne 26000000) { throw "P3_EXPECTED_26MHZ" }
if ($dirty.Count -ne 0) { throw "P3_TRACKED_TREE_NOT_CLEAN" }
if ($staged.Count -ne 0) { throw "P3_INDEX_NOT_CLEAN" }

Assert-G2ProtectedArtifacts

$rawHash = Get-G2Sha256 $script:G2RawFirmwareRelative
$w5100CppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
$w5100CppHash = Get-G2Sha256 $w5100CppRelative
$w5100HHash = Get-G2Sha256 $script:G2SpiHeaderRelative

Write-Host "RAW_FIRMWARE_SHA256=$rawHash"
Write-Host "W5100_CPP_SHA256=$w5100CppHash"
Write-Host "W5100_H_SHA256=$w5100HHash"

if ($rawHash -ne $expectedRawHash) { throw "P3_RAW_HASH_MISMATCH" }
if ($w5100CppHash -ne $expectedW5100CppHash) { throw "P3_W5100_CPP_HASH_MISMATCH" }
if ($w5100HHash -ne $expectedW5100HHash) { throw "P3_W5100_H_HASH_MISMATCH" }

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) { throw "P3_PYTHON_NOT_FOUND" }

$pythonExe = $pythonCommand.Source
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) { throw "P3_ARDUINO_CLI_NOT_FOUND" }

$patcherPath = Join-Path $PSScriptRoot "a14_p3_udp_rx_instrument_patch.py"
$batchPatchPath = Join-Path $PSScriptRoot "a14_p3f_udp_rx_batch4_patch.py"
$bridgePath = Join-Path $PSScriptRoot "a14_p3_udp_rx_instrument_bridge.py"
$resolverPath = Join-Path $PSScriptRoot "a14_nb3_resolve_dut_ip.py"

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p3f_udp_rx_batch4_capacity_{0}" -f $timestamp)
$workRoot = Join-Path $tempRoot "instrumented"
$buildPath = Join-Path $tempRoot "build"
$patchLog = Join-Path $tempRoot "patch.log"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"
$ipLog = Join-Path $tempRoot "dut_ip.log"

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "PYTHON=$pythonExe"
Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== PYTHON SYNTAX PRECHECK ==="

$pyArgs = @(
    "-m", "py_compile",
    $patcherPath,
    $batchPatchPath,
    $bridgePath,
    $resolverPath
)
$pyExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $pyArgs -LogPath (Join-Path $tempRoot "py_compile.log")
Write-Host "PY_COMPILE_EXIT=$pyExit"
if ($pyExit -ne 0) { throw "P3_PYTHON_SYNTAX_FAILED" }

Write-Host ""
Write-Host "=== BUILD ISOLATED DIAGNOSTIC COPY ==="

$patchArgs = @(
    $patcherPath,
    "--repo-root", $script:G2RepoRoot,
    "--work-root", $workRoot
)
$patchExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $patchArgs -LogPath $patchLog
Write-Host "PATCH_EXIT=$patchExit"
Write-Host "PATCH_LOG=$patchLog"

if ($patchExit -ne 0) {
    Get-Content -LiteralPath $patchLog -Tail 160 | ForEach-Object { Write-Host $_ }
    throw "P3_DIAGNOSTIC_PATCH_FAILED"
}

Get-Content -LiteralPath $patchLog | ForEach-Object { Write-Host $_ }

Write-Host ""
Write-Host "=== APPLY UDP RX BATCH4 DIAGNOSTIC PATCH ==="

$diagSketchPath = Join-Path $workRoot "sketch\eth14_raw_transport_server\eth14_raw_transport_server.ino"
$batchPatchLog = Join-Path $tempRoot "batch4_patch.log"
$batchPatchArgs = @(
    $batchPatchPath,
    "--instrumented-sketch", $diagSketchPath
)
$batchPatchExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $batchPatchArgs -LogPath $batchPatchLog

Write-Host "BATCH4_PATCH_EXIT=$batchPatchExit"
Write-Host "BATCH4_PATCH_LOG=$batchPatchLog"

if ($batchPatchExit -ne 0) {
    Get-Content -LiteralPath $batchPatchLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "P3F_BATCH4_PATCH_FAILED"
}

Get-Content -LiteralPath $batchPatchLog | ForEach-Object { Write-Host $_ }

$diagLibrariesRoot = Join-Path $workRoot "libraries"
$diagEthernetRoot = Join-Path $diagLibrariesRoot "JWPLC_Ethernet"
$diagSketchDir = Join-Path $workRoot "sketch\eth14_raw_transport_server"
$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$fqbn = "jwplc_local:esp32:jwplcbasic"

if (-not (Test-Path -LiteralPath $diagEthernetRoot)) { throw "P3_DIAG_ETHERNET_MISSING" }
if (-not (Test-Path -LiteralPath $diagSketchDir)) { throw "P3_DIAG_SKETCH_MISSING" }

Write-Host ""
Write-Host "=== COMPILE DIAGNOSTIC COPY ==="

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $diagLibrariesRoot,
    "--libraries", $repoLibrariesRoot,
    $diagSketchDir
)
$compileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog

Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 160 | ForEach-Object { Write-Host $_ }
    throw "P3_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$diagEthernetUsed = $compileText.IndexOf(
    $diagEthernetRoot,
    [System.StringComparison]::OrdinalIgnoreCase
) -ge 0

Write-Host "DIAGNOSTIC_ETHERNET_LIBRARY_USED=$diagEthernetUsed"

if (-not $diagEthernetUsed) {
    Get-Content -LiteralPath $compileLog -Tail 160 | ForEach-Object { Write-Host $_ }
    throw "P3_DIAGNOSTIC_LIBRARY_NOT_SELECTED"
}

$binCount = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin").Count
Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) { throw "P3_BIN_MISSING" }

Invoke-G2CompileFinishedSound -Success $true

Write-Host ""
Write-Host "=== UPLOAD DIAGNOSTIC COPY ==="

$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildPath,
    $diagSketchDir
)
$uploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog

Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"

if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 160 | ForEach-Object { Write-Host $_ }
    throw "P3_UPLOAD_FAILED"
}

Start-Sleep -Seconds 2

Write-Host ""
Write-Host "=== RESOLVE CURRENT DUT IP ==="

$ipArgs = @($resolverPath, "--serial", $SerialPort, "--timeout", "15")
$ipExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $ipArgs -LogPath $ipLog
Write-Host "IP_RESOLVER_EXIT=$ipExit"
Write-Host "IP_RESOLVER_LOG=$ipLog"

if ($ipExit -ne 0) {
    Get-Content -LiteralPath $ipLog -Tail 160 | ForEach-Object { Write-Host $_ }
    throw "P3_IP_RESOLVER_FAILED"
}

$ipText = [System.IO.File]::ReadAllText($ipLog)
$dutIp = Get-LogValue -Text $ipText -Key "DUT_IP_EFFECTIVE"
Write-Host "DUT_IP_EFFECTIVE=$dutIp"

$durationText = $DurationSeconds.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$results = @{}

Write-Host ""
Write-Host "=== P3F UDP RX 2KB CAPACITY LADDER RUNS ==="

foreach ($payload in $payloads) {
    $recordBytes = [int64]$payload + $udpInternalHeaderBytes
    $completeDatagramsPer2K =
        [int64][math]::Floor(
            $udpRxBufferBytes /
            [double]$recordBytes
        )

    Write-Host ""
    Write-Host ("--- PAYLOAD={0} RECORD_BYTES={1} COMPLETE_DATAGRAMS_PER_2KB={2} ---" -f
        $payload,
        $recordBytes,
        $completeDatagramsPer2K
    )

    $dutMbpsValues = New-Object System.Collections.Generic.List[double]
    $packetsPerHoldValues = New-Object System.Collections.Generic.List[int64]
    $emptyRatioValues = New-Object System.Collections.Generic.List[double]
    $activeHoldAvgValues = New-Object System.Collections.Generic.List[int64]
    $activeHoldMaxValues = New-Object System.Collections.Generic.List[int64]
    $parseAvgValues = New-Object System.Collections.Generic.List[int64]
    $readAvgValues = New-Object System.Collections.Generic.List[int64]
    $spiReadsPerPacketValues = New-Object System.Collections.Generic.List[int64]

    for ($run = 1; $run -le $Runs; ++$run) {
        $runLog = Join-Path $tempRoot ("udp_rx_payload_{0}_run_{1}.log" -f $payload, $run)
        $runArgs = @(
            $bridgePath,
            "--host", $dutIp,
            "--serial", $SerialPort,
            "--duration", $durationText,
            "--mode", "udp-rx",
            "--udp-payload", ([string]$payload)
        )

        $runExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runArgs -LogPath $runLog
        Write-Host "PAYLOAD=$payload RUN=$run RUNNER_EXIT=$runExit LOG=$runLog"

        if ($runExit -ne 0) {
            Get-Content -LiteralPath $runLog -Tail 180 | ForEach-Object { Write-Host $_ }
            throw "P3E_RUNNER_FAILED_PAYLOAD_$payload" + "_RUN_$run"
        }

        $text = [System.IO.File]::ReadAllText($runLog)

        if ((Get-LogValue -Text $text -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
            throw "P3E_FUNCTIONAL_FAIL_PAYLOAD_$payload" + "_RUN_$run"
        }
        if ((Get-LogValue -Text $text -Key "P3_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
            throw "P3E_SNAPSHOT_MISSING_PAYLOAD_$payload" + "_RUN_$run"
        }
        if ((Get-LogValue -Text $text -Key "P3_FINAL_ETH_READY") -ne "YES") {
            throw "P3E_ETH_NOT_READY_PAYLOAD_$payload" + "_RUN_$run"
        }
        if ((Get-LogValue -Text $text -Key "P3_FINAL_ETH_LINK") -ne "UP") {
            throw "P3E_LINK_DOWN_PAYLOAD_$payload" + "_RUN_$run"
        }
        if ((Get-LogValue -Text $text -Key "P3_FINAL_IP") -ne $dutIp) {
            throw "P3E_IP_MISMATCH_PAYLOAD_$payload" + "_RUN_$run"
        }

        $transportErrors = Get-LogInt64 -Text $text -Key "P3_FINAL_TRANSPORT_ERRORS"
        $udpSpiLockErrors = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_SPI_LOCK_ERRORS"
        $packets = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_PACKETS"
        $rxOperations = Get-LogInt64 -Text $text -Key "P3_FINAL_RX_OPERATIONS"
        $packetParseCalls = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_PACKET_PARSE_CALLS"
        $readCalls = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_READ_CALLS"
        $spiReadCalls = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_SPI_READ_CALLS"
        $activeHolds = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_ACTIVE_HOLD_COUNT"
        $serviceHolds = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_SERVICE_HOLD_COUNT"
        $emptyHolds = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_EMPTY_HOLD_COUNT"

        if ($transportErrors -ne 0) { throw "P3E_TRANSPORT_ERRORS_PAYLOAD_$payload" + "_RUN_$run=$transportErrors" }
        if ($udpSpiLockErrors -ne 0) { throw "P3E_UDP_SPI_LOCK_ERRORS_PAYLOAD_$payload" + "_RUN_$run=$udpSpiLockErrors" }
        if ($packets -le 0) { throw "P3E_NO_PACKETS_PAYLOAD_$payload" + "_RUN_$run" }
        if ($packets -ne $rxOperations) { throw "P3E_PACKET_OPERATION_MISMATCH_PAYLOAD_$payload" + "_RUN_$run" }
        if ($packetParseCalls -ne $packets) { throw "P3E_PACKET_PARSE_COUNT_MISMATCH_PAYLOAD_$payload" + "_RUN_$run" }
        if ($readCalls -lt $packets) { throw "P3E_READ_CALL_COUNT_INVALID_PAYLOAD_$payload" + "_RUN_$run" }
        if ($spiReadCalls -le 0) { throw "P3E_SPI_READ_COUNT_EMPTY_PAYLOAD_$payload" + "_RUN_$run" }
        if ($activeHolds -le 0) { throw "P3E_ACTIVE_HOLD_COUNT_EMPTY_PAYLOAD_$payload" + "_RUN_$run" }
        if ($activeHolds -gt $packets) { throw "P3E_ACTIVE_HOLDS_GT_PACKETS_PAYLOAD_$payload" + "_RUN_$run" }
        if ($packets -gt (4 * $activeHolds)) { throw "P3F_PACKETS_GT_BATCH4_CAPACITY_PAYLOAD_$payload" + "_RUN_$run" }
        if ($serviceHolds -le 0) { throw "P3E_SERVICE_HOLD_COUNT_EMPTY_PAYLOAD_$payload" + "_RUN_$run" }
        if ($emptyHolds -lt 0 -or $emptyHolds -gt $serviceHolds) { throw "P3E_EMPTY_HOLD_COUNT_INVALID_PAYLOAD_$payload" + "_RUN_$run" }

        $packetsPerActiveHoldX1000 =
            [int64][math]::Round(
                (1000.0 * $packets) / $activeHolds,
                0,
                [System.MidpointRounding]::AwayFromZero
            )

        $emptyHoldRatioPct =
            (100.0 * $emptyHolds) / $serviceHolds

        $dutMbps = Get-LogDouble -Text $text -Key "SUMMARY_UDP_RX_DUT_MBPS"
        $parseAvg = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_PACKET_PARSE_US_AVG"
        $readAvg = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_READ_US_AVG"
        $spiReadsPerPacketX1000 = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_SPI_READS_PER_PACKET_X1000"
        $activeHoldAvg = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_ACTIVE_HOLD_US_AVG"
        $activeHoldMax = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_ACTIVE_HOLD_US_MAX"

        $dutMbpsValues.Add($dutMbps)
        $packetsPerHoldValues.Add($packetsPerActiveHoldX1000)
        $emptyRatioValues.Add($emptyHoldRatioPct)
        $activeHoldAvgValues.Add($activeHoldAvg)
        $activeHoldMaxValues.Add($activeHoldMax)
        $parseAvgValues.Add($parseAvg)
        $readAvgValues.Add($readAvg)
        $spiReadsPerPacketValues.Add($spiReadsPerPacketX1000)

        Write-Host (
            "P3F_RUN PAYLOAD={0} RUN={1} DUT_MBPS={2:F6} PACKETS={3} ACTIVE_HOLDS={4} PACKETS_PER_ACTIVE_HOLD_X1000={5} SERVICE_HOLDS={6} EMPTY_HOLDS={7} EMPTY_HOLD_RATIO_PCT={8:F2} PARSE_AVG_US={9} READ_AVG_US={10} SPI_READS_PER_PACKET_X1000={11} ACTIVE_HOLD_AVG_US={12} ACTIVE_HOLD_MAX_US={13}" -f
            $payload,
            $run,
            $dutMbps,
            $packets,
            $activeHolds,
            $packetsPerActiveHoldX1000,
            $serviceHolds,
            $emptyHolds,
            $emptyHoldRatioPct,
            $parseAvg,
            $readAvg,
            $spiReadsPerPacketX1000,
            $activeHoldAvg,
            $activeHoldMax
        )
    }

    $medianDutMbps = [double](Get-Median -Values $dutMbpsValues.ToArray())
    $medianPacketsPerHold = [int64](Get-Median -Values $packetsPerHoldValues.ToArray())
    $medianEmptyRatio = [double](Get-Median -Values $emptyRatioValues.ToArray())
    $medianActiveHoldAvg = [int64](Get-Median -Values $activeHoldAvgValues.ToArray())
    $medianActiveHoldMax = [int64](Get-Median -Values $activeHoldMaxValues.ToArray())
    $medianParseAvg = [int64](Get-Median -Values $parseAvgValues.ToArray())
    $medianReadAvg = [int64](Get-Median -Values $readAvgValues.ToArray())
    $medianSpiReadsPerPacket = [int64](Get-Median -Values $spiReadsPerPacketValues.ToArray())

    $results[$payload] = [pscustomobject]@{
        Payload = $payload
        RecordBytes = $recordBytes
        CompleteDatagramsPer2K = $completeDatagramsPer2K
        DutMbps = $medianDutMbps
        PacketsPerActiveHoldX1000 = $medianPacketsPerHold
        EmptyHoldRatioPct = $medianEmptyRatio
        ActiveHoldAvgUs = $medianActiveHoldAvg
        ActiveHoldMaxUs = $medianActiveHoldMax
        ParseAvgUs = $medianParseAvg
        ReadAvgUs = $medianReadAvg
        SpiReadsPerPacketX1000 = $medianSpiReadsPerPacket
    }

    Write-Host (
        "P3F_MEDIAN PAYLOAD={0} RECORD_BYTES={1} COMPLETE_DATAGRAMS_PER_2KB={2} DUT_MBPS={3:F6} PACKETS_PER_ACTIVE_HOLD_X1000={4} EMPTY_HOLD_RATIO_PCT={5:F2} PARSE_AVG_US={6} READ_AVG_US={7} SPI_READS_PER_PACKET_X1000={8} ACTIVE_HOLD_AVG_US={9} ACTIVE_HOLD_MAX_US={10}" -f
        $payload,
        $recordBytes,
        $completeDatagramsPer2K,
        $medianDutMbps,
        $medianPacketsPerHold,
        $medianEmptyRatio,
        $medianParseAvg,
        $medianReadAvg,
        $medianSpiReadsPerPacket,
        $medianActiveHoldAvg,
        $medianActiveHoldMax
    )
}

$r1016 = $results[1016]
$r505 = $results[505]
$r504 = $results[504]
$r500 = $results[500]

$boundaryPphDeltaX1000 =
    $r504.PacketsPerActiveHoldX1000 -
    $r505.PacketsPerActiveHoldX1000

$boundaryThroughputDeltaPct =
    (($r504.DutMbps / $r505.DutMbps) - 1.0) * 100.0

$batch4VsP3e1016DeltaPct =
    (($r1016.DutMbps / $p3eReference1016Mbps) - 1.0) * 100.0

$interpretation = "NO_CLEAR_4PACKET_BOUNDARY_EFFECT"
if (
    $r504.PacketsPerActiveHoldX1000 -ge 3800 -and
    $boundaryPphDeltaX1000 -ge 700
) {
    $interpretation = "STRONG_2KB_FOUR_PACKET_BOUNDARY_EFFECT"
}
elseif ($boundaryPphDeltaX1000 -ge 300) {
    $interpretation = "FOUR_PACKET_BOUNDARY_EFFECT"
}

Write-Host ""
Write-Host "=== P3F CAPACITY LADDER ANALYSIS ==="
Write-Host "P3F_1016_RECORD_BYTES=$($r1016.RecordBytes)"
Write-Host "P3F_505_RECORD_BYTES=$($r505.RecordBytes)"
Write-Host "P3F_504_RECORD_BYTES=$($r504.RecordBytes)"
Write-Host "P3F_500_RECORD_BYTES=$($r500.RecordBytes)"
Write-Host "P3F_1016_COMPLETE_DATAGRAMS_PER_2KB=$($r1016.CompleteDatagramsPer2K)"
Write-Host "P3F_505_COMPLETE_DATAGRAMS_PER_2KB=$($r505.CompleteDatagramsPer2K)"
Write-Host "P3F_504_COMPLETE_DATAGRAMS_PER_2KB=$($r504.CompleteDatagramsPer2K)"
Write-Host "P3F_500_COMPLETE_DATAGRAMS_PER_2KB=$($r500.CompleteDatagramsPer2K)"
Write-Host "P3F_1016_PACKETS_PER_ACTIVE_HOLD_X1000=$($r1016.PacketsPerActiveHoldX1000)"
Write-Host "P3F_505_PACKETS_PER_ACTIVE_HOLD_X1000=$($r505.PacketsPerActiveHoldX1000)"
Write-Host "P3F_504_PACKETS_PER_ACTIVE_HOLD_X1000=$($r504.PacketsPerActiveHoldX1000)"
Write-Host "P3F_500_PACKETS_PER_ACTIVE_HOLD_X1000=$($r500.PacketsPerActiveHoldX1000)"
Write-Host "P3F_505_TO_504_PPH_DELTA_X1000=$boundaryPphDeltaX1000"
Write-Host ("P3F_505_DUT_MBPS={0:F6}" -f $r505.DutMbps)
Write-Host ("P3F_504_DUT_MBPS={0:F6}" -f $r504.DutMbps)
Write-Host ("P3F_500_DUT_MBPS={0:F6}" -f $r500.DutMbps)
Write-Host ("P3F_505_TO_504_THROUGHPUT_DELTA_PCT={0:F2}" -f $boundaryThroughputDeltaPct)
Write-Host ("P3F_BATCH4_1016_VS_P3E_BATCH2_1016_DELTA_PCT={0:F2}" -f $batch4VsP3e1016DeltaPct)
Write-Host "P3F_INTERPRETATION=$interpretation"

Write-Host ""
Write-Host "=== FINAL PRODUCT INVARIANTS ==="

Assert-G2ProtectedArtifacts

$finalHz = Get-G2SpiHz
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
$finalRawHash = Get-G2Sha256 $script:G2RawFirmwareRelative
$finalW5100CppHash = Get-G2Sha256 $w5100CppRelative
$finalW5100HHash = Get-G2Sha256 $script:G2SpiHeaderRelative

Write-Host "FINAL_SPI_HZ=$finalHz"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"
Write-Host "FINAL_RAW_FIRMWARE_SHA256=$finalRawHash"
Write-Host "FINAL_W5100_CPP_SHA256=$finalW5100CppHash"
Write-Host "FINAL_W5100_H_SHA256=$finalW5100HHash"

if ($finalHz -ne 26000000) { throw "P3_FINAL_FREQ_CHANGED" }
if ($finalDirty.Count -ne 0) { throw "P3_PRODUCT_TREE_DIRTY" }
if ($finalStaged.Count -ne 0) { throw "P3_PRODUCT_INDEX_DIRTY" }
if ($finalRawHash -ne $expectedRawHash) { throw "P3_FINAL_RAW_HASH_CHANGED" }
if ($finalW5100CppHash -ne $expectedW5100CppHash) { throw "P3_FINAL_W5100_CPP_HASH_CHANGED" }
if ($finalW5100HHash -ne $expectedW5100HHash) { throw "P3_FINAL_W5100_H_HASH_CHANGED" }

Write-Host ""
Write-Host "A14_P3F_SOCKET_TOPOLOGY=8x2KB_UNCHANGED"
Write-Host "A14_P3F_ALGORITHM=UDP_RX_BATCH4_DIAGNOSTIC"
Write-Host "A14_P3F_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "A14_P3F_DIAGNOSTIC_COPY_ONLY=YES"
Write-Host "A14_P3F_UDP_RX_BATCH4_CAPACITY_LADDER=PASS"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT_FOR_BATCH4_AND_G4_DECISION"
