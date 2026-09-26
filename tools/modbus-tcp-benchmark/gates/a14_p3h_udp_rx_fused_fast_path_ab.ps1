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
$variants = @("INT_FUSED", "INT_LEGACY")

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
        throw ("P3G_LOG_KEY_COUNT_{0}={1}" -f $Key, $matches.Count)
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
        throw "P3G_MEDIAN_EMPTY"
    }

    $middle = [int][math]::Floor($sorted.Count / 2)

    if (($sorted.Count % 2) -eq 1) {
        return $sorted[$middle]
    }

    return ($sorted[$middle - 1] + $sorted[$middle]) / 2
}

Write-Host "============================================================"
Write-Host " A14 P3H - UDP RX INT LEGACY VS FUSED FAST-PATH @ 8x2KB / 26 MHz"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationSeconds -le 0) { throw "P3G_DURATION_INVALID" }
if ($Runs -lt 1) { throw "P3G_RUNS_INVALID" }

$head = Get-G2Head
$effectiveHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($LASTEXITCODE -ne 0) { throw "P3G_CACHED_DIFF_FAILED" }

Write-Host "HEAD=$head"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "RUNS=$Runs"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "SOCKET_TOPOLOGY=8x2KB_UNCHANGED"
Write-Host "ALGORITHM=UDP_RX_BATCH2_FROZEN"
Write-Host "PAYLOADS=$($payloads -join ',')"
Write-Host "VARIANTS=$($variants -join ',')"
Write-Host "SINGLE_VARIABLE=UDP_RX_FUSED_RECEIVE_PATH"
Write-Host "ETH_INT_PIN=GPIO15"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"
Write-Host "DIAGNOSTIC_COPY_ONLY=YES"

if ($effectiveHz -ne 26000000) { throw "P3G_EXPECTED_26MHZ" }
if ($dirty.Count -ne 0) { throw "P3G_TRACKED_TREE_NOT_CLEAN" }
if ($staged.Count -ne 0) { throw "P3G_INDEX_NOT_CLEAN" }

Assert-G2ProtectedArtifacts

$rawHash = Get-G2Sha256 $script:G2RawFirmwareRelative
$w5100CppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
$w5100CppHash = Get-G2Sha256 $w5100CppRelative
$w5100HHash = Get-G2Sha256 $script:G2SpiHeaderRelative

Write-Host "RAW_FIRMWARE_SHA256=$rawHash"
Write-Host "W5100_CPP_SHA256=$w5100CppHash"
Write-Host "W5100_H_SHA256=$w5100HHash"

if ($rawHash -ne $expectedRawHash) { throw "P3G_RAW_HASH_MISMATCH" }
if ($w5100CppHash -ne $expectedW5100CppHash) { throw "P3G_W5100_CPP_HASH_MISMATCH" }
if ($w5100HHash -ne $expectedW5100HHash) { throw "P3G_W5100_H_HASH_MISMATCH" }

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) { throw "P3G_PYTHON_NOT_FOUND" }

$pythonExe = $pythonCommand.Source
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"

if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "P3G_ARDUINO_CLI_NOT_FOUND"
}

$instrumentPatchPath = Join-Path $PSScriptRoot "a14_p3_udp_rx_instrument_patch.py"
$batch2PatchPath = Join-Path $PSScriptRoot "a14_p3b_udp_rx_batch2_patch.py"
$intPatchPath = Join-Path $PSScriptRoot "a14_p3g_udp_rx_int_guided_patch.py"
$fastPatchPath = Join-Path $PSScriptRoot "a14_p3h_udp_rx_fused_fast_path_patch.py"
$bridgePath = Join-Path $PSScriptRoot "a14_p3h_udp_rx_reset_bridge.py"
$resolverPath = Join-Path $PSScriptRoot "a14_nb3_resolve_dut_ip.py"

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p3h_udp_rx_fused_ab_{0}" -f $timestamp)

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
    $intPatchPath,
    $fastPatchPath,
    $bridgePath,
    $resolverPath
)

$pyLog = Join-Path $tempRoot "py_compile.log"
$pyExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $pyArgs -LogPath $pyLog

Write-Host "PY_COMPILE_EXIT=$pyExit"

if ($pyExit -ne 0) {
    Get-Content -LiteralPath $pyLog -Tail 160 | ForEach-Object { Write-Host $_ }
    throw "P3G_PYTHON_SYNTAX_FAILED"
}

