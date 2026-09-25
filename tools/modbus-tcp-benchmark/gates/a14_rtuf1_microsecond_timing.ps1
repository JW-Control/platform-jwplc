param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationPerCaseS = 60.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-NativeToLog {
    param([string]$FilePath, [string[]]$Arguments, [string]$LogPath)

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
Write-Host " A14 RTU-F1 - MICROSECOND TIMING"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath = Get-G2Path $archiveRelative
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave/a14_p5_rtu_slave.ino"
$headerPath = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.h"
$sourcePath = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuf1_microsecond_timing.py"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

foreach ($required in @($archivePath, $masterSketch, $slaveSketch, $headerPath, $sourcePath, $runner, $p5bGate)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw ("RTUF1_REQUIRED_PATH_MISSING={0}" -f $required)
    }
}

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUF1_EXPECTED_ONLY_DIRTY_CORE_A"
}
if ($staged.Count -ne 0) {
    throw "RTUF1_INDEX_NOT_CLEAN"
}

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$archiveHashBefore = Get-G2Sha256 $archiveRelative

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "W5500_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_BEFORE=$archiveHashBefore"
Write-Host "RTU_BAUD=115200"
Write-Host "RTU_FRAME_GAP_US=2000"
Write-Host "TCP_CASES=500,OFF"
Write-Host "PRECOMPILED_RTU_ARCHIVE_FORCE_SOURCE=YES"

if ($coreHash -ne $expectedCoreHash) {
    throw "RTUF1_UNEXPECTED_CORE_HASH"
}

$headerText = [System.IO.File]::ReadAllText($headerPath)
$sourceText = [System.IO.File]::ReadAllText($sourcePath)
$slaveText = [System.IO.File]::ReadAllText($slaveSketch)
$masterText = [System.IO.File]::ReadAllText($masterSketch)

foreach ($needle in @("setFrameGapUs(uint32_t gapUs)", "frameGapUs() const")) {
    if (-not $headerText.Contains($needle)) {
        throw ("RTUF1_HEADER_CONTRACT_MISSING={0}" -f $needle)
    }
}
foreach ($needle in @("_frameGapUs", "_lastByteUs = micros()", "micros() - _lastByteUs")) {
    if (-not $sourceText.Contains($needle)) {
        throw ("RTUF1_SOURCE_CONTRACT_MISSING={0}" -f $needle)
    }
}
if ($sourceText.Contains("_lastByteMs")) {
    throw "RTUF1_OLD_LAST_BYTE_MS_STILL_PRESENT"
}
if (-not $slaveText.Contains("setFrameGapUs(2000UL)")) {
    throw "RTUF1_SLAVE_NEW_API_MISSING"
}
if (-not $masterText.Contains("setFrameGapMs(2)")) {
    throw "RTUF1_MASTER_LEGACY_API_MISSING"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "RTUF1_PYTHON_NOT_FOUND"
}

$pythonExe = $pythonCommand.Source
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_rtuf1_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$archiveBackup = Join-Path $tempRoot "libJWPLC_ModbusRTU.before.a"
$setupLog = Join-Path $tempRoot "setup_source.log"
$runLog = Join-Path $tempRoot "rtuf1.log"
$syntaxLog = Join-Path $tempRoot "python_syntax.log"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$syntaxExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @("-m", "py_compile", $runner) -LogPath $syntaxLog
Write-Host "RTUF1_PYTHON_SYNTAX_EXIT=$syntaxExit"
if ($syntaxExit -ne 0) {
    Get-Content -LiteralPath $syntaxLog | ForEach-Object { Write-Host $_ }
    throw "RTUF1_PYTHON_SYNTAX_FAILED"
}

Copy-Item -LiteralPath $archivePath -Destination $archiveBackup -Force
$archiveHidden = $false
$dutIp = $null

