param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$PythonExe = "C:\Users\jeykc\AppData\Local\Programs\Python\Python311\python.exe",
    [double]$R7DurationS = 600.0,
    [double]$R8DurationS = 300.0,
    [double]$TcpOnlyDurationS = 600.0,
    [double]$ConfirmDurationS = 600.0,
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
        throw "HOST_IDLE_SOURCE_CONTRACT_FAIL_$Label"
    }
}

Assert-G2Branch

$repo = $script:G2RepoRoot
$head = Get-G2Head
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $repo diff --cached --name-only)

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "HOST_IDLE_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    throw "HOST_IDLE_INDEX_NOT_CLEAN"
}

if ($R7DurationS -lt 600.0) {
    throw "HOST_IDLE_R7_DURATION_LT_600"
}
if ($R8DurationS -lt 300.0) {
    throw "HOST_IDLE_R8_DURATION_LT_300"
}
if ($TcpOnlyDurationS -lt 600.0) {
    throw "HOST_IDLE_TCP_ONLY_DURATION_LT_600"
}
if ($ConfirmDurationS -lt 600.0) {
    throw "HOST_IDLE_CONFIRM_DURATION_LT_600"
}
if (-not (Test-Path -LiteralPath $PythonExe)) {
    throw "HOST_IDLE_PYTHON_NOT_FOUND"
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

foreach ($rate in @(50,100,150,200,250,300)) {
    Require-Token $masterIno "RTU_RATE_HZ=$rate" "EXPMIX_FIXED_RATE_$rate"
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "HOST_IDLE_ARDUINO_CLI_NOT_FOUND"
}

$fqbn = "jwplc_local:esp32:jwplcbasic"
$libraries = Get-G2Path "JWPLC/2.1.0/libraries"
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_expmix_master"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_expmix_slave"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_final_tcp_rtu_host_idle_recheck.py"

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo "tools\modbus-tcp-benchmark\results\a14_tcp_rtu_host_idle_recheck_$stamp"
}

New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$sessionLog = Join-Path $ResultRoot "SESSION.log"
Start-Transcript -Path $sessionLog -Force | Out-Null

