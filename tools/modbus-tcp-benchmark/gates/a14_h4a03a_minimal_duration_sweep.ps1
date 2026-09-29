param(
    [string]$SerialPort = "COM14",
    [int]$RunsPerDuration = 3,
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
        throw ("H4A03A_LOG_KEY_COUNT_{0}={1}" -f $Key, $matches.Count)
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
        throw "H4A03A_MEDIAN_EMPTY"
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

    $count = [regex]::Matches($Text, [regex]::Escape($Needle)).Count
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

    $count = [regex]::Matches($Text, [regex]::Escape($Needle)).Count
    Write-Host "$Label=$count"
    if ($count -lt $Minimum) {
        throw ($Label + "_INVALID")
    }
}

function Test-PythonSyntaxNoBytecode {
    param(
        [string]$PythonExe,
        [string[]]$Paths,
        [string]$LogPath
    )

    $code = @"
import ast
import pathlib
import sys
for value in sys.argv[1:]:
    path = pathlib.Path(value)
    ast.parse(path.read_text(encoding='utf-8'), filename=str(path))
    print(f'PYTHON_AST_PASS={path}')
"@

    return Invoke-NativeToLog -FilePath $PythonExe -Arguments (@("-B", "-c", $code) + $Paths) -LogPath $LogPath
}

function Add-Result {
    param(
        [hashtable]$Buckets,
        [int]$Duration,
        [double]$Mbps
    )

    $Buckets[$Duration].Add($Mbps)
}

function Invoke-H4A03AGate {

Write-Host "============================================================"
Write-Host " A14 H4A0.3A - FAST MINIMAL-INSTRUMENTATION DURATION SWEEP"
Write-Host " 5s x3 / 15s x3 / 30s x3"
Write-Host "============================================================"

Assert-G2Branch

if ($RunsPerDuration -ne 3) {
    throw "H4A03A_RUNS_PER_DURATION_MUST_BE_3"
}
if ($UdpPayload -ne 1016) {
    throw "H4A03A_PAYLOAD_MUST_BE_1016_FOR_BATCH2_BOUNDARY"
}

$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
$untracked = @(& git -C $script:G2RepoRoot ls-files --others --exclude-standard)
if ($LASTEXITCODE -ne 0) {
    throw "H4A03A_GIT_QUERY_FAILED"
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
Write-Host "RUNS_PER_DURATION=$RunsPerDuration"
Write-Host "UDP_PAYLOAD=$UdpPayload"
Write-Host "SOCKET_TOPOLOGY=8x2KB"
Write-Host "FAST_COMPONENTS=BATCH2+INT+FUSED+COMMIT2+R1+R2"
Write-Host "PERFORMANCE_MEASUREMENT=MINIMAL"
Write-Host "LATENCY_PROFILING=SEPARATE_NOT_THIS_GATE"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"

if ($spiHz -ne 26000000) {
    throw "H4A03A_EXPECTED_26MHZ"
}
if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "H4A03A_TRACKED_TREE_NOT_CLEAN"
}
if ($staged.Count -ne 0) {
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "H4A03A_INDEX_NOT_CLEAN"
}
if ($productUntracked.Count -ne 0) {
    $productUntracked | ForEach-Object { Write-Host "UNTRACKED_PRODUCT=$_" }
    throw "H4A03A_UNTRACKED_PRODUCT_SOURCE_FOUND"
}

Assert-G2ProtectedArtifacts

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "H4A03A_PYTHON_NOT_FOUND"
}
$pythonExe = $pythonCommand.Source

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "H4A03A_ARDUINO_CLI_NOT_FOUND"
}

$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$repoFirmwareDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server"
$caseRunner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_h4a03a_udp_minimal_case.py"
$rawRunner = Get-G2Path "tools/modbus-tcp-benchmark/pc/eth14_raw_transport_benchmark.py"

