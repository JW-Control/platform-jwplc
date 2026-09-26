param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$DutIp = "192.168.0.159",
    [double]$DurationPerCaseS = 60.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 RTU-H2B - 500K + TCP500 GAP CONFIRMATION"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$expectedArchiveHash = "444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F"
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuh2b_500k_tcp500_confirmation.py"

if (-not (Test-Path -LiteralPath $runner)) { throw "RTUH2B_RUNNER_MISSING" }
if ($DurationPerCaseS -lt 30.0) { throw "RTUH2B_DURATION_TOO_SHORT" }

$ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
if ($MasterPort -notin $ports) { throw "RTUH2B_MASTER_COM_NOT_PRESENT" }
if ($SlavePort -notin $ports) { throw "RTUH2B_SLAVE_COM_NOT_PRESENT" }

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUH2B_EXPECTED_ONLY_DIRTY_CORE_A"
}
if ($staged.Count -ne 0) { throw "RTUH2B_INDEX_NOT_CLEAN" }

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$archiveHash = Get-G2Sha256 $archiveRelative
if ($coreHash -ne $expectedCoreHash) { throw "RTUH2B_UNEXPECTED_CORE_HASH" }
if ($archiveHash -ne $expectedArchiveHash) { throw "RTUH2B_UNEXPECTED_ARCHIVE_HASH" }

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "RTUH2B_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source

& $pythonExe -m py_compile $runner
if ($LASTEXITCODE -ne 0) { throw "RTUH2B_PYTHON_SYNTAX_FAILED" }

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host "DUT_IP=$DutIp"
Write-Host "DURATION_PER_CASE_S=$DurationPerCaseS"
Write-Host "BAUD=500000"
Write-Host "GAPS_US=500,150,100,75,50"
Write-Host "TCP_TARGET_REQ_S=500"
Write-Host "TCP_FC03_QUANTITY=125"
Write-Host "RTU_MOTOR=ASYNC"
Write-Host "TX_MODE=QUEUED"
Write-Host "COMPILE=NO"
Write-Host "UPLOAD=NO"
Write-Host "EXPECTED_LOADED_FIRMWARE=RTU_H2"
Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256=$archiveHash"

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_rtuh2b_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$runLog = Join-Path $tempRoot "rtuh2b.log"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$durationText = $DurationPerCaseS.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & $pythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --host $DutIp --port 502 --duration-per-case $durationText 2>&1 | Tee-Object -FilePath $runLog
    $runExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "RTUH2B_RUNNER_EXIT=$runExit"
Write-Host "RTUH2B_RUNNER_LOG=$runLog"
if ($runExit -ne 0) { throw "RTUH2B_RUN_FAILED" }

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
if (-not ($masterPhysical -and $slavePhysical)) { throw "RTUH2B_TFT_PHYSICAL_REVIEW" }

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

if ($finalCoreHash -ne $expectedCoreHash) { throw "RTUH2B_CORE_HASH_CHANGED" }
if ($finalArchiveHash -ne $expectedArchiveHash) { throw "RTUH2B_ARCHIVE_CHANGED" }
if ($finalDirty.Count -ne 1 -or $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) { throw "RTUH2B_FINAL_DIRTY_SCOPE_INVALID" }
if ($finalStaged.Count -ne 0) { throw "RTUH2B_FINAL_INDEX_NOT_CLEAN" }

Write-Host "A14_RTU_H2B_TCP500_CONFIRMATION_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_500K_AUTO_GAP_DECISION"
