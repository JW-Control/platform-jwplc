param(
    [string]$MasterPort = "COM14"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.14/feature/modbus-tcp"
$oldCoreSha = "6EDF40D105936318A2FD8A84D7F0724571657910E8D92E8538640EC613F4DD68"
$coreRel = "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
$corePath = Join-Path $repoRoot $coreRel
$sourceRel = "JWPLC/2.1.0/cores/jwcontrol/main.cpp"
$sourcePath = Join-Path $repoRoot $sourceRel
$buildScript = Join-Path $repoRoot "tools/build-speed-benchmark/Build-JWPLCPrecompiledCore.ps1"
$verifyScript = Join-Path $repoRoot "tools/build-speed-benchmark/Verify-JWPLCPrecompiledCore.ps1"
$probeDir = Join-Path $repoRoot "tools/modbus-tcp-benchmark/firmware/a14_p5f_datalog_autoservice_probe"
$probeReader = Join-Path $repoRoot "tools/modbus-tcp-benchmark/pc/a14_p5f_datalog_autoservice_probe.py"
$repoLibraries = Join-Path $repoRoot "JWPLC/2.1.0/libraries"
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"

function Get-Sha256Hex {
    param(
        [Parameter(Mandatory = $true)][string]$Path
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $stream = [System.IO.File]::OpenRead($fullPath)
    $sha = [System.Security.Cryptography.SHA256]::Create()

    try {
        $hashBytes = $sha.ComputeHash($stream)
    }
    finally {
        $sha.Dispose()
        $stream.Dispose()
    }

    return ([System.BitConverter]::ToString($hashBytes)).Replace("-", "")
}

function Invoke-NativeCaptured {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    $previous = $ErrorActionPreference

    try {
        $ErrorActionPreference = "Continue"
        & $FilePath @Arguments *>&1 | Tee-Object -FilePath $LogPath
        return $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previous
    }
}

Write-Host "============================================================"
Write-Host " A14 P5-F - REBUILD STALE CORE + DATALOG AUTOSERVICE"
Write-Host "============================================================"

$currentBranch = (& git -C $repoRoot branch --show-current).Trim()
Write-Host "BRANCH=$currentBranch"

if ($currentBranch -ne $expectedBranch) {
    throw "P5F_BRANCH_MISMATCH"
}

$dirtyBefore = @(& git -C $repoRoot diff --name-only)
$stagedBefore = @(& git -C $repoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_BEFORE=$($dirtyBefore.Count)"
Write-Host "STAGED_BEFORE=$($stagedBefore.Count)"

if ($dirtyBefore.Count -ne 0) {
    $dirtyBefore | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "P5F_TREE_NOT_CLEAN"
}

if ($stagedBefore.Count -ne 0) {
    throw "P5F_INDEX_NOT_CLEAN"
}

foreach ($required in @($corePath, $sourcePath, $buildScript, $verifyScript, $probeDir, $probeReader, $arduinoCli)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "P5F_REQUIRED_PATH_MISSING=$required"
    }
}

$actualOldSha = Get-Sha256Hex -Path $corePath
Write-Host "P5F_CORE_SHA_BEFORE=$actualOldSha"

if ($actualOldSha -ne $oldCoreSha) {
    throw "P5F_UNEXPECTED_CORE_BASELINE"
}

$sourceText = [System.IO.File]::ReadAllText($sourcePath)
$callbackCount = ([regex]::Matches($sourceText, [regex]::Escape("jwplcDataLogTickCallback();"))).Count
Write-Host "P5F_SOURCE_DATALOG_TICK_CALL_COUNT=$callbackCount"

if ($callbackCount -ne 1) {
    throw "P5F_SOURCE_CALLBACK_CONTRACT_FAILED"
}

$coreLastCommit = (& git -C $repoRoot log -1 --format=%H -- $coreRel).Trim()
$callbackCommit = (& git -C $repoRoot log -1 -S"jwplcDataLogTickCallback();" --format=%H -- $sourceRel).Trim()

Write-Host "P5F_CORE_LAST_COMMIT=$coreLastCommit"
Write-Host "P5F_DATALOG_CALLBACK_INTRO_COMMIT=$callbackCommit"

if ([string]::IsNullOrWhiteSpace($coreLastCommit)) {
    throw "P5F_CORE_HISTORY_MISSING"
}

if ([string]::IsNullOrWhiteSpace($callbackCommit)) {
    throw "P5F_CALLBACK_HISTORY_MISSING"
}

& git -C $repoRoot merge-base --is-ancestor $coreLastCommit $callbackCommit
$coreBeforeCallback = ($LASTEXITCODE -eq 0)

& git -C $repoRoot merge-base --is-ancestor $callbackCommit $coreLastCommit
$callbackAlreadyInCoreHistory = ($LASTEXITCODE -eq 0)

Write-Host "P5F_CORE_COMMIT_BEFORE_CALLBACK_COMMIT=$coreBeforeCallback"
Write-Host "P5F_CALLBACK_COMMIT_IN_CORE_HISTORY=$callbackAlreadyInCoreHistory"

if (-not $coreBeforeCallback) {
    throw "P5F_HISTORY_ORDER_UNEXPECTED"
}

if ($callbackAlreadyInCoreHistory) {
    throw "P5F_STALE_CORE_NOT_PROVEN"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p5f_{0}" -f $timestamp)
$buildLog = Join-Path $tempRoot "rebuild_core.log"
$verifyLog = Join-Path $tempRoot "verify_core.log"
$probeBuild = Join-Path $tempRoot "probe_build"
$probeCompileLog = Join-Path $tempRoot "probe_compile.log"
$probeUploadLog = Join-Path $tempRoot "probe_upload.log"
$probeSerialLog = Join-Path $tempRoot "probe_serial.log"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
New-Item -ItemType Directory -Force -Path $probeBuild | Out-Null

Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== REBUILD CORE.A FROM CURRENT SOURCE ==="

$buildArgs = @("-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $buildScript, "-ArduinoCli", $arduinoCli, "-Targets", "Basic")
$buildExit = Invoke-NativeCaptured -FilePath "powershell.exe" -Arguments $buildArgs -LogPath $buildLog

Write-Host "P5F_CORE_REBUILD_EXIT=$buildExit"
Write-Host "P5F_CORE_REBUILD_LOG=$buildLog"

if ($buildExit -ne 0) {
    throw "P5F_CORE_REBUILD_FAILED"
}

$newCoreSha = Get-Sha256Hex -Path $corePath
Write-Host "P5F_CORE_SHA_AFTER=$newCoreSha"

if ($newCoreSha -eq $oldCoreSha) {
    throw "P5F_CORE_SHA_DID_NOT_CHANGE"
}

$dirtyAfterBuild = @(& git -C $repoRoot diff --name-only)
Write-Host "P5F_TRACKED_DIRTY_AFTER_REBUILD=$($dirtyAfterBuild.Count)"
$dirtyAfterBuild | ForEach-Object { Write-Host "P5F_DIRTY_AFTER_REBUILD=$_" }

if ($dirtyAfterBuild.Count -ne 1 -or $dirtyAfterBuild[0].Replace("\", "/") -ne $coreRel) {
    throw "P5F_REBUILD_DIRTY_SCOPE_INVALID"
}

Write-Host ""
Write-Host "=== VERIFY NORMAL PRECOMPILED CORE PATH ==="

$verifyArgs = @("-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $verifyScript, "-Target", "Basic", "-ArduinoCli", $arduinoCli)
$verifyExit = Invoke-NativeCaptured -FilePath "powershell.exe" -Arguments $verifyArgs -LogPath $verifyLog

Write-Host "P5F_CORE_VERIFY_EXIT=$verifyExit"
Write-Host "P5F_CORE_VERIFY_LOG=$verifyLog"

if ($verifyExit -ne 0) {
    throw "P5F_CORE_VERIFY_FAILED"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue

if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}

if ($null -eq $pythonCommand) {
    throw "P5F_PYTHON_NOT_FOUND"
}

$pythonExe = $pythonCommand.Source
$pySyntaxLog = Join-Path $tempRoot "py_syntax.log"
$pySyntaxExit = Invoke-NativeCaptured -FilePath $pythonExe -Arguments @("-m", "py_compile", $probeReader) -LogPath $pySyntaxLog

Write-Host "P5F_PYTHON_SYNTAX_EXIT=$pySyntaxExit"

if ($pySyntaxExit -ne 0) {
    throw "P5F_PYTHON_SYNTAX_FAILED"
}

Write-Host ""
Write-Host "=== COMPILE DATALOG AUTOSERVICE PROBE ==="

$fqbn = "jwplc_local:esp32:jwplcbasic"
$compileArgs = @("compile", "--fqbn", $fqbn, "--build-path", $probeBuild, "--libraries", $repoLibraries, $probeDir)
$compileExit = Invoke-NativeCaptured -FilePath $arduinoCli -Arguments $compileArgs -LogPath $probeCompileLog

Write-Host "P5F_PROBE_COMPILE_EXIT=$compileExit"
Write-Host "P5F_PROBE_COMPILE_LOG=$probeCompileLog"

if ($compileExit -ne 0) {
    throw "P5F_PROBE_COMPILE_FAILED"
}

Write-Host ""
Write-Host "=== UPLOAD DATALOG AUTOSERVICE PROBE $MasterPort ==="

$ports = @([System.IO.Ports.SerialPort]::GetPortNames())

if ($MasterPort -notin $ports) {
    throw "P5F_MASTER_PORT_NOT_PRESENT"
}

$uploadArgs = @("upload", "--fqbn", $fqbn, "--port", $MasterPort, "--input-dir", $probeBuild, $probeDir)
$uploadExit = Invoke-NativeCaptured -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $probeUploadLog

Write-Host "P5F_PROBE_UPLOAD_EXIT=$uploadExit"

if ($uploadExit -ne 0) {
    throw "P5F_PROBE_UPLOAD_FAILED"
}

Start-Sleep -Milliseconds 500

Write-Host ""
Write-Host "=== PHYSICAL AUTOSERVICE PROBE ==="

$serialArgs = @($probeReader, "--port", $MasterPort, "--baud", "115200", "--timeout", "30")
$serialExit = Invoke-NativeCaptured -FilePath $pythonExe -Arguments $serialArgs -LogPath $probeSerialLog

Write-Host "P5F_PROBE_SERIAL_EXIT=$serialExit"
Write-Host "P5F_PROBE_SERIAL_LOG=$probeSerialLog"

if ($serialExit -ne 0) {
    throw "P5F_DATALOG_AUTOSERVICE_PHYSICAL_FAILED"
}

$dirtyFinal = @(& git -C $repoRoot diff --name-only)
$stagedFinal = @(& git -C $repoRoot diff --cached --name-only)

Write-Host ""
Write-Host "=== FINAL CONTROLLED STATE ==="
Write-Host "TRACKED_DIRTY_FINAL=$($dirtyFinal.Count)"
Write-Host "STAGED_FINAL=$($stagedFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }

if ($dirtyFinal.Count -ne 1 -or $dirtyFinal[0].Replace("\", "/") -ne $coreRel) {
    throw "P5F_FINAL_DIRTY_SCOPE_INVALID"
}

if ($stagedFinal.Count -ne 0) {
    throw "P5F_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "P5F_STALE_CORE_ROOT_CAUSE=CONFIRMED"
Write-Host "P5F_REBUILT_CORE_AUTOSERVICE=PASS"
Write-Host "P5F_PRODUCT_CANDIDATE_DIRTY_CORE_A=YES"
Write-Host "P5F_CORE_SHA_OLD=$oldCoreSha"
Write-Host "P5F_CORE_SHA_CANDIDATE=$newCoreSha"
Write-Host "A14_P5F_DATALOG_CORE_REPAIR=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_BEFORE_COMMITTING_CORE_A"