$instrumentPatch = Join-Path $PSScriptRoot "a14_p3_udp_rx_instrument_patch.py"
$batch2Patch = Join-Path $PSScriptRoot "a14_p3b_udp_rx_batch2_patch.py"
$intPatch = Join-Path $PSScriptRoot "a14_p3g_udp_rx_int_guided_patch.py"
$fastPatch = Join-Path $PSScriptRoot "a14_p3h_udp_rx_fused_fast_path_patch.py"
$commitPatch = Join-Path $PSScriptRoot "a14_p3j_udp_rx_coalesced_commit_patch.py"
$r1Patch = Join-Path $PSScriptRoot "a14_p3j_r1_postcommit_rearm_patch.py"
$serialIdlePatch = Join-Path $PSScriptRoot "a14_p3j_r2_serial_idle_patch.py"
$minimalPatch = Join-Path $PSScriptRoot "a14_h4a03a_minimal_instrumentation_patch.py"

$required = @(
    $repoFirmwareDir,
    $caseRunner,
    $rawRunner,
    $instrumentPatch,
    $batch2Patch,
    $intPatch,
    $fastPatch,
    $commitPatch,
    $r1Patch,
    $serialIdlePatch,
    $minimalPatch
)
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "H4A03A_REQUIRED_PATH_MISSING=$path"
    }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h4a03a_{0}" -f $timestamp)
$fastBuild = Join-Path $tempRoot "fast_build"
$fastWork = Join-Path $tempRoot "fast_work"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
New-Item -ItemType Directory -Force -Path $fastBuild | Out-Null
Write-Host "H4A03A_TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== PYTHON SYNTAX / AST PRECHECK (NO BYTECODE) ==="
$syntaxPaths = @(
    $caseRunner,
    $rawRunner,
    $instrumentPatch,
    $batch2Patch,
    $intPatch,
    $fastPatch,
    $commitPatch,
    $r1Patch,
    $serialIdlePatch,
    $minimalPatch
)
$syntaxLog = Join-Path $tempRoot "python_ast_syntax.log"
$syntaxExit = Test-PythonSyntaxNoBytecode -PythonExe $pythonExe -Paths $syntaxPaths -LogPath $syntaxLog
Write-Host "H4A03A_PYTHON_AST_EXIT=$syntaxExit"
Get-Content -LiteralPath $syntaxLog | ForEach-Object { Write-Host $_ }
if ($syntaxExit -ne 0) {
    throw "H4A03A_PYTHON_AST_FAILED"
}
Write-Host "H4A03A_PYTHON_AST=PASS"

Write-Host ""
Write-Host "=== PATCH: BUILD PROVEN FAST CANDIDATE IN ISOLATED COPY ==="
$instrumentLog = Join-Path $tempRoot "01_instrument_patch.log"
$instrumentExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @(
    "-B", $instrumentPatch,
    "--repo-root", $script:G2RepoRoot,
    "--work-root", $fastWork
) -LogPath $instrumentLog
Write-Host "H4A03A_INSTRUMENT_PATCH_EXIT=$instrumentExit"
if ($instrumentExit -ne 0) {
    Get-Content -LiteralPath $instrumentLog -Tail 220 | ForEach-Object { Write-Host $_ }
    throw "H4A03A_INSTRUMENT_PATCH_FAILED"
}

$fastSketch = Join-Path $fastWork "sketch\eth14_raw_transport_server\eth14_raw_transport_server.ino"
$fastEthernetRoot = Join-Path $fastWork "libraries\JWPLC_Ethernet"
$fastLibrariesRoot = Join-Path $fastWork "libraries"

$patches = @(
    [PSCustomObject]@{ Name = "BATCH2"; Script = $batch2Patch; Args = @("--instrumented-sketch", $fastSketch) },
    [PSCustomObject]@{ Name = "INT"; Script = $intPatch; Args = @("--ethernet-root", $fastEthernetRoot, "--instrumented-sketch", $fastSketch) },
    [PSCustomObject]@{ Name = "FUSED"; Script = $fastPatch; Args = @("--ethernet-root", $fastEthernetRoot, "--instrumented-sketch", $fastSketch) },
    [PSCustomObject]@{ Name = "COMMIT2"; Script = $commitPatch; Args = @("--ethernet-root", $fastEthernetRoot, "--instrumented-sketch", $fastSketch) },
    [PSCustomObject]@{ Name = "R1"; Script = $r1Patch; Args = @("--instrumented-sketch", $fastSketch) },
    [PSCustomObject]@{ Name = "R2"; Script = $serialIdlePatch; Args = @("--instrumented-sketch", $fastSketch) },
    [PSCustomObject]@{ Name = "MINIMAL"; Script = $minimalPatch; Args = @("--ethernet-root", $fastEthernetRoot, "--instrumented-sketch", $fastSketch) }
)

