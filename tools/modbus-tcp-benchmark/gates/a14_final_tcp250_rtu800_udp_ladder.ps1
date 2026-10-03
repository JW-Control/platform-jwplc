param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$PythonExe = "C:\Users\jeykc\AppData\Local\Programs\Python\Python311\python.exe",
    [double]$LadderDurationS = 300.0,
    [double]$ConfirmDurationS = 600.0,
    [string]$UdpLadderMbps = "0,1,2,4,6,8,10,12",
    [string]$ResultRoot = ""
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

function Require-Token {
    param(
        [string]$Path,
        [string]$Token,
        [string]$Label
    )
    $text = [IO.File]::ReadAllText($Path)
    $pass = $text.Contains($Token)
    Write-Host "$Label=$(if ($pass) { 'PASS' } else { 'FAIL' })"
    if (-not $pass) {
        throw "TRIPLE_SOURCE_CONTRACT_FAIL_$Label"
    }
}

Assert-G2Branch

$repo = $script:G2RepoRoot
$head = Get-G2Head
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $repo diff --cached --name-only)

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "TRIPLE_TRACKED_TREE_NOT_CLEAN"
}
if ($staged.Count -ne 0) {
    throw "TRIPLE_INDEX_NOT_CLEAN"
}
if ($LadderDurationS -lt 300.0) {
    throw "TRIPLE_LADDER_DURATION_LT_300"
}
if ($ConfirmDurationS -lt 600.0) {
    throw "TRIPLE_CONFIRM_DURATION_LT_600"
}
if (-not (Test-Path -LiteralPath $PythonExe)) {
    throw "TRIPLE_PYTHON_NOT_FOUND"
}

$w5100h = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h"
$tcpH = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h"
$rtuCpp = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp"
$spiH = Get-G2Path "JWPLC/2.1.0/libraries/SPI/src/SPI.h"
$ethH = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"

$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_tcp250_rtu800_udp_master"
$masterIno = Join-Path $masterSketch "a14_final_full_runtime_tcp250_rtu800_udp_master.ino"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_expmix_slave"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_final_tcp250_rtu800_udp_ladder.py"

Require-Token $w5100h "SPISettings(26000000, MSBFIRST, SPI_MODE0)" "W5500_26MHZ"
Require-Token $w5100h "#define JWPLC_W5500_RX_FIFO_REUSE 1" "FIFO_REUSE_ON"
Require-Token $spiH "#define JWPLC_SPI_FIFO_REUSE_DLEN_CACHE 1" "DLEN_REUSE_ON"
Require-Token $spiH "#define JWPLC_SPI_FIFO_REUSE_COPY_OUT_64 1" "COPY_OUT_64_ON"
Require-Token $tcpH "#define JWPLC_MODBUS_TCP_INT_GUIDED_RX 0" "INT_DEFAULT_OFF"
Require-Token $tcpH "#define JWPLC_MODBUS_TCP_INT_HOT_POLL_US 0UL" "D2_DEFAULT_OFF"
Require-Token $tcpH "#define JWPLC_MODBUS_TCP_INT_LOAD_ADAPTIVE 0" "D3_DEFAULT_OFF"
Require-Token $tcpH "#define JWPLC_MODBUS_TCP_INT_RSR_DRAIN 0" "E1_DEFAULT_OFF"
Require-Token $rtuCpp "JWPLC_MODBUS_FAST_PARTIAL_HOLD_US = 15000UL" "RTU_PARTIAL_HOLD_15MS"
Require-Token $ethH "jwplcReadPacketFastDeferred" "UDP_FAST_API_PRESENT"
Require-Token $masterIno "UDP_FAST_DIAGNOSTIC=ADDITIVE_ENABLED" "TRIPLE_UDP_DIAGNOSTIC"
Require-Token $masterIno "setRtuScanPaced100Hz()" "TRIPLE_RTU_SCAN100_COMMAND"
Require-Token $masterIno "UDP_FAST_SEQUENCE_RANGE_MISSING" "TRIPLE_UDP_SEQUENCE_TELEMETRY"
Require-Token $runner 'DEFAULT_UDP_LADDER = "0,1,2,4,6,8,10,12"' "TRIPLE_UDP_LADDER"
Require-Token $runner "TCP_TARGET_REQ_S = 250.0" "TRIPLE_TCP250"
Require-Token $runner "RTU_TARGET_REQ_S = 800" "TRIPLE_RTU800"

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "TRIPLE_ARDUINO_CLI_NOT_FOUND"
}

$fqbn = "jwplc_local:esp32:jwplcbasic"
$libraries = Get-G2Path "JWPLC/2.1.0/libraries"

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo "tools\modbus-tcp-benchmark\results\a14_tcp250_rtu800_udp_ladder_$stamp"
}
New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$sessionLog = Join-Path $ResultRoot "SESSION.log"
Start-Transcript -Path $sessionLog -Force | Out-Null

