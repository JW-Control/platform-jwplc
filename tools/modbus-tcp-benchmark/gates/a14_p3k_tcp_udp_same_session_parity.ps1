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

$payloads = @(1016)
$variants = @("INT_COMMIT2_R1")

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
Write-Host " A14 P3K - SAME-SESSION TCP RX VS UDP RX PARITY @ 26 MHz"
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
Write-Host "COMPARISON=TCP_RX_VS_UDP_RX_SAME_FIRMWARE_SAME_SESSION"
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
$commitPatchPath = Join-Path $PSScriptRoot "a14_p3j_udp_rx_coalesced_commit_patch.py"
$r1PatchPath = Join-Path $PSScriptRoot "a14_p3j_r1_postcommit_rearm_patch.py"
$serialIdlePatchPath = Join-Path $PSScriptRoot "a14_p3j_r2_serial_idle_patch.py"
$bridgePath = Join-Path $PSScriptRoot "a14_p3k_transport_parity_bridge.py"
$resolverPath = Join-Path $PSScriptRoot "a14_nb3_resolve_dut_ip.py"

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p3k_transport_parity_{0}" -f $timestamp)

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
    $commitPatchPath,
    $r1PatchPath,
    $serialIdlePatchPath,
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

$preflightCommitLog = Join-Path $preflightRoot "commit_patch.log"
$preflightCommitArgs = @(
    $commitPatchPath,
    "--ethernet-root", $preflightEthernetRoot,
    "--instrumented-sketch", $preflightSketchPath
)

$preflightCommitExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $preflightCommitArgs -LogPath $preflightCommitLog
Write-Host "PREFLIGHT_COMMIT_PATCH_EXIT=$preflightCommitExit"

if ($preflightCommitExit -ne 0) {
    Get-Content -LiteralPath $preflightCommitLog -Tail 200 | ForEach-Object { Write-Host $_ }
    throw "P3J_PREFLIGHT_COMMIT_PATCH_FAILED"
}

Get-Content -LiteralPath $preflightCommitLog | ForEach-Object { Write-Host $_ }

$preflightR1Log = Join-Path $preflightRoot "r1_patch.log"
$preflightR1Args = @(
    $r1PatchPath,
    "--instrumented-sketch", $preflightSketchPath
)

$preflightR1Exit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $preflightR1Args -LogPath $preflightR1Log
Write-Host "PREFLIGHT_R1_PATCH_EXIT=$preflightR1Exit"

if ($preflightR1Exit -ne 0) {
    Get-Content -LiteralPath $preflightR1Log -Tail 200 | ForEach-Object { Write-Host $_ }
    throw "P3J_R1_PREFLIGHT_PATCH_FAILED"
}

Get-Content -LiteralPath $preflightR1Log | ForEach-Object { Write-Host $_ }

$preflightIdleLog = Join-Path $preflightRoot "serial_idle_patch.log"
$preflightIdleArgs = @(
    $serialIdlePatchPath,
    "--instrumented-sketch", $preflightSketchPath
)

$preflightIdleExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $preflightIdleArgs -LogPath $preflightIdleLog
Write-Host "PREFLIGHT_SERIAL_IDLE_PATCH_EXIT=$preflightIdleExit"

if ($preflightIdleExit -ne 0) {
    Get-Content -LiteralPath $preflightIdleLog -Tail 200 | ForEach-Object { Write-Host $_ }
    throw "P3J_R2_PREFLIGHT_SERIAL_IDLE_PATCH_FAILED"
}