foreach ($patch in $patches) {
    $logPath = Join-Path $tempRoot (("02_{0}_patch.log" -f $patch.Name.ToLower()))
    $patchArguments = @("-B", $patch.Script) + @($patch.Args)
    $patchExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $patchArguments -LogPath $logPath
    Write-Host ("H4A03A_{0}_PATCH_EXIT={1}" -f $patch.Name, $patchExit)
    Get-Content -LiteralPath $logPath | ForEach-Object { Write-Host $_ }
    if ($patchExit -ne 0) {
        throw ("H4A03A_{0}_PATCH_FAILED" -f $patch.Name)
    }
}

Write-Host ""
Write-Host "=== RE-READ + SOURCE CONTRACT ==="
$fastSketchText = [System.IO.File]::ReadAllText($fastSketch)
$fastW5100Cpp = Join-Path $fastEthernetRoot "src\utility\w5100.cpp"
$fastW5100CppText = [System.IO.File]::ReadAllText($fastW5100Cpp)
$fastHeader = Join-Path $fastEthernetRoot "src\JWPLC_W5x00_Ethernet.h"
$fastHeaderText = [System.IO.File]::ReadAllText($fastHeader)
$fastW5100H = Join-Path $fastEthernetRoot "src\utility\w5100.h"
$fastW5100HText = [System.IO.File]::ReadAllText($fastW5100H)
$fastSocketCpp = Join-Path $fastEthernetRoot "src\socket.cpp"
$fastSocketText = [System.IO.File]::ReadAllText($fastSocketCpp)
$fastUdpCpp = Join-Path $fastEthernetRoot "src\EthernetUdp.cpp"
$fastUdpText = [System.IO.File]::ReadAllText($fastUdpCpp)

Assert-ContainsExactly -Text $fastSketchText -Needle "static constexpr uint8_t UDP_RX_MAX_PACKETS_PER_HOLD = 2;" -Expected 1 -Label "H4A03A_CONTRACT_BATCH2_COUNT"
Assert-ContainsExactly -Text $fastSketchText -Needle "static constexpr uint8_t ETH_INT_PIN = 15;" -Expected 1 -Label "H4A03A_CONTRACT_INT_PIN_COUNT"
Assert-ContainsExactly -Text $fastSketchText -Needle "udpSocket.jwplcDiagReadPacketFastDeferred(" -Expected 1 -Label "H4A03A_CONTRACT_DEFERRED_READ_COUNT"
Assert-ContainsExactly -Text $fastSketchText -Needle "udpSocket.jwplcDiagCommitRxFast()" -Expected 1 -Label "H4A03A_CONTRACT_COMMIT2_COUNT"
Assert-ContainsExactly -Text $fastSketchText -Needle "uint16_t postCommitRsr = 0;" -Expected 1 -Label "H4A03A_CONTRACT_R1_RSR_COUNT"
Assert-ContainsExactly -Text $fastSketchText -Needle "ETH14_RAW_IDLE=PASS" -Expected 1 -Label "H4A03A_CONTRACT_R2_IDLE_COUNT"
Assert-ContainsAtLeast -Text $fastHeaderText -Needle "socketRecvUDPFastDeferred" -Minimum 1 -Label "H4A03A_CONTRACT_FAST_HEADER"
Assert-ContainsAtLeast -Text $fastSocketText -Needle "EthernetClass::socketRecvUDPFastDeferred" -Minimum 1 -Label "H4A03A_CONTRACT_FAST_SOCKET_IMPL"
Assert-ContainsAtLeast -Text $fastUdpText -Needle "EthernetUDP::jwplcDiagReadPacketFastDeferred" -Minimum 1 -Label "H4A03A_CONTRACT_FAST_UDP_IMPL"
Assert-ContainsAtLeast -Text $fastW5100HText -Needle "SIR_W5500" -Minimum 1 -Label "H4A03A_CONTRACT_SIR"
Assert-ContainsAtLeast -Text $fastW5100HText -Needle "SIMR_W5500" -Minimum 1 -Label "H4A03A_CONTRACT_SIMR"
Assert-ContainsAtLeast -Text $fastW5100HText -Needle "SnIMR" -Minimum 1 -Label "H4A03A_CONTRACT_SNIMR"

