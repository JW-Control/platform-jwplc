param(
    [string]$MasterPort = "COM14",
    [string]$PythonExe = "C:\\Users\\jeykc\\AppData\\Local\\Programs\\Python\\Python311\\python.exe",
    [double]$DurationS = 300.0,
    [string]$ResultRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-NativeToLog {
    param([string]$FilePath, [string[]]$Arguments, [string]$LogPath)
    $prev = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $FilePath @Arguments *> $LogPath
        return [int]$LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
}

function Require-Token {
    param([string]$Path, [string]$Token, [string]$Label)
    $text = [IO.File]::ReadAllText($Path)
    $pass = $text.Contains($Token)
    Write-Host "$Label=$(if ($pass) { 'PASS' } else { 'FAIL' })"
    if (-not $pass) { throw "RAW_UDP_SOURCE_CONTRACT_FAIL_$Label" }
}

Assert-G2Branch
$repo = $script:G2RepoRoot
$head = Get-G2Head
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $repo diff --cached --name-only)
if ($dirty.Count -ne 0) { $dirty | ForEach-Object { Write-Host "DIRTY=$_" }; throw "RAW_UDP_TRACKED_TREE_NOT_CLEAN" }
if ($staged.Count -ne 0) { throw "RAW_UDP_INDEX_NOT_CLEAN" }
if ($DurationS -lt 300.0) { throw "RAW_UDP_DURATION_LT_300" }
if (-not (Test-Path -LiteralPath $PythonExe)) { throw "RAW_UDP_PYTHON_NOT_FOUND" }

$w5100h = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h"
$spiH = Get-G2Path "JWPLC/2.1.0/libraries/SPI/src/SPI.h"
$ethH = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
Require-Token $w5100h "SPISettings(26000000, MSBFIRST, SPI_MODE0)" "W5500_26MHZ"
Require-Token $w5100h "#define JWPLC_W5500_RX_FIFO_REUSE 1" "FIFO_REUSE_ON"
Require-Token $spiH "#define JWPLC_SPI_FIFO_REUSE_DLEN_CACHE 1" "DLEN_REUSE_ON"
Require-Token $spiH "#define JWPLC_SPI_FIFO_REUSE_COPY_OUT_64 1" "COPY_OUT_64_ON"
Require-Token $ethH "jwplcReadPacketFastDeferred" "UDP_FAST_API_PRESENT"

$arduinoCli = "C:\\Program Files\\Arduino PLC IDE Tools\\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) { throw "RAW_UDP_ARDUINO_CLI_NOT_FOUND" }
$fqbn = "jwplc_local:esp32:jwplcbasic"
$libraries = Get-G2Path "JWPLC/2.1.0/libraries"
$rawSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server"
$fastSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_final_udp_fast_server"
$rawRunner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_final_raw_legacy.py"
$fastRunner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_final_udp_fast_ceiling.py"

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo "tools\modbus-tcp-benchmark\results\a14_final_raw_udp_ceiling_$stamp"
}
New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null
$sessionLog = Join-Path $ResultRoot "SESSION.log"
Start-Transcript -Path $sessionLog -Force | Out-Null

