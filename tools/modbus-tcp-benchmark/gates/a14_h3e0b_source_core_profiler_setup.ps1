param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [switch]$SetupOnly,
    [switch]$AllowDirtyCoreCandidate,
    [switch]$AllowMissingModbusRtuArchiveCandidate
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
Write-Host " A14 H3E.0B - SOURCE CORE PROFILER SETUP"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath = Get-G2Path $archiveRelative
$coreRelative = $script:G2CoreRelative

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

$expectedDirty = @($coreRelative.Replace("\", "/"))
if ($AllowMissingModbusRtuArchiveCandidate) {
    $expectedDirty += $archiveRelative
}
$expectedDirty = @($expectedDirty | Sort-Object)
$normalizedDirty = @(
    $dirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)

if ($AllowDirtyCoreCandidate) {
    if ($normalizedDirty.Count -ne $expectedDirty.Count) {
        $normalizedDirty | ForEach-Object { Write-Host "DIRTY=$_" }
        throw "H3E0B_SETUP_DIRTY_COUNT_INVALID"
    }

    for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
        if ($normalizedDirty[$i] -ne $expectedDirty[$i]) {
            throw "H3E0B_SETUP_DIRTY_SCOPE_INVALID"
        }
    }
}
elseif ($dirty.Count -ne 0) {
    throw "H3E0B_SETUP_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    throw "H3E0B_SETUP_INDEX_NOT_CLEAN"
}

$coreHashBefore = Get-G2Sha256 $coreRelative
if ($coreHashBefore -ne $expectedCoreHash) {
    throw "H3E0B_SETUP_CORE_HASH_INVALID"
}

$ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
Write-Host "COM_PORTS=$($ports -join ',')"

if ($MasterPort -notin $ports) {
    throw "H3E0B_SETUP_MASTER_PORT_MISSING"
}
if ($SlavePort -notin $ports) {
    throw "H3E0B_SETUP_SLAVE_PORT_MISSING"
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "H3E0B_SETUP_ARDUINO_CLI_MISSING"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "H3E0B_SETUP_PYTHON_MISSING"
}
$pythonExe = $pythonCommand.Source

$platformRoot = Get-G2Path "JWPLC/2.1.0"
$boardsLocalPath = Join-Path $platformRoot "boards.local.txt"
$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"

$masterDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_h3e0b_core_profiler_master"
$masterSketch = Join-Path $masterDir "a14_h3e0b_core_profiler_master.ino"
$slaveDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave"
$slaveSketch = Join-Path $slaveDir "a14_p5_rtu_slave.ino"
$resolver = Join-Path $PSScriptRoot "a14_p5_resolve_full_runtime_ip.py"

foreach ($required in @($masterSketch,$slaveSketch,$resolver)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E0B_SETUP_REQUIRED_FILE_MISSING=$required"
    }
}

$masterText = [IO.File]::ReadAllText($masterSketch)
if (-not $masterText.Contains("H3E0B_PROFILER=ENABLED")) {
    throw "H3E0B_SETUP_MASTER_PROFILER_CONTRACT_MISSING"
}
if (-not $masterText.Contains("jwplcH3E0BProfilerEnabled")) {
    throw "H3E0B_SETUP_MASTER_ENABLE_HOOK_MISSING"
}

$boardsHadOriginal = Test-Path -LiteralPath $boardsLocalPath
$boardsBytes = $null
$boardsText = ""
if ($boardsHadOriginal) {
    $boardsBytes = [IO.File]::ReadAllBytes($boardsLocalPath)
    $boardsText = [IO.File]::ReadAllText($boardsLocalPath)
}

function Restore-BoardsLocal {
    if ($boardsHadOriginal) {
        [IO.File]::WriteAllBytes($boardsLocalPath, $boardsBytes)
    }
    elseif (Test-Path -LiteralPath $boardsLocalPath) {
        Remove-Item -LiteralPath $boardsLocalPath -Force
    }
}