try {
    Write-Host "=============================================================================="
    Write-Host " ALPHA14 FINAL TRIPLE COEXISTENCE - TCP250 + RTU800 + UDP FAST"
    Write-Host "=============================================================================="
    Write-Host "BRANCH=$(& git -C $repo branch --show-current)"
    Write-Host "HEAD=$head"
    Write-Host "MASTER_PORT=$MasterPort"
    Write-Host "SLAVE_PORT=$SlavePort"
    Write-Host "TCP_TARGET_REQ_S=250"
    Write-Host "RTU_TARGET_REQ_S=800"
    Write-Host "RTU_SCHEDULER_MODE=SCAN_PACED_100HZ"
    Write-Host "RTU_SCAN_TARGET_HZ=100"
    Write-Host "UDP_LADDER_MBPS=$UdpLadderMbps"
    Write-Host "UDP_PAYLOAD_BYTES=1016"
    Write-Host "LADDER_DURATION_S=$LadderDurationS"
    Write-Host "CONFIRM_DURATION_S=$ConfirmDurationS"
    Write-Host "FULL_RUNTIME=DISPLAY_SD_FRAM_RTC_IO_BUTTONS"
    Write-Host "HOST_POLICY=PC_IDLE_NO_GAME_NO_STREAMING_NO_DOWNLOADS"
    Write-Host "RESULT_ROOT=$ResultRoot"

    @(
        "DATE=$(Get-Date -Format o)"
        "BRANCH=$(& git -C $repo branch --show-current)"
        "HEAD=$head"
        "MASTER_PORT=$MasterPort"
        "SLAVE_PORT=$SlavePort"
        "FQBN=$fqbn"
        "W5500_SPI_HZ=$(Get-G2SpiHz)"
        "TCP_TARGET_REQ_S=250"
        "RTU_TARGET_REQ_S=800"
        "RTU_SCHEDULER_MODE=SCAN_PACED_100HZ"
        "RTU_SCAN_TARGET_HZ=100"
        "RTU_WORKLOAD=EXP_MIX_2DI_2DO_2AI_2AO"
        "UDP_LADDER_MBPS=$UdpLadderMbps"
        "UDP_PAYLOAD_BYTES=1016"
        "UDP_MODE=FAST_ADDITIVE_BATCH2"
        "LADDER_DURATION_S=$LadderDurationS"
        "CONFIRM_DURATION_S=$ConfirmDurationS"
        "FULL_RUNTIME=DISPLAY_SD_FRAM_RTC_IO_BUTTONS"
        "HOST_POLICY=IDLE_NO_GAME_NO_STREAMING_NO_DOWNLOADS"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "MANIFEST.txt") -Encoding UTF8

    & git -C $repo diff --check *> (Join-Path $ResultRoot "git_diff_check.log")
    if ($LASTEXITCODE -ne 0) {
        throw "TRIPLE_GIT_DIFF_CHECK_FAILED"
    }

    $syntaxExit = Invoke-NativeToLog $PythonExe @(
        "-m", "py_compile", $runner
    ) (Join-Path $ResultRoot "python_syntax.log")
    Write-Host "PYTHON_SYNTAX_EXIT=$syntaxExit"
    if ($syntaxExit -ne 0) {
        throw "TRIPLE_PYTHON_SYNTAX_FAILED"
    }

    $masterBuild = Join-Path $env:TEMP ("a14_triple_master_" + $stamp)
    $slaveBuild = Join-Path $env:TEMP ("a14_triple_slave_" + $stamp)
    New-Item -ItemType Directory -Force -Path $masterBuild | Out-Null
    New-Item -ItemType Directory -Force -Path $slaveBuild | Out-Null

    Write-Host "TRIPLE_MASTER_COMPILE=START"
    $masterCompile = Invoke-NativeToLog $arduinoCli @(
        "compile", "--verbose",
        "--fqbn", $fqbn,
        "--build-path", $masterBuild,
        "--libraries", $libraries,
        $masterSketch
    ) (Join-Path $ResultRoot "compile_master.log")
    if ($masterCompile -ne 0) {
        Write-Host "=== TRIPLE MASTER COMPILE LOG TAIL ==="
        Get-Content -LiteralPath (Join-Path $ResultRoot "compile_master.log") -Tail 220 |
            ForEach-Object { Write-Host $_ }
        throw "TRIPLE_MASTER_COMPILE_FAILED"
    }

    Write-Host "TRIPLE_SLAVE_COMPILE=START"
    $slaveCompile = Invoke-NativeToLog $arduinoCli @(
        "compile", "--verbose",
        "--fqbn", $fqbn,
        "--build-path", $slaveBuild,
        "--libraries", $libraries,
        $slaveSketch
    ) (Join-Path $ResultRoot "compile_slave.log")
    if ($slaveCompile -ne 0) {
        Write-Host "=== TRIPLE SLAVE COMPILE LOG TAIL ==="
        Get-Content -LiteralPath (Join-Path $ResultRoot "compile_slave.log") -Tail 220 |
            ForEach-Object { Write-Host $_ }
        throw "TRIPLE_SLAVE_COMPILE_FAILED"
    }

    $masterRtuObjects = @(
        Get-ChildItem -LiteralPath $masterBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JWPLC_ModbusRTU.cpp.o" }
    )
    $masterTcpObjects = @(
        Get-ChildItem -LiteralPath $masterBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JWPLC_ModbusTCP.cpp.o" }
    )
    $masterEthObjects = @(
        Get-ChildItem -LiteralPath $masterBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JWPLC_W5x00_Ethernet.cpp.o" }
    )
    $masterUdpObjects = @(
        Get-ChildItem -LiteralPath $masterBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "EthernetUdp.cpp.o" }
    )
    $masterSpiObjects = @(
        Get-ChildItem -LiteralPath $masterBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "SPI.cpp.o" }
    )
    $slaveRtuObjects = @(
        Get-ChildItem -LiteralPath $slaveBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JWPLC_ModbusRTU.cpp.o" }
    )

    $compileText = (
        [IO.File]::ReadAllText((Join-Path $ResultRoot "compile_master.log")) +
        [IO.File]::ReadAllText((Join-Path $ResultRoot "compile_slave.log"))
    )

    $precompiledRtu = [regex]::IsMatch(
        $compileText,
        '(?im)using precompiled library .*JWPLC_ModbusRTU|usando .*precompilad.*JWPLC_ModbusRTU'
    )
    $precompiledTcp = [regex]::IsMatch(
        $compileText,
        '(?im)using precompiled library .*JWPLC_ModbusTCP|usando .*precompilad.*JWPLC_ModbusTCP'
    )
    $precompiledEth = [regex]::IsMatch(
        $compileText,
        '(?im)using precompiled library .*JWPLC_Ethernet|usando .*precompilad.*JWPLC_Ethernet'
    )
    $precompiledSpi = [regex]::IsMatch(
        $compileText,
        '(?im)using precompiled library .*SPI|usando .*precompilad.*SPI'
    )

    Write-Host "MASTER_RTU_SOURCE_OBJECT_COUNT=$($masterRtuObjects.Count)"
    Write-Host "MASTER_TCP_SOURCE_OBJECT_COUNT=$($masterTcpObjects.Count)"
    Write-Host "MASTER_ETH_SOURCE_OBJECT_COUNT=$($masterEthObjects.Count)"
    Write-Host "MASTER_UDP_SOURCE_OBJECT_COUNT=$($masterUdpObjects.Count)"
    Write-Host "MASTER_SPI_SOURCE_OBJECT_COUNT=$($masterSpiObjects.Count)"
    Write-Host "SLAVE_RTU_SOURCE_OBJECT_COUNT=$($slaveRtuObjects.Count)"
    Write-Host "MODBUS_RTU_PRECOMPILED_MARKER=$(if ($precompiledRtu) { 'YES' } else { 'NO' })"
    Write-Host "MODBUS_TCP_PRECOMPILED_MARKER=$(if ($precompiledTcp) { 'YES' } else { 'NO' })"
    Write-Host "ETHERNET_PRECOMPILED_MARKER=$(if ($precompiledEth) { 'YES' } else { 'NO' })"
    Write-Host "SPI_PRECOMPILED_MARKER=$(if ($precompiledSpi) { 'YES' } else { 'NO' })"

    if (
        $masterRtuObjects.Count -ne 1 -or
        $masterTcpObjects.Count -ne 1 -or
        $masterEthObjects.Count -ne 1 -or
        $masterUdpObjects.Count -ne 1 -or
        $masterSpiObjects.Count -ne 1 -or
        $slaveRtuObjects.Count -ne 1 -or
        $precompiledRtu -or
        $precompiledTcp -or
        $precompiledEth -or
        $precompiledSpi
    ) {
        throw "TRIPLE_SOURCE_FIRST_NOT_PROVEN"
    }

    Write-Host "TRIPLE_UPLOAD_SLAVE=START"
    $uploadSlave = Invoke-NativeToLog $arduinoCli @(
        "upload",
        "--fqbn", $fqbn,
        "--port", $SlavePort,
        "--input-dir", $slaveBuild,
        $slaveSketch
    ) (Join-Path $ResultRoot "upload_slave.log")
    if ($uploadSlave -ne 0) {
        throw "TRIPLE_SLAVE_UPLOAD_FAILED"
    }

    Write-Host "TRIPLE_UPLOAD_MASTER=START"
    $uploadMaster = Invoke-NativeToLog $arduinoCli @(
        "upload",
        "--fqbn", $fqbn,
        "--port", $MasterPort,
        "--input-dir", $masterBuild,
        $masterSketch
    ) (Join-Path $ResultRoot "upload_master.log")
    if ($uploadMaster -ne 0) {
        throw "TRIPLE_MASTER_UPLOAD_FAILED"
    }

    Start-Sleep -Seconds 3

    $runnerLog = Join-Path $ResultRoot "runner.log"
    $runnerExit = Invoke-NativeToLog $PythonExe @(
        "-u", $runner,
        "--master-serial", $MasterPort,
        "--slave-serial", $SlavePort,
        "--output-root", $ResultRoot,
        "--ladder-duration", $LadderDurationS.ToString([Globalization.CultureInfo]::InvariantCulture),
        "--confirm-duration", $ConfirmDurationS.ToString([Globalization.CultureInfo]::InvariantCulture),
        "--udp-ladder", $UdpLadderMbps
    ) $runnerLog

    Get-Content -LiteralPath $runnerLog -Tail 160 |
        ForEach-Object { Write-Host $_ }

    if ($runnerExit -ne 0) {
        throw "TRIPLE_RUNNER_FAILED"
    }

    $finalStatusPath = Join-Path $ResultRoot "FINAL_STATUS.txt"
    if (-not (Test-Path -LiteralPath $finalStatusPath)) {
        throw "TRIPLE_FINAL_STATUS_MISSING"
    }

    $finalStatusText = [IO.File]::ReadAllText($finalStatusPath)
    $pass = $finalStatusText.Contains(
        "A14_FINAL_TRIPLE_COEXISTENCE=PASS_TRIPLE_COEXISTENCE_CONFIRMED"
    )
    $characterizedNoPositive = $finalStatusText.Contains(
        "A14_FINAL_TRIPLE_COEXISTENCE=CHARACTERIZED_NO_POSITIVE_UDP_OPERATIONAL"
    )
    $characterizedBaselineNotStrict = $finalStatusText.Contains(
        "A14_FINAL_TRIPLE_COEXISTENCE=CHARACTERIZED_BASELINE_NOT_OPERATIONAL"
    )

    $resultLabel = if ($pass) {
        "PASS"
    }
    elseif ($characterizedBaselineNotStrict) {
        "CHARACTERIZED_BASELINE_NOT_OPERATIONAL"
    }
    elseif ($characterizedNoPositive) {
        "CHARACTERIZED_NO_POSITIVE_UDP_OPERATIONAL"
    }
    else {
        "FAIL"
    }

    Write-Host "TRIPLE_RESULT=$resultLabel"
    if (
        -not $pass -and
        -not $characterizedNoPositive -and
        -not $characterizedBaselineNotStrict
    ) {
        throw "TRIPLE_CRITERIA_NOT_MET"
    }

    $finalHead = Get-G2Head
    $finalDirty = @(Get-G2TrackedDirtyPaths)
    $finalStaged = @(& git -C $repo diff --cached --name-only)

    if ($finalHead -ne $head) {
        throw "TRIPLE_HEAD_CHANGED"
    }
    if ($finalDirty.Count -ne 0) {
        throw "TRIPLE_TREE_DIRTY_AT_END"
    }
    if ($finalStaged.Count -ne 0) {
        throw "TRIPLE_INDEX_DIRTY_AT_END"
    }

    $gateStatus = if ($pass) {
        "PASS_TRIPLE_COEXISTENCE_CONFIRMED"
    }
    elseif ($characterizedBaselineNotStrict) {
        "CHARACTERIZED_BASELINE_NOT_OPERATIONAL"
    }
    else {
        "CHARACTERIZED_NO_POSITIVE_UDP_OPERATIONAL"
    }

    @(
        "A14_FINAL_TRIPLE_GATE=$gateStatus"
        "INITIAL_HEAD=$head"
        "FINAL_HEAD=$finalHead"
        "RUNNER_EXIT=$runnerExit"
        "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
        "STAGED_FINAL=$($finalStaged.Count)"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "GATE_STATUS.txt") -Encoding UTF8

    Write-Host "A14_FINAL_TRIPLE_GATE=$gateStatus"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
catch {
    @(
        "A14_FINAL_TRIPLE_GATE=FAIL"
        "ERROR=$($_.Exception.Message)"
        "INITIAL_HEAD=$head"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "GATE_STATUS.txt") -Encoding UTF8
    throw
}
finally {
    Stop-Transcript | Out-Null
}
