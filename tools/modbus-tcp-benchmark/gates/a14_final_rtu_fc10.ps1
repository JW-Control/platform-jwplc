param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$PythonExe = "C:\\Users\\jeykc\\AppData\\Local\\Programs\\Python\\Python311\\python.exe",
    [double]$DurationS = 600.0
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
    throw "R_FC10_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    throw "R_FC10_INDEX_NOT_CLEAN"
}

if ($DurationS -lt 600.0) {
    throw "R_FC10_DURATION_LT_600"
}

if (-not (Test-Path -LiteralPath $PythonExe)) {
    throw "R_FC10_PYTHON_NOT_FOUND"
}

$header = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.h"
$cpp = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp"
$headerText = [IO.File]::ReadAllText($header)
$cppText = [IO.File]::ReadAllText($cpp)

foreach ($token in @(
    "requestWriteMultipleRegisters",
    "writeMultipleRegistersSync",
    "writeMultipleRegisters",
    "JWPLC_MODBUS_MASTER_OP_WRITE_MULTIPLE_REGISTERS"
)) {
    if (-not $headerText.Contains($token) -and -not $cppText.Contains($token)) {
        throw "R_FC10_SOURCE_TOKEN_MISSING=$token"
    }
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultRoot = Join-Path $repo "tools\modbus-tcp-benchmark\results\a14_rtu_fc10_$stamp"
New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "R_FC10_ARDUINO_CLI_NOT_FOUND"
}

$fqbn = "jwplc_local:esp32:jwplcbasic"
$libraries = Get-G2Path "JWPLC/2.1.0/libraries"
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_rtu_fc10_master"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_rtu_fc10_slave"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_final_rtu_fc10_qualification.py"

$masterBuild = Join-Path $env:TEMP ("a14_fc10_master_" + $stamp)
$slaveBuild = Join-Path $env:TEMP ("a14_fc10_slave_" + $stamp)
New-Item -ItemType Directory -Force -Path $masterBuild | Out-Null
New-Item -ItemType Directory -Force -Path $slaveBuild | Out-Null

Write-Host "=============================================================================="
Write-Host " ALPHA14 FINAL RTU - R-FC10 MASTER QUALIFICATION"
Write-Host "=============================================================================="
Write-Host "HEAD=$head"
Write-Host "DURATION_S=$DurationS"
Write-Host "PROFILE=500K_FIFO9_8_BULK_QUEUED_STRUCTURAL_SLAVE"
Write-Host "SERIAL_POLICY=QUIET_DURING_WINDOW"

& $PythonExe -m py_compile $runner
if ($LASTEXITCODE -ne 0) {
    throw "R_FC10_PYTHON_SYNTAX_FAILED"
}

& $arduinoCli compile --verbose --fqbn $fqbn --build-path $masterBuild --libraries $libraries $masterSketch *> (Join-Path $resultRoot "compile_master.log")
if ($LASTEXITCODE -ne 0) {
    throw "R_FC10_MASTER_COMPILE_FAILED"
}

& $arduinoCli compile --verbose --fqbn $fqbn --build-path $slaveBuild --libraries $libraries $slaveSketch *> (Join-Path $resultRoot "compile_slave.log")
if ($LASTEXITCODE -ne 0) {
    throw "R_FC10_SLAVE_COMPILE_FAILED"
}

$compileText = (
    [IO.File]::ReadAllText((Join-Path $resultRoot "compile_master.log")) +
    [IO.File]::ReadAllText((Join-Path $resultRoot "compile_slave.log"))
)

$masterSourceObjects = @(
    Get-ChildItem -LiteralPath $masterBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "JWPLC_ModbusRTU.cpp.o" }
)

$slaveSourceObjects = @(
    Get-ChildItem -LiteralPath $slaveBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "JWPLC_ModbusRTU.cpp.o" }
)

$precompiledMarker = [regex]::IsMatch(
    $compileText,
    '(?im)using precompiled library .*JWPLC_ModbusRTU|usando .*precompilad.*JWPLC_ModbusRTU'
)

Write-Host "MASTER_RTU_SOURCE_OBJECT_COUNT=$($masterSourceObjects.Count)"
Write-Host "SLAVE_RTU_SOURCE_OBJECT_COUNT=$($slaveSourceObjects.Count)"
Write-Host "MODBUS_RTU_PRECOMPILED_MARKER=$(if ($precompiledMarker) { 'YES' } else { 'NO' })"

if ($masterSourceObjects.Count -ne 1 -or
    $slaveSourceObjects.Count -ne 1 -or
    $precompiledMarker) {
    throw "R_FC10_SOURCE_FIRST_NOT_PROVEN"
}

& $arduinoCli upload --fqbn $fqbn --port $SlavePort --input-dir $slaveBuild $slaveSketch *> (Join-Path $resultRoot "upload_slave.log")
if ($LASTEXITCODE -ne 0) {
    throw "R_FC10_SLAVE_UPLOAD_FAILED"
}

& $arduinoCli upload --fqbn $fqbn --port $MasterPort --input-dir $masterBuild $masterSketch *> (Join-Path $resultRoot "upload_master.log")
if ($LASTEXITCODE -ne 0) {
    throw "R_FC10_MASTER_UPLOAD_FAILED"
}

Start-Sleep -Seconds 3

$runnerLog = Join-Path $resultRoot "runner.log"

& $PythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --duration $DurationS --output-root $resultRoot *> $runnerLog
$runnerExit = $LASTEXITCODE

Get-Content -LiteralPath $runnerLog -Tail 80 |
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
    throw "R_FC10_QUALIFICATION_FAILED"
}

if ($dirtyFinal.Count -ne 0) {
    throw "R_FC10_TREE_DIRTY_AT_END"
}

if ($stagedFinal.Count -ne 0) {
    throw "R_FC10_INDEX_DIRTY_AT_END"
}

Write-Host "A14_RTU_FC10_GATE=PASS"
Write-Host "RESULT_ROOT=$resultRoot"
