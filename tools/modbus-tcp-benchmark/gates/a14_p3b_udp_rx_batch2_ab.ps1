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
$baselineMedianDutMbps = [double]10.875310
$baselineMedianActiveHoldAvgUs = [int64]829
$baselineMedianActiveHoldMaxUs = [int64]1449

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
Write-Host " A14 P3B - UDP RX BATCH2 A/B @ 26 MHz"
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
Write-Host "ALGORITHM_CHANGE=UDP_RX_MAX_PACKETS_PER_HOLD_1_TO_2"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"
Write-Host "DIAGNOSTIC_COPY_ONLY=YES"
Write-Host "BASELINE_MEDIAN_UDP_RX_DUT_MBPS=$baselineMedianDutMbps"
Write-Host "BASELINE_MEDIAN_ACTIVE_HOLD_AVG_US=$baselineMedianActiveHoldAvgUs"
Write-Host "BASELINE_MEDIAN_ACTIVE_HOLD_MAX_US=$baselineMedianActiveHoldMaxUs"

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
$batchPatchPath = Join-Path $PSScriptRoot "a14_p3b_udp_rx_batch2_patch.py"
$bridgePath = Join-Path $PSScriptRoot "a14_p3_udp_rx_instrument_bridge.py"
$resolverPath = Join-Path $PSScriptRoot "a14_nb3_resolve_dut_ip.py"

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p3b_udp_rx_batch2_{0}" -f $timestamp)
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
Write-Host "=== APPLY SINGLE-VARIABLE UDP RX BATCH2 PATCH ==="

$diagSketchPath = Join-Path $workRoot "sketch\eth14_raw_transport_server\eth14_raw_transport_server.ino"
$batchPatchLog = Join-Path $tempRoot "batch2_patch.log"
$batchPatchArgs = @(
    $batchPatchPath,
    "--instrumented-sketch", $diagSketchPath
)
$batchPatchExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $batchPatchArgs -LogPath $batchPatchLog

Write-Host "BATCH2_PATCH_EXIT=$batchPatchExit"
Write-Host "BATCH2_PATCH_LOG=$batchPatchLog"

