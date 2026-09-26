param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationPerCaseS = 60.0
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

Write-Host "============================================================"
Write-Host " A14 RTU-F3 - BLOCKING VS QUEUED TX"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"

$archivePath = Get-G2Path $archiveRelative
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave/a14_p5_rtu_slave.ino"
$modbusHeader = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.h"
$rs485Header = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_RS485/src/JWPLC_RS485.h"
$hardwareConfig = Get-G2Path "JWPLC/2.1.0/cores/jwcontrol/jwplc_hardware_config.h"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuf3_queued_tx.py"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

foreach ($required in @(
    $archivePath,
    $masterSketch,
    $slaveSketch,
    $modbusHeader,
    $rs485Header,
    $hardwareConfig,
    $runner,
    $p5bGate
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw ("RTUF3_REQUIRED_PATH_MISSING={0}" -f $required)
    }
}

if ($DurationPerCaseS -lt 30.0) {
    throw "RTUF3_DURATION_TOO_SHORT"
}

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUF3_EXPECTED_ONLY_DIRTY_CORE_A"
}

if ($staged.Count -ne 0) {
    throw "RTUF3_INDEX_NOT_CLEAN"
}

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$archiveHashBefore = Get-G2Sha256 $archiveRelative

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "W5500_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_BEFORE=$archiveHashBefore"
Write-Host "RTU_BAUD=115200"
Write-Host "RTU_FRAME_GAP_US=500"
Write-Host "TX_MODES=BLOCKING,QUEUED"
Write-Host "TCP_CASES_PER_MODE=500,OFF"
Write-Host "DURATION_PER_CASE_S=$DurationPerCaseS"
Write-Host "PRECOMPILED_RTU_ARCHIVE_FORCE_SOURCE=YES"

if ($coreHash -ne $expectedCoreHash) {
    throw "RTUF3_UNEXPECTED_CORE_HASH"
}

$modbusText = [System.IO.File]::ReadAllText($modbusHeader)
$rs485Text = [System.IO.File]::ReadAllText($rs485Header)
$hardwareText = [System.IO.File]::ReadAllText($hardwareConfig)

foreach ($needle in @(
    "setQueuedTxEnabled(bool enabled)",
    "queuedTxEnabled() const",
    "queuedTxActive() const"
)) {
    if (-not $modbusText.Contains($needle)) {
        throw ("RTUF3_MODBUS_CONTRACT_MISSING={0}" -f $needle)
    }
}

foreach ($needle in @(
    "writeQueued(const uint8_t *buffer, size_t size)",
    "queuedWriteSupported() const",
    "txBufferSize() const",
    "autoDirection() const"
)) {
    if (-not $rs485Text.Contains($needle)) {
        throw ("RTUF3_RS485_CONTRACT_MISSING={0}" -f $needle)
    }
}

foreach ($needle in @(
    "#define JWPLC_RS485_AUTO_DIRECTION 1",
    "#define JWPLC_RS485_TX_BUFFER_SIZE 512U"
)) {
    if (-not $hardwareText.Contains($needle)) {
        throw ("RTUF3_HARDWARE_CONTRACT_MISSING={0}" -f $needle)
    }
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue

if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}

if ($null -eq $pythonCommand) {
    throw "RTUF3_PYTHON_NOT_FOUND"
}

$pythonExe = $pythonCommand.Source
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_rtuf3_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$archiveBackup = Join-Path $tempRoot "libJWPLC_ModbusRTU.before.a"
$setupLog = Join-Path $tempRoot "setup_source.log"
$runLog = Join-Path $tempRoot "rtuf3.log"
$syntaxLog = Join-Path $tempRoot "python_syntax.log"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$syntaxExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @("-m", "py_compile", $runner) -LogPath $syntaxLog
Write-Host "RTUF3_PYTHON_SYNTAX_EXIT=$syntaxExit"

if ($syntaxExit -ne 0) {
    Get-Content -LiteralPath $syntaxLog | ForEach-Object { Write-Host $_ }
    throw "RTUF3_PYTHON_SYNTAX_FAILED"
}

Copy-Item -LiteralPath $archivePath -Destination $archiveBackup -Force

$archiveHidden = $false
$dutIp = $null

