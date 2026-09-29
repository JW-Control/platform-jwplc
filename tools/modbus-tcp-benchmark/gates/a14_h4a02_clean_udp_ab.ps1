param(
    [string]$SerialPort = "COM14",
    [double]$DurationSeconds = 15.0,
    [int]$Runs = 3,
    [int]$UdpPayload = 1016
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-NativeToLog {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$LogPath
    )

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
    param(
        [string]$Text,
        [string]$Key
    )

    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))

    if ($matches.Count -ne 1) {
        throw ("H4A02_LOG_KEY_COUNT_{0}={1}" -f $Key, $matches.Count)
    }

    return $matches[0].Groups[1].Value.Trim()
}

function Get-LogDouble {
    param(
        [string]$Text,
        [string]$Key
    )

    return [double]::Parse(
        (Get-LogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Get-Median {
    param([double[]]$Values)

    $sorted = @($Values | Sort-Object)

    if ($sorted.Count -eq 0) {
        throw "H4A02_MEDIAN_EMPTY"
    }

    $middle = [int][math]::Floor($sorted.Count / 2)

    if (($sorted.Count % 2) -eq 1) {
        return [double]$sorted[$middle]
    }

    return [double](($sorted[$middle - 1] + $sorted[$middle]) / 2.0)
}

function Assert-ContainsExactly {
    param(
        [string]$Text,
        [string]$Needle,
        [int]$Expected,
        [string]$Label
    )

    $count = [regex]::Matches(
        $Text,
        [regex]::Escape($Needle)
    ).Count

    Write-Host "$Label=$count"

    if ($count -ne $Expected) {
        throw ($Label + "_INVALID")
    }
}

function Assert-ContainsAtLeast {
    param(
        [string]$Text,
        [string]$Needle,
        [int]$Minimum,
        [string]$Label
    )

    $count = [regex]::Matches(
        $Text,
        [regex]::Escape($Needle)
    ).Count

    Write-Host "$Label=$count"

    if ($count -lt $Minimum) {
        throw ($Label + "_INVALID")
    }
}

Write-Host "============================================================"
Write-Host " A14 H4A0.2 - CLEAN UDP RX A/B"
Write-Host " LEGACY CURRENT vs FAST CANDIDATE"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationSeconds -lt 10.0) {
    throw "H4A02_DURATION_MUST_BE_AT_LEAST_10S"
}

if ($Runs -lt 3) {
    throw "H4A02_RUNS_MUST_BE_AT_LEAST_3"
}

if ($UdpPayload -ne 1016) {
    throw "H4A02_PAYLOAD_MUST_BE_1016_FOR_BATCH2_BOUNDARY"
}

$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
$untracked = @(& git -C $script:G2RepoRoot ls-files --others --exclude-standard)

if ($LASTEXITCODE -ne 0) {
    throw "H4A02_GIT_QUERY_FAILED"
}

$productUntracked = @(
    $untracked |
        ForEach-Object { $_.Replace("\", "/") } |
        Where-Object {
            $_.StartsWith("JWPLC/2.1.0/") -or
            $_.StartsWith("tools/modbus-tcp-benchmark/firmware/")
        }
)

Write-Host "HEAD=$head"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "UNTRACKED_COUNT=$($untracked.Count)"
Write-Host "UNTRACKED_PRODUCT_COUNT=$($productUntracked.Count)"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host ("DURATION_S={0:F1}" -f $DurationSeconds)
Write-Host "RUNS=$Runs"
Write-Host "UDP_PAYLOAD=$UdpPayload"
Write-Host "SOCKET_TOPOLOGY=8x2KB"
Write-Host "TEST_DESIGN=SEPARATE_BUILD_SEPARATE_UPLOAD_PER_RUN"
Write-Host "TCP_INTERLEAVING=NO"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"

if ($spiHz -ne 26000000) {
    throw "H4A02_EXPECTED_26MHZ"
}

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "H4A02_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "H4A02_INDEX_NOT_CLEAN"
}

if ($productUntracked.Count -ne 0) {
    $productUntracked | ForEach-Object { Write-Host "UNTRACKED_PRODUCT=$_" }
    throw "H4A02_UNTRACKED_PRODUCT_SOURCE_FOUND"
}

Assert-G2ProtectedArtifacts

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "H4A02_PYTHON_NOT_FOUND"
}
$pythonExe = $pythonCommand.Source

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "H4A02_ARDUINO_CLI_NOT_FOUND"
}

$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$repoFirmwareDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server"
$caseRunner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_h4a02_udp_clean_case.py"
$rawRunner = Get-G2Path "tools/modbus-tcp-benchmark/pc/eth14_raw_transport_benchmark.py"

$instrumentPatch = Join-Path $PSScriptRoot "a14_p3_udp_rx_instrument_patch.py"
$batch2Patch = Join-Path $PSScriptRoot "a14_p3b_udp_rx_batch2_patch.py"
$intPatch = Join-Path $PSScriptRoot "a14_p3g_udp_rx_int_guided_patch.py"
$fastPatch = Join-Path $PSScriptRoot "a14_p3h_udp_rx_fused_fast_path_patch.py"
$commitPatch = Join-Path $PSScriptRoot "a14_p3j_udp_rx_coalesced_commit_patch.py"
$r1Patch = Join-Path $PSScriptRoot "a14_p3j_r1_postcommit_rearm_patch.py"

$required = @(
    $repoFirmwareDir,
    $caseRunner,
    $rawRunner,
    $instrumentPatch,
    $batch2Patch,
    $intPatch,
    $fastPatch,
    $commitPatch,
    $r1Patch
)

foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "H4A02_REQUIRED_PATH_MISSING=$path"
    }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h4a02_{0}" -f $timestamp)
