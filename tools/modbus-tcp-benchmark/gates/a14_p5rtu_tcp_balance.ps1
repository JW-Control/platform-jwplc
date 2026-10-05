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
Write-Host " A14 P5 - RTU/TCP BALANCE SWEEP"
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
Write-Host "TCP_PROFILE=FC03_125_UNPACED"
Write-Host "RTU_PROFILES=OFF,50,100,150,200,250,300,UNPACED"
Write-Host "RTU_BAUD=115200"
Write-Host "RTU_FRAME_GAP_TEST_MS=2"
Write-Host "EXPECTED_CORE_SHA256=$expectedCoreHash"
Write-Host "PRODUCT_LIBRARY_MUTATION=NO"
Write-Host "BENCHMARK_FIRMWARE_ONLY=YES"

if ($spiHz -ne 26000000) { throw "P5RTUTCP_EXPECTED_26MHZ" }
if ($DurationPerCaseS -lt 30.0) { throw "P5RTUTCP_DURATION_TOO_SHORT" }
if ($staged.Count -ne 0) { throw "P5RTUTCP_INDEX_NOT_CLEAN" }
if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "P5RTUTCP_EXPECTED_ONLY_DIRTY_CORE_A"
}

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$sdHash = Get-G2Sha256 $script:G2SdRelative
Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "LIBJW_SD_A_SHA256=$sdHash"
if ($coreHash -ne $expectedCoreHash) { throw "P5RTUTCP_UNEXPECTED_CORE_HASH" }
if ($sdHash -ne $script:G2SdSha256) { throw "P5RTUTCP_SD_HASH_MISMATCH" }

$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave/a14_p5_rtu_slave.ino"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_p5rtu_tcp_balance.py"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

foreach ($required in @($masterSketch, $slaveSketch, $runner, $p5bGate)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw ("P5RTUTCP_REQUIRED_PATH_MISSING={0}" -f $required)
    }
}

$masterText = [System.IO.File]::ReadAllText($masterSketch)
$slaveText = [System.IO.File]::ReadAllText($slaveSketch)

foreach ($needle in @(
    "setRtuTargetHz(50)", "setRtuTargetHz(100)", "setRtuTargetHz(150)",
    "setRtuTargetHz(200)", "setRtuTargetHz(250)", "setRtuTargetHz(300)",
    "setRtuUnpaced()"
)) {
    if (-not $masterText.Contains($needle)) {
        throw ("P5RTUTCP_MASTER_CONTRACT_MISSING={0}" -f $needle)
    }
}
if (-not $slaveText.Contains("JWPLC_ModbusRTU.setFrameGapMs(2)")) {
    throw "P5RTUTCP_SLAVE_GAP2_COMMAND_MISSING"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "P5RTUTCP_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p5rtutcp_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$syntaxLog = Join-Path $tempRoot "python_syntax.log"
$setupLog = Join-Path $tempRoot "setup.log"
$runLog = Join-Path $tempRoot "balance.log"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$syntaxExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @("-m", "py_compile", $runner) -LogPath $syntaxLog
Write-Host "P5RTUTCP_PYTHON_SYNTAX_EXIT=$syntaxExit"
if ($syntaxExit -ne 0) {
    Get-Content -LiteralPath $syntaxLog | ForEach-Object { Write-Host $_ }
    throw "P5RTUTCP_PYTHON_SYNTAX_FAILED"
}

Write-Host ""
Write-Host "=== COMPILE / UPLOAD FULL RUNTIME ==="

$setupArgs = @(
    "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass",
    "-File", $p5bGate,
    "-MasterPort", $MasterPort,
    "-SlavePort", $SlavePort,
    "-SetupOnly",
    "-AllowDirtyCoreCandidate"
)
$setupExit = Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $setupArgs -LogPath $setupLog
Write-Host "P5RTUTCP_SETUP_EXIT=$setupExit"
Write-Host "P5RTUTCP_SETUP_LOG=$setupLog"
Get-Content -LiteralPath $setupLog | ForEach-Object { Write-Host $_ }
if ($setupExit -ne 0) { throw "P5RTUTCP_SETUP_FAILED" }

$setupText = [System.IO.File]::ReadAllText($setupLog)
$ipMatch = [regex]::Match($setupText, "(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$")
if (-not $ipMatch.Success) { throw "P5RTUTCP_DUT_IP_MISSING" }
$dutIp = $ipMatch.Groups[1].Value.Trim()
Write-Host "P5RTUTCP_DUT_IP=$dutIp"

Write-Host ""
Write-Host "=== RUN RTU/TCP BALANCE SWEEP ==="

$durationText = $DurationPerCaseS.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$runArgs = @(
    $runner,
    "--master-serial", $MasterPort,
    "--slave-serial", $SlavePort,
    "--host", $dutIp,
    "--port", "502",
    "--duration-per-case", $durationText
)
$runExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runArgs -LogPath $runLog
Write-Host "P5RTUTCP_RUNNER_EXIT=$runExit"
Write-Host "P5RTUTCP_RUNNER_LOG=$runLog"
Get-Content -LiteralPath $runLog | ForEach-Object { Write-Host $_ }
if ($runExit -ne 0) { throw "P5RTUTCP_RUN_FAILED" }

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
if (-not ($masterPhysical -and $slavePhysical)) { throw "P5RTUTCP_TFT_PHYSICAL_REVIEW" }

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

if ($finalCoreHash -ne $expectedCoreHash) { throw "P5RTUTCP_CORE_HASH_CHANGED" }
if ($finalSdHash -ne $script:G2SdSha256) { throw "P5RTUTCP_SD_HASH_CHANGED" }
if ($finalHz -ne 26000000) { throw "P5RTUTCP_SPI_CHANGED" }
if ($finalDirty.Count -ne 1 -or $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    throw "P5RTUTCP_FINAL_DIRTY_SCOPE_INVALID"
}
if ($finalStaged.Count -ne 0) { throw "P5RTUTCP_FINAL_INDEX_NOT_CLEAN" }

Write-Host "A14_P5_RTU_TCP_BALANCE_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_BALANCE_DECISION"