function Enable-SourceCoreOverride {
    $base = $boardsText

    $base = [regex]::Replace(
        $base,
        '(?ms)^# BEGIN JWPLC_SOURCE_CORE_BUILD\r?\n.*?^# END JWPLC_SOURCE_CORE_BUILD\r?\n?',
        ''
    ).TrimEnd()

    $base = [regex]::Replace(
        $base,
        '(?ms)^# BEGIN JWPLC_P2_PRECOMPILED_CORE\r?\n.*?^# END JWPLC_P2_PRECOMPILED_CORE\r?\n?',
        ''
    ).TrimEnd()

    $lines = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($base)) {
        $lines.Add($base)
        $lines.Add("")
    }

    $lines.Add("# BEGIN JWPLC_SOURCE_CORE_BUILD")
    $lines.Add("jwplcbasic.build.core=jwcontrol")
    $lines.Add("jwplcbasic.build.extra_libs=")
    $lines.Add("# END JWPLC_SOURCE_CORE_BUILD")

    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [IO.File]::WriteAllText(
        $boardsLocalPath,
        (($lines -join [Environment]::NewLine) + [Environment]::NewLine),
        $utf8
    )
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h3e0b_setup_{0}" -f $timestamp)
$masterBuild = Join-Path $tempRoot "build_master_source_core"
$slaveBuild = Join-Path $tempRoot "build_slave"
$masterCompileLog = Join-Path $tempRoot "compile_master_source_core.log"
$slaveCompileLog = Join-Path $tempRoot "compile_slave.log"
$masterUploadLog = Join-Path $tempRoot "upload_master.log"
$slaveUploadLog = Join-Path $tempRoot "upload_slave.log"
$resolverLog = Join-Path $tempRoot "resolver.log"

New-Item -ItemType Directory -Force -Path $masterBuild | Out-Null
New-Item -ItemType Directory -Force -Path $slaveBuild | Out-Null

$fqbn = "jwplc_local:esp32:jwplcbasic"

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "CORE_A_SHA256_BEFORE=$coreHashBefore"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "H3E0B_CORE_MODE=SOURCE_TEMPORARY"
Write-Host "H3E0B_PRECOMPILED_CORE_MUTATION=NO"

Write-Host ""
Write-Host "=== COMPILE SLAVE NORMAL ==="

$slaveArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $slaveBuild,
    "--libraries", $repoLibrariesRoot,
    $slaveDir
)

$slaveExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $slaveArgs -LogPath $slaveCompileLog
Write-Host "SLAVE_COMPILE_EXIT=$slaveExit"
if ($slaveExit -ne 0) {
    Get-Content -LiteralPath $slaveCompileLog -Tail 60 | ForEach-Object { Write-Host $_ }
    throw "H3E0B_SETUP_SLAVE_COMPILE_FAILED"
}

Write-Host ""
Write-Host "=== COMPILE MASTER FROM SOURCE CORE ==="

try {
    Enable-SourceCoreOverride

    $masterArgs = @(
        "compile",
        "--fqbn", $fqbn,
        "--build-path", $masterBuild,
        "--libraries", $repoLibrariesRoot,
        $masterDir
    )

    $masterExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $masterArgs -LogPath $masterCompileLog
}
finally {
    Restore-BoardsLocal
}

Write-Host "MASTER_COMPILE_EXIT=$masterExit"
if ($masterExit -ne 0) {
    Get-Content -LiteralPath $masterCompileLog -Tail 60 | ForEach-Object { Write-Host $_ }
    throw "H3E0B_SETUP_MASTER_COMPILE_FAILED"
}

$compileDbPath = Join-Path $masterBuild "compile_commands.json"
if (-not (Test-Path -LiteralPath $compileDbPath)) {
    throw "H3E0B_SETUP_COMPILE_DB_MISSING"
}

