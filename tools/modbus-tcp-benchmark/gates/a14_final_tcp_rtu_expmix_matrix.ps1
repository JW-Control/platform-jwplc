param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$PythonExe = "C:\\Users\\jeykc\\AppData\\Local\\Programs\\Python\\Python311\\python.exe",
    [double]$DurationS = 300.0,
    [string]$Targets = "off,100,250,500,750,1000"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

Assert-G2Branch

$repo = $script:G2RepoRoot
$head = Get-G2Head
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $repo diff --cached --name-only)

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "R4_EXPMIX_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    throw "R4_EXPMIX_INDEX_NOT_CLEAN"
}

if ($DurationS -lt 300.0) {
    throw "R4_EXPMIX_DURATION_LT_300"
}

if (-not (Test-Path -LiteralPath $PythonExe)) {
    throw "R4_EXPMIX_PYTHON_NOT_FOUND"
}

$w5100h = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h"
$tcpH = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h"
$spiH = Get-G2Path "JWPLC/2.1.0/libraries/SPI/src/SPI.h"

$w5100Text = [IO.File]::ReadAllText($w5100h)
$tcpText = [IO.File]::ReadAllText($tcpH)
$spiText = [IO.File]::ReadAllText($spiH)

$sourceChecks = [ordered]@{
    "W5500_26MHZ" =
        $w5100Text.Contains("SPISettings(26000000, MSBFIRST, SPI_MODE0)")
    "FIFO_REUSE_ON" =
        $w5100Text.Contains("#define JWPLC_W5500_RX_FIFO_REUSE 1")
    "DLEN_REUSE_ON" =
        $spiText.Contains("#define JWPLC_SPI_FIFO_REUSE_DLEN_CACHE 1")
    "COPY_OUT_64_ON" =
        $spiText.Contains("#define JWPLC_SPI_FIFO_REUSE_COPY_OUT_64 1")
    "INT_DEFAULT_OFF" =
        $tcpText.Contains("#define JWPLC_MODBUS_TCP_INT_GUIDED_RX 0")
    "D2_DEFAULT_OFF" =
        $tcpText.Contains("#define JWPLC_MODBUS_TCP_INT_HOT_POLL_US 0UL")
    "D3_DEFAULT_OFF" =
        $tcpText.Contains("#define JWPLC_MODBUS_TCP_INT_LOAD_ADAPTIVE 0")
    "E1_DEFAULT_OFF" =
        $tcpText.Contains("#define JWPLC_MODBUS_TCP_INT_RSR_DRAIN 0")
}

foreach ($entry in $sourceChecks.GetEnumerator()) {
    Write-Host "$($entry.Key)=$(if ($entry.Value) { 'PASS' } else { 'FAIL' })"
    if (-not $entry.Value) {
        throw "R4_EXPMIX_SOURCE_DEFAULT_FAIL_$($entry.Key)"
    }
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultRoot = Join-Path $repo "tools\modbus-tcp-benchmark\results\a14_tcp_rtu_expmix_$stamp"
New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "R4_EXPMIX_ARDUINO_CLI_NOT_FOUND"
}

$fqbn = "jwplc_local:esp32:jwplcbasic"
$libraries = Get-G2Path "JWPLC/2.1.0/libraries"

$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_expmix_master"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_expmix_slave"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_final_tcp_rtu_expmix_matrix.py"

$masterBuild = Join-Path $env:TEMP ("a14_r4_master_" + $stamp)
$slaveBuild = Join-Path $env:TEMP ("a14_r4_slave_" + $stamp)
New-Item -ItemType Directory -Force -Path $masterBuild | Out-Null
New-Item -ItemType Directory -Force -Path $slaveBuild | Out-Null