Assert-ContainsExactly -Text $fastW5100CppText -Needle "++jwplcDiagReadCallsCounter;" -Expected 0 -Label "H4A03A_CONTRACT_W5100_CALL_COUNTER_OFF"
Assert-ContainsExactly -Text $fastW5100CppText -Needle "jwplcDiagReadBytesCounter += len;" -Expected 0 -Label "H4A03A_CONTRACT_W5100_BYTE_COUNTER_OFF"
Assert-ContainsExactly -Text $fastSketchText -Needle "const uint32_t fastStartUs = micros();" -Expected 0 -Label "H4A03A_CONTRACT_FAST_TIMER_OFF"
Assert-ContainsExactly -Text $fastSketchText -Needle "const uint32_t udpRxHoldStartUs =" -Expected 0 -Label "H4A03A_CONTRACT_HOLD_TIMER_OFF"
Assert-ContainsExactly -Text $fastSketchText -Needle "const uint32_t tcpSpiHoldStartUs = micros();" -Expected 0 -Label "H4A03A_CONTRACT_TCP_TIMER_OFF"
Assert-ContainsExactly -Text $fastSketchText -Needle "    updateLoopTiming();" -Expected 0 -Label "H4A03A_CONTRACT_LOOP_PROFILING_OFF"
Assert-ContainsExactly -Text $fastSketchText -Needle "++ethIntIsrCount;" -Expected 0 -Label "H4A03A_CONTRACT_ISR_COUNTER_OFF"
Assert-ContainsExactly -Text $fastSketchText -Needle "++udpRxIntSkipCount;" -Expected 0 -Label "H4A03A_CONTRACT_INT_SKIP_COUNTER_OFF"
Assert-ContainsExactly -Text $fastSketchText -Needle "++udpRxIntWakeCount;" -Expected 0 -Label "H4A03A_CONTRACT_INT_WAKE_COUNTER_OFF"
Assert-ContainsExactly -Text $fastSketchText -Needle "++udpRxIntLowFallbackCount;" -Expected 0 -Label "H4A03A_CONTRACT_INT_FALLBACK_COUNTER_OFF"
Write-Host "H4A03A_SOURCE_CONTRACT=PASS"

$fqbn = "jwplc_local:esp32:jwplcbasic"
$fastSketchDir = Split-Path -Parent $fastSketch
$compileLog = Join-Path $tempRoot "03_fast_minimal_compile.log"
Write-Host ""
Write-Host "=== COMPILE FAST MINIMAL CANDIDATE ==="
$compileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $fastBuild,
    "--libraries", $fastLibrariesRoot,
    "--libraries", $repoLibrariesRoot,
    $fastSketchDir
) -LogPath $compileLog
Write-Host "H4A03A_COMPILE_EXIT=$compileExit"
if ($compileExit -ne 0) {
    Get-Content -LiteralPath $compileLog -Tail 260 | ForEach-Object { Write-Host $_ }
    throw "H4A03A_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$diagEthernetUsed = $compileText.IndexOf(
    $fastEthernetRoot,
    [System.StringComparison]::OrdinalIgnoreCase
) -ge 0
Write-Host "H4A03A_ISOLATED_FAST_ETHERNET_LIBRARY_USED=$diagEthernetUsed"
if (-not $diagEthernetUsed) {
    throw "H4A03A_FAST_LIBRARY_NOT_SELECTED"
}
Write-Host "H4A03A_COMPILE=PASS"

$results = @{
    5 = New-Object System.Collections.Generic.List[double]
    15 = New-Object System.Collections.Generic.List[double]
    30 = New-Object System.Collections.Generic.List[double]
}
$runCounts = @{ 5 = 0; 15 = 0; 30 = 0 }