try {
    Write-Host "=============================================================================="
    Write-Host " ALPHA14 FINAL RAW TCP/UDP CEILING"
    Write-Host "=============================================================================="
    Write-Host "BRANCH=$(& git -C $repo branch --show-current)"
    Write-Host "HEAD=$head"
    Write-Host "MASTER_PORT=$MasterPort"
    Write-Host "DURATION_S=$DurationS"
    Write-Host "CASES=TCP_RX,TCP_TX,UDP_RX_LEGACY,UDP_TX,UDP_RX_FAST"
    Write-Host "RESULT_ROOT=$ResultRoot"

    @(
        "DATE=$(Get-Date -Format o)"
        "BRANCH=$(& git -C $repo branch --show-current)"
        "HEAD=$head"
        "MASTER_PORT=$MasterPort"
        "FQBN=$fqbn"
        "W5500_SPI_HZ=$(Get-G2SpiHz)"
        "DURATION_S=$DurationS"
        "CASES=TCP_RX,TCP_TX,UDP_RX_LEGACY,UDP_TX,UDP_RX_FAST"
        "UDP_FAST_PAYLOAD=1016"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "MANIFEST.txt") -Encoding UTF8

    & git -C $repo diff --check *> (Join-Path $ResultRoot "git_diff_check.log")
    if ($LASTEXITCODE -ne 0) { throw "RAW_UDP_GIT_DIFF_CHECK_FAILED" }

    $syntaxLog = Join-Path $ResultRoot "python_syntax.log"
    $syntaxExit = Invoke-NativeToLog $PythonExe @("-m","py_compile",$rawRunner,$fastRunner) $syntaxLog
    if ($syntaxExit -ne 0) { throw "RAW_UDP_PYTHON_SYNTAX_FAILED" }

    $legacyRoot = Join-Path $ResultRoot "F1A_RAW_LEGACY"
    New-Item -ItemType Directory -Force -Path $legacyRoot | Out-Null
    $rawBuild = Join-Path $env:TEMP ("a14_final_raw_" + $stamp)
    New-Item -ItemType Directory -Force -Path $rawBuild | Out-Null

    Write-Host "F1A_COMPILE=START"
    $compileRaw = Invoke-NativeToLog $arduinoCli @("compile","--verbose","--fqbn",$fqbn,"--build-path",$rawBuild,"--libraries",$libraries,$rawSketch) (Join-Path $legacyRoot "compile.log")
    if ($compileRaw -ne 0) { throw "F1A_COMPILE_FAILED" }

    Write-Host "F1A_UPLOAD=START"
    $uploadRaw = Invoke-NativeToLog $arduinoCli @("upload","--fqbn",$fqbn,"--port",$MasterPort,"--input-dir",$rawBuild,$rawSketch) (Join-Path $legacyRoot "upload.log")
    if ($uploadRaw -ne 0) { throw "F1A_UPLOAD_FAILED" }
    Start-Sleep -Seconds 3

    Write-Host "F1A_RUN=START"
    $rawRun = Invoke-NativeToLog $PythonExe @("-u",$rawRunner,"--serial",$MasterPort,"--duration",$DurationS.ToString([Globalization.CultureInfo]::InvariantCulture),"--output-root",$legacyRoot) (Join-Path $legacyRoot "runner.log")
    Get-Content -LiteralPath (Join-Path $legacyRoot "runner.log") -Tail 20 | ForEach-Object { Write-Host $_ }
    if ($rawRun -ne 0) { throw "F1A_RAW_LEGACY_FAILED" }

    $fastRoot = Join-Path $ResultRoot "F1B_UDP_FAST"
    New-Item -ItemType Directory -Force -Path $fastRoot | Out-Null
    $fastBuild = Join-Path $env:TEMP ("a14_final_udp_fast_" + $stamp)
    New-Item -ItemType Directory -Force -Path $fastBuild | Out-Null

    Write-Host "F1B_COMPILE=START"
    $compileFast = Invoke-NativeToLog $arduinoCli @("compile","--verbose","--fqbn",$fqbn,"--build-path",$fastBuild,"--libraries",$libraries,$fastSketch) (Join-Path $fastRoot "compile.log")
    if ($compileFast -ne 0) { throw "F1B_COMPILE_FAILED" }

    Write-Host "F1B_UPLOAD=START"
    $uploadFast = Invoke-NativeToLog $arduinoCli @("upload","--fqbn",$fqbn,"--port",$MasterPort,"--input-dir",$fastBuild,$fastSketch) (Join-Path $fastRoot "upload.log")
    if ($uploadFast -ne 0) { throw "F1B_UPLOAD_FAILED" }
    Start-Sleep -Seconds 3

    Write-Host "F1B_RUN=START"
    $fastRun = Invoke-NativeToLog $PythonExe @("-u",$fastRunner,"--serial",$MasterPort,"--duration",$DurationS.ToString([Globalization.CultureInfo]::InvariantCulture),"--payload","1016","--output-root",$fastRoot) (Join-Path $fastRoot "runner.log")
    Get-Content -LiteralPath (Join-Path $fastRoot "runner.log") -Tail 12 | ForEach-Object { Write-Host $_ }
    if ($fastRun -ne 0) { throw "F1B_UDP_FAST_FAILED" }

    $legacyCsv = Join-Path $legacyRoot "SUMMARY.csv"
    $fastCsv = Join-Path $fastRoot "SUMMARY.csv"
    if (-not (Test-Path -LiteralPath $legacyCsv)) { throw "F1A_SUMMARY_MISSING" }
    if (-not (Test-Path -LiteralPath $fastCsv)) { throw "F1B_SUMMARY_MISSING" }

    $finalHead = Get-G2Head
    $finalDirty = @(Get-G2TrackedDirtyPaths)
    $finalStaged = @(& git -C $repo diff --cached --name-only)
    if ($finalHead -ne $head) { throw "RAW_UDP_HEAD_CHANGED" }
    if ($finalDirty.Count -ne 0) { throw "RAW_UDP_TREE_DIRTY_AT_END" }
    if ($finalStaged.Count -ne 0) { throw "RAW_UDP_INDEX_DIRTY_AT_END" }

    @(
        "A14_FINAL_RAW_UDP_CEILING=PASS_CHARACTERIZED"
        "INITIAL_HEAD=$head"
        "FINAL_HEAD=$finalHead"
        "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
        "STAGED_FINAL=$($finalStaged.Count)"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "FINAL_STATUS.txt") -Encoding UTF8

    Write-Host "A14_FINAL_RAW_UDP_CEILING=PASS_CHARACTERIZED"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
catch {
    @(
        "A14_FINAL_RAW_UDP_CEILING=FAIL"
        "ERROR=$($_.Exception.Message)"
        "INITIAL_HEAD=$head"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "FINAL_STATUS.txt") -Encoding UTF8
    throw
}
finally {
    Stop-Transcript | Out-Null
}