$legacyBuild = Join-Path $tempRoot "legacy_build"
$fastBuild = Join-Path $tempRoot "fast_build"
$fastWork = Join-Path $tempRoot "fast_work"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
New-Item -ItemType Directory -Force -Path $legacyBuild | Out-Null
New-Item -ItemType Directory -Force -Path $fastBuild | Out-Null

Write-Host "H4A02_TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== PYTHON SYNTAX PRECHECK ==="

$syntaxLog = Join-Path $tempRoot "python_syntax.log"
$syntaxArgs = @(
    "-m", "py_compile",
    $caseRunner,
    $rawRunner,
    $instrumentPatch,
    $batch2Patch,
    $intPatch,
    $fastPatch,
    $commitPatch,
    $r1Patch
)

$syntaxExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $syntaxArgs -LogPath $syntaxLog
Write-Host "H4A02_PYTHON_SYNTAX_EXIT=$syntaxExit"

if ($syntaxExit -ne 0) {
    Get-Content -LiteralPath $syntaxLog -Tail 200 | ForEach-Object { Write-Host $_ }
    throw "H4A02_PYTHON_SYNTAX_FAILED"
}

Write-Host "H4A02_PYTHON_SYNTAX=PASS"

Write-Host ""
Write-Host "=== PREPARE FAST CANDIDATE FROM CURRENT HEAD ==="

$instrumentLog = Join-Path $tempRoot "instrument_patch.log"
$instrumentExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @(
    $instrumentPatch,
    "--repo-root", $script:G2RepoRoot,
    "--work-root", $fastWork
) -LogPath $instrumentLog

Write-Host "H4A02_INSTRUMENT_PATCH_EXIT=$instrumentExit"
if ($instrumentExit -ne 0) {
    Get-Content -LiteralPath $instrumentLog -Tail 200 | ForEach-Object { Write-Host $_ }
    throw "H4A02_INSTRUMENT_PATCH_FAILED"
}

$fastSketch = Join-Path $fastWork "sketch\eth14_raw_transport_server\eth14_raw_transport_server.ino"
$fastEthernetRoot = Join-Path $fastWork "libraries\JWPLC_Ethernet"
$fastLibrariesRoot = Join-Path $fastWork "libraries"

