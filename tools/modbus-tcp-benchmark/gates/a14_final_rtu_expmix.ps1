param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$PythonExe = "C:\\Users\\jeykc\\AppData\\Local\\Programs\\Python\\Python311\\python.exe",
    [double]$DurationS = 300.0
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
    throw "R3_EXPMIX_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    throw "R3_EXPMIX_INDEX_NOT_CLEAN"
}

if ($DurationS -lt 300.0) {
    throw "R3_EXPMIX_DURATION_LT_300"
}

if (-not (Test-Path -LiteralPath $PythonExe)) {
    throw "R3_EXPMIX_PYTHON_NOT_FOUND"
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultRoot = Join-Path $repo "tools\modbus-tcp-benchmark\results\a14_rtu_expmix_$stamp"
New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "R3_EXPMIX_ARDUINO_CLI_NOT_FOUND"
}

$fqbn = "jwplc_local:esp32:jwplcbasic"
$libraries = Get-G2Path "JWPLC/2.1.0/libraries"

$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_rtu_expmix_master"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_rtu_expmix_slave"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_final_rtu_expmix_qualification.py"

$masterBuild = Join-Path $env:TEMP ("a14_expmix_master_" + $stamp)
$slaveBuild = Join-Path $env:TEMP ("a14_expmix_slave_" + $stamp)
New-Item -ItemType Directory -Force -Path $masterBuild | Out-Null
New-Item -ItemType Directory -Force -Path $slaveBuild | Out-Null

Write-Host "=============================================================================="
Write-Host " ALPHA14 FINAL RTU - R3 EXP-MIX"
Write-Host "=============================================================================="
Write-Host "HEAD=$head"
Write-Host "DURATION_PER_PATTERN_S=$DurationS"
Write-Host "PATTERN_A=3DI_3DO_1AI_1AO"
Write-Host "PATTERN_B=2DI_2DO_2AI_2AO"
Write-Host "TRANSACTIONS_PER_SCAN=8"
Write-Host "PROFILE=500K_FIFO9_8_BULK_QUEUED_STRUCTURAL_SLAVE"
Write-Host "TCP=OFF"
Write-Host "SERIAL_POLICY=QUIET_DURING_WINDOW"

# Syntax check in memory; avoid __pycache__.
$escapedRunner = $runner.Replace("'", "''")
$syntaxCode = "import ast,pathlib; ast.parse(pathlib.Path(r'$escapedRunner').read_text(encoding='utf-8'))"
& $PythonExe -c $syntaxCode
if ($LASTEXITCODE -ne 0) {
    throw "R3_EXPMIX_PYTHON_SYNTAX_FAILED"
}

& $arduinoCli compile --verbose --fqbn $fqbn --build-path $masterBuild --libraries $libraries $masterSketch *> (Join-Path $resultRoot "compile_master.log")
if ($LASTEXITCODE -ne 0) {
    throw "R3_EXPMIX_MASTER_COMPILE_FAILED"
}

& $arduinoCli compile --verbose --fqbn $fqbn --build-path $slaveBuild --libraries $libraries $slaveSketch *> (Join-Path $resultRoot "compile_slave.log")
if ($LASTEXITCODE -ne 0) {
    throw "R3_EXPMIX_SLAVE_COMPILE_FAILED"
}

$masterSourceObjects = @(
    Get-ChildItem -LiteralPath $masterBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "JWPLC_ModbusRTU.cpp.o" }
)

$slaveSourceObjects = @(
    Get-ChildItem -LiteralPath $slaveBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "JWPLC_ModbusRTU.cpp.o" }
)

$compileText = (
    [IO.File]::ReadAllText((Join-Path $resultRoot "compile_master.log")) +
    [IO.File]::ReadAllText((Join-Path $resultRoot "compile_slave.log"))
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
    throw "R3_EXPMIX_SOURCE_FIRST_NOT_PROVEN"
}

& $arduinoCli upload --fqbn $fqbn --port $SlavePort --input-dir $slaveBuild $slaveSketch *> (Join-Path $resultRoot "upload_slave.log")
if ($LASTEXITCODE -ne 0) {
    throw "R3_EXPMIX_SLAVE_UPLOAD_FAILED"
}

& $arduinoCli upload --fqbn $fqbn --port $MasterPort --input-dir $masterBuild $masterSketch *> (Join-Path $resultRoot "upload_master.log")
if ($LASTEXITCODE -ne 0) {
    throw "R3_EXPMIX_MASTER_UPLOAD_FAILED"
}

Start-Sleep -Seconds 3

$runnerLog = Join-Path $resultRoot "runner.log"

& $PythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --duration $DurationS --output-root $resultRoot *> $runnerLog
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
    throw "R3_EXPMIX_QUALIFICATION_FAILED"
}

if ($dirtyFinal.Count -ne 0) {
    throw "R3_EXPMIX_TREE_DIRTY_AT_END"
}

if ($stagedFinal.Count -ne 0) {
    throw "R3_EXPMIX_INDEX_DIRTY_AT_END"
}

Write-Host "A14_RTU_EXPMIX_GATE=PASS"
Write-Host "RESULT_ROOT=$resultRoot"