# Balanced temporal order: every duration appears once in positions 1, 2 and 3.
$schedule = @(
    [PSCustomObject]@{ Cycle = 1; Duration = 5 },
    [PSCustomObject]@{ Cycle = 1; Duration = 15 },
    [PSCustomObject]@{ Cycle = 1; Duration = 30 },
    [PSCustomObject]@{ Cycle = 2; Duration = 15 },
    [PSCustomObject]@{ Cycle = 2; Duration = 30 },
    [PSCustomObject]@{ Cycle = 2; Duration = 5 },
    [PSCustomObject]@{ Cycle = 3; Duration = 30 },
    [PSCustomObject]@{ Cycle = 3; Duration = 5 },
    [PSCustomObject]@{ Cycle = 3; Duration = 15 }
)
Write-Host "H4A03A_EXECUTION_ORDER=5,15,30,15,30,5,30,5,15"
Write-Host "H4A03A_FRESH_UPLOAD_PER_CASE=YES"

$caseIndex = 0
foreach ($spec in $schedule) {
    $caseIndex += 1
    $duration = [int]$spec.Duration
    $runCounts[$duration] = [int]$runCounts[$duration] + 1
    $runNumber = [int]$runCounts[$duration]

    Write-Host ""
    Write-Host "============================================================"
    Write-Host (" H4A0.3A CASE {0}/9 - {1}s RUN {2}/3" -f $caseIndex, $duration, $runNumber)
    Write-Host "============================================================"

    $uploadLog = Join-Path $tempRoot ("04_d{0}_run{1}_upload.log" -f $duration, $runNumber)
    $uploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments @(
        "upload",
        "--fqbn", $fqbn,
        "--port", $SerialPort,
        "--input-dir", $fastBuild,
        $fastSketchDir
    ) -LogPath $uploadLog
    Write-Host ("H4A03A_D{0}_RUN{1}_UPLOAD_EXIT={2}" -f $duration, $runNumber, $uploadExit)
    if ($uploadExit -ne 0) {
        Get-Content -LiteralPath $uploadLog -Tail 240 | ForEach-Object { Write-Host $_ }
        throw ("H4A03A_D{0}_RUN{1}_UPLOAD_FAILED" -f $duration, $runNumber)
    }

    Start-Sleep -Seconds 3

    $caseLog = Join-Path $tempRoot ("05_d{0}_run{1}_udp_rx.log" -f $duration, $runNumber)
    $caseExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @(
        "-B", "-u", $caseRunner,
        "--serial", $SerialPort,
        "--duration", $duration.ToString([System.Globalization.CultureInfo]::InvariantCulture),
        "--udp-payload", $UdpPayload.ToString()
    ) -LogPath $caseLog

    Write-Host ("H4A03A_D{0}_RUN{1}_CASE_EXIT={2}" -f $duration, $runNumber, $caseExit)
    Write-Host ("H4A03A_D{0}_RUN{1}_LOG={2}" -f $duration, $runNumber, $caseLog)
    Get-Content -LiteralPath $caseLog | ForEach-Object { Write-Host $_ }
    if ($caseExit -ne 0) {
        throw ("H4A03A_D{0}_RUN{1}_CASE_FAILED" -f $duration, $runNumber)
    }

    $caseText = [System.IO.File]::ReadAllText($caseLog)
    if ((Get-LogValue -Text $caseText -Key "H4A03A_FUNCTIONAL_PASS") -ne "YES") {
        throw ("H4A03A_D{0}_RUN{1}_FUNCTIONAL_FAIL" -f $duration, $runNumber)
    }

    foreach ($errorKey in @(
        "H4A03A_TRANSPORT_ERRORS",
        "H4A03A_UDP_SPI_LOCK_ERRORS",
        "H4A03A_TCP_SPI_LOCK_ERRORS",
        "H4A03A_SPI_LOCK_ERRORS_TOTAL"
    )) {
        if ((Get-LogDouble -Text $caseText -Key $errorKey) -ne 0.0) {
            throw ("H4A03A_D{0}_RUN{1}_{2}_NONZERO" -f $duration, $runNumber, $errorKey)
        }
    }

    $mbps = Get-LogDouble -Text $caseText -Key "H4A03A_DUT_MBPS"
    Add-Result -Buckets $results -Duration $duration -Mbps $mbps
    Write-Host ("H4A03A_RUN_RESULT DURATION={0}s RUN={1} DUT_MBPS={2:F6} PASS=YES" -f $duration, $runNumber, $mbps)
}

