param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [ValidateSet("ARCHIVE", "SOURCE")]
    [string]$DisplayLinkage = "ARCHIVE",
    [string]$ExpectedDisplayArchiveHash = "2974D42C847C1B7C7AB3A7B74DA42E2F17969FB852B47A8D434F57F70DA924AF",
    [string[]]$AdditionalAllowedDirtyPaths = @(),
    [ValidateSet(40000000, 80000000)]
    [int]$TftSpiHz = 80000000,
    [string]$MasterDirRelative = "tools/modbus-tcp-benchmark/firmware/a14_h3e0b_core_profiler_master",
    [string]$MasterSketchName = "a14_h3e0b_core_profiler_master.ino",
    [string[]]$MasterBuildProperties = @(),
    [switch]$PreflightOnly,
    [double]$DurationS = 300.0,
    [double]$BucketSeconds = 60.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.1B.1 - DISPLAY LINKAGE LEG"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationS -lt 300.0) { throw "H3E1B1_DURATION_MUST_BE_AT_LEAST_300S" }
if ($BucketSeconds -le 0.0) { throw "H3E1B1_BUCKET_SECONDS_INVALID" }

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$expectedArchiveHash = "444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F"
$expectedDisplayArchiveHash = $ExpectedDisplayArchiveHash.Trim().ToUpperInvariant()

if ($expectedDisplayArchiveHash -notmatch '^[0-9A-F]{64}$') {
    throw "H3E1B1_EXPECTED_DISPLAY_HASH_INVALID"
}
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath = Get-G2Path $archiveRelative
$displayArchiveRelative = "JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a"
$displayArchivePath = Get-G2Path $displayArchiveRelative

$coreMain = Get-G2Path "JWPLC/2.1.0/cores/jwcontrol/main.cpp"
$coreHeader = Get-G2Path "JWPLC/2.1.0/cores/jwcontrol/jwplc_h3e0b_profile.h"
$masterDir = Get-G2Path $MasterDirRelative
$masterSketch = Join-Path $masterDir $MasterSketchName
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_h3e0b_core_runtime_attribution.py"
$setupGate = Join-Path $PSScriptRoot "a14_h3e1b1_display_linkage_setup.ps1"

foreach ($required in @($archivePath,$displayArchivePath,$coreMain,$coreHeader,$masterSketch,$runner,$setupGate)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E1B1_REQUIRED_PATH_MISSING=$required"
    }
}

