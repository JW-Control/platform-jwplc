param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$PythonExe = "C:\\Users\\jeykc\\AppData\\Local\\Programs\\Python\\Python311\\python.exe",
    [double]$FullDurationS = 600.0,
    [double]$SweepDurationS = 300.0,
    [double]$ProfileDurationS = 300.0,
    [string]$ResultRoot = ""
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

function Require-Token {
    param([string]$Path, [string]$Token, [string]$Label)
    $text = [IO.File]::ReadAllText($Path)
    $pass = $text.Contains($Token)
    Write-Host "$Label=$(if ($pass) { 'PASS' } else { 'FAIL' })"
    if (-not $pass) { throw "FINAL_CAMPAIGN_SOURCE_CONTRACT_$Label" }
}

Assert-G2Branch

$head = Get-G2Head
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "FINAL_CAMPAIGN_TRACKED_TREE_NOT_CLEAN"
}
if ($staged.Count -ne 0) { throw "FINAL_CAMPAIGN_INDEX_NOT_CLEAN" }
if ($FullDurationS -lt 600.0) { throw "FINAL_CAMPAIGN_FULL_DURATION_LT_600" }
if ($SweepDurationS -lt 300.0) { throw "FINAL_CAMPAIGN_SWEEP_DURATION_LT_300" }
if ($ProfileDurationS -lt 300.0) { throw "FINAL_CAMPAIGN_PROFILE_DURATION_LT_300" }
if (-not (Test-Path -LiteralPath $PythonExe)) { throw "FINAL_CAMPAIGN_PYTHON_NOT_FOUND" }

if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $ResultRoot = Join-Path $script:G2RepoRoot "tools\modbus-tcp-benchmark\results\a14_final_capability_$stamp"
}
New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$sessionLog = Join-Path $ResultRoot "SESSION.log"
Start-Transcript -Path $sessionLog -Force | Out-Null

