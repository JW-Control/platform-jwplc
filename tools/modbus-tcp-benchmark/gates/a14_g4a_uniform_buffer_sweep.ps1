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

$payloads = @(1472, 1016)
$topologies = @(
    [pscustomobject]@{ Label = "8x2KB"; Sockets = 8; BufferKB = 2 },
    [pscustomobject]@{ Label = "4x4KB"; Sockets = 4; BufferKB = 4 },
    [pscustomobject]@{ Label = "2x8KB"; Sockets = 2; BufferKB = 8 }
)

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
        throw ("G4A_LOG_KEY_COUNT_{0}={1}" -f $Key, $matches.Count)
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
        throw "G4A_MEDIAN_EMPTY"
    }

    $middle = [int][math]::Floor($sorted.Count / 2)
    if (($sorted.Count % 2) -eq 1) {
        return $sorted[$middle]
    }

    return ($sorted[$middle - 1] + $sorted[$middle]) / 2
}

Write-Host "============================================================"
Write-Host " A14 G4-A - UNIFORM W5500 BUFFER SWEEP @ 26 MHz / BATCH2"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationSeconds -le 0) { throw "G4A_DURATION_INVALID" }
if ($Runs -lt 1) { throw "G4A_RUNS_INVALID" }

$head = Get-G2Head
$effectiveHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($LASTEXITCODE -ne 0) { throw "G4A_CACHED_DIFF_FAILED" }

Write-Host "HEAD=$head"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "RUNS=$Runs"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "ALGORITHM=UDP_RX_BATCH2_FROZEN"
Write-Host "PAYLOADS=$($payloads -join ',')"
Write-Host "TOPOLOGIES=8x2KB,4x4KB,2x8KB"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"
Write-Host "DIAGNOSTIC_COPY_ONLY=YES"

if ($effectiveHz -ne 26000000) { throw "G4A_EXPECTED_26MHZ" }
if ($dirty.Count -ne 0) { throw "G4A_TRACKED_TREE_NOT_CLEAN" }
if ($staged.Count -ne 0) { throw "G4A_INDEX_NOT_CLEAN" }

Assert-G2ProtectedArtifacts

$rawHash = Get-G2Sha256 $script:G2RawFirmwareRelative
$w5100CppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
$w5100CppHash = Get-G2Sha256 $w5100CppRelative
$w5100HHash = Get-G2Sha256 $script:G2SpiHeaderRelative

Write-Host "RAW_FIRMWARE_SHA256=$rawHash"
Write-Host "W5100_CPP_SHA256=$w5100CppHash"
Write-Host "W5100_H_SHA256=$w5100HHash"

if ($rawHash -ne $expectedRawHash) { throw "G4A_RAW_HASH_MISMATCH" }
if ($w5100CppHash -ne $expectedW5100CppHash) { throw "G4A_W5100_CPP_HASH_MISMATCH" }
if ($w5100HHash -ne $expectedW5100HHash) { throw "G4A_W5100_H_HASH_MISMATCH" }

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) { throw "G4A_PYTHON_NOT_FOUND" }

$pythonExe = $pythonCommand.Source
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) { throw "G4A_ARDUINO_CLI_NOT_FOUND" }

$instrumentPatchPath = Join-Path $PSScriptRoot "a14_p3_udp_rx_instrument_patch.py"
$batch2PatchPath = Join-Path $PSScriptRoot "a14_p3b_udp_rx_batch2_patch.py"
$topologyPatchPath = Join-Path $PSScriptRoot "a14_g4a_uniform_socket_topology_patch.py"
$bridgePath = Join-Path $PSScriptRoot "a14_p3_udp_rx_instrument_bridge.py"
$resolverPath = Join-Path $PSScriptRoot "a14_nb3_resolve_dut_ip.py"

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_g4a_uniform_buffer_sweep_{0}" -f $timestamp)
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

Write-Host "PYTHON=$pythonExe"
Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== PYTHON SYNTAX PRECHECK ==="

$pyArgs = @(
    "-m", "py_compile",
    $instrumentPatchPath,
    $batch2PatchPath,
    $topologyPatchPath,
    $bridgePath,
    $resolverPath
)

$pyLog = Join-Path $tempRoot "py_compile.log"
$pyExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $pyArgs -LogPath $pyLog
Write-Host "PY_COMPILE_EXIT=$pyExit"