$patches = @(
    @{ Name = "BATCH2"; Script = $batch2Patch; Args = @("--instrumented-sketch", $fastSketch) },
    @{ Name = "INT"; Script = $intPatch; Args = @("--ethernet-root", $fastEthernetRoot, "--instrumented-sketch", $fastSketch) },
    @{ Name = "FUSED"; Script = $fastPatch; Args = @("--ethernet-root", $fastEthernetRoot, "--instrumented-sketch", $fastSketch) },
    @{ Name = "COMMIT2"; Script = $commitPatch; Args = @("--ethernet-root", $fastEthernetRoot, "--instrumented-sketch", $fastSketch) },
    @{ Name = "R1"; Script = $r1Patch; Args = @("--instrumented-sketch", $fastSketch) }
)

foreach ($patch in $patches) {
    $logPath = Join-Path $tempRoot (($patch.Name.ToLower()) + "_patch.log")
    $patchExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @($patch.Script) + $patch.Args -LogPath $logPath
    Write-Host ("H4A02_{0}_PATCH_EXIT={1}" -f $patch.Name, $patchExit)

    if ($patchExit -ne 0) {
        Get-Content -LiteralPath $logPath -Tail 220 | ForEach-Object { Write-Host $_ }
        throw ("H4A02_{0}_PATCH_FAILED" -f $patch.Name)
    }

    Get-Content -LiteralPath $logPath | ForEach-Object { Write-Host $_ }
}

Write-Host ""
Write-Host "=== FAST SOURCE CONTRACT ==="

$fastSketchText = [System.IO.File]::ReadAllText($fastSketch)
$fastHeader = Join-Path $fastEthernetRoot "src\JWPLC_W5x00_Ethernet.h"
$fastHeaderText = [System.IO.File]::ReadAllText($fastHeader)
$fastW5100H = Join-Path $fastEthernetRoot "src\utility\w5100.h"
$fastW5100HText = [System.IO.File]::ReadAllText($fastW5100H)
$fastSocketCpp = Join-Path $fastEthernetRoot "src\socket.cpp"
$fastSocketText = [System.IO.File]::ReadAllText($fastSocketCpp)
$fastUdpCpp = Join-Path $fastEthernetRoot "src\EthernetUdp.cpp"
$fastUdpText = [System.IO.File]::ReadAllText($fastUdpCpp)

Assert-ContainsExactly -Text $fastSketchText -Needle "static constexpr uint8_t UDP_RX_MAX_PACKETS_PER_HOLD = 2;" -Expected 1 -Label "H4A02_VERIFY_BATCH2_COUNT"
Assert-ContainsExactly -Text $fastSketchText -Needle "static constexpr uint8_t ETH_INT_PIN = 15;" -Expected 1 -Label "H4A02_VERIFY_INT_PIN_COUNT"
Assert-ContainsAtLeast -Text $fastSketchText -Needle "jwplcDiagReadPacketFastDeferred(" -Minimum 1 -Label "H4A02_VERIFY_DEFERRED_READ_CALL_COUNT"
Assert-ContainsExactly -Text $fastSketchText -Needle "udpSocket.jwplcDiagCommitRxFast()" -Expected 1 -Label "H4A02_VERIFY_COMMIT_CALL_COUNT"
Assert-ContainsAtLeast -Text $fastSketchText -Needle "postCommitRsr" -Minimum 1 -Label "H4A02_VERIFY_POSTCOMMIT_RSR_COUNT"
Assert-ContainsExactly -Text $fastSketchText -Needle "P3J-R1: do not clear RECV before draining" -Expected 1 -Label "H4A02_VERIFY_R1_CLEAR_ORDER_COUNT"
Assert-ContainsAtLeast -Text $fastHeaderText -Needle "socketRecvUDPFastDeferred" -Minimum 1 -Label "H4A02_VERIFY_FAST_HEADER_DECL_COUNT"
Assert-ContainsAtLeast -Text $fastSocketText -Needle "EthernetClass::socketRecvUDPFastDeferred" -Minimum 1 -Label "H4A02_VERIFY_FAST_SOCKET_IMPL_COUNT"
Assert-ContainsAtLeast -Text $fastUdpText -Needle "EthernetUDP::jwplcDiagReadPacketFastDeferred" -Minimum 1 -Label "H4A02_VERIFY_FAST_UDP_IMPL_COUNT"
Assert-ContainsAtLeast -Text $fastW5100HText -Needle "SIR_W5500" -Minimum 1 -Label "H4A02_VERIFY_SIR_COUNT"
Assert-ContainsAtLeast -Text $fastW5100HText -Needle "SIMR_W5500" -Minimum 1 -Label "H4A02_VERIFY_SIMR_COUNT"
Assert-ContainsAtLeast -Text $fastW5100HText -Needle "SnIMR" -Minimum 1 -Label "H4A02_VERIFY_SNIMR_COUNT"