$compileDbText = [IO.File]::ReadAllText($compileDbPath)
$sourceCorePass =
    $compileDbText.Contains("cores/jwcontrol/main.cpp") -or
    $compileDbText.Contains("cores\\jwcontrol\\main.cpp")

Write-Host "MASTER_SOURCE_CORE_MAIN_COMPILED=$sourceCorePass"
if (-not $sourceCorePass) {
    throw "H3E0B_SETUP_SOURCE_CORE_NOT_CONFIRMED"
}

$coreHashAfterCompile = Get-G2Sha256 $coreRelative
Write-Host "CORE_A_SHA256_AFTER_COMPILE=$coreHashAfterCompile"

if ($coreHashAfterCompile -ne $coreHashBefore) {
    throw "H3E0B_SETUP_PRECOMPILED_CORE_CHANGED_DURING_SOURCE_BUILD"
}

Write-Host ""
Write-Host "=== UPLOAD SLAVE $SlavePort ==="

$slaveUploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SlavePort,
    "--input-dir", $slaveBuild,
    $slaveDir
)
$slaveUploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $slaveUploadArgs -LogPath $slaveUploadLog
Write-Host "SLAVE_UPLOAD_EXIT=$slaveUploadExit"
if ($slaveUploadExit -ne 0) {
    Get-Content -LiteralPath $slaveUploadLog -Tail 80 | ForEach-Object { Write-Host $_ }
    throw "H3E0B_SETUP_SLAVE_UPLOAD_FAILED"
}

Start-Sleep -Seconds 2

Write-Host ""
Write-Host "=== UPLOAD MASTER SOURCE CORE $MasterPort ==="

$masterUploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $MasterPort,
    "--input-dir", $masterBuild,
    $masterDir
)
$masterUploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $masterUploadArgs -LogPath $masterUploadLog
Write-Host "MASTER_UPLOAD_EXIT=$masterUploadExit"
if ($masterUploadExit -ne 0) {
    Get-Content -LiteralPath $masterUploadLog -Tail 80 | ForEach-Object { Write-Host $_ }
    throw "H3E0B_SETUP_MASTER_UPLOAD_FAILED"
}

Start-Sleep -Seconds 2

Write-Host ""
Write-Host "=== PHYSICAL READY PREFLIGHT ==="

$resolverArgs = @($resolver, "--serial", $MasterPort, "--timeout", "45")
$resolverExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $resolverArgs -LogPath $resolverLog
Write-Host "H3E0B_SETUP_RESOLVER_EXIT=$resolverExit"

if ($resolverExit -ne 0) {
    Get-Content -LiteralPath $resolverLog -Tail 80 | ForEach-Object { Write-Host $_ }
    throw "H3E0B_SETUP_PREFLIGHT_FAILED"
}

$resolverText = [IO.File]::ReadAllText($resolverLog)
$ipMatch = [regex]::Match(
    $resolverText,
    "(?m)^P5_DUT_IP_EFFECTIVE=(.+?)\r?$"
)
if (-not $ipMatch.Success) {
    throw "H3E0B_SETUP_IP_MISSING"
}
$dutIp = $ipMatch.Groups[1].Value.Trim()

Get-Content -LiteralPath $resolverLog | ForEach-Object { Write-Host $_ }

$coreHashFinal = Get-G2Sha256 $coreRelative
Write-Host "CORE_A_SHA256_FINAL=$coreHashFinal"
if ($coreHashFinal -ne $coreHashBefore) {
    throw "H3E0B_SETUP_PRECOMPILED_CORE_FINAL_CHANGED"
}

Restore-BoardsLocal

Write-Host "A14_H3E0B_SETUP_ONLY=PASS"
Write-Host "H3E0B_SETUP_ONLY_MASTER_IP=$dutIp"
Write-Host "H3E0B_PRECOMPILED_CORE_PRESERVED=YES"

if (-not $SetupOnly) {
    throw "H3E0B_SETUP_ONLY_REQUIRED"
}
