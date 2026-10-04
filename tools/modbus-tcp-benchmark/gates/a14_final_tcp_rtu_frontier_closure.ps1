param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$PythonExe = "C:\Users\jeykc\AppData\Local\Programs\Python\Python311\python.exe",
    [double]$UnpacedDurationS = 600.0,
    [double]$TcpOnlyDurationS = 600.0,
    [double]$SweepDurationS = 300.0,
    [double]$ConfirmDurationS = 600.0,
    [string]$RtuRates = "0,50,100,150,200,250,300,350,400,450,500,550,600,650,700",
    [string]$ResultRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

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
        throw "R7R8_SOURCE_CONTRACT_FAIL_$Label"
    }
}

Assert-G2Branch

$repo = $script:G2RepoRoot
$head = Get-G2Head
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $repo diff --cached --name-only)

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "R7R8_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    throw "R7R8_INDEX_NOT_CLEAN"
}

if ($UnpacedDurationS -lt 600.0) {
    throw "R7R8_UNPACED_DURATION_LT_600"
}

if ($TcpOnlyDurationS -lt 600.0) {
    throw "R7R8_TCP_ONLY_DURATION_LT_600"
}

if ($SweepDurationS -lt 300.0) {
    throw "R7R8_SWEEP_DURATION_LT_300"
}

if ($ConfirmDurationS -lt 600.0) {
    throw "R7R8_CONFIRM_DURATION_LT_600"
}

if (-not (Test-Path -LiteralPath $PythonExe)) {
    throw "R7R8_PYTHON_NOT_FOUND"
}

$w5100h = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h"
$tcpH = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h"
$rtuCpp = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp"
$spiH = Get-G2Path "JWPLC/2.1.0/libraries/SPI/src/SPI.h"
$masterIno = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_expmix_master/a14_final_full_runtime_expmix_master.ino"

Require-Token $w5100h "SPISettings(26000000, MSBFIRST, SPI_MODE0)" "W5500_26MHZ"
Require-Token $w5100h "#define JWPLC_W5500_RX_FIFO_REUSE 1" "FIFO_REUSE_ON"
Require-Token $spiH "#define JWPLC_SPI_FIFO_REUSE_DLEN_CACHE 1" "DLEN_REUSE_ON"
Require-Token $spiH "#define JWPLC_SPI_FIFO_REUSE_COPY_OUT_64 1" "COPY_OUT_64_ON"
Require-Token $tcpH "#define JWPLC_MODBUS_TCP_INT_GUIDED_RX 0" "INT_DEFAULT_OFF"
Require-Token $tcpH "#define JWPLC_MODBUS_TCP_INT_HOT_POLL_US 0UL" "D2_DEFAULT_OFF"
Require-Token $tcpH "#define JWPLC_MODBUS_TCP_INT_LOAD_ADAPTIVE 0" "D3_DEFAULT_OFF"
Require-Token $tcpH "#define JWPLC_MODBUS_TCP_INT_RSR_DRAIN 0" "E1_DEFAULT_OFF"
Require-Token $rtuCpp "JWPLC_MODBUS_FAST_PARTIAL_HOLD_US = 15000UL" "RTU_PARTIAL_HOLD_15MS"

foreach ($rate in @(350,400,450,500,550,600,650,700)) {
    Require-Token $masterIno "RTU_RATE_HZ=$rate" "EXPMIX_FIXED_RATE_$rate"
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "R7R8_ARDUINO_CLI_NOT_FOUND"
}

$fqbn = "jwplc_local:esp32:jwplcbasic"
$libraries = Get-G2Path "JWPLC/2.1.0/libraries"
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_expmix_master"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_expmix_slave"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_final_tcp_rtu_frontier_closure.py"

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"

if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo "tools\modbus-tcp-benchmark\results\a14_tcp_rtu_frontier_closure_$stamp"
}

New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$sessionLog = Join-Path $ResultRoot "SESSION.log"
Start-Transcript -Path $sessionLog -Force | Out-Null

