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
Write-Host " A14 RTU-H3E.0B - CORE RUNTIME ATTRIBUTION"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationS -lt 300.0) { throw "RTUH3E0B_DURATION_MUST_BE_AT_LEAST_300S" }
if ($BucketSeconds -le 0.0) { throw "RTUH3E0B_BUCKET_SECONDS_INVALID" }

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$expectedArchiveHash = "444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F"
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath = Get-G2Path $archiveRelative

$coreMain = Get-G2Path "JWPLC/2.1.0/cores/jwcontrol/main.cpp"
$coreHeader = Get-G2Path "JWPLC/2.1.0/cores/jwcontrol/jwplc_h3e0b_profile.h"
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_h3e0b_core_profiler_master/a14_h3e0b_core_profiler_master.ino"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_h3e0b_core_runtime_attribution.py"
$setupGate = Join-Path $PSScriptRoot "a14_h3e0b_source_core_profiler_setup.ps1"

foreach ($required in @($archivePath,$coreMain,$coreHeader,$masterSketch,$runner,$setupGate)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "RTUH3E0B_REQUIRED_PATH_MISSING=$required"
    }
}

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUH3E0B_EXPECTED_ONLY_DIRTY_CORE_A"
}
if ($staged.Count -ne 0) { throw "RTUH3E0B_INDEX_NOT_CLEAN" }

$coreHashBefore = Get-G2Sha256 $script:G2CoreRelative
$archiveHashBefore = Get-G2Sha256 $archiveRelative

if ($coreHashBefore -ne $expectedCoreHash) { throw "RTUH3E0B_UNEXPECTED_CORE_HASH" }
if ($archiveHashBefore -ne $expectedArchiveHash) { throw "RTUH3E0B_UNEXPECTED_ARCHIVE_HASH" }

$coreText = [IO.File]::ReadAllText($coreMain)
$headerText = [IO.File]::ReadAllText($coreHeader)
$masterText = [IO.File]::ReadAllText($masterSketch)

$checks = @(
    [PSCustomObject]@{ Label="H3E0B_CORE_ENABLE_HOOK"; Pass=$coreText.Contains("jwplcH3E0BProfilerEnabled") },
    [PSCustomObject]@{ Label="H3E0B_CORE_TCP_PRE"; Pass=$coreText.Contains("g_h3e0b_stats.tcp_pre") },
    [PSCustomObject]@{ Label="H3E0B_CORE_TCP_POST"; Pass=$coreText.Contains("g_h3e0b_stats.tcp_post") },
    [PSCustomObject]@{ Label="H3E0B_CORE_TASK_YIELD"; Pass=$coreText.Contains("g_h3e0b_stats.task_yield") },
    [PSCustomObject]@{ Label="H3E0B_CORE_SYS_IO"; Pass=$coreText.Contains("g_h3e0b_stats.system_io") },
    [PSCustomObject]@{ Label="H3E0B_CORE_SYS_ETH"; Pass=$coreText.Contains("g_h3e0b_stats.system_ethernet") },
    [PSCustomObject]@{ Label="H3E0B_CORE_SYS_DATALOG"; Pass=$coreText.Contains("g_h3e0b_stats.system_datalog") },
    [PSCustomObject]@{ Label="H3E0B_CORE_SYS_DISPLAY"; Pass=$coreText.Contains("g_h3e0b_stats.system_display") },
    [PSCustomObject]@{ Label="H3E0B_HEADER_WORST"; Pass=$headerText.Contains("worst_outside_us") },
    [PSCustomObject]@{ Label="H3E0B_MASTER_ENABLED"; Pass=$masterText.Contains("H3E0B_PROFILER=ENABLED") },
    [PSCustomObject]@{ Label="H3E0B_MASTER_RESET"; Pass=$masterText.Contains("jwplcH3E0BReset") }
)

foreach ($check in $checks) {
    Write-Host "$($check.Label)=$($check.Pass)"
    if (-not $check.Pass) { throw "RTUH3E0B_SOURCE_CONTRACT_FAILED_$($check.Label)" }
}
Write-Host "RTUH3E0B_SOURCE_CONTRACT=PASS"

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "RTUH3E0B_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source