Get-Content -LiteralPath $preflightIdleLog | ForEach-Object { Write-Host $_ }
Write-Host "P3J_R2_PATCH_COMPOSITION_PREFLIGHT=PASS"

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
        throw "P3I_FAST_PATCH_FAILED_$variant"
    }

    Get-Content -LiteralPath $fastLog | ForEach-Object { Write-Host $_ }

    if ($variant -eq "INT_COMMIT2_R1") {
        $commitLog = Join-Path $variantRoot "commit_patch.log"
        $commitArgs = @(
            $commitPatchPath,
            "--ethernet-root", $diagEthernetRoot,
            "--instrumented-sketch", $diagSketchPath
        )

        $commitExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $commitArgs -LogPath $commitLog
        Write-Host "COMMIT_PATCH_EXIT=$commitExit"

        if ($commitExit -ne 0) {
            Get-Content -LiteralPath $commitLog -Tail 180 | ForEach-Object { Write-Host $_ }
            throw "P3J_COMMIT_PATCH_FAILED"
        }

        Get-Content -LiteralPath $commitLog | ForEach-Object { Write-Host $_ }

        $r1Log = Join-Path $variantRoot "r1_patch.log"
        $r1Args = @(
            $r1PatchPath,
            "--instrumented-sketch", $diagSketchPath
        )

        $r1Exit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $r1Args -LogPath $r1Log
        Write-Host "R1_PATCH_EXIT=$r1Exit"

        if ($r1Exit -ne 0) {
            Get-Content -LiteralPath $r1Log -Tail 180 | ForEach-Object { Write-Host $_ }
            throw "P3J_R1_PATCH_FAILED"
        }

        Get-Content -LiteralPath $r1Log | ForEach-Object { Write-Host $_ }
    }
    else {
        Write-Host "COMMIT_PATCH=NOT_APPLIED"
        Write-Host "R1_PATCH=NOT_APPLIED"
    }

    $serialIdleLog = Join-Path $variantRoot "serial_idle_patch.log"
    $serialIdleArgs = @(
        $serialIdlePatchPath,
        "--instrumented-sketch", $diagSketchPath
    )

    $serialIdleExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $serialIdleArgs -LogPath $serialIdleLog
    Write-Host "SERIAL_IDLE_PATCH_EXIT=$serialIdleExit"

    if ($serialIdleExit -ne 0) {
        Get-Content -LiteralPath $serialIdleLog -Tail 180 | ForEach-Object { Write-Host $_ }
        throw "P3J_R2_SERIAL_IDLE_PATCH_FAILED_$variant"
    }

    Get-Content -LiteralPath $serialIdleLog | ForEach-Object { Write-Host $_ }

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

    $udpBlock1 = New-Object System.Collections.Generic.List[double]
    $tcpBlock1 = New-Object System.Collections.Generic.List[double]
    $udpBlock2 = New-Object System.Collections.Generic.List[double]
    $tcpBlock2 = New-Object System.Collections.Generic.List[double]

    $sequences = @{
        1 = @("udp-rx", "tcp-rx", "udp-rx", "tcp-rx", "udp-rx", "tcp-rx")
        2 = @("tcp-rx", "udp-rx", "tcp-rx", "udp-rx", "tcp-rx", "udp-rx")
    }

    foreach ($block in 1, 2) {
        $udpRun = 0
        $tcpRun = 0

        Write-Host ""
        Write-Host "============================================================"
        Write-Host " P3K BLOCK=$block"
        Write-Host "============================================================"
        Write-Host "ORDER=$($sequences[$block] -join ',')"

        foreach ($mode in $sequences[$block]) {
            if ($mode -eq "udp-rx") {
                ++$udpRun
                $modeRun = $udpRun
                $summaryKey = "SUMMARY_UDP_RX_DUT_MBPS"
                $modeLabel = "UDP_RX"
            }
            else {
                ++$tcpRun
                $modeRun = $tcpRun
                $summaryKey = "SUMMARY_TCP_RX_DUT_MBPS"
                $modeLabel = "TCP_RX"
            }

            $runLog = Join-Path $variantRoot (
                "p3k_block_{0}_{1}_run_{2}.log" -f
                $block,
                $modeLabel,
                $modeRun
            )

            $runArgs = @(
                $bridgePath,
                "--host", $dutIp,
                "--serial", $SerialPort,
                "--duration", $durationText,
                "--mode", $mode,
                "--tcp-chunk", "4096",
                "--udp-payload", "1016"
            )

            $runExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runArgs -LogPath $runLog

            Write-Host (
                "P3K_RUN BLOCK={0} MODE={1} RUN={2} EXIT={3} LOG={4}" -f
                $block,
                $modeLabel,
                $modeRun,
                $runExit,
                $runLog
            )

            if ($runExit -ne 0) {
                Get-Content -LiteralPath $runLog -Tail 220 | ForEach-Object { Write-Host $_ }
                throw ("P3K_RUNNER_FAILED_B{0}_{1}_R{2}" -f $block, $modeLabel, $modeRun)
            }

            $runText = [System.IO.File]::ReadAllText($runLog)

            if ((Get-LogValue -Text $runText -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
                throw ("P3K_FUNCTIONAL_FAIL_B{0}_{1}_R{2}" -f $block, $modeLabel, $modeRun)
            }

            if ((Get-LogValue -Text $runText -Key "P3J_R2_QUIESCENCE_PASS") -ne "YES") {
                throw ("P3K_QUIESCENCE_FAIL_B{0}_{1}_R{2}" -f $block, $modeLabel, $modeRun)
            }

            if ((Get-LogValue -Text $runText -Key "P3J_R2_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
                throw ("P3K_SNAPSHOT_MISSING_B{0}_{1}_R{2}" -f $block, $modeLabel, $modeRun)
            }

            if ((Get-LogValue -Text $runText -Key "P3J_R2_FINAL_ETH_READY") -ne "YES") {
                throw ("P3K_ETH_NOT_READY_B{0}_{1}_R{2}" -f $block, $modeLabel, $modeRun)
            }

            if ((Get-LogValue -Text $runText -Key "P3J_R2_FINAL_ETH_LINK") -ne "UP") {
                throw ("P3K_LINK_DOWN_B{0}_{1}_R{2}" -f $block, $modeLabel, $modeRun)
            }

            $errors = Get-LogInt64 -Text $runText -Key "P3J_R2_FINAL_TRANSPORT_ERRORS"
            $spiErrors = Get-LogInt64 -Text $runText -Key "P3J_R2_FINAL_UDP_SPI_LOCK_ERRORS"

            if ($errors -ne 0) {
                throw ("P3K_TRANSPORT_ERRORS_B{0}_{1}_R{2}={3}" -f $block, $modeLabel, $modeRun, $errors)
            }

            if ($spiErrors -ne 0) {
                throw ("P3K_SPI_LOCK_ERRORS_B{0}_{1}_R{2}={3}" -f $block, $modeLabel, $modeRun, $spiErrors)
            }

            if ($mode -eq "udp-rx") {
                if ((Get-LogValue -Text $runText -Key "P3J_R2_ARM_RESET_PASS") -ne "YES") {
                    throw ("P3K_UDP_ARM_RESET_FAIL_B{0}_R{1}" -f $block, $modeRun)
                }
            }

            $dutMbps = Get-LogDouble -Text $runText -Key $summaryKey

            if ($dutMbps -lt 8.0) {
                throw ("P3K_THROUGHPUT_SANITY_FLOOR_B{0}_{1}_R{2}={3}" -f $block, $modeLabel, $modeRun, $dutMbps)
            }

            if ($block -eq 1) {
                if ($mode -eq "udp-rx") { $udpBlock1.Add($dutMbps) }
                else { $tcpBlock1.Add($dutMbps) }
            }
            else {
                if ($mode -eq "udp-rx") { $udpBlock2.Add($dutMbps) }
                else { $tcpBlock2.Add($dutMbps) }
            }

            Write-Host (
                "P3K_RESULT BLOCK={0} MODE={1} RUN={2} DUT_MBPS={3:F6}" -f
                $block,
                $modeLabel,
                $modeRun,
                $dutMbps
            )
        }
    }
}

$udp1 = [double](Get-Median -Values $udpBlock1.ToArray())
$tcp1 = [double](Get-Median -Values $tcpBlock1.ToArray())
$udp2 = [double](Get-Median -Values $udpBlock2.ToArray())
$tcp2 = [double](Get-Median -Values $tcpBlock2.ToArray())

$udpAll = @($udpBlock1.ToArray() + $udpBlock2.ToArray())
$tcpAll = @($tcpBlock1.ToArray() + $tcpBlock2.ToArray())

$udpMedian = [double](Get-Median -Values $udpAll)
$tcpMedian = [double](Get-Median -Values $tcpAll)

$gain1 = (($udp1 / $tcp1) - 1.0) * 100.0
$gain2 = (($udp2 / $tcp2) - 1.0) * 100.0
$gainAggregate = (($udpMedian / $tcpMedian) - 1.0) * 100.0
$gainSpread = [math]::Abs($gain2 - $gain1)

$udpDrift = (($udp2 / $udp1) - 1.0) * 100.0
$tcpDrift = (($tcp2 / $tcp1) - 1.0) * 100.0
$driftMismatch = [math]::Abs($udpDrift - $tcpDrift)

$interpretation = "UDP_TCP_PARITY_INCONCLUSIVE"

if (
    [math]::Abs($gain1) -le 2.0 -and
    [math]::Abs($gain2) -le 2.0 -and
    [math]::Abs($gainAggregate) -le 2.0 -and
    $gainSpread -le 1.0
) {
    $interpretation = "UDP_TCP_PARITY_CONFIRMED_WITHIN_2PCT"
}
elseif (
    $gain1 -gt 2.0 -and
    $gain2 -gt 2.0 -and
    $gainSpread -le 1.0
) {
    $interpretation = "UDP_REPEATABLE_ABOVE_TCP"
}
elseif (
    $gain1 -lt -2.0 -and
    $gain2 -lt -2.0 -and
    $gainSpread -le 1.0
) {
    $interpretation = "UDP_REPEATABLE_BELOW_TCP"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " P3K SAME-SESSION TRANSPORT PARITY SUMMARY"
Write-Host "============================================================"
Write-Host ("P3K_UDP_BLOCK1_MEDIAN_MBPS={0:F6}" -f $udp1)
Write-Host ("P3K_TCP_BLOCK1_MEDIAN_MBPS={0:F6}" -f $tcp1)
Write-Host ("P3K_BLOCK1_UDP_VS_TCP_PCT={0:F2}" -f $gain1)
Write-Host ("P3K_UDP_BLOCK2_MEDIAN_MBPS={0:F6}" -f $udp2)
Write-Host ("P3K_TCP_BLOCK2_MEDIAN_MBPS={0:F6}" -f $tcp2)
Write-Host ("P3K_BLOCK2_UDP_VS_TCP_PCT={0:F2}" -f $gain2)
Write-Host ("P3K_GAIN_SPREAD_PP={0:F2}" -f $gainSpread)
Write-Host ("P3K_UDP_MEDIAN_MBPS={0:F6}" -f $udpMedian)
Write-Host ("P3K_TCP_MEDIAN_MBPS={0:F6}" -f $tcpMedian)
Write-Host ("P3K_AGGREGATE_UDP_VS_TCP_PCT={0:F2}" -f $gainAggregate)
Write-Host ("P3K_UDP_ABSOLUTE_DRIFT_PCT={0:F2}" -f $udpDrift)
Write-Host ("P3K_TCP_ABSOLUTE_DRIFT_PCT={0:F2}" -f $tcpDrift)
Write-Host ("P3K_COMMON_MODE_DRIFT_MISMATCH_PP={0:F2}" -f $driftMismatch)
Write-Host "P3K_INTERPRETATION=$interpretation"
Write-Host "A14_P3K_TRANSPORT_PARITY_MEASUREMENT=PASS"
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

if ($finalHz -ne 26000000) { throw "P3K_FINAL_FREQ_CHANGED" }
if ($finalDirty.Count -ne 0) { throw "P3G_PRODUCT_TREE_DIRTY" }
if ($finalStaged.Count -ne 0) { throw "P3G_PRODUCT_INDEX_DIRTY" }
if ($finalRawHash -ne $expectedRawHash) { throw "P3K_FINAL_RAW_HASH_CHANGED" }
if ($finalW5100CppHash -ne $expectedW5100CppHash) { throw "P3K_FINAL_W5100_CPP_HASH_CHANGED" }
if ($finalW5100HHash -ne $expectedW5100HHash) { throw "P3K_FINAL_W5100_H_HASH_CHANGED" }

Write-Host ""
Write-Host "A14_P3K_SPI_HZ=26000000"
Write-Host "A14_P3K_SOCKET_TOPOLOGY=8x2KB_UNCHANGED"
Write-Host "A14_P3K_ALGORITHM=UDP_RX_BATCH2_INT_AND_FUSED_FROZEN"
Write-Host "A14_P3K_ETH_INT_PIN=GPIO15"
Write-Host "A14_P3K_ISR_SPI_ACCESS=NO"
Write-Host "A14_P3K_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "A14_P3K_DIAGNOSTIC_COPY_ONLY=YES"
Write-Host "A14_P3K_PER_RUN_SERIAL_RESET_GUARD=YES"
Write-Host "A14_P3K_SERIAL_IDLE_QUIESCENCE_BARRIER=YES"
Write-Host "A14_P3K_BLOCK_QUIESCENT_POSTCOMMIT_REARM_AB=PASS"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT_FOR_TCP_UDP_PARITY_DECISION"