foreach ($duration in @(5, 15, 30)) {
    if ($results[$duration].Count -ne 3) {
        throw ("H4A03A_DURATION_{0}_RUN_COUNT_INVALID={1}" -f $duration, $results[$duration].Count)
    }
}

$median5 = Get-Median -Values $results[5].ToArray()
$median15 = Get-Median -Values $results[15].ToArray()
$median30 = Get-Median -Values $results[30].ToArray()
$min5 = ($results[5] | Measure-Object -Minimum).Minimum
$max5 = ($results[5] | Measure-Object -Maximum).Maximum
$min15 = ($results[15] | Measure-Object -Minimum).Minimum
$max15 = ($results[15] | Measure-Object -Maximum).Maximum
$min30 = ($results[30] | Measure-Object -Minimum).Minimum
$max30 = ($results[30] | Measure-Object -Maximum).Maximum

$historical = 13.866349
$nearHistoricalFloor = $historical * 0.99
$recovery5 = ($median5 / $historical) * 100.0
$recovery15 = ($median15 / $historical) * 100.0
$recovery30 = ($median30 / $historical) * 100.0
$drop15Vs5 = (($median15 / $median5) - 1.0) * 100.0
$drop30Vs5 = (($median30 / $median5) - 1.0) * 100.0
$near5 = $median5 -ge $nearHistoricalFloor
$near15 = $median15 -ge $nearHistoricalFloor
$near30 = $median30 -ge $nearHistoricalFloor