$durationText = $DurationSeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$fqbn = "jwplc_local:esp32:jwplcbasic"
$results = @{}

Write-Host ""
Write-Host "=== PATCH COMPOSITION PREFLIGHT ==="

$preflightRoot = Join-Path $tempRoot "PATCH_PREFLIGHT"
$preflightWorkRoot = Join-Path $preflightRoot "instrumented"
$preflightInstrumentLog = Join-Path $preflightRoot "instrument_patch.log"
$preflightBatch2Log = Join-Path $preflightRoot "batch2_patch.log"
$preflightIntLog = Join-Path $preflightRoot "int_patch.log"

New-Item -ItemType Directory -Force -Path $preflightRoot | Out-Null

$preflightInstrumentArgs = @(
    $instrumentPatchPath,
    "--repo-root", $script:G2RepoRoot,
    "--work-root", $preflightWorkRoot
)

$preflightInstrumentExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $preflightInstrumentArgs -LogPath $preflightInstrumentLog
Write-Host "PREFLIGHT_INSTRUMENT_PATCH_EXIT=$preflightInstrumentExit"

if ($preflightInstrumentExit -ne 0) {
    Get-Content -LiteralPath $preflightInstrumentLog -Tail 160 | ForEach-Object { Write-Host $_ }
    throw "P3G_PREFLIGHT_INSTRUMENT_PATCH_FAILED"
}

$preflightEthernetRoot = Join-Path $preflightWorkRoot "libraries\JWPLC_Ethernet"
$preflightSketchPath = Join-Path $preflightWorkRoot "sketch\eth14_raw_transport_server\eth14_raw_transport_server.ino"

$preflightBatch2Args = @(
    $batch2PatchPath,
    "--instrumented-sketch", $preflightSketchPath
)

$preflightBatch2Exit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $preflightBatch2Args -LogPath $preflightBatch2Log
Write-Host "PREFLIGHT_BATCH2_PATCH_EXIT=$preflightBatch2Exit"

if ($preflightBatch2Exit -ne 0) {
    Get-Content -LiteralPath $preflightBatch2Log -Tail 160 | ForEach-Object { Write-Host $_ }
    throw "P3G_PREFLIGHT_BATCH2_PATCH_FAILED"
}

$preflightIntArgs = @(
    $intPatchPath,
    "--ethernet-root", $preflightEthernetRoot,
    "--instrumented-sketch", $preflightSketchPath
)

$preflightIntExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $preflightIntArgs -LogPath $preflightIntLog
Write-Host "PREFLIGHT_INT_PATCH_EXIT=$preflightIntExit"

if ($preflightIntExit -ne 0) {
    Get-Content -LiteralPath $preflightIntLog -Tail 200 | ForEach-Object { Write-Host $_ }
    throw "P3G_PREFLIGHT_INT_PATCH_FAILED"
}

Get-Content -LiteralPath $preflightIntLog | ForEach-Object { Write-Host $_ }

$preflightFastLog = Join-Path $preflightRoot "fast_patch.log"
$preflightFastArgs = @(
    $fastPatchPath,
    "--ethernet-root", $preflightEthernetRoot,
    "--instrumented-sketch", $preflightSketchPath
)

$preflightFastExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $preflightFastArgs -LogPath $preflightFastLog
Write-Host "PREFLIGHT_FAST_PATCH_EXIT=$preflightFastExit"

if ($preflightFastExit -ne 0) {
    Get-Content -LiteralPath $preflightFastLog -Tail 200 | ForEach-Object { Write-Host $_ }
    throw "P3H_PREFLIGHT_FAST_PATCH_FAILED"
}

Get-Content -LiteralPath $preflightFastLog | ForEach-Object { Write-Host $_ }
Write-Host "P3H_PATCH_COMPOSITION_PREFLIGHT=PASS"