try {
    Write-Host "=============================================================================="
    Write-Host " ALPHA14 FINAL HOST-IDLE RECHECK - TCP x RTU EXP-MIX"
    Write-Host "=============================================================================="
    Write-Host "IMPORTANT=LEAVE_TEST_PC_IDLE_NO_GAME_NO_STREAMING_NO_DOWNLOADS"
    Write-Host "BRANCH=$(& git -C $repo branch --show-current)"
    Write-Host "HEAD=$head"
    Write-Host "MASTER_PORT=$MasterPort"
    Write-Host "SLAVE_PORT=$SlavePort"
    Write-Host "R7_DURATION_S=$R7DurationS"
    Write-Host "R8_DURATION_S=$R8DurationS"
    Write-Host "TCP_ONLY_DURATION_S=$TcpOnlyDurationS"
    Write-Host "CONFIRM_DURATION_S=$ConfirmDurationS"
    Write-Host "R7_TARGETS=OFF,900,925,950,975,1000"
    Write-Host "R8_REP1=0,50,100,150,200,250,300"
    Write-Host "R8_REP2=300,250,200,150,100,50,0"
    Write-Host "TCP_TARGET_REQ_S=1000"
    Write-Host "RTU_WORKLOAD=2DI_2DO_2AI_2AO"
    Write-Host "RTU_PROFILE=500K_FIFO9_8_BULK_QUEUED_STRUCTURAL_SLAVE"
    Write-Host "RTU_TIMEOUT_MS=25"
    Write-Host "RTU_PARTIAL_HOLD_US=15000"
    Write-Host "FULL_RUNTIME=DISPLAY_SD_FRAM_RTC_IO_BUTTONS"
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
        "R7_DURATION_S=$R7DurationS"
        "R8_DURATION_S=$R8DurationS"
        "TCP_ONLY_DURATION_S=$TcpOnlyDurationS"
        "CONFIRM_DURATION_S=$ConfirmDurationS"
        "R7_TARGETS=OFF,900,925,950,975,1000"
        "R8_REP1=0,50,100,150,200,250,300"
        "R8_REP2=300,250,200,150,100,50,0"
        "TCP_TARGET_REQ_S=1000"
        "HOST_POLICY=IDLE_NO_GAME_NO_STREAMING_NO_DOWNLOADS"
        "RTU_WORKLOAD=EXP_MIX_2DI_2DO_2AI_2AO"
        "RTU_PARTIAL_HOLD_US=15000"
        "TCP_POLICY=C0_POLLING"
    ) | Set-Content -LiteralPath $manifest -Encoding UTF8

    & git -C $repo diff --check *> (Join-Path $ResultRoot "git_diff_check.log")
    if ($LASTEXITCODE -ne 0) {
        throw "HOST_IDLE_GIT_DIFF_CHECK_FAILED"
    }

    $escapedRunner = $runner.Replace("'", "''")
    $syntaxCode = "import ast,pathlib; ast.parse(pathlib.Path(r'$escapedRunner').read_text(encoding='utf-8'))"
    & $PythonExe -c $syntaxCode *> (Join-Path $ResultRoot "python_syntax.log")
    if ($LASTEXITCODE -ne 0) {
        throw "HOST_IDLE_PYTHON_SYNTAX_FAILED"
    }

    $masterBuild = Join-Path $env:TEMP ("a14_host_idle_master_" + $stamp)
    $slaveBuild = Join-Path $env:TEMP ("a14_host_idle_slave_" + $stamp)
    New-Item -ItemType Directory -Force -Path $masterBuild | Out-Null
    New-Item -ItemType Directory -Force -Path $slaveBuild | Out-Null

    Write-Host "HOST_IDLE_MASTER_COMPILE=START"
    & $arduinoCli compile --verbose --fqbn $fqbn --build-path $masterBuild --libraries $libraries $masterSketch *> (Join-Path $ResultRoot "compile_master.log")
    if ($LASTEXITCODE -ne 0) {
        throw "HOST_IDLE_MASTER_COMPILE_FAILED"
    }

    Write-Host "HOST_IDLE_SLAVE_COMPILE=START"
    & $arduinoCli compile --verbose --fqbn $fqbn --build-path $slaveBuild --libraries $libraries $slaveSketch *> (Join-Path $ResultRoot "compile_slave.log")
    if ($LASTEXITCODE -ne 0) {
        throw "HOST_IDLE_SLAVE_COMPILE_FAILED"
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
        throw "HOST_IDLE_SOURCE_FIRST_NOT_PROVEN"
    }

    Write-Host "HOST_IDLE_UPLOAD_SLAVE=START"
    & $arduinoCli upload --fqbn $fqbn --port $SlavePort --input-dir $slaveBuild $slaveSketch *> (Join-Path $ResultRoot "upload_slave.log")
    if ($LASTEXITCODE -ne 0) {
        throw "HOST_IDLE_SLAVE_UPLOAD_FAILED"
    }

    Write-Host "HOST_IDLE_UPLOAD_MASTER=START"
    & $arduinoCli upload --fqbn $fqbn --port $MasterPort --input-dir $masterBuild $masterSketch *> (Join-Path $ResultRoot "upload_master.log")
    if ($LASTEXITCODE -ne 0) {
        throw "HOST_IDLE_MASTER_UPLOAD_FAILED"
    }

    Start-Sleep -Seconds 3

    $runnerLog = Join-Path $ResultRoot "runner.log"
    & $PythonExe -u $runner `
        --master-serial $MasterPort `
        --slave-serial $SlavePort `
        --output-root $ResultRoot `
        --r7-duration $R7DurationS `
        --r8-duration $R8DurationS `
        --tcp-only-duration $TcpOnlyDurationS `
        --confirm-duration $ConfirmDurationS `
        --tcp-target 1000 *> $runnerLog

    $runnerExit = $LASTEXITCODE

    Get-Content -LiteralPath $runnerLog -Tail 220 |
        ForEach-Object { Write-Host $_ }

    if ($runnerExit -ne 0) {
        throw "HOST_IDLE_RUNNER_FAILED"
    }

    $finalHead = Get-G2Head
    $finalDirty = @(Get-G2TrackedDirtyPaths)
    $finalStaged = @(& git -C $repo diff --cached --name-only)

    @(
        "A14_FINAL_HOST_IDLE_RECHECK_GATE=PASS_CHARACTERIZED"
        "INITIAL_HEAD=$head"
        "FINAL_HEAD=$finalHead"
        "RUNNER_EXIT=$runnerExit"
        "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
        "STAGED_FINAL=$($finalStaged.Count)"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "GATE_STATUS.txt") -Encoding UTF8

    if ($finalHead -ne $head) {
        throw "HOST_IDLE_HEAD_CHANGED"
    }
    if ($finalDirty.Count -ne 0) {
        throw "HOST_IDLE_TREE_DIRTY_AT_END"
    }
    if ($finalStaged.Count -ne 0) {
        throw "HOST_IDLE_INDEX_DIRTY_AT_END"
    }

    Write-Host "A14_FINAL_HOST_IDLE_RECHECK_GATE=PASS_CHARACTERIZED"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
catch {
    @(
        "A14_FINAL_HOST_IDLE_RECHECK_GATE=FAIL"
        "ERROR=$($_.Exception.Message)"
        "INITIAL_HEAD=$head"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "GATE_STATUS.txt") -Encoding UTF8
    throw
}
finally {
    Stop-Transcript | Out-Null
}
