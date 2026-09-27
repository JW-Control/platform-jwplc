param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationS = 300.0,
    [double]$BucketSeconds = 60.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 RTU-H3E.0 - SERVICE GAP PROFILER"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationS -lt 300.0) { throw "RTUH3E0_DURATION_MUST_BE_AT_LEAST_300S" }
if ($BucketSeconds -le 0.0) { throw "RTUH3E0_BUCKET_SECONDS_INVALID" }

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$expectedArchiveHash = "444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F"
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath = Get-G2Path $archiveRelative
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_h3e0_runtime_profiler_master/a14_h3e0_runtime_profiler_master.ino"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_h3e0_service_gap_profiler.py"
$setupGate = Join-Path $PSScriptRoot "a14_h3e0_physical_profiler_setup.ps1"

foreach ($required in @($archivePath,$masterSketch,$runner,$setupGate)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "RTUH3E0_REQUIRED_PATH_MISSING=$required"
    }
}

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUH3E0_EXPECTED_ONLY_DIRTY_CORE_A"
}
if ($staged.Count -ne 0) {
    throw "RTUH3E0_INDEX_NOT_CLEAN"
}

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$archiveHashBefore = Get-G2Sha256 $archiveRelative

if ($coreHash -ne $expectedCoreHash) {
    throw "RTUH3E0_UNEXPECTED_CORE_HASH"
}
if ($archiveHashBefore -ne $expectedArchiveHash) {
    throw "RTUH3E0_UNEXPECTED_ARCHIVE_HASH"
}

$masterText = [IO.File]::ReadAllText($masterSketch)

$checks = @(
    [PSCustomObject]@{ Label="H3E0_PROFILER_ENABLED"; Pass=$masterText.Contains("H3E_PROFILER=ENABLED") },
    [PSCustomObject]@{ Label="H3E0_WORST_GAP"; Pass=$masterText.Contains("H3E_WORST_GAP_US=") },
    [PSCustomObject]@{ Label="H3E0_WORST_UNACCOUNTED"; Pass=$masterText.Contains("H3E_WORST_UNACCOUNTED_US=") },
    [PSCustomObject]@{ Label="H3E0_PROFILE_RTU"; Pass=$masterText.Contains('h3ePrintProfile("RTU"') },
    [PSCustomObject]@{ Label="H3E0_PROFILE_DISPLAY"; Pass=$masterText.Contains('h3ePrintProfile("DISPLAY"') },
    [PSCustomObject]@{ Label="H3E0_PROFILE_SD_APPEND"; Pass=$masterText.Contains('h3ePrintProfile("SD_APPEND"') },
    [PSCustomObject]@{ Label="H3E0_PROFILE_FRAM"; Pass=$masterText.Contains('h3ePrintProfile("FRAM"') },
    [PSCustomObject]@{ Label="H3E0_PROFILE_SPI"; Pass=$masterText.Contains('h3ePrintProfile("SPI"') },
    [PSCustomObject]@{ Label="H3E0_CRC_DEFAULT_TEST"; Pass=$masterText.Contains("RTU_CRC_MODE=BITWISE") }
)

foreach ($check in $checks) {
    Write-Host "$($check.Label)=$($check.Pass)"
    if (-not $check.Pass) {
        throw "RTUH3E0_SOURCE_CONTRACT_FAILED_$($check.Label)"
    }
}

Write-Host "RTUH3E0_SOURCE_CONTRACT=PASS"

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "RTUH3E0_PYTHON_NOT_FOUND"
}
$pythonExe = $pythonCommand.Source

& $pythonExe -m py_compile $runner
if ($LASTEXITCODE -ne 0) {
    throw "RTUH3E0_PYTHON_SYNTAX_FAILED"
}
Write-Host "RTUH3E0_PYTHON_SYNTAX=PASS"

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "MASTER_RX_FIFO_FULL=9"
Write-Host "SLAVE_RX_FIFO_FULL=8"
Write-Host "CRC_MODE=BITWISE"
Write-Host "DURATION_S=$DurationS"
Write-Host "TCP=500"
Write-Host "RTUH3E0_PREFLIGHT=PASS"

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h3e0_profiler_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$archiveBackup = Join-Path $tempRoot "libJWPLC_ModbusRTU.before.a"
$setupLog = Join-Path $tempRoot "setup.log"
$runLog = Join-Path $tempRoot "h3e0_profiler.log"

Copy-Item -LiteralPath $archivePath -Destination $archiveBackup -Force

$archiveHidden = $false
$dutIp = $null

try {
    Remove-Item -LiteralPath $archivePath -Force
    $archiveHidden = $true

    Write-Host ""
    Write-Host "=== FORCE FRESH SOURCE COMPILE / UPLOAD H3E0 ==="

    & $setupGate -MasterPort $MasterPort -SlavePort $SlavePort -SetupOnly -AllowDirtyCoreCandidate -AllowMissingModbusRtuArchiveCandidate *>&1 |
        Tee-Object -FilePath $setupLog

    $setupText = [IO.File]::ReadAllText($setupLog)
    $ipMatch = [regex]::Match(
        $setupText,
        "(?m)^H3E0SETUP_SETUP_ONLY_MASTER_IP=(.+?)\r?$")

    if (-not $ipMatch.Success) {
        throw "RTUH3E0_SETUP_IP_MISSING"
    }

    $dutIp = $ipMatch.Groups[1].Value.Trim()
}
finally {
    if ($archiveHidden) {
        Copy-Item -LiteralPath $archiveBackup -Destination $archivePath -Force
    }
}

if ((Get-G2Sha256 $archiveRelative) -ne $archiveHashBefore) {
    throw "RTUH3E0_ARCHIVE_RESTORE_HASH_MISMATCH"
}

$durationText = $DurationS.ToString([Globalization.CultureInfo]::InvariantCulture)
$bucketText = $BucketSeconds.ToString([Globalization.CultureInfo]::InvariantCulture)

Write-Host ""
Write-Host "=== PHYSICAL H3E0 PROFILE WINDOW ==="

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & $pythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --host $dutIp --port 502 --duration $durationText --bucket-seconds $bucketText 2>&1 |
        Tee-Object -FilePath $runLog
    $runExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "RTUH3E0_RUNNER_EXIT=$runExit"
Write-Host "RTUH3E0_RUNNER_LOG=$runLog"

if ($runExit -ne 0) {
    throw "RTUH3E0_RUN_FAILED"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"

$masterAnswer = Read-Host "MASTER COM14 estable y sin parpadeo/cortes visibles? (S/N)"
$slaveAnswer = Read-Host "SLAVE COM4 estable y sin parpadeo/cortes visibles? (S/N)"

if ($masterAnswer.Trim().ToUpper() -ne "S" -or
    $slaveAnswer.Trim().ToUpper() -ne "S") {
    throw "RTUH3E0_TFT_PHYSICAL_REVIEW"
}

$finalCoreHash = Get-G2Sha256 $script:G2CoreRelative
$finalArchiveHash = Get-G2Sha256 $archiveRelative
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($finalCoreHash -ne $expectedCoreHash) {
    throw "RTUH3E0_CORE_HASH_CHANGED"
}
if ($finalArchiveHash -ne $archiveHashBefore) {
    throw "RTUH3E0_ARCHIVE_CHANGED"
}
if ($finalDirty.Count -ne 1 -or
    $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    throw "RTUH3E0_FINAL_DIRTY_SCOPE_INVALID"
}
if ($finalStaged.Count -ne 0) {
    throw "RTUH3E0_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "A14_RTU_H3E0_SERVICE_GAP_PROFILER_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_SERVICE_GAP_ATTRIBUTION"