try {
    Write-Host "=============================================================================="
    Write-Host " ALPHA14 - FINAL CAPABILITY OVERNIGHT CAMPAIGN"
    Write-Host "=============================================================================="
    Write-Host "HEAD=$head"
    Write-Host "MASTER_PORT=$MasterPort"
    Write-Host "SLAVE_PORT=$SlavePort"
    Write-Host "FULL_DURATION_S=$FullDurationS"
    Write-Host "SWEEP_DURATION_S=$SweepDurationS"
    Write-Host "PROFILE_DURATION_S=$ProfileDurationS"
    Write-Host "RESULT_ROOT=$ResultRoot"
    Write-Host "PERFORMANCE_SERIAL_POLICY=QUIET_DURING_WINDOW"
    Write-Host "PROFILE_SERIAL_POLICY=QUIET_DURING_WINDOW_FINAL_SNAPSHOT_ONLY"
    Write-Host "TCP_POLICY=C0_POLLING"
    Write-Host "INT_POLICY=OFF"

    $manifestDir = Join-Path $ResultRoot "00_preflight"
    New-Item -ItemType Directory -Force -Path $manifestDir | Out-Null

    $manifest = Join-Path $manifestDir "MANIFEST.txt"
    @(
        "DATE=$(Get-Date -Format o)"
        "BRANCH=$(& git -C $script:G2RepoRoot branch --show-current)"
        "HEAD=$head"
        "W5500_SPI_HZ=$(Get-G2SpiHz)"
        "FULL_DURATION_S=$FullDurationS"
        "SWEEP_DURATION_S=$SweepDurationS"
        "PROFILE_DURATION_S=$ProfileDurationS"
        "MASTER_PORT=$MasterPort"
        "SLAVE_PORT=$SlavePort"
        "TCP_POLICY=C0_POLLING"
        "INT_V2=NOT_PROMOTED"
        "RTU_CAPACITY_PROFILE=FC03_ONLY_NOT_UNIVERSAL_DEFAULT"
    ) | Set-Content -LiteralPath $manifest -Encoding UTF8

    & git -C $script:G2RepoRoot diff --check *> (Join-Path $manifestDir "git_diff_check.log")
    if ($LASTEXITCODE -ne 0) { throw "FINAL_CAMPAIGN_GIT_DIFF_CHECK_FAILED" }

    $w5100h = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h"
    $tcpH = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h"
    Require-Token $w5100h "#define JWPLC_W5500_RX_FIFO_REUSE 1" "FIFO_REUSE_DEFAULT_ON"
    Require-Token $w5100h "SPISettings(26000000, MSBFIRST, SPI_MODE0)" "W5500_26MHZ"
    Require-Token $tcpH "#define JWPLC_MODBUS_TCP_INT_GUIDED_RX 0" "INT_DEFAULT_OFF"
    Require-Token $tcpH "#define JWPLC_MODBUS_TCP_INT_HOT_POLL_US 0UL" "D2_DEFAULT_OFF"
    Require-Token $tcpH "#define JWPLC_MODBUS_TCP_INT_LOAD_ADAPTIVE 0" "D3_DEFAULT_OFF"
    Require-Token $tcpH "#define JWPLC_MODBUS_TCP_INT_RSR_DRAIN 0" "E1_DEFAULT_OFF"

    $arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
    if (-not (Test-Path -LiteralPath $arduinoCli)) { throw "FINAL_CAMPAIGN_ARDUINO_CLI_NOT_FOUND" }
    $fqbn = "jwplc_local:esp32:jwplcbasic"
    $repoLibraries = Get-G2Path "JWPLC/2.1.0/libraries"

    # ------------------------------------------------------------------
    # F1A: RAW legacy TCP RX/TX + UDP RX/TX, one 5 minute case each.
    # ------------------------------------------------------------------
    $f1Legacy = Join-Path $ResultRoot "F1_raw_legacy"
    New-Item -ItemType Directory -Force -Path $f1Legacy | Out-Null
    $rawDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server"
    $rawBuild = Join-Path $f1Legacy "build"
    New-Item -ItemType Directory -Force -Path $rawBuild | Out-Null

    Write-Host "F1A=COMPILE_UPLOAD_RAW_LEGACY"
    $compileLegacy = Invoke-NativeToLog $arduinoCli @(
        "compile","--verbose","--fqbn",$fqbn,
        "--build-path",$rawBuild,
        "--libraries",$repoLibraries,
        $rawDir
    ) (Join-Path $f1Legacy "compile.log")
    if ($compileLegacy -ne 0) { throw "F1A_COMPILE_FAILED" }

    $uploadLegacy = Invoke-NativeToLog $arduinoCli @(
        "upload","--fqbn",$fqbn,"--port",$MasterPort,
        "--input-dir",$rawBuild,$rawDir
    ) (Join-Path $f1Legacy "upload.log")
    if ($uploadLegacy -ne 0) { throw "F1A_UPLOAD_FAILED" }
    Start-Sleep -Seconds 3

    $rawRunner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_final_raw_legacy.py"
    $rawRun = Invoke-NativeToLog $PythonExe @(
        "-u",$rawRunner,
        "--serial",$MasterPort,
        "--duration",$SweepDurationS.ToString([Globalization.CultureInfo]::InvariantCulture),
        "--output-root",$f1Legacy
    ) (Join-Path $f1Legacy "runner.log")
    if ($rawRun -ne 0) { throw "F1A_RAW_LEGACY_FAILED" }
    Get-Content -LiteralPath (Join-Path $f1Legacy "runner.log") -Tail 12 | ForEach-Object { Write-Host $_ }

    # ------------------------------------------------------------------
    # F1B: product UDP FAST API, separate 5 minute measurement.
    # ------------------------------------------------------------------
    $f1Fast = Join-Path $ResultRoot "F1_udp_fast"
    New-Item -ItemType Directory -Force -Path $f1Fast | Out-Null
    $fastDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_udp_fast_server"
    $fastBuild = Join-Path $f1Fast "build"
    New-Item -ItemType Directory -Force -Path $fastBuild | Out-Null

    Write-Host "F1B=COMPILE_UPLOAD_UDP_FAST"
    $compileFast = Invoke-NativeToLog $arduinoCli @(
        "compile","--verbose","--fqbn",$fqbn,
        "--build-path",$fastBuild,
        "--libraries",$repoLibraries,
        $fastDir
    ) (Join-Path $f1Fast "compile.log")
    if ($compileFast -ne 0) { throw "F1B_COMPILE_FAILED" }

    $uploadFast = Invoke-NativeToLog $arduinoCli @(
        "upload","--fqbn",$fqbn,"--port",$MasterPort,
        "--input-dir",$fastBuild,$fastDir
    ) (Join-Path $f1Fast "upload.log")
    if ($uploadFast -ne 0) { throw "F1B_UPLOAD_FAILED" }
    Start-Sleep -Seconds 3

    $fastRunner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_final_udp_fast_ceiling.py"
    $fastRun = Invoke-NativeToLog $PythonExe @(
        "-u",$fastRunner,
        "--serial",$MasterPort,
        "--duration",$SweepDurationS.ToString([Globalization.CultureInfo]::InvariantCulture),
        "--output-root",$f1Fast
    ) (Join-Path $f1Fast "runner.log")
    if ($fastRun -ne 0) { throw "F1B_UDP_FAST_FAILED" }
    Get-Content -LiteralPath (Join-Path $f1Fast "runner.log") -Tail 8 | ForEach-Object { Write-Host $_ }

    # ------------------------------------------------------------------
    # Full-runtime setup once. Subsequent gates only reconfigure via Serial.
    # ------------------------------------------------------------------
    $setupDir = Join-Path $ResultRoot "FULL_RUNTIME_SETUP"
    New-Item -ItemType Directory -Force -Path $setupDir | Out-Null
    $p5b = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"
    $setupLog = Join-Path $setupDir "setup.log"

    Write-Host "FULL_RUNTIME=COMPILE_UPLOAD_ONCE"
    $setupExit = Invoke-NativeToLog "powershell.exe" @(
        "-NoLogo","-NoProfile","-ExecutionPolicy","Bypass",
        "-File",$p5b,
        "-MasterPort",$MasterPort,
        "-SlavePort",$SlavePort,
        "-PythonExe",$PythonExe,
        "-SetupOnly"
    ) $setupLog
    if ($setupExit -ne 0) { throw "FINAL_CAMPAIGN_FULL_RUNTIME_SETUP_FAILED" }

    $setupText = [IO.File]::ReadAllText($setupLog)
    $ipMatch = [regex]::Match($setupText, "(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$")
    if (-not $ipMatch.Success) { throw "FINAL_CAMPAIGN_DUT_IP_MISSING" }
    $dutIp = $ipMatch.Groups[1].Value.Trim()
    Write-Host "FULL_RUNTIME_DUT_IP=$dutIp"

    $tempMatch = [regex]::Match($setupText, "(?m)^TEMP_ROOT=(.+?)\r?$")
    if ($tempMatch.Success -and (Test-Path -LiteralPath $tempMatch.Groups[1].Value.Trim())) {
        Copy-Item -LiteralPath $tempMatch.Groups[1].Value.Trim() -Destination (Join-Path $setupDir "p5b_artifacts") -Recurse -Force
    }

    # ------------------------------------------------------------------
    # F2..F8 final full-runtime campaign.
    # ------------------------------------------------------------------
    $fullRoot = Join-Path $ResultRoot "FULL_RUNTIME"
    New-Item -ItemType Directory -Force -Path $fullRoot | Out-Null
    $fullRunner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_final_full_runtime_campaign.py"
    $fullLog = Join-Path $fullRoot "campaign.log"

    Write-Host "FULL_RUNTIME_CAMPAIGN=START"
    $fullExit = Invoke-NativeToLog $PythonExe @(
        "-u",$fullRunner,
        "--master-serial",$MasterPort,
        "--slave-serial",$SlavePort,
        "--host",$dutIp,
        "--output-root",$fullRoot,
        "--full-duration",$FullDurationS.ToString([Globalization.CultureInfo]::InvariantCulture),
        "--sweep-duration",$SweepDurationS.ToString([Globalization.CultureInfo]::InvariantCulture),
        "--profile-duration",$ProfileDurationS.ToString([Globalization.CultureInfo]::InvariantCulture)
    ) $fullLog

    Get-Content -LiteralPath $fullLog -Tail 40 | ForEach-Object { Write-Host $_ }
    if ($fullExit -ne 0) { throw "FINAL_CAMPAIGN_FULL_RUNTIME_FAILED" }

    $finalHead = Get-G2Head
    $finalDirty = @(Get-G2TrackedDirtyPaths)
    $finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

    @(
        "A14_FINAL_CAPABILITY_OVERNIGHT=PASS_CHARACTERIZED"
        "INITIAL_HEAD=$head"
        "FINAL_HEAD=$finalHead"
        "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
        "STAGED_FINAL=$($finalStaged.Count)"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "FINAL_STATUS.txt") -Encoding UTF8

    if ($finalHead -ne $head) { throw "FINAL_CAMPAIGN_HEAD_CHANGED" }
    if ($finalDirty.Count -ne 0) { throw "FINAL_CAMPAIGN_TREE_DIRTY_AT_END" }
    if ($finalStaged.Count -ne 0) { throw "FINAL_CAMPAIGN_INDEX_DIRTY_AT_END" }

    Write-Host "A14_FINAL_CAPABILITY_OVERNIGHT=PASS_CHARACTERIZED"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
catch {
    @(
        "A14_FINAL_CAPABILITY_OVERNIGHT=FAIL"
        "ERROR=$($_.Exception.Message)"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "FINAL_STATUS.txt") -Encoding UTF8
    throw
}
finally {
    Stop-Transcript | Out-Null
}