try {
    Remove-Item -LiteralPath $archivePath -Force
    $archiveHidden = $true

    Write-Host ""
    Write-Host "=== FORCE SOURCE COMPILE / UPLOAD ONCE ==="

    $setupArgs = @(
        "-NoLogo",
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $p5bGate,
        "-MasterPort", $MasterPort,
        "-SlavePort", $SlavePort,
        "-SetupOnly",
        "-AllowDirtyCoreCandidate",
        "-AllowMissingModbusRtuArchiveCandidate"
    )

    $setupExit = Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $setupArgs -LogPath $setupLog

    Write-Host "RTUF3_SETUP_EXIT=$setupExit"
    Write-Host "RTUF3_SETUP_LOG=$setupLog"

    Get-Content -LiteralPath $setupLog | ForEach-Object { Write-Host $_ }

    if ($setupExit -ne 0) {
        throw "RTUF3_SETUP_FAILED"
    }

    $setupText = [System.IO.File]::ReadAllText($setupLog)
    $tempRootMatch = [regex]::Match($setupText, "(?m)^TEMP_ROOT=(.+?)\r?$")
    $ipMatch = [regex]::Match($setupText, "(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$")

    if (-not $tempRootMatch.Success -or -not $ipMatch.Success) {
        throw "RTUF3_SETUP_METADATA_MISSING"
    }

    $p5bTempRoot = $tempRootMatch.Groups[1].Value.Trim()
    $dutIp = $ipMatch.Groups[1].Value.Trim()

    foreach ($buildName in @("build_master", "build_slave")) {
        $buildPath = Join-Path $p5bTempRoot $buildName

        if (-not (Test-Path -LiteralPath $buildPath)) {
            throw ("RTUF3_BUILD_PATH_MISSING={0}" -f $buildPath)
        }

        foreach ($objectPattern in @("JWPLC_ModbusRTU.cpp.o*", "JWPLC_RS485.cpp.o*")) {
            $objects = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File | Where-Object { $_.Name -like $objectPattern })

            if ($objects.Count -lt 1) {
                throw ("RTUF3_SOURCE_OBJECT_MISSING={0}:{1}" -f $buildPath, $objectPattern)
            }

            Write-Host ("RTUF3_SOURCE_OBJECT={0}" -f $objects[0].FullName)
        }
    }

    Write-Host "RTUF3_SOURCE_COMPILE=PASS"
    Write-Host "RTUF3_DUT_IP=$dutIp"
}
finally {
    if ($archiveHidden) {
        Copy-Item -LiteralPath $archiveBackup -Destination $archivePath -Force
    }
}

$archiveHashAfter = Get-G2Sha256 $archiveRelative
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_AFTER=$archiveHashAfter"

if ($archiveHashAfter -ne $archiveHashBefore) {
    throw "RTUF3_ARCHIVE_RESTORE_HASH_MISMATCH"
}

$dirtyAfterRestore = @(Get-G2TrackedDirtyPaths)

if ($dirtyAfterRestore.Count -ne 1 -or $dirtyAfterRestore[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirtyAfterRestore | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUF3_DIRTY_SCOPE_AFTER_RESTORE_INVALID"
}

if ([string]::IsNullOrWhiteSpace($dutIp)) {
    throw "RTUF3_DUT_IP_NOT_RESOLVED"
}

Write-Host ""
Write-Host "=== PHYSICAL RTU-F3 A/B ==="

$durationText = $DurationPerCaseS.ToString([System.Globalization.CultureInfo]::InvariantCulture)

$runArgs = @(
    "-u",
    $runner,
    "--master-serial", $MasterPort,
    "--slave-serial", $SlavePort,
    "--host", $dutIp,
    "--port", "502",
    "--duration-per-case", $durationText
)

$runExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runArgs -LogPath $runLog

Write-Host "RTUF3_RUNNER_EXIT=$runExit"
Write-Host "RTUF3_RUNNER_LOG=$runLog"

Get-Content -LiteralPath $runLog | ForEach-Object { Write-Host $_ }

if ($runExit -ne 0) {
    throw "RTUF3_RUN_FAILED"
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
    throw "RTUF3_TFT_PHYSICAL_REVIEW"
}

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

if ($finalCoreHash -ne $expectedCoreHash) {
    throw "RTUF3_CORE_HASH_CHANGED"
}

if ($finalArchiveHash -ne $archiveHashBefore) {
    throw "RTUF3_ARCHIVE_CHANGED"
}

if ($finalDirty.Count -ne 1 -or $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    throw "RTUF3_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "RTUF3_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "A14_RTU_F3_QUEUED_TX_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_BEFORE_BAUDRATE"