& $pythonExe -m py_compile $runner
if ($LASTEXITCODE -ne 0) { throw "RTUH3E0B_PYTHON_SYNTAX_FAILED" }
Write-Host "RTUH3E0B_PYTHON_SYNTAX=PASS"

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "MASTER_RX_FIFO_FULL=9"
Write-Host "SLAVE_RX_FIFO_FULL=8"
Write-Host "CRC_MODE=BITWISE"
Write-Host "DURATION_S=$DurationS"
Write-Host "TCP=500"
Write-Host "CORE_MODE=SOURCE_TEMPORARY_MASTER_ONLY"
Write-Host "PRECOMPILED_CORE_MUTATION=NO"
Write-Host "RTUH3E0B_PREFLIGHT=PASS"

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h3e0b_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$setupLog = Join-Path $tempRoot "setup.log"
$runLog = Join-Path $tempRoot "h3e0b.log"

Write-Host ""
Write-Host "=== SOURCE CORE COMPILE / UPLOAD H3E0B ==="

& $setupGate -MasterPort $MasterPort -SlavePort $SlavePort -SetupOnly -AllowDirtyCoreCandidate *>&1 |
    Tee-Object -FilePath $setupLog

$setupText = [IO.File]::ReadAllText($setupLog)
$ipMatch = [regex]::Match(
    $setupText,
    "(?m)^H3E0B_SETUP_ONLY_MASTER_IP=(.+?)\r?$"
)

if (-not $ipMatch.Success) {
    throw "RTUH3E0B_SETUP_IP_MISSING"
}
if (-not $setupText.Contains("MASTER_SOURCE_CORE_MAIN_COMPILED=True")) {
    throw "RTUH3E0B_SOURCE_CORE_CONFIRMATION_MISSING"
}
if (-not $setupText.Contains("H3E0B_PRECOMPILED_CORE_PRESERVED=YES")) {
    throw "RTUH3E0B_PRECOMPILED_CORE_PRESERVATION_MISSING"
}
if (-not $setupText.Contains("H3E0B_MODBUS_RTU_ARCHIVE_RESTORED=YES")) {
    throw "RTUH3E0B_MODBUS_RTU_ARCHIVE_RESTORE_CONFIRMATION_MISSING"
}
if (-not $setupText.Contains("H3E0B_INSTALLED_CORE_PRESERVED=YES")) {
    throw "RTUH3E0B_INSTALLED_CORE_PRESERVATION_MISSING"
}

$dutIp = $ipMatch.Groups[1].Value.Trim()

if ((Get-G2Sha256 $archiveRelative) -ne $archiveHashBefore) {
    throw "RTUH3E0B_ARCHIVE_CHANGED_AFTER_SETUP"
}
if ((Get-G2Sha256 $script:G2CoreRelative) -ne $coreHashBefore) {
    throw "RTUH3E0B_PRECOMPILED_CORE_CHANGED_AFTER_SETUP"
}

$durationText = $DurationS.ToString([Globalization.CultureInfo]::InvariantCulture)
$bucketText = $BucketSeconds.ToString([Globalization.CultureInfo]::InvariantCulture)

Write-Host ""
Write-Host "=== H3E0B CORE ATTRIBUTION WINDOW ==="

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & $pythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --host $dutIp --port 502 --duration $durationText --bucket-seconds $bucketText 2>&1 | Tee-Object -FilePath $runLog
    $runExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "RTUH3E0B_RUNNER_EXIT=$runExit"
Write-Host "RTUH3E0B_RUNNER_LOG=$runLog"

if ($runExit -ne 0) { throw "RTUH3E0B_DIAGNOSTIC_CAPTURE_FAILED" }

Write-Host ""
Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"

$masterAnswer = Read-Host "MASTER COM14 estable y sin parpadeo/cortes visibles? (S/N)"
$slaveAnswer = Read-Host "SLAVE COM4 estable y sin parpadeo/cortes visibles? (S/N)"

if ($masterAnswer.Trim().ToUpper() -ne "S" -or $slaveAnswer.Trim().ToUpper() -ne "S") {
    throw "RTUH3E0B_TFT_PHYSICAL_REVIEW"
}

$finalCoreHash = Get-G2Sha256 $script:G2CoreRelative
$finalArchiveHash = Get-G2Sha256 $archiveRelative
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($finalCoreHash -ne $coreHashBefore) { throw "RTUH3E0B_PRECOMPILED_CORE_CHANGED" }
if ($finalArchiveHash -ne $archiveHashBefore) { throw "RTUH3E0B_ARCHIVE_CHANGED" }
if ($finalDirty.Count -ne 1 -or $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) { throw "RTUH3E0B_FINAL_DIRTY_SCOPE_INVALID" }
if ($finalStaged.Count -ne 0) { throw "RTUH3E0B_FINAL_INDEX_NOT_CLEAN" }

Write-Host "A14_RTU_H3E0B_CORE_RUNTIME_ATTRIBUTION_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_20MS_ROOT_CAUSE"
