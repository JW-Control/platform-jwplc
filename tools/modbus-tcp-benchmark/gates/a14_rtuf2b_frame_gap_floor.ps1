param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationPerCaseS = 30.0
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
Write-Host " A14 RTU-F2B - FRAME GAP FLOOR"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"

$archivePath = Get-G2Path $archiveRelative
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave/a14_p5_rtu_slave.ino"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuf2b_frame_gap_floor.py"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

foreach ($required in @(
    $archivePath,
    $masterSketch,
    $slaveSketch,
    $runner,
    $p5bGate
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw ("RTUF2B_REQUIRED_PATH_MISSING={0}" -f $required)
    }
}

if ($DurationPerCaseS -lt 20.0) {
    throw "RTUF2B_DURATION_TOO_SHORT"
}

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if (
    $dirty.Count -ne 1 -or
    $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative
) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUF2B_EXPECTED_ONLY_DIRTY_CORE_A"
}

if ($staged.Count -ne 0) {
    throw "RTUF2B_INDEX_NOT_CLEAN"
}

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$archiveHashBefore = Get-G2Sha256 $archiveRelative

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "W5500_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_BEFORE=$archiveHashBefore"
Write-Host "RTU_BAUD=115200"
Write-Host "RTU_GAPS_US=1000,750,600,500,400,350,300"
Write-Host "TCP=OFF"
Write-Host "DURATION_PER_CASE_S=$DurationPerCaseS"
Write-Host "PRECOMPILED_RTU_ARCHIVE_FORCE_SOURCE=YES"

if ($coreHash -ne $expectedCoreHash) {
    throw "RTUF2B_UNEXPECTED_CORE_HASH"
}

$masterText = [System.IO.File]::ReadAllText($masterSketch)
$slaveText = [System.IO.File]::ReadAllText($slaveSketch)

foreach ($gap in @(1000, 750, 600, 500, 400, 350, 300)) {
    $masterNeedle = "setRtuFrameGapUs({0}UL)" -f $gap
    $slaveNeedle = "setFrameGapUs({0}UL)" -f $gap

    if (-not $masterText.Contains($masterNeedle)) {
        throw ("RTUF2B_MASTER_GAP_MISSING={0}" -f $gap)
    }

    if (-not $slaveText.Contains($slaveNeedle)) {
        throw ("RTUF2B_SLAVE_GAP_MISSING={0}" -f $gap)
    }
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "RTUF2B_PYTHON_NOT_FOUND"
}

$pythonExe = $pythonCommand.Source
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_rtuf2b_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$archiveBackup = Join-Path $tempRoot "libJWPLC_ModbusRTU.before.a"
$setupLog = Join-Path $tempRoot "setup_source.log"
$runLog = Join-Path $tempRoot "rtuf2b.log"
$syntaxLog = Join-Path $tempRoot "python_syntax.log"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$syntaxExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @("-m", "py_compile", $runner) -LogPath $syntaxLog
Write-Host "RTUF2B_PYTHON_SYNTAX_EXIT=$syntaxExit"

if ($syntaxExit -ne 0) {
    Get-Content -LiteralPath $syntaxLog | ForEach-Object { Write-Host $_ }
    throw "RTUF2B_PYTHON_SYNTAX_FAILED"
}

Copy-Item -LiteralPath $archivePath -Destination $archiveBackup -Force

$archiveHidden = $false

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

    Write-Host "RTUF2B_SETUP_EXIT=$setupExit"
    Write-Host "RTUF2B_SETUP_LOG=$setupLog"
    Get-Content -LiteralPath $setupLog | ForEach-Object { Write-Host $_ }

    if ($setupExit -ne 0) {
        throw "RTUF2B_SETUP_FAILED"
    }

    $setupText = [System.IO.File]::ReadAllText($setupLog)

    $tempRootMatch = [regex]::Match(
        $setupText,
        "(?m)^TEMP_ROOT=(.+?)\r?$"
    )

    if (-not $tempRootMatch.Success) {
        throw "RTUF2B_P5B_TEMP_ROOT_MISSING"
    }

    $p5bTempRoot = $tempRootMatch.Groups[1].Value.Trim()

    foreach ($buildName in @(
        "build_master",
        "build_slave"
    )) {
        $buildPath = Join-Path $p5bTempRoot $buildName

        if (-not (Test-Path -LiteralPath $buildPath)) {
            throw ("RTUF2B_BUILD_PATH_MISSING={0}" -f $buildPath)
        }

        $objects = @(
            Get-ChildItem -LiteralPath $buildPath -Recurse -File |
                Where-Object {
                    $_.Name -like "JWPLC_ModbusRTU.cpp.o*" -or
                    $_.Name -like "JWPLC_ModbusRTU.cpp.obj*"
                }
        )

        if ($objects.Count -lt 1) {
            throw ("RTUF2B_SOURCE_OBJECT_MISSING={0}" -f $buildPath)
        }

        Write-Host ("RTUF2B_SOURCE_OBJECT={0}" -f $objects[0].FullName)
    }

    Write-Host "RTUF2B_SOURCE_COMPILE=PASS"
}
finally {
    if ($archiveHidden) {
        Copy-Item -LiteralPath $archiveBackup -Destination $archivePath -Force
    }
}

$archiveHashAfter = Get-G2Sha256 $archiveRelative
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_AFTER=$archiveHashAfter"

if ($archiveHashAfter -ne $archiveHashBefore) {
    throw "RTUF2B_ARCHIVE_RESTORE_HASH_MISMATCH"
}

$dirtyAfterRestore = @(Get-G2TrackedDirtyPaths)

if (
    $dirtyAfterRestore.Count -ne 1 -or
    $dirtyAfterRestore[0].Replace("\", "/") -ne $script:G2CoreRelative
) {
    $dirtyAfterRestore | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUF2B_DIRTY_SCOPE_AFTER_RESTORE_INVALID"
}

Write-Host ""
Write-Host "=== PHYSICAL RTU-F2B FLOOR SWEEP ==="

$durationText = $DurationPerCaseS.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$runArgs = @(
    "-u",
    $runner,
    "--master-serial", $MasterPort,
    "--slave-serial", $SlavePort,
    "--duration-per-case", $durationText
)

$runExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runArgs -LogPath $runLog

Write-Host "RTUF2B_RUNNER_EXIT=$runExit"
Write-Host "RTUF2B_RUNNER_LOG=$runLog"

Get-Content -LiteralPath $runLog | ForEach-Object { Write-Host $_ }

if ($runExit -ne 0) {
    throw "RTUF2B_RUN_FAILED"
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
    throw "RTUF2B_TFT_PHYSICAL_REVIEW"
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
    throw "RTUF2B_CORE_HASH_CHANGED"
}

if ($finalArchiveHash -ne $archiveHashBefore) {
    throw "RTUF2B_ARCHIVE_CHANGED"
}

if (
    $finalDirty.Count -ne 1 -or
    $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative
) {
    throw "RTUF2B_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "RTUF2B_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "A14_RTU_F2B_FRAME_GAP_FLOOR_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_BEFORE_RTU_F3_TX"
