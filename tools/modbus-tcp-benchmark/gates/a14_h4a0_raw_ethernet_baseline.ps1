param(
    [string]$Port = "COM14",
    [double]$DurationPerCaseS = 15.0,
    [int]$Repetitions = 3,
    [int]$TcpChunk = 4096,
    [int]$UdpPayload = 1472
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

function Assert-ExactSha256 {
    param(
        [string]$RelativePath,
        [string]$Expected,
        [string]$Label
    )

    $actual = Get-G2Sha256 $RelativePath
    Write-Host "$Label=$actual"

    if ($actual -ne $Expected) {
        throw ($Label + "_MISMATCH")
    }
}

Write-Host "============================================================"
Write-Host " A14 H4A0 - POST-H3E RAW ETHERNET CEILING BASELINE"
Write-Host " TCP RX/TX + UDP RX/TX"
Write-Host "============================================================"

Assert-G2Branch

$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
$untracked = @(& git -C $script:G2RepoRoot ls-files --others --exclude-standard)

if ($LASTEXITCODE -ne 0) {
    throw "H4A0_GIT_QUERY_FAILED"
}

$productUntracked = @(
    $untracked |
        ForEach-Object { $_.Replace("\", "/") } |
        Where-Object {
            $_.StartsWith("JWPLC/2.1.0/") -or
            $_.StartsWith("tools/modbus-tcp-benchmark/firmware/")
        }
)

Write-Host "HEAD=$head"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "UNTRACKED_COUNT=$($untracked.Count)"
Write-Host "UNTRACKED_PRODUCT_COUNT=$($productUntracked.Count)"
Write-Host "PORT=$Port"
Write-Host ("DURATION_PER_CASE_S={0:F0}" -f $DurationPerCaseS)
Write-Host "REPETITIONS=$Repetitions"
Write-Host "TCP_CHUNK=$TcpChunk"
Write-Host "UDP_PAYLOAD=$UdpPayload"
Write-Host "W5500_TARGET_HZ=26000000"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"
Write-Host "RTU_TRAFFIC=OFF"
Write-Host "AUTOLOAD_NORMAL=YES"
Write-Host "DISPLAY_STACK=JWPLC_Display+JWPLC_TFT"
Write-Host "EXTERNAL_TFT_ESPI=NO"

if ($spiHz -ne 26000000) {
    throw "H4A0_EXPECTED_26MHZ"
}

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "H4A0_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "H4A0_INDEX_NOT_CLEAN"
}

if ($productUntracked.Count -ne 0) {
    $productUntracked | ForEach-Object { Write-Host "UNTRACKED_PRODUCT=$_" }
    throw "H4A0_UNTRACKED_PRODUCT_SOURCE_FOUND"
}

Assert-G2ProtectedArtifacts

Assert-ExactSha256 -RelativePath "JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a" -Expected "52B9BC617FACB77705161B4F07E6D45571043E4473934EFE19A1F5444BB5D986" -Label "H4A0_DISPLAY_SHA256"
Assert-ExactSha256 -RelativePath "JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a" -Expected "5D860A131811DD9A7EB6FA55F5674B1D78B0DE7DFAF8748CE18A60CEED2D3738" -Label "H4A0_TFT_SHA256"
Assert-ExactSha256 -RelativePath "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a" -Expected "486BE38AE088B94898E516FFBC125855F22C2EC5EE8A6FA9E10F35D7CAC3A3BE" -Label "H4A0_MODBUS_RTU_SHA256"

if ($DurationPerCaseS -lt 10.0) {
    throw "H4A0_DURATION_MUST_BE_AT_LEAST_10S"
}

if ($Repetitions -lt 3) {
    throw "H4A0_REPETITIONS_MUST_BE_AT_LEAST_3"
}

if ($TcpChunk -le 0) {
    throw "H4A0_TCP_CHUNK_INVALID"
}

if ($UdpPayload -lt 8 -or $UdpPayload -gt 1472) {
    throw "H4A0_UDP_PAYLOAD_INVALID"
}

$ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
Write-Host "COM_PORTS=$($ports -join ',')"

if ($Port -notin $ports) {
    throw "H4A0_COM_NOT_PRESENT"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "H4A0_PYTHON_NOT_FOUND"
}
$pythonExe = $pythonCommand.Source

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "H4A0_ARDUINO_CLI_NOT_FOUND"
}

$firmwareDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server"
$firmwareSketch = Join-Path $firmwareDir "eth14_raw_transport_server.ino"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_h4a0_raw_transport_requalification.py"
$historicalRunner = Get-G2Path "tools/modbus-tcp-benchmark/pc/eth14_raw_transport_benchmark.py"

foreach ($required in @($firmwareSketch, $runner, $historicalRunner)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H4A0_REQUIRED_PATH_MISSING=$required"
    }
}

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h4a0_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$buildDir = Join-Path $tempRoot "build_raw"
$compileLog = Join-Path $tempRoot "compile_raw.log"
$uploadLog = Join-Path $tempRoot "upload_raw.log"
$runLog = Join-Path $tempRoot "h4a0_raw.log"
$syntaxLog = Join-Path $tempRoot "python_syntax.log"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "FIRMWARE=$firmwareSketch"
Write-Host "RUNNER=$runner"

$syntaxExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @("-m", "py_compile", $runner, $historicalRunner) -LogPath $syntaxLog
Write-Host "H4A0_PYTHON_SYNTAX_EXIT=$syntaxExit"

if ($syntaxExit -ne 0) {
    Get-Content -LiteralPath $syntaxLog -Tail 200 | ForEach-Object { Write-Host $_ }
    throw "H4A0_PYTHON_SYNTAX_FAILED"
}

Write-Host "H4A0_PYTHON_SYNTAX=PASS"

$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$fqbn = "jwplc_local:esp32:jwplcbasic"

Write-Host ""
Write-Host "=== COMPILE RAW SERVER AGAINST FINAL PACKAGE ==="

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildDir,
    "--libraries", $repoLibrariesRoot,
    $firmwareDir
)

$compileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog
Write-Host "H4A0_COMPILE_EXIT=$compileExit"
Write-Host "H4A0_COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Get-Content -LiteralPath $compileLog -Tail 240 | ForEach-Object { Write-Host $_ }
    throw "H4A0_COMPILE_FAILED"
}

$binCount = @(Get-ChildItem -LiteralPath $buildDir -Recurse -File -Filter "*.bin").Count
Write-Host "H4A0_BIN_COUNT=$binCount"

if ($binCount -lt 1) {
    throw "H4A0_BIN_MISSING"
}

$displayObjects = @(
    Get-ChildItem -LiteralPath $buildDir -Recurse -File |
        Where-Object { $_.Name -like "JWPLC_Display.cpp.o*" }
).Count

$tftObjects = @(
    Get-ChildItem -LiteralPath $buildDir -Recurse -File |
        Where-Object { $_.Name -like "JWPLC_TFT.cpp.o*" }
).Count

$externalTftObjects = @(
    Get-ChildItem -LiteralPath $buildDir -Recurse -File |
        Where-Object { $_.Name -like "TFT_eSPI.cpp.o*" }
).Count

Write-Host "H4A0_DISPLAY_SOURCE_OBJECT_COUNT=$displayObjects"
Write-Host "H4A0_TFT_SOURCE_OBJECT_COUNT=$tftObjects"
Write-Host "H4A0_EXTERNAL_TFT_ESPI_OBJECT_COUNT=$externalTftObjects"

if ($displayObjects -ne 0) {
    throw "H4A0_DISPLAY_NOT_PRECOMPILED"
}

if ($tftObjects -ne 0) {
    throw "H4A0_TFT_NOT_PRECOMPILED"
}

if ($externalTftObjects -ne 0) {
    throw "H4A0_EXTERNAL_TFT_ESPI_SELECTED"
}

Write-Host "H4A0_FINAL_DISPLAY_LINKAGE=PASS"

Write-Host ""
Write-Host "=== UPLOAD RAW SERVER $Port ==="

$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $Port,
    "--input-dir", $buildDir,
    $firmwareDir
)

$uploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog
Write-Host "H4A0_UPLOAD_EXIT=$uploadExit"
Write-Host "H4A0_UPLOAD_LOG=$uploadLog"

if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 220 | ForEach-Object { Write-Host $_ }
    throw "H4A0_UPLOAD_FAILED"
}

Start-Sleep -Seconds 3

Write-Host ""
Write-Host "=== H4A0 RAW ETHERNET 3x4 BASELINE ==="

$durationText = $DurationPerCaseS.ToString([System.Globalization.CultureInfo]::InvariantCulture)

$runArgs = @(
    "-u", $runner,
    "--serial", $Port,
    "--duration", $durationText,
    "--repetitions", $Repetitions.ToString(),
    "--tcp-chunk", $TcpChunk.ToString(),
    "--udp-payload", $UdpPayload.ToString()
)

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & $pythonExe @runArgs 2>&1 | Tee-Object -FilePath $runLog
    $runExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "H4A0_RUNNER_EXIT=$runExit"
Write-Host "H4A0_RUNNER_LOG=$runLog"

if ($runExit -ne 0) {
    throw "H4A0_RAW_BASELINE_REVIEW"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"

$tftAnswer = Read-Host "¿TFT COM14 estable, operativo y sin cortes/parpadeo anormal durante H4A0? (S/N)"
$tftPhysical = $tftAnswer.Trim().ToUpper() -eq "S"
Write-Host "H4A0_TFT_PHYSICAL_PASS=$tftPhysical"

if (-not $tftPhysical) {
    throw "H4A0_TFT_PHYSICAL_REVIEW"
}

$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "H4A0_FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
Write-Host "H4A0_FINAL_STAGED_COUNT=$($finalStaged.Count)"

if ($finalDirty.Count -ne 0) {
    $finalDirty | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
    throw "H4A0_REPOSITORY_MUTATED"
}

if ($finalStaged.Count -ne 0) {
    throw "H4A0_INDEX_MUTATED"
}

Assert-G2ProtectedArtifacts

Write-Host "H4A0_REPOSITORY_MUTATION=NO"
Write-Host "A14_H4A0_RAW_ETHERNET_BASELINE_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_RAW_CHUNK_BATCH_CEILING_SWEEP"
