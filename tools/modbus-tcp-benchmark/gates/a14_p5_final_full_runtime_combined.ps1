param(
    [string]$DutPort = "COM14",
    [string]$MasterPort = "COM4",
    [double]$TcpRate = 1000.0,
    [double]$DurationS = 60.0
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

function Get-LogValue {
    param([string]$Text, [string]$Key)
    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))
    if ($matches.Count -ne 1) {
        throw ("P5_LOG_KEY_COUNT_{0}={1}" -f $Key, $matches.Count)
    }
    return $matches[0].Groups[1].Value.Trim()
}

Write-Host "============================================================"
Write-Host " A14 P5 - FINAL FULL RUNTIME + TCP1000 + RTU50"
Write-Host "============================================================"

Assert-G2Branch

$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0) { throw "P5_CACHED_DIFF_FAILED" }

Write-Host "HEAD=$head"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "DUT_PORT=$DutPort"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host ("TCP_TARGET_REQ_S={0:F0}" -f $TcpRate)
Write-Host ("DURATION_S={0:F0}" -f $DurationS)
Write-Host "RTU_TARGET_HZ=50"
Write-Host "RTU_SLAVE_ID=2"
Write-Host "RTU_BAUD=115200"
Write-Host "RTU_CONFIG=8N1"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"
Write-Host "DIAGNOSTIC_COPY_ONLY=YES"

if ($spiHz -ne 26000000) { throw "P5_EXPECTED_26MHZ" }
if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "P5_TRACKED_TREE_NOT_CLEAN"
}
if ($staged.Count -ne 0) {
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "P5_INDEX_NOT_CLEAN"
}
if ($TcpRate -le 0) { throw "P5_TCP_RATE_INVALID" }
if ($DurationS -lt 30.0) { throw "P5_DURATION_TOO_SHORT" }

Assert-G2ProtectedArtifacts

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) { throw "P5_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source

$pyLauncher = Get-Command py.exe -ErrorAction SilentlyContinue
if ($null -eq $pyLauncher) {
    $pyLauncher = Get-Command py -ErrorAction SilentlyContinue
}
if ($null -eq $pyLauncher) { throw "P5_PY_LAUNCHER_NOT_FOUND" }

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) { throw "P5_ARDUINO_CLI_NOT_FOUND" }

$sourceSketchDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_perf_full_runtime_realistic"
$sourceSketch = Join-Path $sourceSketchDir "a14_perf_full_runtime_realistic.ino"
$patchPath = Join-Path $PSScriptRoot "a14_p5_combined_runtime_rtu_patch.py"
$resolverPath = Join-Path $PSScriptRoot "a14_p5_resolve_full_runtime_ip.py"
$combinedHarness = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_p5_combined_rtu50_tcp_harness.ps1"

foreach ($required in @($sourceSketch, $patchPath, $resolverPath, $combinedHarness)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "P5_REQUIRED_FILE_MISSING=$required"
    }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p5_final_{0}" -f $timestamp)
$diagSketchDir = Join-Path $tempRoot "a14_perf_full_runtime_realistic"
$diagSketch = Join-Path $diagSketchDir "a14_perf_full_runtime_realistic.ino"
$buildPath = Join-Path $tempRoot "build"
$patchLog = Join-Path $tempRoot "patch.log"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"
$resolverLog = Join-Path $tempRoot "resolver.log"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
Copy-Item -LiteralPath $sourceSketchDir -Destination $diagSketchDir -Recurse
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "DIAGNOSTIC_SKETCH=$diagSketch"
Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"

Write-Host ""
Write-Host "=== STATIC / SYNTAX PREFLIGHT ==="

$pyCompileArgs = @("-m", "py_compile", $patchPath, $resolverPath)
$pyCompileLog = Join-Path $tempRoot "py_compile.log"
$pyCompileExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $pyCompileArgs -LogPath $pyCompileLog
Write-Host "PY_COMPILE_EXIT=$pyCompileExit"
if ($pyCompileExit -ne 0) {
    Get-Content -LiteralPath $pyCompileLog -Tail 180 | ForEach-Object { Write-Host $_ }
    throw "P5_PYTHON_SYNTAX_FAILED"
}

$syntaxValidator = Join-Path $PSScriptRoot "assert_ps1_syntax.ps1"
$syntaxArgs = @("-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $syntaxValidator, "-Path", $combinedHarness)
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & powershell.exe @syntaxArgs
    $syntaxExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}
if ($syntaxExit -ne 0) { throw "P5_POWERSHELL_SYNTAX_FAILED" }

Write-Host ""
Write-Host "=== APPLY DIAGNOSTIC RTU PATCH ==="
$patchArgs = @($patchPath, "--sketch", $diagSketch)
$patchExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $patchArgs -LogPath $patchLog
Write-Host "P5_PATCH_EXIT=$patchExit"
if ($patchExit -ne 0) {
    Get-Content -LiteralPath $patchLog -Tail 220 | ForEach-Object { Write-Host $_ }
    throw "P5_PATCH_FAILED"
}
Get-Content -LiteralPath $patchLog | ForEach-Object { Write-Host $_ }