Write-Host "H4A02_FAST_SOURCE_CONTRACT=PASS"

$fqbn = "jwplc_local:esp32:jwplcbasic"

Write-Host ""
Write-Host "=== COMPILE LEGACY CURRENT PRODUCT ==="

$legacyCompileLog = Join-Path $tempRoot "legacy_compile.log"
$legacyCompileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $legacyBuild,
    "--libraries", $repoLibrariesRoot,
    $repoFirmwareDir
) -LogPath $legacyCompileLog

Write-Host "H4A02_LEGACY_COMPILE_EXIT=$legacyCompileExit"
if ($legacyCompileExit -ne 0) {
    Get-Content -LiteralPath $legacyCompileLog -Tail 240 | ForEach-Object { Write-Host $_ }
    throw "H4A02_LEGACY_COMPILE_FAILED"
}

Write-Host ""
Write-Host "=== COMPILE FAST DIAGNOSTIC COPY ==="

$fastSketchDir = Split-Path -Parent $fastSketch
$fastCompileLog = Join-Path $tempRoot "fast_compile.log"
$fastCompileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $fastBuild,
    "--libraries", $fastLibrariesRoot,
    "--libraries", $repoLibrariesRoot,
    $fastSketchDir
) -LogPath $fastCompileLog

Write-Host "H4A02_FAST_COMPILE_EXIT=$fastCompileExit"
if ($fastCompileExit -ne 0) {
    Get-Content -LiteralPath $fastCompileLog -Tail 240 | ForEach-Object { Write-Host $_ }
    throw "H4A02_FAST_COMPILE_FAILED"
}

$fastCompileText = [System.IO.File]::ReadAllText($fastCompileLog)
$diagEthernetUsed = $fastCompileText.IndexOf(
    $fastEthernetRoot,
    [System.StringComparison]::OrdinalIgnoreCase
) -ge 0

Write-Host "H4A02_DIAGNOSTIC_ETHERNET_LIBRARY_USED=$diagEthernetUsed"
if (-not $diagEthernetUsed) {
    throw "H4A02_FAST_DIAGNOSTIC_LIBRARY_NOT_SELECTED"
}

$legacyValues = New-Object System.Collections.Generic.List[double]
$fastValues = New-Object System.Collections.Generic.List[double]
$legacyLoopMax = New-Object System.Collections.Generic.List[double]
$fastLoopMax = New-Object System.Collections.Generic.List[double]