[string[]]$dirty = @(Get-G2TrackedDirtyPaths)
[string[]]$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$expectedDirty = @(
    @($script:G2CoreRelative.Replace("\", "/")) +
    @(
        $AdditionalAllowedDirtyPaths |
            ForEach-Object { $_.Replace("\", "/") }
    )
) | Sort-Object -Unique

[string[]]$normalizedDirty = @(
    $dirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)

if ($normalizedDirty.Count -ne $expectedDirty.Count) {
    $normalizedDirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "H3E1B1_DIRTY_COUNT_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($normalizedDirty[$i] -ne $expectedDirty[$i]) {
        throw "H3E1B1_DIRTY_SCOPE_INVALID"
    }
}
if ($staged.Count -ne 0) { throw "H3E1B1_INDEX_NOT_CLEAN" }

$coreHashBefore = Get-G2Sha256 $script:G2CoreRelative
$archiveHashBefore = Get-G2Sha256 $archiveRelative
$displayArchiveHashBefore = Get-G2Sha256 $displayArchiveRelative

if ($coreHashBefore -ne $expectedCoreHash) { throw "H3E1B1_UNEXPECTED_CORE_HASH" }
if ($archiveHashBefore -ne $expectedArchiveHash) { throw "H3E1B1_UNEXPECTED_RTU_ARCHIVE_HASH" }
if ($displayArchiveHashBefore -ne $expectedDisplayArchiveHash) { throw "H3E1B1_UNEXPECTED_DISPLAY_ARCHIVE_HASH" }

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
    if (-not $check.Pass) { throw "H3E1B1_SOURCE_CONTRACT_FAILED_$($check.Label)" }
}
Write-Host "H3E1B1_SOURCE_CONTRACT=PASS"

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "H3E1B1_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source

$pythonAstScript = @'
import ast
import pathlib
import sys
path = pathlib.Path(sys.argv[1])
source = path.read_text(encoding="utf-8")
ast.parse(source, filename=str(path))
'@

$pythonAstScript | & $pythonExe - $runner
if ($LASTEXITCODE -ne 0) { throw "H3E1B1_PYTHON_SYNTAX_FAILED" }
Write-Host "H3E1B1_PYTHON_SYNTAX=PASS"
Write-Host "H3E1B1_PYTHON_SYNTAX_METHOD=AST_STDIN_NO_PYC"

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "MASTER_RX_FIFO_FULL=9"
Write-Host "SLAVE_RX_FIFO_FULL=8"
Write-Host "CRC_MODE=BITWISE"
Write-Host "DURATION_S=$DurationS"
Write-Host "TCP=500"
Write-Host "CORE_MODE=SOURCE_TEMPORARY_MASTER_ONLY"
Write-Host "PRECOMPILED_CORE_MUTATION=NO"
Write-Host "DISPLAY_LINKAGE=$DisplayLinkage"
Write-Host "DISPLAY_EXPECTED_ARCHIVE_SHA256=$expectedDisplayArchiveHash"
Write-Host "DISPLAY_ADDITIONAL_ALLOWED_DIRTY_COUNT=$($AdditionalAllowedDirtyPaths.Count)"
Write-Host "TFT_SPI_HZ=$TftSpiHz"
Write-Host "MASTER_DIR_RELATIVE=$MasterDirRelative"
Write-Host "MASTER_SKETCH_NAME=$MasterSketchName"
Write-Host "MASTER_BUILD_PROPERTY_COUNT=$($MasterBuildProperties.Count)"

if ($PreflightOnly) {
    [object[]]$setupPreflight = @(
        & $setupGate -MasterPort $MasterPort -SlavePort $SlavePort -DisplayLinkage $DisplayLinkage -ExpectedDisplayArchiveHash $expectedDisplayArchiveHash -AdditionalAllowedDirtyPaths $AdditionalAllowedDirtyPaths -TftSpiHz $TftSpiHz -MasterDirRelative $MasterDirRelative -MasterSketchName $MasterSketchName -MasterBuildProperties $MasterBuildProperties -PreflightOnly -AllowDirtyCoreCandidate *>&1
    )
    $setupPreflight | ForEach-Object { Write-Host $_ }

    $setupPreflightText =
        ($setupPreflight | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

    if (-not $setupPreflightText.Contains("A14_H3E1B1_PREFLIGHT_ONLY=PASS")) {
        throw "H3E1B1_SETUP_PREFLIGHT_CONFIRMATION_MISSING"
    }
    if (-not $setupPreflightText.Contains("DISPLAY_LINKAGE=$DisplayLinkage")) {
        throw "H3E1B1_SETUP_PREFLIGHT_LINKAGE_MISMATCH"
    }

    Write-Host "H3E1B1_PREFLIGHT=PASS"
    Write-Host "PREFLIGHT_COMPILES=NO"
    Write-Host "PREFLIGHT_UPLOADS=NO"
    Write-Output "A14_H3E1B1_LINKAGE_LEG_PREFLIGHT_ONLY=PASS"
    return
}

Write-Host "H3E1B1_PREFLIGHT=PASS"

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h3e1b1_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$setupLog = Join-Path $tempRoot "setup.log"
$runLog = Join-Path $tempRoot "h3e1b1.log"

Write-Host ""
Write-Host "=== H3E1B1 COMPILE / UPLOAD DISPLAY LINKAGE ==="

& $setupGate -MasterPort $MasterPort -SlavePort $SlavePort -DisplayLinkage $DisplayLinkage -ExpectedDisplayArchiveHash $expectedDisplayArchiveHash -AdditionalAllowedDirtyPaths $AdditionalAllowedDirtyPaths -TftSpiHz $TftSpiHz -MasterDirRelative $MasterDirRelative -MasterSketchName $MasterSketchName -MasterBuildProperties $MasterBuildProperties -SetupOnly -AllowDirtyCoreCandidate *>&1 |
    Tee-Object -FilePath $setupLog

$setupText = [IO.File]::ReadAllText($setupLog)
$ipMatch = [regex]::Match(
    $setupText,
    "(?m)^H3E1B1_SETUP_ONLY_MASTER_IP=(.+?)\r?$"
)

if (-not $ipMatch.Success) {
    throw "H3E1B1_SETUP_IP_MISSING"
}
if (-not $setupText.Contains("MASTER_SOURCE_CORE_MAIN_COMPILED=True")) {
    throw "H3E1B1_SOURCE_CORE_CONFIRMATION_MISSING"
}
if (-not $setupText.Contains("H3E1B1_PRECOMPILED_CORE_PRESERVED=YES")) {
    throw "H3E1B1_PRECOMPILED_CORE_PRESERVATION_MISSING"
}
if (-not $setupText.Contains("H3E1B1_MODBUS_RTU_ARCHIVE_RESTORED=YES")) {
    throw "H3E1B1_MODBUS_RTU_ARCHIVE_RESTORE_CONFIRMATION_MISSING"
}
if (-not $setupText.Contains("H3E1B1_INSTALLED_CORE_PRESERVED=YES")) {
    throw "H3E1B1_INSTALLED_CORE_PRESERVATION_MISSING"
}
if (-not $setupText.Contains("H3E1B1_DISPLAY_ARCHIVE_RESTORED=YES")) {
    throw "H3E1B1_DISPLAY_ARCHIVE_RESTORE_CONFIRMATION_MISSING"
}
if (-not $setupText.Contains("H3E1B1_DISPLAY_LINKAGE_FINAL=$DisplayLinkage")) {
    throw "H3E1B1_DISPLAY_LINKAGE_FINAL_MISMATCH"
}

$expectedLinkageProof =
    if ($DisplayLinkage -eq "ARCHIVE") {
        "DISPLAY_LINKAGE_PROOF=ARCHIVE_NO_SOURCE_OBJECTS"
    }
    else {
        "DISPLAY_LINKAGE_PROOF=SOURCE_OBJECTS_PRESENT"
    }

if (-not $setupText.Contains($expectedLinkageProof)) {
    throw "H3E1B1_DISPLAY_LINKAGE_PROOF_MISSING"
}

$dutIp = $ipMatch.Groups[1].Value.Trim()

if ((Get-G2Sha256 $archiveRelative) -ne $archiveHashBefore) {
    throw "H3E1B1_ARCHIVE_CHANGED_AFTER_SETUP"
}
if ((Get-G2Sha256 $script:G2CoreRelative) -ne $coreHashBefore) {
    throw "H3E1B1_PRECOMPILED_CORE_CHANGED_AFTER_SETUP"
}
if ((Get-G2Sha256 $displayArchiveRelative) -ne $displayArchiveHashBefore) {
    throw "H3E1B1_DISPLAY_ARCHIVE_CHANGED_AFTER_SETUP"
}

$durationText = $DurationS.ToString([Globalization.CultureInfo]::InvariantCulture)
$bucketText = $BucketSeconds.ToString([Globalization.CultureInfo]::InvariantCulture)

Write-Host ""
Write-Host "=== H3E1B1 LINKAGE MEASUREMENT WINDOW ==="

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & $pythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --host $dutIp --port 502 --duration $durationText --bucket-seconds $bucketText 2>&1 | Tee-Object -FilePath $runLog
    $runExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "H3E1B1_RUNNER_EXIT=$runExit"
Write-Host "H3E1B1_RUNNER_LOG=$runLog"
Write-Host "H3E1B1_DISPLAY_LINKAGE=$DisplayLinkage"

if ($runExit -ne 0) { throw "H3E1B1_DIAGNOSTIC_CAPTURE_FAILED" }

Write-Host ""
Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"

$masterAnswer = Read-Host "MASTER COM14 estable, misma HMI de referencia y sin parpadeo/cortes visibles? (S/N)"
$slaveAnswer = Read-Host "SLAVE COM4 estable y sin parpadeo/cortes visibles? (S/N)"

if ($masterAnswer.Trim().ToUpper() -ne "S" -or $slaveAnswer.Trim().ToUpper() -ne "S") {
    throw "H3E1B1_TFT_PHYSICAL_REVIEW"
}

$finalCoreHash = Get-G2Sha256 $script:G2CoreRelative
$finalArchiveHash = Get-G2Sha256 $archiveRelative
$finalDisplayArchiveHash = Get-G2Sha256 $displayArchiveRelative
[string[]]$finalDirty = @(Get-G2TrackedDirtyPaths)
[string[]]$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($finalCoreHash -ne $coreHashBefore) { throw "H3E1B1_PRECOMPILED_CORE_CHANGED" }
if ($finalArchiveHash -ne $archiveHashBefore) { throw "H3E1B1_RTU_ARCHIVE_CHANGED" }
if ($finalDisplayArchiveHash -ne $displayArchiveHashBefore) { throw "H3E1B1_DISPLAY_ARCHIVE_CHANGED" }
[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)

if ($normalizedFinalDirty.Count -ne $expectedDirty.Count) {
    throw "H3E1B1_FINAL_DIRTY_COUNT_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($normalizedFinalDirty[$i] -ne $expectedDirty[$i]) {
        throw "H3E1B1_FINAL_DIRTY_SCOPE_INVALID"
    }
}
if ($finalStaged.Count -ne 0) { throw "H3E1B1_FINAL_INDEX_NOT_CLEAN" }

Write-Host "H3E1B1_DISPLAY_LINKAGE_FINAL=$DisplayLinkage"
Write-Host "A14_H3E1B1_DISPLAY_LINKAGE_LEG_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_NEXT_AB_LEG"