try {
    Remove-Item -LiteralPath $archivePath -Force
    $archiveHidden = $true

    Write-Host ""
    Write-Host "=== FORCE SOURCE COMPILE / UPLOAD ==="

    $setupArgs = @(
        "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass",
        "-File", $p5bGate,
        "-MasterPort", $MasterPort,
        "-SlavePort", $SlavePort,
        "-SetupOnly",
        "-AllowDirtyCoreCandidate",
        "-AllowMissingModbusRtuArchiveCandidate"
    )

    $setupExit = Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $setupArgs -LogPath $setupLog
    Write-Host "RTUF1_SETUP_EXIT=$setupExit"
    Write-Host "RTUF1_SETUP_LOG=$setupLog"
    Get-Content -LiteralPath $setupLog | ForEach-Object { Write-Host $_ }

    if ($setupExit -ne 0) {
        throw "RTUF1_SETUP_FAILED"
    }

    $setupText = [System.IO.File]::ReadAllText($setupLog)
    $masterCompileMatch = [regex]::Match($setupText, "(?m)^MASTER_COMPILE_LOG=(.+?)\r?$")
    $slaveCompileMatch = [regex]::Match($setupText, "(?m)^SLAVE_COMPILE_LOG=(.+?)\r?$")
    $ipMatch = [regex]::Match($setupText, "(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$")

    if (-not $masterCompileMatch.Success -or -not $slaveCompileMatch.Success -or -not $ipMatch.Success) {
        throw "RTUF1_SETUP_METADATA_MISSING"
    }

    $masterCompileLog = $masterCompileMatch.Groups[1].Value.Trim()
    $slaveCompileLog = $slaveCompileMatch.Groups[1].Value.Trim()
    $dutIp = $ipMatch.Groups[1].Value.Trim()

    foreach ($compileLog in @($masterCompileLog, $slaveCompileLog)) {
        if (-not (Test-Path -LiteralPath $compileLog)) {
            throw ("RTUF1_COMPILE_LOG_MISSING={0}" -f $compileLog)
        }
    }

    $tempRootMatch = [regex]::Match(
        $setupText,
        "(?m)^TEMP_ROOT=(.+?)\r?$"
    )

    if (-not $tempRootMatch.Success) {
        throw "RTUF1_P5B_TEMP_ROOT_MISSING"
    }

    $p5bTempRoot = $tempRootMatch.Groups[1].Value.Trim()
    $masterBuild = Join-Path $p5bTempRoot "build_master"
    $slaveBuild = Join-Path $p5bTempRoot "build_slave"

    foreach ($buildPath in @($masterBuild, $slaveBuild)) {
        if (-not (Test-Path -LiteralPath $buildPath)) {
            throw ("RTUF1_BUILD_PATH_MISSING={0}" -f $buildPath)
        }

        $modbusObjects = @(
            Get-ChildItem -LiteralPath $buildPath -Recurse -File |
                Where-Object {
                    $_.Name -like "JWPLC_ModbusRTU.cpp.o*" -or
                    $_.Name -like "JWPLC_ModbusRTU.cpp.obj*"
                }
        )

        if ($modbusObjects.Count -lt 1) {
            throw ("RTUF1_SOURCE_OBJECT_MISSING={0}" -f $buildPath)
        }

        Write-Host ("RTUF1_SOURCE_OBJECT={0}" -f $modbusObjects[0].FullName)
    }

    Write-Host "RTUF1_MASTER_SOURCE_COMPILED=YES"
    Write-Host "RTUF1_SLAVE_SOURCE_COMPILED=YES"
    Write-Host "RTUF1_DUT_IP=$dutIp"
}
finally {
    if ($archiveHidden) {
        Copy-Item -LiteralPath $archiveBackup -Destination $archivePath -Force
    }
}

$archiveHashAfter = Get-G2Sha256 $archiveRelative
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_AFTER=$archiveHashAfter"

if ($archiveHashAfter -ne $archiveHashBefore) {
    throw "RTUF1_ARCHIVE_RESTORE_HASH_MISMATCH"
}

$dirtyAfterRestore = @(Get-G2TrackedDirtyPaths)

if ($dirtyAfterRestore.Count -ne 1 -or $dirtyAfterRestore[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirtyAfterRestore | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUF1_DIRTY_SCOPE_AFTER_RESTORE_INVALID"
}

if ([string]::IsNullOrWhiteSpace($dutIp)) {
    throw "RTUF1_DUT_IP_NOT_RESOLVED"
}

Write-Host ""
Write-Host "=== PHYSICAL RTU-F1 CHARACTERIZATION ==="

$durationText = $DurationPerCaseS.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$runArgs = @(
    "-u", $runner,
    "--master-serial", $MasterPort,
    "--slave-serial", $SlavePort,
    "--host", $dutIp,
    "--port", "502",
    "--duration-per-case", $durationText
)

$runExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runArgs -LogPath $runLog
Write-Host "RTUF1_RUNNER_EXIT=$runExit"
Write-Host "RTUF1_RUNNER_LOG=$runLog"
Get-Content -LiteralPath $runLog | ForEach-Object { Write-Host $_ }

if ($runExit -ne 0) {
    throw "RTUF1_RUN_FAILED"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"

$masterAnswer = Read-Host "¿MASTER COM14 estable y sin parpadeo/cortes visibles? (S/N)"
$slaveAnswer = Read-Host "¿SLAVE COM4 estable y sin parpadeo/cortes visibles? (S/N)"

$masterPhysical = $masterAnswer.Trim().ToUpper() -eq "S"
$slavePhysical = $slaveAnswer.Trim().ToUpper() -eq "S"

Write-Host "MASTER_TFT_PHYSICAL_PASS=$masterPhysical"
Write-Host "SLAVE_TFT_PHYSICAL_PASS=$slavePhysical"

if (-not ($masterPhysical -and $slavePhysical)) {
    throw "RTUF1_TFT_PHYSICAL_REVIEW"
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
    throw "RTUF1_CORE_HASH_CHANGED"
}
if ($finalArchiveHash -ne $archiveHashBefore) {
    throw "RTUF1_ARCHIVE_CHANGED"
}
if ($finalDirty.Count -ne 1 -or $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    throw "RTUF1_FINAL_DIRTY_SCOPE_INVALID"
}
if ($finalStaged.Count -ne 0) {
    throw "RTUF1_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "A14_RTU_F1_MICROSECOND_TIMING_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_BEFORE_RTU_F2"