function Invoke-H4A02Variant {
    param(
        [string]$Variant,
        [string]$BuildPath,
        [string]$SketchDir,
        [System.Collections.Generic.List[double]]$Values,
        [System.Collections.Generic.List[double]]$LoopMaxValues
    )

    for ($run = 1; $run -le $Runs; ++$run) {
        Write-Host ""
        Write-Host "============================================================"
        Write-Host (" H4A0.2 {0} RUN {1}/{2}" -f $Variant, $run, $Runs)
        Write-Host "============================================================"

        $uploadLog = Join-Path $tempRoot ("{0}_run{1}_upload.log" -f $Variant.ToLower(), $run)
        $uploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments @(
            "upload",
            "--fqbn", $fqbn,
            "--port", $SerialPort,
            "--input-dir", $BuildPath,
            $SketchDir
        ) -LogPath $uploadLog

        Write-Host ("H4A02_{0}_RUN{1}_UPLOAD_EXIT={2}" -f $Variant, $run, $uploadExit)

        if ($uploadExit -ne 0) {
            Get-Content -LiteralPath $uploadLog -Tail 220 | ForEach-Object { Write-Host $_ }
            throw ("H4A02_{0}_RUN{1}_UPLOAD_FAILED" -f $Variant, $run)
        }

        Start-Sleep -Seconds 3

        $caseLog = Join-Path $tempRoot ("{0}_run{1}_udp_rx.log" -f $Variant.ToLower(), $run)
        $durationText = $DurationSeconds.ToString(
            [System.Globalization.CultureInfo]::InvariantCulture
        )

        $caseExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @(
            "-u", $caseRunner,
            "--serial", $SerialPort,
            "--duration", $durationText,
            "--udp-payload", $UdpPayload.ToString(),
            "--variant", $Variant
        ) -LogPath $caseLog

        Write-Host ("H4A02_{0}_RUN{1}_CASE_EXIT={2}" -f $Variant, $run, $caseExit)
        Write-Host ("H4A02_{0}_RUN{1}_LOG={2}" -f $Variant, $run, $caseLog)

        Get-Content -LiteralPath $caseLog | ForEach-Object { Write-Host $_ }

        if ($caseExit -ne 0) {
            throw ("H4A02_{0}_RUN{1}_CASE_FAILED" -f $Variant, $run)
        }

        $caseText = [System.IO.File]::ReadAllText($caseLog)

        if ((Get-LogValue -Text $caseText -Key "H4A02_FUNCTIONAL_PASS") -ne "YES") {
            throw ("H4A02_{0}_RUN{1}_FUNCTIONAL_FAIL" -f $Variant, $run)
        }

        $mbps = Get-LogDouble -Text $caseText -Key "H4A02_DUT_MBPS"
        $Values.Add($mbps)

        $loopPattern = "(?m)^H4A02_SNAPSHOT_LOOP_GAP_MAX_US=(.*)\r?$"
        $loopMatch = [regex]::Match($caseText, $loopPattern)
        if ($loopMatch.Success) {
            $loopValue = [double]::Parse(
                $loopMatch.Groups[1].Value.Trim(),
                [System.Globalization.CultureInfo]::InvariantCulture
            )
            $LoopMaxValues.Add($loopValue)
        }

        if ($Variant -eq "FAST") {
            foreach ($key in @(
                "H4A02_SNAPSHOT_ETH_INT_CONFIGURED",
                "H4A02_SNAPSHOT_UDP_RX_PACKETS",
                "H4A02_SNAPSHOT_UDP_RX_SERVICE_HOLD_COUNT",
                "H4A02_SNAPSHOT_UDP_RX_ACTIVE_HOLD_COUNT",
                "H4A02_SNAPSHOT_UDP_RX_EMPTY_HOLD_COUNT",
                "H4A02_SNAPSHOT_UDP_RX_SPI_READS_PER_PACKET_X1000"
            )) {
                if (-not [regex]::IsMatch($caseText, "(?m)^" + [regex]::Escape($key) + "=")) {
                    throw ("H4A02_FAST_RUNTIME_MARKER_MISSING={0}" -f $key)
                }
            }

            if ((Get-LogValue -Text $caseText -Key "H4A02_SNAPSHOT_ETH_INT_CONFIGURED") -ne "YES") {
                throw "H4A02_FAST_INT_NOT_CONFIGURED"
            }

            $packets = Get-LogDouble -Text $caseText -Key "H4A02_SNAPSHOT_UDP_RX_PACKETS"
            $holds = Get-LogDouble -Text $caseText -Key "H4A02_SNAPSHOT_UDP_RX_ACTIVE_HOLD_COUNT"
            $emptyHolds = Get-LogDouble -Text $caseText -Key "H4A02_SNAPSHOT_UDP_RX_EMPTY_HOLD_COUNT"

            if ($holds -le 0) {
                throw "H4A02_FAST_ACTIVE_HOLDS_ZERO"
            }

            $packetsPerHold = $packets / $holds

            Write-Host ("H4A02_FAST_RUN{0}_PACKETS_PER_ACTIVE_HOLD={1:F6}" -f $run, $packetsPerHold)

            if ($packetsPerHold -lt 1.95 -or $packetsPerHold -gt 2.05) {
                throw "H4A02_FAST_BATCH2_RUNTIME_NOT_OBSERVED"
            }

            if ($emptyHolds -ne 0) {
                throw "H4A02_FAST_EMPTY_HOLDS_NONZERO"
            }
        }
    }
}

