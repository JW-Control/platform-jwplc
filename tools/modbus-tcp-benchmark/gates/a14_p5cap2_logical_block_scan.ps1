param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationPerCaseS = 30.0
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
Write-Host " A14 P5-CAP2 - FINAL LOGICAL BLOCK SCAN"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host ("DURATION_PER_CASE_S={0:F0}" -f $DurationPerCaseS)
Write-Host "LOGICAL_BLOCKS=125,250,500,1000"
Write-Host "FC03_MAX_REGISTERS_PER_REQUEST=125"
Write-Host "EXPECTED_CORE_SHA256=$expectedCoreHash"
Write-Host "FINAL_CAPACITY_TEST=YES"

if ($spiHz -ne 26000000) {
    throw "P5CAP2_EXPECTED_26MHZ"
}

if ($DurationPerCaseS -lt 30.0) {
    throw "P5CAP2_DURATION_PER_CASE_TOO_SHORT"
}

if ($staged.Count -ne 0) {
    throw "P5CAP2_INDEX_NOT_CLEAN"
}

if (
    $dirty.Count -ne 1 -or
    $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative
) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "P5CAP2_EXPECTED_ONLY_DIRTY_CORE_A"
}

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$sdHash = Get-G2Sha256 $script:G2SdRelative

Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "LIBJW_SD_A_SHA256=$sdHash"

if ($coreHash -ne $expectedCoreHash) {
    throw "P5CAP2_UNEXPECTED_CORE_HASH"
}

if ($sdHash -ne $script:G2SdSha256) {
    throw "P5CAP2_SD_HASH_MISMATCH"
}

$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_p5cap2_logical_block_scan.py"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"

foreach ($required in @($masterSketch, $runner, $p5bGate, $arduinoCli)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw ("P5CAP2_REQUIRED_PATH_MISSING={0}" -f $required)
    }
}

$masterText = [System.IO.File]::ReadAllText($masterSketch)
if (-not $masterText.Contains("HOLDING_COUNT = 1000")) {
    throw "P5CAP2_HOLDING_MAP_NOT_1000"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "P5CAP2_PYTHON_NOT_FOUND"
}
$pythonExe = $pythonCommand.Source

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p5cap2_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$syntaxLog = Join-Path $tempRoot "python_syntax.log"
$setupLog = Join-Path $tempRoot "setup.log"
$runLog = Join-Path $tempRoot "logical_blocks.log"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$syntaxExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @("-m", "py_compile", $runner) -LogPath $syntaxLog
Write-Host "P5CAP2_PYTHON_SYNTAX_EXIT=$syntaxExit"
if ($syntaxExit -ne 0) {
    Get-Content -LiteralPath $syntaxLog | ForEach-Object { Write-Host $_ }
    throw "P5CAP2_PYTHON_SYNTAX_FAILED"
}

Write-Host ""
Write-Host "=== COMPILE / UPLOAD FULL RUNTIME WITH 1000-REGISTER MAP ==="

$setupArgs = @(
    "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass",
    "-File", $p5bGate,
    "-MasterPort", $MasterPort,
    "-SlavePort", $SlavePort,
    "-SetupOnly",
    "-AllowDirtyCoreCandidate"
)

$setupExit = Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $setupArgs -LogPath $setupLog
Write-Host "P5CAP2_SETUP_EXIT=$setupExit"
Write-Host "P5CAP2_SETUP_LOG=$setupLog"
Get-Content -LiteralPath $setupLog | ForEach-Object { Write-Host $_ }

if ($setupExit -ne 0) {
    throw "P5CAP2_SETUP_FAILED"
}

$setupText = [System.IO.File]::ReadAllText($setupLog)
$ipMatch = [regex]::Match(
    $setupText,
    "(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$"
)

if (-not $ipMatch.Success) {
    throw "P5CAP2_DUT_IP_MISSING"
}

$dutIp = $ipMatch.Groups[1].Value.Trim()
Write-Host "P5CAP2_DUT_IP=$dutIp"

Write-Host ""
Write-Host "=== RUN LOGICAL BLOCK SCAN ==="

$durationText = $DurationPerCaseS.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$runArgs = @(
    $runner,
    "--master-serial", $MasterPort,
    "--slave-serial", $SlavePort,
    "--host", $dutIp,
    "--port", "502",
    "--duration-per-case", $durationText
)

$runExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runArgs -LogPath $runLog
Write-Host "P5CAP2_RUNNER_EXIT=$runExit"
Write-Host "P5CAP2_RUNNER_LOG=$runLog"
Get-Content -LiteralPath $runLog | ForEach-Object { Write-Host $_ }

if ($runExit -ne 0) {
    throw "P5CAP2_RUN_FAILED"
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
    throw "P5CAP2_TFT_PHYSICAL_REVIEW"
}

$finalCoreHash = Get-G2Sha256 $script:G2CoreRelative
$finalSdHash = Get-G2Sha256 $script:G2SdRelative
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
$finalHz = Get-G2SpiHz

Write-Host ""
Write-Host "=== FINAL CONTROLLED STATE ==="
Write-Host "CORE_A_SHA256=$finalCoreHash"
Write-Host "LIBJW_SD_A_SHA256=$finalSdHash"
Write-Host "FINAL_SPI_HZ=$finalHz"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"

if ($finalCoreHash -ne $expectedCoreHash) {
    throw "P5CAP2_CORE_HASH_CHANGED"
}
if ($finalSdHash -ne $script:G2SdSha256) {
    throw "P5CAP2_SD_HASH_CHANGED"
}
if ($finalHz -ne 26000000) {
    throw "P5CAP2_SPI_CHANGED"
}
if (
    $finalDirty.Count -ne 1 -or
    $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative
) {
    throw "P5CAP2_FINAL_DIRTY_SCOPE_INVALID"
}
if ($finalStaged.Count -ne 0) {
    throw "P5CAP2_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "A14_P5CAP2_FINAL_CAPACITY_TEST=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_THEN_CLOSE_P5"