foreach ($variant in $variants) {
    Write-Host ""
    Write-Host "============================================================"
    Write-Host " VARIANT=$variant"
    Write-Host "============================================================"

    $variantRoot = Join-Path $tempRoot $variant
    $workRoot = Join-Path $variantRoot "instrumented"
    $buildPath = Join-Path $variantRoot "build"

    New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

    $instrumentLog = Join-Path $variantRoot "instrument_patch.log"
    $batch2Log = Join-Path $variantRoot "batch2_patch.log"
    $intLog = Join-Path $variantRoot "int_patch.log"
    $compileLog = Join-Path $variantRoot "compile.log"
    $uploadLog = Join-Path $variantRoot "upload.log"
    $ipLog = Join-Path $variantRoot "dut_ip.log"

    $instrumentArgs = @(
        $instrumentPatchPath,
        "--repo-root", $script:G2RepoRoot,
        "--work-root", $workRoot
    )

    $instrumentExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $instrumentArgs -LogPath $instrumentLog
    Write-Host "INSTRUMENT_PATCH_EXIT=$instrumentExit"

    if ($instrumentExit -ne 0) {
        Get-Content -LiteralPath $instrumentLog -Tail 160 | ForEach-Object { Write-Host $_ }
        throw "P3G_INSTRUMENT_PATCH_FAILED_$variant"
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
        throw "P3G_BATCH2_PATCH_FAILED_$variant"
    }

    $intArgs = @(
        $intPatchPath,
        "--ethernet-root", $diagEthernetRoot,
        "--instrumented-sketch", $diagSketchPath
    )

    $intExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $intArgs -LogPath $intLog
    Write-Host "INT_PATCH_EXIT=$intExit"

    if ($intExit -ne 0) {
        Get-Content -LiteralPath $intLog -Tail 160 | ForEach-Object { Write-Host $_ }
        throw "P3H_INT_PATCH_FAILED_$variant"
    }

    Get-Content -LiteralPath $intLog | ForEach-Object { Write-Host $_ }

    if ($variant -eq "INT_FUSED") {
        $fastLog = Join-Path $variantRoot "fast_patch.log"
        $fastArgs = @(
            $fastPatchPath,
            "--ethernet-root", $diagEthernetRoot,
            "--instrumented-sketch", $diagSketchPath
        )

        $fastExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $fastArgs -LogPath $fastLog
        Write-Host "FAST_PATCH_EXIT=$fastExit"

        if ($fastExit -ne 0) {
            Get-Content -LiteralPath $fastLog -Tail 180 | ForEach-Object { Write-Host $_ }
            throw "P3H_FAST_PATCH_FAILED"
        }

        Get-Content -LiteralPath $fastLog | ForEach-Object { Write-Host $_ }
    }
    else {
        Write-Host "FAST_PATCH=NOT_APPLIED"
    }

    Write-Host ""
    Write-Host "=== COMPILE $variant ==="

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
        throw "P3H_COMPILE_FAILED_$variant"
    }

    $compileText = [System.IO.File]::ReadAllText($compileLog)

    $diagEthernetUsed = $compileText.IndexOf(
        $diagEthernetRoot,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0

    Write-Host "DIAGNOSTIC_ETHERNET_LIBRARY_USED=$diagEthernetUsed"

    if (-not $diagEthernetUsed) {
        throw "P3G_DIAGNOSTIC_LIBRARY_NOT_SELECTED_$variant"
    }

    $binCount = @(
        Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin"
    ).Count

    Write-Host "BIN_COUNT=$binCount"

    if ($binCount -lt 1) {
        throw "P3G_BIN_MISSING_$variant"
    }

    Invoke-G2CompileFinishedSound -Success $true

    Write-Host ""
    Write-Host "=== UPLOAD $variant ==="

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
        throw "P3H_UPLOAD_FAILED_$variant"
    }

    Start-Sleep -Seconds 2

    Write-Host ""
    Write-Host "=== RESOLVE DUT IP $variant ==="

    $ipArgs = @(
        $resolverPath,
        "--serial", $SerialPort,
        "--timeout", "15"
    )

    $ipExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $ipArgs -LogPath $ipLog
    Write-Host "IP_RESOLVER_EXIT=$ipExit"

    if ($ipExit -ne 0) {
        Get-Content -LiteralPath $ipLog -Tail 160 | ForEach-Object { Write-Host $_ }
        throw "P3G_IP_RESOLVER_FAILED_$variant"
    }

    $ipText = [System.IO.File]::ReadAllText($ipLog)
    $dutIp = Get-LogValue -Text $ipText -Key "DUT_IP_EFFECTIVE"

    Write-Host "DUT_IP_EFFECTIVE=$dutIp"

    foreach ($payload in $payloads) {
        $dutMbpsValues = New-Object System.Collections.Generic.List[double]
        $packetsPerHoldValues = New-Object System.Collections.Generic.List[int64]
        $emptyRatioValues = New-Object System.Collections.Generic.List[double]
        $serviceHoldValues = New-Object System.Collections.Generic.List[int64]
        $activeHoldAvgValues = New-Object System.Collections.Generic.List[int64]
        $activeHoldMaxValues = New-Object System.Collections.Generic.List[int64]
        $skipValues = New-Object System.Collections.Generic.List[int64]
        $wakeValues = New-Object System.Collections.Generic.List[int64]
        $spiReadsPerPacketValues = New-Object System.Collections.Generic.List[int64]
        $totalReadCallsValues = New-Object System.Collections.Generic.List[int64]

        Write-Host ""
        Write-Host "--- VARIANT=$variant PAYLOAD=$payload ---"

        for ($run = 1; $run -le $Runs; ++$run) {
            $runLog = Join-Path $variantRoot (
                "udp_rx_{0}_payload_{1}_run_{2}.log" -f
                $variant,
                $payload,
                $run
            )

            $runArgs = @(
                $bridgePath,
                "--host", $dutIp,
                "--serial", $SerialPort,
                "--duration", $durationText,
                "--mode", "udp-rx",
                "--udp-payload", ([string]$payload)
            )

            $runExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runArgs -LogPath $runLog
            Write-Host "P3H_RUN VARIANT=$variant PAYLOAD=$payload RUN=$run RUNNER_EXIT=$runExit LOG=$runLog"

            if ($runExit -ne 0) {
                Get-Content -LiteralPath $runLog -Tail 200 | ForEach-Object { Write-Host $_ }
                throw "P3G_RUNNER_FAILED_$variant" + "_P$payload" + "_R$run"
            }

            $text = [System.IO.File]::ReadAllText($runLog)

            if ((Get-LogValue -Text $text -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
                throw "P3G_FUNCTIONAL_FAIL_$variant" + "_P$payload" + "_R$run"
            }

            if ((Get-LogValue -Text $text -Key "P3H_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
                throw "P3G_SNAPSHOT_MISSING_$variant" + "_P$payload" + "_R$run"
            }

            if ((Get-LogValue -Text $text -Key "P3H_ARM_RESET_PASS") -ne "YES") {
                throw "P3H_ARM_RESET_GUARD_FAILED_$variant" + "_P$payload" + "_R$run"
            }

            if ((Get-LogInt64 -Text $text -Key "P3H_ARM_RESET_RX_BYTES") -ne 0) {
                throw "P3H_ARM_RESET_RX_BYTES_NONZERO_$variant" + "_P$payload" + "_R$run"
            }

            if ((Get-LogInt64 -Text $text -Key "P3H_ARM_RESET_RX_OPERATIONS") -ne 0) {
                throw "P3H_ARM_RESET_RX_OPERATIONS_NONZERO_$variant" + "_P$payload" + "_R$run"
            }

            if ((Get-LogValue -Text $text -Key "P3H_FINAL_ETH_READY") -ne "YES") {
                throw "P3G_ETH_NOT_READY_$variant" + "_P$payload" + "_R$run"
            }

            if ((Get-LogValue -Text $text -Key "P3H_FINAL_ETH_LINK") -ne "UP") {
                throw "P3G_LINK_DOWN_$variant" + "_P$payload" + "_R$run"
            }

            if ((Get-LogValue -Text $text -Key "P3H_FINAL_IP") -ne $dutIp) {
                throw "P3G_IP_MISMATCH_$variant" + "_P$payload" + "_R$run"
            }

            $transportErrors = Get-LogInt64 -Text $text -Key "P3H_FINAL_TRANSPORT_ERRORS"
            $udpSpiLockErrors = Get-LogInt64 -Text $text -Key "P3H_FINAL_UDP_SPI_LOCK_ERRORS"
            $packets = Get-LogInt64 -Text $text -Key "P3H_FINAL_UDP_RX_PACKETS"
            $rxOperations = Get-LogInt64 -Text $text -Key "P3H_FINAL_RX_OPERATIONS"
            $activeHolds = Get-LogInt64 -Text $text -Key "P3H_FINAL_UDP_RX_ACTIVE_HOLD_COUNT"
            $serviceHolds = Get-LogInt64 -Text $text -Key "P3H_FINAL_UDP_RX_SERVICE_HOLD_COUNT"
            $emptyHolds = Get-LogInt64 -Text $text -Key "P3H_FINAL_UDP_RX_EMPTY_HOLD_COUNT"

            if ($transportErrors -ne 0) {
                throw "P3G_TRANSPORT_ERRORS_$variant" + "_P$payload" + "_R$run=$transportErrors"
            }

            if ($udpSpiLockErrors -ne 0) {
                throw "P3G_SPI_LOCK_ERRORS_$variant" + "_P$payload" + "_R$run=$udpSpiLockErrors"
            }

            if ($packets -le 0) {
                throw "P3G_NO_PACKETS_$variant" + "_P$payload" + "_R$run"
            }

            if ($packets -ne $rxOperations) {
                throw "P3G_PACKET_OPERATION_MISMATCH_$variant" + "_P$payload" + "_R$run"
            }

            if ($activeHolds -le 0 -or $serviceHolds -le 0) {
                throw "P3G_HOLD_COUNT_INVALID_$variant" + "_P$payload" + "_R$run"
            }

            if ($activeHolds -gt $packets) {
                throw "P3G_ACTIVE_HOLDS_GT_PACKETS_$variant" + "_P$payload" + "_R$run"
            }

            if ($packets -gt (2 * $activeHolds)) {
                throw "P3G_PACKETS_GT_BATCH2_CAPACITY_$variant" + "_P$payload" + "_R$run"
            }

            if ($emptyHolds -lt 0 -or $emptyHolds -gt $serviceHolds) {
                throw "P3G_EMPTY_HOLD_INVALID_$variant" + "_P$payload" + "_R$run"
            }

            if ((Get-LogValue -Text $text -Key "P3H_FINAL_ETH_INT_CONFIGURED") -ne "YES") {
                throw "P3H_INT_NOT_CONFIGURED_$variant" + "_P$payload" + "_R$run"
            }

            $intPin = Get-LogInt64 -Text $text -Key "P3H_FINAL_ETH_INT_PIN"
            $intSocket = Get-LogInt64 -Text $text -Key "P3H_FINAL_ETH_INT_UDP_SOCKET"
            $skipCount = Get-LogInt64 -Text $text -Key "P3H_FINAL_UDP_RX_INT_SKIP_COUNT"
            $wakeCount = Get-LogInt64 -Text $text -Key "P3H_FINAL_UDP_RX_INT_WAKE_COUNT"

            if ($intPin -ne 15) {
                throw "P3H_INT_PIN_MISMATCH=$intPin"
            }

            if ($intSocket -ne 0) {
                throw "P3H_EXPECTED_UDP_SOCKET0_ACTUAL=$intSocket"
            }

            if ($skipCount -le 0 -or $wakeCount -le 0) {
                throw "P3H_INT_ACTIVITY_MISSING_$variant" + "_P$payload" + "_R$run"
            }

            $skipValues.Add($skipCount)
            $wakeValues.Add($wakeCount)

            $packetsPerActiveHoldX1000 =
                [int64][math]::Round(
                    (1000.0 * $packets) / $activeHolds,
                    0,
                    [System.MidpointRounding]::AwayFromZero
                )

            $emptyHoldRatioPct =
                (100.0 * $emptyHolds) / $serviceHolds

            $dutMbps = Get-LogDouble -Text $text -Key "SUMMARY_UDP_RX_DUT_MBPS"
            $activeHoldAvg = Get-LogInt64 -Text $text -Key "P3H_FINAL_UDP_RX_ACTIVE_HOLD_US_AVG"
            $activeHoldMax = Get-LogInt64 -Text $text -Key "P3H_FINAL_UDP_RX_ACTIVE_HOLD_US_MAX"
            $spiReadsPerPacketX1000 = Get-LogInt64 -Text $text -Key "P3H_FINAL_UDP_RX_SPI_READS_PER_PACKET_X1000"
            $totalReadCalls = Get-LogInt64 -Text $text -Key "P3H_FINAL_W5100_DIAG_READ_CALLS_TOTAL"

            $dutMbpsValues.Add($dutMbps)
            $packetsPerHoldValues.Add($packetsPerActiveHoldX1000)
            $emptyRatioValues.Add($emptyHoldRatioPct)
            $serviceHoldValues.Add($serviceHolds)
            $activeHoldAvgValues.Add($activeHoldAvg)
            $activeHoldMaxValues.Add($activeHoldMax)
            $spiReadsPerPacketValues.Add($spiReadsPerPacketX1000)
            $totalReadCallsValues.Add($totalReadCalls)

            Write-Host (
                "P3H_RESULT VARIANT={0} PAYLOAD={1} RUN={2} DUT_MBPS={3:F6} PACKETS={4} SERVICE_HOLDS={5} ACTIVE_HOLDS={6} EMPTY_HOLDS={7} EMPTY_HOLD_RATIO_PCT={8:F2} PACKETS_PER_ACTIVE_HOLD_X1000={9} ACTIVE_HOLD_AVG_US={10} ACTIVE_HOLD_MAX_US={11}" -f
                $variant,
                $payload,
                $run,
                $dutMbps,
                $packets,
                $serviceHolds,
                $activeHolds,
                $emptyHolds,
                $emptyHoldRatioPct,
                $packetsPerActiveHoldX1000,
                $activeHoldAvg,
                $activeHoldMax
            )
        }

        $medianDutMbps = [double](Get-Median -Values $dutMbpsValues.ToArray())
        $medianPacketsPerHold = [int64](Get-Median -Values $packetsPerHoldValues.ToArray())
        $medianEmptyRatio = [double](Get-Median -Values $emptyRatioValues.ToArray())
        $medianServiceHolds = [int64](Get-Median -Values $serviceHoldValues.ToArray())
        $medianActiveHoldAvg = [int64](Get-Median -Values $activeHoldAvgValues.ToArray())
        $medianActiveHoldMax = [int64](Get-Median -Values $activeHoldMaxValues.ToArray())

        $medianSkips = [int64](Get-Median -Values $skipValues.ToArray())
        $medianWakes = [int64](Get-Median -Values $wakeValues.ToArray())
        $medianSpiReadsPerPacket = [int64](Get-Median -Values $spiReadsPerPacketValues.ToArray())
        $medianTotalReadCalls = [int64](Get-Median -Values $totalReadCallsValues.ToArray())

        $key = "{0}|{1}" -f $variant, $payload

        $results[$key] = [pscustomobject]@{
            Variant = $variant
            Payload = $payload
            DutMbps = $medianDutMbps
            PacketsPerActiveHoldX1000 = $medianPacketsPerHold
            EmptyHoldRatioPct = $medianEmptyRatio
            ServiceHolds = $medianServiceHolds
            ActiveHoldAvgUs = $medianActiveHoldAvg
            ActiveHoldMaxUs = $medianActiveHoldMax
            IntSkips = $medianSkips
            IntWakes = $medianWakes
            SpiReadsPerPacketX1000 = $medianSpiReadsPerPacket
            TotalReadCalls = $medianTotalReadCalls
        }

        Write-Host (
            "P3H_MEDIAN VARIANT={0} PAYLOAD={1} DUT_MBPS={2:F6} SERVICE_HOLDS={3} EMPTY_HOLD_RATIO_PCT={4:F2} PACKETS_PER_ACTIVE_HOLD_X1000={5} ACTIVE_HOLD_AVG_US={6} ACTIVE_HOLD_MAX_US={7} INT_SKIPS={8} INT_WAKES={9} SPI_READS_PER_PACKET_X1000={10} TOTAL_W5500_READ_CALLS={11}" -f
            $variant,
            $payload,
            $medianDutMbps,
            $medianServiceHolds,
            $medianEmptyRatio,
            $medianPacketsPerHold,
            $medianActiveHoldAvg,
            $medianActiveHoldMax,
            $medianSkips,
            $medianWakes,
            $medianSpiReadsPerPacket,
            $medianTotalReadCalls
        )
    }
}

Write-Host ""
Write-Host "============================================================"
Write-Host " P3H COMPARATIVE SUMMARY"
Write-Host "============================================================"

foreach ($payload in $payloads) {
    $legacy = $results["INT_LEGACY|$payload"]
    $fused = $results["INT_FUSED|$payload"]

    $throughputDeltaPct =
        (($fused.DutMbps / $legacy.DutMbps) - 1.0) * 100.0

    $spiReadsReductionPct =
        (1.0 - (
            $fused.SpiReadsPerPacketX1000 /
            [double]$legacy.SpiReadsPerPacketX1000
        )) * 100.0

    $totalReadReductionPct =
        (1.0 - (
            $fused.TotalReadCalls /
            [double]$legacy.TotalReadCalls
        )) * 100.0

    Write-Host (
        "P3H_SUMMARY PAYLOAD={0} LEGACY_MBPS={1:F6} FUSED_MBPS={2:F6} THROUGHPUT_DELTA_PCT={3:F2} LEGACY_SPI_READS_PER_PACKET_X1000={4} FUSED_SPI_READS_PER_PACKET_X1000={5} SPI_READS_REDUCTION_PCT={6:F2} LEGACY_TOTAL_READ_CALLS={7} FUSED_TOTAL_READ_CALLS={8} TOTAL_READ_CALL_REDUCTION_PCT={9:F2} LEGACY_HOLD_AVG_US={10} FUSED_HOLD_AVG_US={11} LEGACY_HOLD_MAX_US={12} FUSED_HOLD_MAX_US={13}" -f
        $payload,
        $legacy.DutMbps,
        $fused.DutMbps,
        $throughputDeltaPct,
        $legacy.SpiReadsPerPacketX1000,
        $fused.SpiReadsPerPacketX1000,
        $spiReadsReductionPct,
        $legacy.TotalReadCalls,
        $fused.TotalReadCalls,
        $totalReadReductionPct,
        $legacy.ActiveHoldAvgUs,
        $fused.ActiveHoldAvgUs,
        $legacy.ActiveHoldMaxUs,
        $fused.ActiveHoldMaxUs
    )
}

$legacy1016 = $results["INT_LEGACY|1016"]
$fused1016 = $results["INT_FUSED|1016"]

$gain1016 =
    (($fused1016.DutMbps / $legacy1016.DutMbps) - 1.0) * 100.0

$readsReduction1016 =
    (1.0 - (
        $fused1016.SpiReadsPerPacketX1000 /
        [double]$legacy1016.SpiReadsPerPacketX1000
    )) * 100.0

$interpretation = "FUSED_NO_CLEAR_BENEFIT"

if (
    $gain1016 -ge 2.0 -and
    $readsReduction1016 -ge 10.0
) {
    $interpretation = "FUSED_THROUGHPUT_AND_SPI_READ_GAIN"
}
elseif (
    $fused1016.DutMbps -ge ($legacy1016.DutMbps * 0.99) -and
    $readsReduction1016 -ge 15.0
) {
    $interpretation = "FUSED_SPI_EFFICIENCY_GAIN_WITHOUT_MATERIAL_THROUGHPUT_LOSS"
}

Write-Host ""
Write-Host ("P3H_1016_THROUGHPUT_DELTA_PCT={0:F2}" -f $gain1016)
Write-Host ("P3H_1016_SPI_READ_REDUCTION_PCT={0:F2}" -f $readsReduction1016)
Write-Host "P3H_INTERPRETATION=$interpretation"

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

if ($finalHz -ne 26000000) { throw "P3H_FINAL_FREQ_CHANGED" }
if ($finalDirty.Count -ne 0) { throw "P3G_PRODUCT_TREE_DIRTY" }
if ($finalStaged.Count -ne 0) { throw "P3G_PRODUCT_INDEX_DIRTY" }
if ($finalRawHash -ne $expectedRawHash) { throw "P3H_FINAL_RAW_HASH_CHANGED" }
if ($finalW5100CppHash -ne $expectedW5100CppHash) { throw "P3H_FINAL_W5100_CPP_HASH_CHANGED" }
if ($finalW5100HHash -ne $expectedW5100HHash) { throw "P3H_FINAL_W5100_H_HASH_CHANGED" }

Write-Host ""
Write-Host "A14_P3H_SPI_HZ=26000000"
Write-Host "A14_P3H_SOCKET_TOPOLOGY=8x2KB_UNCHANGED"
Write-Host "A14_P3H_ALGORITHM=UDP_RX_BATCH2_AND_INT_FROZEN"
Write-Host "A14_P3H_ETH_INT_PIN=GPIO15"
Write-Host "A14_P3H_ISR_SPI_ACCESS=NO"
Write-Host "A14_P3H_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "A14_P3H_DIAGNOSTIC_COPY_ONLY=YES"
Write-Host "A14_P3H_PER_RUN_SERIAL_RESET_GUARD=YES"
Write-Host "A14_P3H_UDP_RX_FUSED_FAST_PATH_AB=PASS"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT_FOR_FUSED_RX_DECISION"
