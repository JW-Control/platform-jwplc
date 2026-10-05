param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationPerCaseS = 30.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 RTU-H2 - 500K GAP SWEEP"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath = Get-G2Path $archiveRelative
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuh2_500k_gap_sweep.py"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

foreach ($required in @($archivePath, $runner, $p5bGate)) {
    if (-not (Test-Path -LiteralPath $required)) { throw ("RTUH2_REQUIRED_PATH_MISSING={0}" -f $required) }
}

if ($DurationPerCaseS -lt 20.0) { throw "RTUH2_DURATION_TOO_SHORT" }

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUH2_EXPECTED_ONLY_DIRTY_CORE_A"
}
if ($staged.Count -ne 0) { throw "RTUH2_INDEX_NOT_CLEAN" }

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$archiveHashBefore = Get-G2Sha256 $archiveRelative
Write-Host "HEAD=$(Get-G2Head)"
Write-Host "W5500_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_BEFORE=$archiveHashBefore"
Write-Host "RTU_BAUD=500000"
Write-Host "GAPS_US=500,300,200,150,100,75,50"
Write-Host "RTU_MOTOR=ASYNC"
Write-Host "TX_MODE=QUEUED"
Write-Host "TCP=OFF"
Write-Host "DURATION_PER_CASE_S=$DurationPerCaseS"

if ($coreHash -ne $expectedCoreHash) { throw "RTUH2_UNEXPECTED_CORE_HASH" }

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "RTUH2_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source
& $pythonExe -m py_compile $runner
if ($LASTEXITCODE -ne 0) { throw "RTUH2_PYTHON_SYNTAX_FAILED" }
Write-Host "RTUH2_PYTHON_SYNTAX=PASS"

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_rtuh2_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$archiveBackup = Join-Path $tempRoot "libJWPLC_ModbusRTU.before.a"
$setupLog = Join-Path $tempRoot "setup_source.log"
$runLog = Join-Path $tempRoot "rtuh2.log"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
Copy-Item -LiteralPath $archivePath -Destination $archiveBackup -Force

$archiveHidden = $false
try {
    Remove-Item -LiteralPath $archivePath -Force
    $archiveHidden = $true
    Write-Host ""
    Write-Host "=== FORCE SOURCE COMPILE / UPLOAD ONCE ==="
    & $p5bGate -MasterPort $MasterPort -SlavePort $SlavePort -SetupOnly -AllowDirtyCoreCandidate -AllowMissingModbusRtuArchiveCandidate *> $setupLog
    Get-Content -LiteralPath $setupLog | ForEach-Object { Write-Host $_ }
    $setupText = [System.IO.File]::ReadAllText($setupLog)
    $tempRootMatch = [regex]::Match($setupText, "(?m)^TEMP_ROOT=(.+?)\r?$")
    if (-not $tempRootMatch.Success) { throw "RTUH2_P5B_TEMP_ROOT_MISSING" }
    $p5bTempRoot = $tempRootMatch.Groups[1].Value.Trim()
    foreach ($buildName in @("build_master", "build_slave")) {
        $buildPath = Join-Path $p5bTempRoot $buildName
        foreach ($objectPattern in @("JWPLC_ModbusRTU.cpp.o*", "JWPLC_RS485.cpp.o*")) {
            $objects = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File | Where-Object { $_.Name -like $objectPattern })
            if ($objects.Count -lt 1) { throw ("RTUH2_SOURCE_OBJECT_MISSING={0}:{1}" -f $buildPath, $objectPattern) }
            Write-Host ("RTUH2_SOURCE_OBJECT={0}" -f $objects[0].FullName)
        }
    }
    Write-Host "RTUH2_SOURCE_COMPILE=PASS"
}
finally {
    if ($archiveHidden) { Copy-Item -LiteralPath $archiveBackup -Destination $archivePath -Force }
}

$archiveHashAfter = Get-G2Sha256 $archiveRelative
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_AFTER=$archiveHashAfter"
if ($archiveHashAfter -ne $archiveHashBefore) { throw "RTUH2_ARCHIVE_RESTORE_HASH_MISMATCH" }

Write-Host ""
Write-Host "=== PHYSICAL RTU-H2 SWEEP ==="
$durationText = $DurationPerCaseS.ToString([System.Globalization.CultureInfo]::InvariantCulture)
& $pythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --duration-per-case $durationText 2>&1 | Tee-Object -FilePath $runLog
$runExit = [int]$LASTEXITCODE
Write-Host "RTUH2_RUNNER_EXIT=$runExit"
Write-Host "RTUH2_RUNNER_LOG=$runLog"
if ($runExit -ne 0) { throw "RTUH2_RUN_FAILED" }

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
if (-not ($masterPhysical -and $slavePhysical)) { throw "RTUH2_TFT_PHYSICAL_REVIEW" }

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
if ($finalCoreHash -ne $expectedCoreHash) { throw "RTUH2_CORE_HASH_CHANGED" }
if ($finalArchiveHash -ne $archiveHashBefore) { throw "RTUH2_ARCHIVE_CHANGED" }
if ($finalDirty.Count -ne 1 -or $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) { throw "RTUH2_FINAL_DIRTY_SCOPE_INVALID" }
if ($finalStaged.Count -ne 0) { throw "RTUH2_FINAL_INDEX_NOT_CLEAN" }

Write-Host "A14_RTU_H2_500K_GAP_SWEEP_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_BEFORE_H2B_TCP500_CONFIRMATION"