if ($near5 -and $near15 -and $near30) {
    $interpretation = "HISTORICAL_RECOVERED_ALL_DURATIONS"
}
elseif ($near5 -and (-not $near15 -or -not $near30)) {
    $interpretation = "DURATION_DEPENDENT_CANDIDATE"
}
elseif ((-not $near5) -and (-not $near15) -and (-not $near30)) {
    $interpretation = "PERSISTENT_GAP_REQUIRES_CAUSAL_ISOLATION"
}
else {
    $interpretation = "MIXED_REVIEW"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " H4A0.3A PERFORMANCE SUMMARY"
Write-Host "============================================================"
Write-Host ("H4A03A_HISTORICAL_P3K_MBPS={0:F6}" -f $historical)
Write-Host "H4A03A_NEAR_HISTORICAL_HEURISTIC_PCT=99.0"
Write-Host ("H4A03A_NEAR_HISTORICAL_FLOOR_MBPS={0:F6}" -f $nearHistoricalFloor)
Write-Host ("H4A03A_5S_MEDIAN_MBPS={0:F6}" -f $median5)
Write-Host ("H4A03A_5S_MIN_MBPS={0:F6}" -f $min5)
Write-Host ("H4A03A_5S_MAX_MBPS={0:F6}" -f $max5)
Write-Host ("H4A03A_5S_RECOVERY_PCT={0:F2}" -f $recovery5)
Write-Host "H4A03A_5S_NEAR_HISTORICAL=$near5"
Write-Host ("H4A03A_15S_MEDIAN_MBPS={0:F6}" -f $median15)
Write-Host ("H4A03A_15S_MIN_MBPS={0:F6}" -f $min15)
Write-Host ("H4A03A_15S_MAX_MBPS={0:F6}" -f $max15)
Write-Host ("H4A03A_15S_RECOVERY_PCT={0:F2}" -f $recovery15)
Write-Host "H4A03A_15S_NEAR_HISTORICAL=$near15"
Write-Host ("H4A03A_30S_MEDIAN_MBPS={0:F6}" -f $median30)
Write-Host ("H4A03A_30S_MIN_MBPS={0:F6}" -f $min30)
Write-Host ("H4A03A_30S_MAX_MBPS={0:F6}" -f $max30)
Write-Host ("H4A03A_30S_RECOVERY_PCT={0:F2}" -f $recovery30)
Write-Host "H4A03A_30S_NEAR_HISTORICAL=$near30"
Write-Host ("H4A03A_15S_VS_5S_PCT={0:F2}" -f $drop15Vs5)
Write-Host ("H4A03A_30S_VS_5S_PCT={0:F2}" -f $drop30Vs5)
Write-Host "H4A03A_INTERPRETATION=$interpretation"
Write-Host "H4A03A_INTERPRETATION_IS_GATE_VERDICT=NO"

Write-Host ""
Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"
$tftAnswer = Read-Host "¿TFT COM14 estable y operativo durante H4A0.3A? (S/N)"
$tftPhysical = $tftAnswer.Trim().ToUpper() -eq "S"
Write-Host "H4A03A_TFT_PHYSICAL_PASS=$tftPhysical"
if (-not $tftPhysical) {
    throw "H4A03A_TFT_PHYSICAL_REVIEW"
}

$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
Write-Host "H4A03A_FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
Write-Host "H4A03A_FINAL_STAGED_COUNT=$($finalStaged.Count)"
if ($finalDirty.Count -ne 0) {
    $finalDirty | ForEach-Object { Write-Host "FINAL_DIRTY=$_" }
    throw "H4A03A_REPOSITORY_MUTATED"
}
if ($finalStaged.Count -ne 0) {
    throw "H4A03A_INDEX_MUTATED"
}
Assert-G2ProtectedArtifacts

$summaryPath = Join-Path $tempRoot "SUMMARY.log"
$summaryLines = @(
    "A14_H4A03A_MINIMAL_DURATION_SWEEP=PASS",
    "HEAD=$head",
    "W5500_SPI_HZ=$spiHz",
    "UDP_PAYLOAD=$UdpPayload",
    ("H4A03A_5S_MEDIAN_MBPS={0:F6}" -f $median5),
    ("H4A03A_15S_MEDIAN_MBPS={0:F6}" -f $median15),
    ("H4A03A_30S_MEDIAN_MBPS={0:F6}" -f $median30),
    ("H4A03A_5S_RECOVERY_PCT={0:F2}" -f $recovery5),
    ("H4A03A_15S_RECOVERY_PCT={0:F2}" -f $recovery15),
    ("H4A03A_30S_RECOVERY_PCT={0:F2}" -f $recovery30),
    ("H4A03A_15S_VS_5S_PCT={0:F2}" -f $drop15Vs5),
    ("H4A03A_30S_VS_5S_PCT={0:F2}" -f $drop30Vs5),
    "H4A03A_INTERPRETATION=$interpretation",
    "H4A03A_TFT_PHYSICAL_PASS=$tftPhysical",
    "H4A03A_PRODUCT_SOURCE_MUTATION=NO",
    "HARNESS_FAILURE=NO",
    "PRODUCT_FAILURE=NO_EVIDENCE",
    "HARDWARE_FAILURE=NO_EVIDENCE",
    "NEXT=RETURN_TO_CHAT_INTERPRET_H4A03A_DO_NOT_ADVANCE"
)
[System.IO.File]::WriteAllLines($summaryPath, [string[]]$summaryLines)

Write-Host "H4A03A_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "HARNESS_FAILURE=NO"
Write-Host "PRODUCT_FAILURE=NO_EVIDENCE"
Write-Host "HARDWARE_FAILURE=NO_EVIDENCE"
Write-Host "H4A03A_SUMMARY_LOG=$summaryPath"
Write-Host "A14_H4A03A_MINIMAL_DURATION_SWEEP=PASS"
Write-Host "NEXT=RETURN_TO_CHAT_INTERPRET_H4A03A_DO_NOT_ADVANCE"
}

try {
    Invoke-H4A03AGate
}
catch {
    Write-Host ""
    Write-Host "============================================================"
    Write-Host " H4A0.3A ABORTED - CLASSIFICATION REQUIRED"
    Write-Host "============================================================"
    Write-Host "HARNESS_FAILURE=UNCLASSIFIED"
    Write-Host "PRODUCT_FAILURE=UNCLASSIFIED"
    Write-Host "HARDWARE_FAILURE=UNCLASSIFIED"
    Write-Host "H4A03A_FAILURE_REQUIRES_CLASSIFICATION=YES"
    Write-Host ("H4A03A_EXCEPTION={0}" -f $_.Exception.Message)
    exit 1
}