$folderName = Split-Path $diagSketchDir -Leaf
$inoBaseName = [System.IO.Path]::GetFileNameWithoutExtension($diagSketch)
Write-Host "SKETCH_FOLDER=$folderName"
Write-Host "MAIN_INO_BASENAME=$inoBaseName"
if ($folderName -ne $inoBaseName) { throw "P5_ARDUINO_SKETCH_NAME_CONTRACT_FAILED" }

$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$fqbn = "jwplc_local:esp32:jwplcbasic"

Write-Host ""
Write-Host "=== COMPILE P5 DIAGNOSTIC CANDIDATE ==="
$compileArgs = @("compile", "--fqbn", $fqbn, "--build-path", $buildPath, "--libraries", $repoLibrariesRoot, $diagSketchDir)
$compileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog
Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"
if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 240 | ForEach-Object { Write-Host $_ }
    throw "P5_COMPILE_FAILED"
}
$binCount = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin").Count
Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) { throw "P5_BIN_MISSING" }
Invoke-G2CompileFinishedSound -Success $true

Write-Host ""
Write-Host "=== UPLOAD P5 DIAGNOSTIC CANDIDATE ==="
$uploadArgs = @("upload", "--fqbn", $fqbn, "--port", $DutPort, "--input-dir", $buildPath, $diagSketchDir)
$uploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog
Write-Host "UPLOAD_EXIT=$uploadExit"
if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 220 | ForEach-Object { Write-Host $_ }
    throw "P5_UPLOAD_FAILED"
}

Start-Sleep -Seconds 2

Write-Host ""
Write-Host "=== RESOLVE FULL RUNTIME IP ==="
$resolverArgs = @($resolverPath, "--serial", $DutPort, "--timeout", "45")
$resolverExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $resolverArgs -LogPath $resolverLog
Write-Host "P5_RESOLVER_EXIT=$resolverExit"
if ($resolverExit -ne 0) {
    Get-Content -LiteralPath $resolverLog -Tail 260 | ForEach-Object { Write-Host $_ }
    throw "P5_DUT_NOT_READY"
}
$resolverText = [System.IO.File]::ReadAllText($resolverLog)
$dutIp = Get-LogValue -Text $resolverText -Key "P5_DUT_IP_EFFECTIVE"
Write-Host "P5_DUT_IP_EFFECTIVE=$dutIp"

Write-Host ""
Write-Host "============================================================"
Write-Host " P5 PHYSICAL PRECONDITIONS"
Write-Host "============================================================"
Write-Host "ETHERNET=CONNECTED"
Write-Host "DUT=$DutPort"
Write-Host "RTU_MASTER=$MasterPort"
Write-Host "RS485=DUT_SLAVE2_CONNECTED_TO_MASTER"
Write-Host "MICROSD=INSERTED"
Write-Host "TFT_RTC_FRAM_BUTTONS_IO=NORMAL_RUNTIME"
Write-Host "El harness pedira dos observaciones fisicas al finalizar."

Write-Host ""
Write-Host "=== RUN COMBINED QUALIFICATION ==="
$rateText = $TcpRate.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$durationText = $DurationS.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$combinedArgs = @(
    "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass",
    "-File", $combinedHarness,
    "-DutPort", $DutPort,
    "-MasterPort", $MasterPort,
    "-DutIp", $dutIp,
    "-TcpRate", $rateText,
    "-DurationS", $durationText,
    "-RequireCleanTracked"
)
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & powershell.exe @combinedArgs
    $combinedExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}
Write-Host "P5_COMBINED_EXIT=$combinedExit"
if ($combinedExit -ne 0) { throw "P5_COMBINED_QUALIFICATION_FAILED" }

Write-Host ""
Write-Host "=== FINAL PRODUCT INVARIANTS ==="
Assert-G2ProtectedArtifacts
$finalHz = Get-G2SpiHz
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0) { throw "P5_FINAL_CACHED_DIFF_FAILED" }
Write-Host "FINAL_SPI_HZ=$finalHz"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"
if ($finalHz -ne 26000000) { throw "P5_FINAL_FREQ_CHANGED" }
if ($finalDirty.Count -ne 0) { throw "P5_PRODUCT_TREE_DIRTY" }
if ($finalStaged.Count -ne 0) { throw "P5_PRODUCT_INDEX_DIRTY" }

Write-Host ""
Write-Host "A14_P5_W5500_SPI_HZ=26000000"
Write-Host "A14_P5_TCP_TARGET_REQ_S=$rateText"
Write-Host "A14_P5_RTU_TARGET_HZ=50"
Write-Host "A14_P5_FULL_RUNTIME_PROFILE=REALISTIC_CURRENT"
Write-Host "A14_P5_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "A14_P5_DIAGNOSTIC_COPY_ONLY=YES"
Write-Host "A14_P5_FINAL_FULL_RUNTIME_COMBINED=PASS"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT_FOR_P5_DECISION"