if ($batchPatchExit -ne 0) {
    Get-Content -LiteralPath $batchPatchLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "P3B_BATCH2_PATCH_FAILED"
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

$dutMbpsValues = New-Object System.Collections.Generic.List[double]
$parseAvgValues = New-Object System.Collections.Generic.List[int64]
$parseMaxValues = New-Object System.Collections.Generic.List[int64]
$readAvgValues = New-Object System.Collections.Generic.List[int64]
$readMaxValues = New-Object System.Collections.Generic.List[int64]
$spiReadsPerPacketValues = New-Object System.Collections.Generic.List[int64]
$activeHoldAvgValues = New-Object System.Collections.Generic.List[int64]
$activeHoldMaxValues = New-Object System.Collections.Generic.List[int64]
$bytesPerActiveHoldValues = New-Object System.Collections.Generic.List[int64]

Write-Host ""
Write-Host "=== UDP RX BATCH2 A/B RUNS ==="

for ($run = 1; $run -le $Runs; ++$run) {
    $runLog = Join-Path $tempRoot ("udp_rx_run_{0}.log" -f $run)
    $runArgs = @(
        $bridgePath,
        "--host", $dutIp,
        "--serial", $SerialPort,
        "--duration", $durationText,
        "--mode", "udp-rx",
        "--udp-payload", "1472"
    )

    $runExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runArgs -LogPath $runLog
    Write-Host "RUN=$run RUNNER_EXIT=$runExit LOG=$runLog"

    if ($runExit -ne 0) {
        Get-Content -LiteralPath $runLog -Tail 180 | ForEach-Object { Write-Host $_ }
        throw "P3_RUNNER_FAILED_$run"
    }

    $text = [System.IO.File]::ReadAllText($runLog)

    if ((Get-LogValue -Text $text -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
        throw "P3_FUNCTIONAL_FAIL_$run"
    }
    if ((Get-LogValue -Text $text -Key "P3_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
        throw "P3_SNAPSHOT_MISSING_$run"
    }
    if ((Get-LogValue -Text $text -Key "P3_FINAL_ETH_READY") -ne "YES") {
        throw "P3_ETH_NOT_READY_$run"
    }
    if ((Get-LogValue -Text $text -Key "P3_FINAL_ETH_LINK") -ne "UP") {
        throw "P3_LINK_DOWN_$run"
    }
    if ((Get-LogValue -Text $text -Key "P3_FINAL_IP") -ne $dutIp) {
        throw "P3_IP_MISMATCH_$run"
    }

    $transportErrors = Get-LogInt64 -Text $text -Key "P3_FINAL_TRANSPORT_ERRORS"
    $udpSpiLockErrors = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_SPI_LOCK_ERRORS"
    $packets = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_PACKETS"
    $rxOperations = Get-LogInt64 -Text $text -Key "P3_FINAL_RX_OPERATIONS"
    $packetParseCalls = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_PACKET_PARSE_CALLS"
    $readCalls = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_READ_CALLS"
    $spiReadCalls = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_SPI_READ_CALLS"
    $activeHolds = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_ACTIVE_HOLD_COUNT"

    if ($transportErrors -ne 0) { throw "P3_TRANSPORT_ERRORS_$run=$transportErrors" }
    if ($udpSpiLockErrors -ne 0) { throw "P3_UDP_SPI_LOCK_ERRORS_$run=$udpSpiLockErrors" }
    if ($packets -le 0) { throw "P3_NO_PACKETS_$run" }
    if ($packets -ne $rxOperations) { throw "P3_PACKET_OPERATION_MISMATCH_$run" }
    if ($packetParseCalls -ne $packets) { throw "P3_PACKET_PARSE_COUNT_MISMATCH_$run" }
    if ($readCalls -lt $packets) { throw "P3_READ_CALL_COUNT_INVALID_$run" }
    if ($spiReadCalls -le 0) { throw "P3_SPI_READ_COUNT_EMPTY_$run" }
    if ($activeHolds -ne $packets) { throw "P3_ACTIVE_HOLD_COUNT_MISMATCH_$run" }

    $dutMbps = Get-LogDouble -Text $text -Key "SUMMARY_UDP_RX_DUT_MBPS"
    $parseAvg = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_PACKET_PARSE_US_AVG"
    $parseMax = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_PACKET_PARSE_US_MAX"
    $readAvg = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_READ_US_AVG"
    $readMax = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_READ_US_MAX"
    $spiReadsPerPacketX1000 = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_SPI_READS_PER_PACKET_X1000"
    $serviceHolds = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_SERVICE_HOLD_COUNT"
    $emptyHolds = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_EMPTY_HOLD_COUNT"
    $activeHoldAvg = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_ACTIVE_HOLD_US_AVG"
    $activeHoldMax = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_ACTIVE_HOLD_US_MAX"
    $bytesPerActiveHoldX1000 = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_BYTES_PER_ACTIVE_HOLD_X1000"
    $spiReadBytesPerPacketX1000 = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_SPI_READ_BYTES_PER_PACKET_X1000"

    $dutMbpsValues.Add($dutMbps)
    $parseAvgValues.Add($parseAvg)
    $parseMaxValues.Add($parseMax)
    $readAvgValues.Add($readAvg)
    $readMaxValues.Add($readMax)
    $spiReadsPerPacketValues.Add($spiReadsPerPacketX1000)
    $activeHoldAvgValues.Add($activeHoldAvg)
    $activeHoldMaxValues.Add($activeHoldMax)
    $bytesPerActiveHoldValues.Add($bytesPerActiveHoldX1000)

    Write-Host (
        "P3_RUN={0} DUT_MBPS={1:F6} PACKETS={2} PACKET_PARSE_AVG_US={3} PACKET_PARSE_MAX_US={4} READ_AVG_US={5} READ_MAX_US={6} SPI_READS_PER_PACKET_X1000={7} SPI_READ_BYTES_PER_PACKET_X1000={8} SERVICE_HOLDS={9} EMPTY_HOLDS={10} ACTIVE_HOLD_AVG_US={11} ACTIVE_HOLD_MAX_US={12} BYTES_PER_ACTIVE_HOLD_X1000={13}" -f
        $run,
        $dutMbps,
        $packets,
        $parseAvg,
        $parseMax,
        $readAvg,
        $readMax,
        $spiReadsPerPacketX1000,
        $spiReadBytesPerPacketX1000,
        $serviceHolds,
        $emptyHolds,
        $activeHoldAvg,
        $activeHoldMax,
        $bytesPerActiveHoldX1000
    )
}

$medianDutMbps = Get-Median -Values $dutMbpsValues.ToArray()
$medianParseAvg = Get-Median -Values $parseAvgValues.ToArray()
$medianParseMax = Get-Median -Values $parseMaxValues.ToArray()
$medianReadAvg = Get-Median -Values $readAvgValues.ToArray()
$medianReadMax = Get-Median -Values $readMaxValues.ToArray()
$medianSpiReadsPerPacket = Get-Median -Values $spiReadsPerPacketValues.ToArray()
$medianActiveHoldAvg = Get-Median -Values $activeHoldAvgValues.ToArray()
$medianActiveHoldMax = Get-Median -Values $activeHoldMaxValues.ToArray()
$medianBytesPerActiveHold = Get-Median -Values $bytesPerActiveHoldValues.ToArray()

Write-Host ""
Write-Host "=== P3 MEDIANS ==="
Write-Host ("P3_MEDIAN_UDP_RX_DUT_MBPS={0:F6}" -f $medianDutMbps)
Write-Host "P3_MEDIAN_PACKET_PARSE_AVG_US=$medianParseAvg"
Write-Host "P3_MEDIAN_PACKET_PARSE_MAX_US=$medianParseMax"
Write-Host "P3_MEDIAN_READ_AVG_US=$medianReadAvg"
Write-Host "P3_MEDIAN_READ_MAX_US=$medianReadMax"
Write-Host "P3_MEDIAN_SPI_READS_PER_PACKET_X1000=$medianSpiReadsPerPacket"
Write-Host "P3_MEDIAN_ACTIVE_HOLD_AVG_US=$medianActiveHoldAvg"
Write-Host "P3_MEDIAN_ACTIVE_HOLD_MAX_US=$medianActiveHoldMax"
Write-Host "P3_MEDIAN_BYTES_PER_ACTIVE_HOLD_X1000=$medianBytesPerActiveHold"

$throughputGainPct = (($medianDutMbps / $baselineMedianDutMbps) - 1.0) * 100.0
$holdAvgGainPct = (($medianActiveHoldAvg / $baselineMedianActiveHoldAvgUs) - 1.0) * 100.0
$holdMaxGainPct = (($medianActiveHoldMax / $baselineMedianActiveHoldMaxUs) - 1.0) * 100.0

Write-Host ""
Write-Host "=== P3B BATCH2 VS BASELINE ==="
Write-Host ("P3B_BASELINE_MEDIAN_DUT_MBPS={0:F6}" -f $baselineMedianDutMbps)
Write-Host ("P3B_BATCH2_MEDIAN_DUT_MBPS={0:F6}" -f $medianDutMbps)
Write-Host ("P3B_THROUGHPUT_DELTA_PCT={0:F2}" -f $throughputGainPct)
Write-Host "P3B_BASELINE_ACTIVE_HOLD_AVG_US=$baselineMedianActiveHoldAvgUs"
Write-Host "P3B_BATCH2_ACTIVE_HOLD_AVG_US=$medianActiveHoldAvg"
Write-Host ("P3B_ACTIVE_HOLD_AVG_DELTA_PCT={0:F2}" -f $holdAvgGainPct)
Write-Host "P3B_BASELINE_ACTIVE_HOLD_MAX_US=$baselineMedianActiveHoldMaxUs"
Write-Host "P3B_BATCH2_ACTIVE_HOLD_MAX_US=$medianActiveHoldMax"
Write-Host ("P3B_ACTIVE_HOLD_MAX_DELTA_PCT={0:F2}" -f $holdMaxGainPct)

$p3bDecision = "REJECT_NO_GAIN"
if ($medianActiveHoldMax -gt 10000) {
    $p3bDecision = "REJECT_HOLD_GT_10MS"
}
elseif ($medianActiveHoldMax -gt 5000) {
    $p3bDecision = "REVIEW_HOLD_GT_5MS"
}
elseif ($throughputGainPct -ge 3.0) {
    $p3bDecision = "KEEP_FOR_NEXT_AB"
}

Write-Host "P3B_DECISION=$p3bDecision"

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
Write-Host "A14_P3B_SINGLE_VARIABLE=UDP_RX_MAX_PACKETS_PER_HOLD_1_TO_2"
Write-Host "A14_P3B_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "A14_P3B_DIAGNOSTIC_COPY_ONLY=YES"
Write-Host "A14_P3B_UDP_RX_BATCH2_AB=PASS"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT_FOR_BATCH2_DECISION"