try {
    Write-Host "=============================================================================="
    Write-Host " ALPHA14 FINAL R7/R8 - TCP x RTU EXP-MIX FRONTIER CLOSURE"
    Write-Host "=============================================================================="
    Write-Host "BRANCH=$(& git -C $repo branch --show-current)"
    Write-Host "HEAD=$head"
    Write-Host "MASTER_PORT=$MasterPort"
    Write-Host "SLAVE_PORT=$SlavePort"
    Write-Host "UNPACED_DURATION_S=$UnpacedDurationS"
    Write-Host "TCP_ONLY_DURATION_S=$TcpOnlyDurationS"
    Write-Host "SWEEP_DURATION_S=$SweepDurationS"
    Write-Host "CONFIRM_DURATION_S=$ConfirmDurationS"
    Write-Host "RTU_RATES=$RtuRates"
    Write-Host "TCP_TARGET_REQ_S=1000"
    Write-Host "RTU_WORKLOAD=2DI_2DO_2AI_2AO"
    Write-Host "RTU_PROFILE=500K_FIFO9_8_BULK_QUEUED_STRUCTURAL_SLAVE"
    Write-Host "RTU_TIMEOUT_MS=25"
    Write-Host "RTU_PARTIAL_HOLD_US=15000"
    Write-Host "FULL_RUNTIME=DISPLAY_SD_FRAM_RTC_IO_BUTTONS"
    Write-Host "SERIAL_POLICY=QUIET_DURING_WINDOW"
    Write-Host "RESULT_ROOT=$ResultRoot"

    $manifest = Join-Path $ResultRoot "MANIFEST.txt"
    @(
        "DATE=$(Get-Date -Format o)"
        "BRANCH=$(& git -C $repo branch --show-current)"
        "HEAD=$head"
        "MASTER_PORT=$MasterPort"
        "SLAVE_PORT=$SlavePort"
        "FQBN=$fqbn"
        "W5500_SPI_HZ=$(Get-G2SpiHz)"
        "UNPACED_DURATION_S=$UnpacedDurationS"
        "TCP_ONLY_DURATION_S=$TcpOnlyDurationS"
        "SWEEP_DURATION_S=$SweepDurationS"
        "CONFIRM_DURATION_S=$ConfirmDurationS"
        "RTU_RATES=$RtuRates"
        "TCP_TARGET_REQ_S=1000"
        "RTU_WORKLOAD=EXP_MIX_2DI_2DO_2AI_2AO"
        "RTU_PARTIAL_HOLD_US=15000"
        "TCP_POLICY=C0_POLLING"
        "INT_DEFAULT=OFF"
        "D2_DEFAULT=OFF"
        "D3_DEFAULT=OFF"
        "E1_DEFAULT=OFF"
    ) | Set-Content -LiteralPath $manifest -Encoding UTF8

    & git -C $repo diff --check *> (Join-Path $ResultRoot "git_diff_check.log")
    if ($LASTEXITCODE -ne 0) {
        throw "R7R8_GIT_DIFF_CHECK_FAILED"
    }

    $escapedRunner = $runner.Replace("'", "''")
    $syntaxCode = "import ast,pathlib; ast.parse(pathlib.Path(r'$escapedRunner').read_text(encoding='utf-8'))"
    & $PythonExe -c $syntaxCode *> (Join-Path $ResultRoot "python_syntax.log")
    if ($LASTEXITCODE -ne 0) {
        throw "R7R8_PYTHON_SYNTAX_FAILED"
    }

    $masterBuild = Join-Path $env:TEMP ("a14_r7r8_master_" + $stamp)
    $slaveBuild = Join-Path $env:TEMP ("a14_r7r8_slave_" + $stamp)
    New-Item -ItemType Directory -Force -Path $masterBuild | Out-Null
    New-Item -ItemType Directory -Force -Path $slaveBuild | Out-Null

    Write-Host "R7R8_MASTER_COMPILE=START"
    & $arduinoCli compile --verbose --fqbn $fqbn --build-path $masterBuild --libraries $libraries $masterSketch *> (Join-Path $ResultRoot "compile_master.log")
    if ($LASTEXITCODE -ne 0) {
        throw "R7R8_MASTER_COMPILE_FAILED"
    }

    Write-Host "R7R8_SLAVE_COMPILE=START"
    & $arduinoCli compile --verbose --fqbn $fqbn --build-path $slaveBuild --libraries $libraries $slaveSketch *> (Join-Path $ResultRoot "compile_slave.log")
    if ($LASTEXITCODE -ne 0) {
        throw "R7R8_SLAVE_COMPILE_FAILED"
    }

    $masterRtuObjects = @(
        Get-ChildItem -LiteralPath $masterBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JWPLC_ModbusRTU.cpp.o" }
    )
    $masterTcpObjects = @(
        Get-ChildItem -LiteralPath $masterBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JWPLC_ModbusTCP.cpp.o" }
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

    Write-Host "MASTER_RTU_SOURCE_OBJECT_COUNT=$($masterRtuObjects.Count)"
    Write-Host "MASTER_TCP_SOURCE_OBJECT_COUNT=$($masterTcpObjects.Count)"
    Write-Host "SLAVE_RTU_SOURCE_OBJECT_COUNT=$($slaveRtuObjects.Count)"
    Write-Host "MODBUS_RTU_PRECOMPILED_MARKER=$(if ($precompiledRtu) { 'YES' } else { 'NO' })"
    Write-Host "MODBUS_TCP_PRECOMPILED_MARKER=$(if ($precompiledTcp) { 'YES' } else { 'NO' })"

    if ($masterRtuObjects.Count -ne 1 -or
        $masterTcpObjects.Count -ne 1 -or
        $slaveRtuObjects.Count -ne 1 -or
        $precompiledRtu -or
        $precompiledTcp) {
        throw "R7R8_SOURCE_FIRST_NOT_PROVEN"
    }

    Write-Host "R7R8_UPLOAD_SLAVE=START"
    & $arduinoCli upload --fqbn $fqbn --port $SlavePort --input-dir $slaveBuild $slaveSketch *> (Join-Path $ResultRoot "upload_slave.log")
    if ($LASTEXITCODE -ne 0) {
        throw "R7R8_SLAVE_UPLOAD_FAILED"
    }

    Write-Host "R7R8_UPLOAD_MASTER=START"
    & $arduinoCli upload --fqbn $fqbn --port $MasterPort --input-dir $masterBuild $masterSketch *> (Join-Path $ResultRoot "upload_master.log")
    if ($LASTEXITCODE -ne 0) {
        throw "R7R8_MASTER_UPLOAD_FAILED"
    }

    Start-Sleep -Seconds 3

    $runnerLog = Join-Path $ResultRoot "runner.log"
    & $PythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --output-root $ResultRoot --unpaced-duration $UnpacedDurationS --tcp-only-duration $TcpOnlyDurationS --sweep-duration $SweepDurationS --confirm-duration $ConfirmDurationS --tcp-target 1000 --rtu-rates $RtuRates *> $runnerLog

    $runnerExit = $LASTEXITCODE

    Get-Content -LiteralPath $runnerLog -Tail 180 |
        ForEach-Object { Write-Host $_ }

    if ($runnerExit -ne 0) {
        throw "R7R8_RUNNER_FAILED"
    }

    $finalHead = Get-G2Head
    $finalDirty = @(Get-G2TrackedDirtyPaths)
    $finalStaged = @(& git -C $repo diff --cached --name-only)

    @(
        "A14_FINAL_TCP_RTU_FRONTIER_CLOSURE_GATE=PASS_CHARACTERIZED"
        "INITIAL_HEAD=$head"
        "FINAL_HEAD=$finalHead"
        "RUNNER_EXIT=$runnerExit"
        "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
        "STAGED_FINAL=$($finalStaged.Count)"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "GATE_STATUS.txt") -Encoding UTF8

    if ($finalHead -ne $head) {
        throw "R7R8_HEAD_CHANGED"
    }
    if ($finalDirty.Count -ne 0) {
        throw "R7R8_TREE_DIRTY_AT_END"
    }
    if ($finalStaged.Count -ne 0) {
        throw "R7R8_INDEX_DIRTY_AT_END"
    }

    Write-Host "A14_FINAL_TCP_RTU_FRONTIER_CLOSURE_GATE=PASS_CHARACTERIZED"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
catch {
    @(
        "A14_FINAL_TCP_RTU_FRONTIER_CLOSURE_GATE=FAIL"
        "ERROR=$($_.Exception.Message)"
        "INITIAL_HEAD=$head"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "GATE_STATUS.txt") -Encoding UTF8
    throw
}
finally {
    Stop-Transcript | Out-Null
}