if ($pyExit -ne 0) {
    Get-Content -LiteralPath $pyLog -Tail 160 | ForEach-Object { Write-Host $_ }
    throw "G4A_PYTHON_SYNTAX_FAILED"
}

$results = @{}
$durationText = $DurationSeconds.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$fqbn = "jwplc_local:esp32:jwplcbasic"

foreach ($topology in $topologies) {
    $label = [string]$topology.Label
    $sockets = [int]$topology.Sockets
    $bufferKB = [int]$topology.BufferKB

    Write-Host ""
    Write-Host "============================================================"
    Write-Host " TOPOLOGY=$label SOCKETS=$sockets BUFFER_KB=$bufferKB"
    Write-Host "============================================================"

    $topologyRoot = Join-Path $tempRoot $label
    $workRoot = Join-Path $topologyRoot "instrumented"
    $buildPath = Join-Path $topologyRoot "build"
    New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

    $instrumentLog = Join-Path $topologyRoot "instrument_patch.log"
    $batch2Log = Join-Path $topologyRoot "batch2_patch.log"
    $topologyLog = Join-Path $topologyRoot "topology_patch.log"
    $compileLog = Join-Path $topologyRoot "compile.log"
    $uploadLog = Join-Path $topologyRoot "upload.log"
    $ipLog = Join-Path $topologyRoot "dut_ip.log"

    Write-Host ""
    Write-Host "=== BUILD ISOLATED DIAGNOSTIC COPY: $label ==="

    $instrumentArgs = @(
        $instrumentPatchPath,
        "--repo-root", $script:G2RepoRoot,
        "--work-root", $workRoot
    )
    $instrumentExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $instrumentArgs -LogPath $instrumentLog
    Write-Host "INSTRUMENT_PATCH_EXIT=$instrumentExit"

    if ($instrumentExit -ne 0) {
        Get-Content -LiteralPath $instrumentLog -Tail 160 | ForEach-Object { Write-Host $_ }
        throw "G4A_INSTRUMENT_PATCH_FAILED_$label"
    }

    $diagLibrariesRoot = Join-Path $workRoot "libraries"
    $diagEthernetRoot = Join-Path $diagLibrariesRoot "JWPLC_Ethernet"
    $diagSketchDir = Join-Path $workRoot "sketch\eth14_raw_transport_server"
    $diagSketchPath = Join-Path $diagSketchDir "eth14_raw_transport_server.ino"

    $batch2Args = @(
        $batch2PatchPath,
        "--instrumented-sketch", $diagSketchPath
    )
    $batch2Exit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $batch2Args -LogPath $batch2Log
    Write-Host "BATCH2_PATCH_EXIT=$batch2Exit"

    if ($batch2Exit -ne 0) {
        Get-Content -LiteralPath $batch2Log -Tail 120 | ForEach-Object { Write-Host $_ }
        throw "G4A_BATCH2_PATCH_FAILED_$label"
    }

    $topologyArgs = @(
        $topologyPatchPath,
        "--ethernet-root", $diagEthernetRoot,
        "--sockets", ([string]$sockets)
    )
    $topologyExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $topologyArgs -LogPath $topologyLog
    Write-Host "TOPOLOGY_PATCH_EXIT=$topologyExit"

    if ($topologyExit -ne 0) {
        Get-Content -LiteralPath $topologyLog -Tail 120 | ForEach-Object { Write-Host $_ }
        throw "G4A_TOPOLOGY_PATCH_FAILED_$label"
    }

    Get-Content -LiteralPath $topologyLog | ForEach-Object { Write-Host $_ }

    Write-Host ""
    Write-Host "=== COMPILE: $label ==="

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
        Get-Content -LiteralPath $compileLog -Tail 180 | ForEach-Object { Write-Host $_ }
        throw "G4A_COMPILE_FAILED_$label"
    }

    $compileText = [System.IO.File]::ReadAllText($compileLog)
    $diagEthernetUsed = $compileText.IndexOf(
        $diagEthernetRoot,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0

    Write-Host "DIAGNOSTIC_ETHERNET_LIBRARY_USED=$diagEthernetUsed"

    if (-not $diagEthernetUsed) {
        throw "G4A_DIAGNOSTIC_LIBRARY_NOT_SELECTED_$label"
    }

    $binCount = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin").Count
    Write-Host "BIN_COUNT=$binCount"
    if ($binCount -lt 1) { throw "G4A_BIN_MISSING_$label" }

    Invoke-G2CompileFinishedSound -Success $true

    Write-Host ""
    Write-Host "=== UPLOAD: $label ==="

    $uploadArgs = @(
        "upload",
        "--fqbn", $fqbn,
        "--port", $SerialPort,
        "--input-dir", $buildPath,
        $diagSketchDir
    )

    $uploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog
    Write-Host "UPLOAD_EXIT=$uploadExit"

    if ($uploadExit -ne 0) {
        Get-Content -LiteralPath $uploadLog -Tail 180 | ForEach-Object { Write-Host $_ }
        throw "G4A_UPLOAD_FAILED_$label"
    }

    Start-Sleep -Seconds 2

    Write-Host ""
    Write-Host "=== RESOLVE DUT IP: $label ==="

    $ipArgs = @(
        $resolverPath,
        "--serial", $SerialPort,
        "--timeout", "15"
    )

    $ipExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $ipArgs -LogPath $ipLog
    Write-Host "IP_RESOLVER_EXIT=$ipExit"

    if ($ipExit -ne 0) {
        Get-Content -LiteralPath $ipLog -Tail 160 | ForEach-Object { Write-Host $_ }
        throw "G4A_IP_RESOLVER_FAILED_$label"
    }

    $ipText = [System.IO.File]::ReadAllText($ipLog)
    $dutIp = Get-LogValue -Text $ipText -Key "DUT_IP_EFFECTIVE"
    Write-Host "DUT_IP_EFFECTIVE=$dutIp"

    foreach ($payload in $payloads) {
        Write-Host ""
        Write-Host ("--- TOPOLOGY={0} PAYLOAD={1} ---" -f $label, $payload)

        $dutMbpsValues = New-Object System.Collections.Generic.List[double]
        $packetsPerHoldValues = New-Object System.Collections.Generic.List[int64]
        $emptyRatioValues = New-Object System.Collections.Generic.List[double]
        $activeHoldAvgValues = New-Object System.Collections.Generic.List[int64]
        $activeHoldMaxValues = New-Object System.Collections.Generic.List[int64]
        $parseAvgValues = New-Object System.Collections.Generic.List[int64]
        $readAvgValues = New-Object System.Collections.Generic.List[int64]
        $spiReadsPerPacketValues = New-Object System.Collections.Generic.List[int64]

        for ($run = 1; $run -le $Runs; ++$run) {
            $runLog = Join-Path $topologyRoot ("udp_rx_{0}_payload_{1}_run_{2}.log" -f $label, $payload, $run)
            $runArgs = @(
                $bridgePath,
                "--host", $dutIp,
                "--serial", $SerialPort,
                "--duration", $durationText,
                "--mode", "udp-rx",
                "--udp-payload", ([string]$payload)
            )

            $runExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runArgs -LogPath $runLog
            Write-Host "G4A_RUN TOPOLOGY=$label PAYLOAD=$payload RUN=$run RUNNER_EXIT=$runExit LOG=$runLog"

            if ($runExit -ne 0) {
                Get-Content -LiteralPath $runLog -Tail 180 | ForEach-Object { Write-Host $_ }
                throw "G4A_RUNNER_FAILED_$label" + "_P$payload" + "_R$run"
            }

            $text = [System.IO.File]::ReadAllText($runLog)

            if ((Get-LogValue -Text $text -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
                throw "G4A_FUNCTIONAL_FAIL_$label" + "_P$payload" + "_R$run"
            }
            if ((Get-LogValue -Text $text -Key "P3_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
                throw "G4A_SNAPSHOT_MISSING_$label" + "_P$payload" + "_R$run"
            }
            if ((Get-LogValue -Text $text -Key "P3_FINAL_ETH_READY") -ne "YES") {
                throw "G4A_ETH_NOT_READY_$label" + "_P$payload" + "_R$run"
            }
            if ((Get-LogValue -Text $text -Key "P3_FINAL_ETH_LINK") -ne "UP") {
                throw "G4A_LINK_DOWN_$label" + "_P$payload" + "_R$run"
            }
            if ((Get-LogValue -Text $text -Key "P3_FINAL_IP") -ne $dutIp) {
                throw "G4A_IP_MISMATCH_$label" + "_P$payload" + "_R$run"
            }

            $transportErrors = Get-LogInt64 -Text $text -Key "P3_FINAL_TRANSPORT_ERRORS"
            $udpSpiLockErrors = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_SPI_LOCK_ERRORS"
            $packets = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_PACKETS"
            $rxOperations = Get-LogInt64 -Text $text -Key "P3_FINAL_RX_OPERATIONS"
            $packetParseCalls = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_PACKET_PARSE_CALLS"
            $activeHolds = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_ACTIVE_HOLD_COUNT"
            $serviceHolds = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_SERVICE_HOLD_COUNT"
            $emptyHolds = Get-LogInt64 -Text $text -Key "P3_FINAL_UDP_RX_EMPTY_HOLD_COUNT"

            if ($transportErrors -ne 0) { throw "G4A_TRANSPORT_ERRORS_$label" + "_P$payload" + "_R$run=$transportErrors" }
            if ($udpSpiLockErrors -ne 0) { throw "G4A_SPI_LOCK_ERRORS_$label" + "_P$payload" + "_R$run=$udpSpiLockErrors" }
            if ($packets -le 0) { throw "G4A_NO_PACKETS_$label" + "_P$payload" + "_R$run" }
            if ($packets -ne $rxOperations) { throw "G4A_PACKET_OPERATION_MISMATCH_$label" + "_P$payload" + "_R$run" }
            if ($packetParseCalls -ne $packets) { throw "G4A_PARSE_COUNT_MISMATCH_$label" + "_P$payload" + "_R$run" }
            if ($activeHolds -le 0) { throw "G4A_ACTIVE_HOLD_EMPTY_$label" + "_P$payload" + "_R$run" }
            if ($activeHolds -gt $packets) { throw "G4A_ACTIVE_HOLDS_GT_PACKETS_$label" + "_P$payload" + "_R$run" }
            if ($packets -gt (2 * $activeHolds)) { throw "G4A_PACKETS_GT_BATCH2_CAPACITY_$label" + "_P$payload" + "_R$run" }
            if ($serviceHolds -le 0) { throw "G4A_SERVICE_HOLD_EMPTY_$label" + "_P$payload" + "_R$run" }
            if ($emptyHolds -lt 0 -or $emptyHolds -gt $serviceHolds) { throw "G4A_EMPTY_HOLD_INVALID_$label" + "_P$payload" + "_R$run" }

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
                "G4A_RESULT TOPOLOGY={0} PAYLOAD={1} RUN={2} DUT_MBPS={3:F6} PACKETS={4} PACKETS_PER_ACTIVE_HOLD_X1000={5} EMPTY_HOLD_RATIO_PCT={6:F2} PARSE_AVG_US={7} READ_AVG_US={8} SPI_READS_PER_PACKET_X1000={9} ACTIVE_HOLD_AVG_US={10} ACTIVE_HOLD_MAX_US={11}" -f
                $label,
                $payload,
                $run,
                $dutMbps,
                $packets,
                $packetsPerActiveHoldX1000,
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

        $key = "{0}|{1}" -f $label, $payload
        $results[$key] = [pscustomobject]@{
            Label = $label
            Sockets = $sockets
            BufferKB = $bufferKB
            Payload = $payload
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
            "G4A_MEDIAN TOPOLOGY={0} PAYLOAD={1} DUT_MBPS={2:F6} PACKETS_PER_ACTIVE_HOLD_X1000={3} EMPTY_HOLD_RATIO_PCT={4:F2} PARSE_AVG_US={5} READ_AVG_US={6} SPI_READS_PER_PACKET_X1000={7} ACTIVE_HOLD_AVG_US={8} ACTIVE_HOLD_MAX_US={9}" -f
            $label,
            $payload,
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
}

$r8_1472 = $results["8x2KB|1472"]
$r4_1472 = $results["4x4KB|1472"]
$r2_1472 = $results["2x8KB|1472"]
$r8_1016 = $results["8x2KB|1016"]
$r4_1016 = $results["4x4KB|1016"]
$r2_1016 = $results["2x8KB|1016"]

$gain4_1472 = (($r4_1472.DutMbps / $r8_1472.DutMbps) - 1.0) * 100.0
$gain2_1472 = (($r2_1472.DutMbps / $r8_1472.DutMbps) - 1.0) * 100.0
$gain4_1016 = (($r4_1016.DutMbps / $r8_1016.DutMbps) - 1.0) * 100.0
$gain2_1016 = (($r2_1016.DutMbps / $r8_1016.DutMbps) - 1.0) * 100.0

Write-Host ""
Write-Host "============================================================"
Write-Host " G4-A COMPARATIVE SUMMARY"
Write-Host "============================================================"

foreach ($payload in $payloads) {
    foreach ($topology in $topologies) {
        $key = "{0}|{1}" -f $topology.Label, $payload
        $r = $results[$key]
        Write-Host (
            "G4A_SUMMARY TOPOLOGY={0} SOCKETS={1} BUFFER_KB={2} PAYLOAD={3} DUT_MBPS={4:F6} PACKETS_PER_ACTIVE_HOLD_X1000={5} EMPTY_HOLD_RATIO_PCT={6:F2} ACTIVE_HOLD_AVG_US={7} ACTIVE_HOLD_MAX_US={8}" -f
            $r.Label,
            $r.Sockets,
            $r.BufferKB,
            $r.Payload,
            $r.DutMbps,
            $r.PacketsPerActiveHoldX1000,
            $r.EmptyHoldRatioPct,
            $r.ActiveHoldAvgUs,
            $r.ActiveHoldMaxUs
        )
    }
}

Write-Host ""
Write-Host ("G4A_4x4_1472_DELTA_VS_8x2_PCT={0:F2}" -f $gain4_1472)
Write-Host ("G4A_2x8_1472_DELTA_VS_8x2_PCT={0:F2}" -f $gain2_1472)
Write-Host ("G4A_4x4_1016_DELTA_VS_8x2_PCT={0:F2}" -f $gain4_1016)
Write-Host ("G4A_2x8_1016_DELTA_VS_8x2_PCT={0:F2}" -f $gain2_1016)

$interpretation = "NO_UNIFORM_BUFFER_GAIN"
if ($gain4_1472 -ge 5.0 -or $gain4_1016 -ge 5.0) {
    $interpretation = "4x4_UNIFORM_BUFFER_GAIN_WORTH_CONCURRENCY_REVIEW"
}
if ($gain2_1472 -ge 5.0 -or $gain2_1016 -ge 5.0) {
    if ($interpretation -eq "4x4_UNIFORM_BUFFER_GAIN_WORTH_CONCURRENCY_REVIEW") {
        $interpretation = "BUFFER_SIZE_TREND_PRESENT_REVIEW_4x4_AND_2x8"
    }
    else {
        $interpretation = "2x8_GAIN_ONLY_CONCURRENCY_COST_HIGH"
    }
}

Write-Host "G4A_INTERPRETATION=$interpretation"

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

if ($finalHz -ne 26000000) { throw "G4A_FINAL_FREQ_CHANGED" }
if ($finalDirty.Count -ne 0) { throw "G4A_PRODUCT_TREE_DIRTY" }
if ($finalStaged.Count -ne 0) { throw "G4A_PRODUCT_INDEX_DIRTY" }
if ($finalRawHash -ne $expectedRawHash) { throw "G4A_FINAL_RAW_HASH_CHANGED" }
if ($finalW5100CppHash -ne $expectedW5100CppHash) { throw "G4A_FINAL_W5100_CPP_HASH_CHANGED" }
if ($finalW5100HHash -ne $expectedW5100HHash) { throw "G4A_FINAL_W5100_H_HASH_CHANGED" }

Write-Host ""
Write-Host "A14_G4A_SPI_HZ=26000000"
Write-Host "A14_G4A_ALGORITHM=UDP_RX_BATCH2_FROZEN"
Write-Host "A14_G4A_PAYLOADS=1472,1016"
Write-Host "A14_G4A_TOPOLOGIES=8x2KB,4x4KB,2x8KB"
Write-Host "A14_G4A_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "A14_G4A_DIAGNOSTIC_COPY_ONLY=YES"
Write-Host "A14_G4A_UNIFORM_BUFFER_SWEEP=PASS"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT_FOR_BUFFER_TOPOLOGY_DECISION"
