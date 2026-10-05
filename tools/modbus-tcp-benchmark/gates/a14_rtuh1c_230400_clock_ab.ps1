param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationPerCaseS = 30.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 RTU-H1C - 230400 UART CLOCK A/B"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath = Get-G2Path $archiveRelative
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave/a14_p5_rtu_slave.ino"
$hardwareSerial = Get-G2Path "JWPLC/2.1.0/cores/jwcontrol/HardwareSerial.cpp"
$uartHal = Get-G2Path "JWPLC/2.1.0/cores/jwcontrol/esp32-hal-uart.c"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuh1c_230400_clock_ab.py"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

foreach ($required in @($archivePath, $masterSketch, $slaveSketch, $hardwareSerial, $uartHal, $runner, $p5bGate)) {
    if (-not (Test-Path -LiteralPath $required)) { throw ("RTUH1C_REQUIRED_PATH_MISSING={0}" -f $required) }
}

if ($DurationPerCaseS -lt 20.0) { throw "RTUH1C_DURATION_TOO_SHORT" }

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUH1C_EXPECTED_ONLY_DIRTY_CORE_A"
}
if ($staged.Count -ne 0) { throw "RTUH1C_INDEX_NOT_CLEAN" }

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$archiveHashBefore = Get-G2Sha256 $archiveRelative
if ($coreHash -ne $expectedCoreHash) { throw "RTUH1C_UNEXPECTED_CORE_HASH" }

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "W5500_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_BEFORE=$archiveHashBefore"
Write-Host "BAUD=230400"
Write-Host "FRAME_GAP_US=500"
Write-Host "CLOCK_CASES=AUTO,APB_FORCED"
Write-Host "TCP=OFF"
Write-Host "DURATION_PER_CASE_S=$DurationPerCaseS"
Write-Host "FRESH_FIRMWARE_REQUIRED=YES"

$masterText = [System.IO.File]::ReadAllText($masterSketch)
$slaveText = [System.IO.File]::ReadAllText($slaveSketch)
$hardwareSerialText = [System.IO.File]::ReadAllText($hardwareSerial)
$uartHalText = [System.IO.File]::ReadAllText($uartHal)

foreach ($needle in @("setRtu230400ApbForced", "setClockSource(UART_CLK_SRC_APB)", "RTU_CLOCK_PROFILE=")) {
    if (-not $masterText.Contains($needle)) { throw ("RTUH1C_MASTER_CONTRACT_MISSING={0}" -f $needle) }
    if (-not $slaveText.Contains($needle)) { throw ("RTUH1C_SLAVE_CONTRACT_MISSING={0}" -f $needle) }
}

foreach ($needle in @("must be called before starting UART using begin()", "bool HardwareSerial::setClockSource")) {
    if (-not $hardwareSerialText.Contains($needle)) { throw ("RTUH1C_HWSERIAL_CONTRACT_MISSING={0}" -f $needle) }
}

foreach ($needle in @("#define REF_TICK_BAUDRATE_LIMIT 250000", "baudrate <= REF_TICK_BAUDRATE_LIMIT", "UART_SCLK_REF_TICK", "UART_SCLK_APB")) {
    if (-not $uartHalText.Contains($needle)) { throw ("RTUH1C_UART_CLOCK_CONTRACT_MISSING={0}" -f $needle) }
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "RTUH1C_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source
& $pythonExe -m py_compile $runner
if ($LASTEXITCODE -ne 0) { throw "RTUH1C_PYTHON_SYNTAX_FAILED" }
Write-Host "RTUH1C_PYTHON_SYNTAX=PASS"

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_rtuh1c_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$archiveBackup = Join-Path $tempRoot "libJWPLC_ModbusRTU.before.a"
$setupLog = Join-Path $tempRoot "setup_source.log"
$runLog = Join-Path $tempRoot "rtuh1c.log"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
Copy-Item -LiteralPath $archivePath -Destination $archiveBackup -Force

$archiveHidden = $false
try {
    Remove-Item -LiteralPath $archivePath -Force
    $archiveHidden = $true
    Write-Host ""
    Write-Host "=== FORCE FRESH SOURCE COMPILE / UPLOAD ==="
    & $p5bGate -MasterPort $MasterPort -SlavePort $SlavePort -SetupOnly -AllowDirtyCoreCandidate -AllowMissingModbusRtuArchiveCandidate *> $setupLog
    Get-Content -LiteralPath $setupLog | ForEach-Object { Write-Host $_ }
    $setupText = [System.IO.File]::ReadAllText($setupLog)
    $tempRootMatch = [regex]::Match($setupText, "(?m)^TEMP_ROOT=(.+?)\r?$")
    if (-not $tempRootMatch.Success) { throw "RTUH1C_P5B_TEMP_ROOT_MISSING" }
    $p5bTempRoot = $tempRootMatch.Groups[1].Value.Trim()
    foreach ($buildName in @("build_master", "build_slave")) {
        $buildPath = Join-Path $p5bTempRoot $buildName
        foreach ($objectPattern in @("JWPLC_ModbusRTU.cpp.o*", "JWPLC_RS485.cpp.o*")) {
            $objects = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File | Where-Object { $_.Name -like $objectPattern })
            if ($objects.Count -lt 1) { throw ("RTUH1C_SOURCE_OBJECT_MISSING={0}:{1}" -f $buildPath, $objectPattern) }
            Write-Host ("RTUH1C_SOURCE_OBJECT={0}" -f $objects[0].FullName)
        }
    }
    Write-Host "RTUH1C_FRESH_SOURCE_COMPILE=PASS"
}
finally {
    if ($archiveHidden) { Copy-Item -LiteralPath $archiveBackup -Destination $archivePath -Force }
}

$archiveHashAfter = Get-G2Sha256 $archiveRelative
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_AFTER=$archiveHashAfter"
if ($archiveHashAfter -ne $archiveHashBefore) { throw "RTUH1C_ARCHIVE_RESTORE_HASH_MISMATCH" }

Write-Host ""
Write-Host "=== PHYSICAL RTU-H1C A/B ==="
$durationText = $DurationPerCaseS.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & $pythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --duration-per-case $durationText 2>&1 | Tee-Object -FilePath $runLog
    $runExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "RTUH1C_RUNNER_EXIT=$runExit"
Write-Host "RTUH1C_RUNNER_LOG=$runLog"
if ($runExit -ne 0) { throw "RTUH1C_CAUSAL_TEST_INCONCLUSIVE" }

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
if (-not ($masterPhysical -and $slavePhysical)) { throw "RTUH1C_TFT_PHYSICAL_REVIEW" }

$finalCoreHash = Get-G2Sha256 $script:G2CoreRelative
$finalArchiveHash = Get-G2Sha256 $archiveRelative
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
Write-Host ""
Write-Host "=== FINAL CONTROLLED STATE ==="
Write-Host "CORE_A_SHA256=$finalCoreHash"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256=$finalArchiveHash"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"
if ($finalCoreHash -ne $expectedCoreHash) { throw "RTUH1C_CORE_HASH_CHANGED" }
if ($finalArchiveHash -ne $archiveHashBefore) { throw "RTUH1C_ARCHIVE_CHANGED" }
if ($finalDirty.Count -ne 1 -or $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) { throw "RTUH1C_FINAL_DIRTY_SCOPE_INVALID" }
if ($finalStaged.Count -ne 0) { throw "RTUH1C_FINAL_INDEX_NOT_CLEAN" }

Write-Host "A14_RTU_H1C_230400_CLOCK_AB_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_CLOCK_POLICY_DECISION"
