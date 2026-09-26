param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationPerCaseS = 30.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 RTU-H1D - PACKAGE APB POLICY MATRIX"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$rs485Header = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_RS485/src/JWPLC_RS485.h"
$rs485Source = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_RS485/src/JWPLC_RS485.cpp"
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave/a14_p5_rtu_slave.ino"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuh1d_apb_policy_matrix.py"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

foreach ($required in @($rs485Header, $rs485Source, $masterSketch, $slaveSketch, $runner, $p5bGate)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw ("RTUH1D_REQUIRED_PATH_MISSING={0}" -f $required)
    }
}

if ($DurationPerCaseS -lt 20.0) {
    throw "RTUH1D_DURATION_TOO_SHORT"
}

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUH1D_EXPECTED_ONLY_DIRTY_CORE_A"
}
if ($staged.Count -ne 0) {
    throw "RTUH1D_INDEX_NOT_CLEAN"
}

$coreHash = Get-G2Sha256 $script:G2CoreRelative
if ($coreHash -ne $expectedCoreHash) {
    throw "RTUH1D_UNEXPECTED_CORE_HASH"
}

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "W5500_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "CLOCK_POLICY=APB_FORCED"
Write-Host "BAUDS=115200,230400,250000,460800,500000"
Write-Host "FRAME_GAP_US=500"
Write-Host "TCP=OFF"
Write-Host "DURATION_PER_CASE_S=$DurationPerCaseS"

$headerText = [System.IO.File]::ReadAllText($rs485Header)
$sourceText = [System.IO.File]::ReadAllText($rs485Source)
$masterText = [System.IO.File]::ReadAllText($masterSketch)
$slaveText = [System.IO.File]::ReadAllText($slaveSketch)

foreach ($needle in @(
    "JWPLC_RS485_FORCE_APB_CLOCK",
    "CONFIG_IDF_TARGET_ESP32",
    "clockSourceString"
)) {
    if (-not $headerText.Contains($needle)) {
        throw ("RTUH1D_HEADER_CONTRACT_MISSING={0}" -f $needle)
    }
}

foreach ($needle in @(
    "#if JWPLC_RS485_FORCE_APB_CLOCK",
    "setClockSource(UART_CLK_SRC_APB)",
    "JWPLC_RS485_CLOCK_SOURCE_FAILED",
    "APB_FORCED"
)) {
    if (-not $sourceText.Contains($needle)) {
        throw ("RTUH1D_SOURCE_CONTRACT_MISSING={0}" -f $needle)
    }
}

foreach ($pair in @(
    @("MASTER", $masterText),
    @("SLAVE", $slaveText)
)) {
    if (-not $pair[1].Contains("JWPLC_RS485.clockSourceString()")) {
        throw ("RTUH1D_TELEMETRY_CONTRACT_MISSING={0}" -f $pair[0])
    }
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "RTUH1D_PYTHON_NOT_FOUND"
}
$pythonExe = $pythonCommand.Source

& $pythonExe -m py_compile $runner
if ($LASTEXITCODE -ne 0) {
    throw "RTUH1D_PYTHON_SYNTAX_FAILED"
}
Write-Host "RTUH1D_PYTHON_SYNTAX=PASS"

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_rtuh1d_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$setupLog = Join-Path $tempRoot "setup.log"
$runLog = Join-Path $tempRoot "rtuh1d.log"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

Write-Host ""
Write-Host "=== COMPILE / UPLOAD FRESH H1D FIRMWARE ==="
& $p5bGate -MasterPort $MasterPort -SlavePort $SlavePort -SetupOnly -AllowDirtyCoreCandidate *> $setupLog
Get-Content -LiteralPath $setupLog | ForEach-Object { Write-Host $_ }

$setupText = [System.IO.File]::ReadAllText($setupLog)
$tempRootMatch = [regex]::Match($setupText, "(?m)^TEMP_ROOT=(.+?)\r?$")
if (-not $tempRootMatch.Success) {
    throw "RTUH1D_P5B_TEMP_ROOT_MISSING"
}
$p5bTempRoot = $tempRootMatch.Groups[1].Value.Trim()

foreach ($buildName in @("build_master", "build_slave")) {
    $buildPath = Join-Path $p5bTempRoot $buildName
    $objects = @(
        Get-ChildItem -LiteralPath $buildPath -Recurse -File |
        Where-Object { $_.Name -like "JWPLC_RS485.cpp.o*" }
    )
    if ($objects.Count -lt 1) {
        throw ("RTUH1D_RS485_SOURCE_OBJECT_MISSING={0}" -f $buildPath)
    }
    Write-Host ("RTUH1D_RS485_SOURCE_OBJECT={0}" -f $objects[0].FullName)
}
Write-Host "RTUH1D_FRESH_RS485_SOURCE_COMPILE=PASS"

Write-Host ""
Write-Host "=== PHYSICAL RTU-H1D MATRIX ==="
$durationText = $DurationPerCaseS.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & $pythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --duration-per-case $durationText 2>&1 |
        Tee-Object -FilePath $runLog
    $runExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "RTUH1D_RUNNER_EXIT=$runExit"
Write-Host "RTUH1D_RUNNER_LOG=$runLog"
if ($runExit -ne 0) {
    throw "RTUH1D_APB_POLICY_MATRIX_FAILED"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"
$masterAnswer = Read-Host "MASTER COM14 estable y sin parpadeo/cortes visibles? (S/N)"
$slaveAnswer = Read-Host "SLAVE COM4 estable y sin parpadeo/cortes visibles? (S/N)"
$masterPhysical = $masterAnswer.Trim().ToUpper() -eq "S"
$slavePhysical = $slaveAnswer.Trim().ToUpper() -eq "S"
Write-Host "MASTER_TFT_PHYSICAL_PASS=$masterPhysical"
Write-Host "SLAVE_TFT_PHYSICAL_PASS=$slavePhysical"
if (-not ($masterPhysical -and $slavePhysical)) {
    throw "RTUH1D_TFT_PHYSICAL_REVIEW"
}

$finalCoreHash = Get-G2Sha256 $script:G2CoreRelative
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host ""
Write-Host "=== FINAL CONTROLLED STATE ==="
Write-Host "CORE_A_SHA256=$finalCoreHash"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"

if ($finalCoreHash -ne $expectedCoreHash) {
    throw "RTUH1D_CORE_HASH_CHANGED"
}
if ($finalDirty.Count -ne 1 -or $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    throw "RTUH1D_FINAL_DIRTY_SCOPE_INVALID"
}
if ($finalStaged.Count -ne 0) {
    throw "RTUH1D_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "A14_RTU_H1D_APB_POLICY_MATRIX_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_APB_POLICY_ADOPTION_DECISION"