Write-Host "=============================================================================="
Write-Host " ALPHA14 FINAL R4 - FULL RUNTIME TCP x RTU EXP-MIX"
Write-Host "=============================================================================="
Write-Host "HEAD=$head"
Write-Host "DURATION_PER_CASE_S=$DurationS"
Write-Host "TCP_TARGETS=$Targets"
Write-Host "RTU_WORKLOAD=2DI_2DO_2AI_2AO_UNPACED"
Write-Host "RTU_PROFILE=500K_FIFO9_8_BULK_QUEUED_STRUCTURAL_SLAVE"
Write-Host "FULL_RUNTIME=DISPLAY_SD_FRAM_RTC_IO_BUTTONS"
Write-Host "SERIAL_POLICY=QUIET_DURING_WINDOW"

# Syntax check without generating __pycache__.
$escapedRunner = $runner.Replace("'", "''")
$syntaxCode = "import ast,pathlib; ast.parse(pathlib.Path(r'$escapedRunner').read_text(encoding='utf-8'))"
& $PythonExe -c $syntaxCode
if ($LASTEXITCODE -ne 0) {
    throw "R4_EXPMIX_PYTHON_SYNTAX_FAILED"
}

& $arduinoCli compile --verbose --fqbn $fqbn --build-path $masterBuild --libraries $libraries $masterSketch *> (Join-Path $resultRoot "compile_master.log")
if ($LASTEXITCODE -ne 0) {
    throw "R4_EXPMIX_MASTER_COMPILE_FAILED"
}

& $arduinoCli compile --verbose --fqbn $fqbn --build-path $slaveBuild --libraries $libraries $slaveSketch *> (Join-Path $resultRoot "compile_slave.log")
if ($LASTEXITCODE -ne 0) {
    throw "R4_EXPMIX_SLAVE_COMPILE_FAILED"
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
    [IO.File]::ReadAllText((Join-Path $resultRoot "compile_master.log")) +
    [IO.File]::ReadAllText((Join-Path $resultRoot "compile_slave.log"))
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
    throw "R4_EXPMIX_SOURCE_FIRST_NOT_PROVEN"
}

& $arduinoCli upload --fqbn $fqbn --port $SlavePort --input-dir $slaveBuild $slaveSketch *> (Join-Path $resultRoot "upload_slave.log")
if ($LASTEXITCODE -ne 0) {
    throw "R4_EXPMIX_SLAVE_UPLOAD_FAILED"
}

& $arduinoCli upload --fqbn $fqbn --port $MasterPort --input-dir $masterBuild $masterSketch *> (Join-Path $resultRoot "upload_master.log")
if ($LASTEXITCODE -ne 0) {
    throw "R4_EXPMIX_MASTER_UPLOAD_FAILED"
}

Start-Sleep -Seconds 3

$runnerLog = Join-Path $resultRoot "runner.log"

& $PythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --duration $DurationS --targets $Targets --output-root $resultRoot *> $runnerLog
$runnerExit = $LASTEXITCODE

Get-Content -LiteralPath $runnerLog -Tail 120 |
    ForEach-Object { Write-Host $_ }

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $repo diff --cached --name-only)

@(
    "HEAD=$head"
    "RESULT_ROOT=$resultRoot"
    "RUNNER_EXIT=$runnerExit"
    "TRACKED_DIRTY_FINAL=$($dirtyFinal.Count)"
    "STAGED_FINAL=$($stagedFinal.Count)"
) | Set-Content -LiteralPath (Join-Path $resultRoot "FINAL_STATUS.txt") -Encoding UTF8

if ($runnerExit -ne 0) {
    throw "R4_EXPMIX_MATRIX_FAILED"
}

if ($dirtyFinal.Count -ne 0) {
    throw "R4_EXPMIX_TREE_DIRTY_AT_END"
}

if ($stagedFinal.Count -ne 0) {
    throw "R4_EXPMIX_INDEX_DIRTY_AT_END"
}

Write-Host "A14_R4_EXPMIX_GATE=PASS_CHARACTERIZED"
Write-Host "RESULT_ROOT=$resultRoot"