# Legacy first, then fast. Every individual run starts from a fresh upload.
Invoke-H4A02Variant -Variant "LEGACY" -BuildPath $legacyBuild -SketchDir $repoFirmwareDir -Values $legacyValues -LoopMaxValues $legacyLoopMax
Invoke-H4A02Variant -Variant "FAST" -BuildPath $fastBuild -SketchDir $fastSketchDir -Values $fastValues -LoopMaxValues $fastLoopMax

$legacyMedian = Get-Median -Values $legacyValues.ToArray()
$fastMedian = Get-Median -Values $fastValues.ToArray()

$legacyMin = ($legacyValues | Measure-Object -Minimum).Minimum
$legacyMax = ($legacyValues | Measure-Object -Maximum).Maximum
$fastMin = ($fastValues | Measure-Object -Minimum).Minimum
$fastMax = ($fastValues | Measure-Object -Maximum).Maximum

$gainPct = (($fastMedian / $legacyMedian) - 1.0) * 100.0

Write-Host ""
Write-Host "============================================================"
Write-Host " H4A0.2 CLEAN A/B SUMMARY"
Write-Host "============================================================"
Write-Host ("H4A02_LEGACY_MEDIAN_MBPS={0:F6}" -f $legacyMedian)
Write-Host ("H4A02_LEGACY_MIN_MBPS={0:F6}" -f $legacyMin)
Write-Host ("H4A02_LEGACY_MAX_MBPS={0:F6}" -f $legacyMax)
Write-Host ("H4A02_FAST_MEDIAN_MBPS={0:F6}" -f $fastMedian)
Write-Host ("H4A02_FAST_MIN_MBPS={0:F6}" -f $fastMin)
Write-Host ("H4A02_FAST_MAX_MBPS={0:F6}" -f $fastMax)
Write-Host ("H4A02_FAST_VS_LEGACY_GAIN_PCT={0:F2}" -f $gainPct)

if ($legacyLoopMax.Count -gt 0) {
    Write-Host ("H4A02_LEGACY_LOOP_GAP_MAX_US_MAX={0:F0}" -f (($legacyLoopMax | Measure-Object -Maximum).Maximum))
}
if ($fastLoopMax.Count -gt 0) {
    Write-Host ("H4A02_FAST_LOOP_GAP_MAX_US_MAX={0:F0}" -f (($fastLoopMax | Measure-Object -Maximum).Maximum))
}

$fastGainMeaningful = $gainPct -ge 10.0
Write-Host "H4A02_FAST_GAIN_GE_10PCT=$fastGainMeaningful"

Write-Host ""
Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"

$tftAnswer = Read-Host "¿TFT COM14 estable y operativo durante H4A0.2? (S/N)"
$tftPhysical = $tftAnswer.Trim().ToUpper() -eq "S"
Write-Host "H4A02_TFT_PHYSICAL_PASS=$tftPhysical"

if (-not $tftPhysical) {
    throw "H4A02_TFT_PHYSICAL_REVIEW"
}

$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "H4A02_FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
Write-Host "H4A02_FINAL_STAGED_COUNT=$($finalStaged.Count)"

if ($finalDirty.Count -ne 0) {
    throw "H4A02_REPOSITORY_MUTATED"
}

if ($finalStaged.Count -ne 0) {
    throw "H4A02_INDEX_MUTATED"
}

Assert-G2ProtectedArtifacts

Write-Host "H4A02_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "A14_H4A02_CLEAN_UDP_AB=PASS"

if ($fastGainMeaningful) {
    Write-Host "H4A02_UDP_FAST_PATH_VALUE=CONFIRMED"
    Write-Host "NEXT=DESIGN_PRODUCTIZABLE_UDP_FAST_PATH_WITHOUT_DIAGNOSTIC_COUNTERS"
}
else {
    Write-Host "H4A02_UDP_FAST_PATH_VALUE=REVIEW"
    Write-Host "NEXT=ISOLATE_BATCH2_INT_FUSED_COMMIT2_R1_COMPONENTS_ON_CURRENT_RUNTIME"
}
