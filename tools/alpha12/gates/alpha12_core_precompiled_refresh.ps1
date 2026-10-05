param(
    [string]$ArduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe",
    [string]$ResultRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"

$coreRel = "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
$corePath = Join-Path $repo $coreRel
$coreSourceRel = "JWPLC/2.1.0/cores/jwcontrol"

$buildScript = Join-Path $repo "tools\build-speed-benchmark\Build-JWPLCPrecompiledCore.ps1"
$verifyScript = Join-Path $repo "tools\build-speed-benchmark\Verify-JWPLCPrecompiledCore.ps1"

function Get-Sha256Lower {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-TrackedDirty {
    return @(
        & git -C $repo diff --name-only
    )
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

if ($branch -ne $expectedBranch) {
    throw "A12_CORE_BRANCH_MISMATCH expected=$expectedBranch actual=$branch"
}

$trackedDirty = @(Get-TrackedDirty)
$staged = @(& git -C $repo diff --cached --name-only)

if ($trackedDirty.Count -ne 0) {
    $trackedDirty | ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }
    throw "A12_CORE_TRACKED_TREE_NOT_CLEAN"
}
if ($staged.Count -ne 0) {
    throw "A12_CORE_INDEX_NOT_CLEAN"
}

& git -C $repo diff --check
if ($LASTEXITCODE -ne 0) {
    throw "A12_CORE_GIT_DIFF_CHECK_FAILED"
}

foreach ($required in @($corePath, $buildScript, $verifyScript)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "A12_CORE_REQUIRED_PATH_MISSING=$required"
    }
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\arduino-ide\resources\app\lib\backend\resources\arduino-cli.exe"),
        "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            $ArduinoCli = $candidate
            break
        }
    }
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "A12_CORE_ARDUINO_CLI_NOT_FOUND"
}

$sourceCommit = (& git -C $repo log -1 --format=%H -- $coreSourceRel).Trim()
$sourceDate = (& git -C $repo log -1 --format=%cI -- $coreSourceRel).Trim()
$archiveCommit = (& git -C $repo log -1 --format=%H -- $coreRel).Trim()
$archiveDate = (& git -C $repo log -1 --format=%cI -- $coreRel).Trim()

if ([string]::IsNullOrWhiteSpace($sourceCommit) -or
    [string]::IsNullOrWhiteSpace($archiveCommit)) {
    throw "A12_CORE_GIT_HISTORY_RESOLUTION_FAILED"
}

$oldSha = Get-Sha256Lower $corePath
$oldBytes = (Get-Item -LiteralPath $corePath).Length

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo "tools\alpha12\results\core_precompiled_refresh_$stamp"
}
New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$backup = Join-Path $env:TEMP ("a12_core_backup_" + $stamp + ".a")
Copy-Item -LiteralPath $corePath -Destination $backup -Force

$buildRoot = Join-Path $ResultRoot "builder"
$buildLog = Join-Path $ResultRoot "build_core.log"
$verifyBasicLog = Join-Path $ResultRoot "verify_basic.log"
$verifyCoreLog = Join-Path $ResultRoot "verify_core.log"
$summaryPath = Join-Path $ResultRoot "SUMMARY.txt"

Write-Host "=============================================================================="
Write-Host " ALPHA12 - CORE.A PRECOMPILED REFRESH"
Write-Host "=============================================================================="
Write-Host "BRANCH=$branch"
Write-Host "HEAD=$head"
Write-Host "SOURCE_LAST_COMMIT=$sourceCommit"
Write-Host "SOURCE_LAST_DATE=$sourceDate"
Write-Host "ARCHIVE_LAST_COMMIT=$archiveCommit"
Write-Host "ARCHIVE_LAST_DATE=$archiveDate"
Write-Host "OLD_CORE_BYTES=$oldBytes"
Write-Host "OLD_CORE_SHA256=$oldSha"
Write-Host "ARDUINO_CLI=$ArduinoCli"
Write-Host "RESULT_ROOT=$ResultRoot"

@(
    "DATE=$(Get-Date -Format o)"
    "BRANCH=$branch"
    "HEAD=$head"
    "SOURCE_LAST_COMMIT=$sourceCommit"
    "SOURCE_LAST_DATE=$sourceDate"
    "ARCHIVE_LAST_COMMIT=$archiveCommit"
    "ARCHIVE_LAST_DATE=$archiveDate"
    "OLD_CORE_BYTES=$oldBytes"
    "OLD_CORE_SHA256=$oldSha"
    "ARDUINO_CLI=$ArduinoCli"
) | Set-Content -LiteralPath (Join-Path $ResultRoot "MANIFEST.txt") -Encoding UTF8

$success = $false

try {
    Write-Host ""
    Write-Host "=== BUILD CURRENT SOURCE -> CORE.A ==="

    try {
        & $buildScript             -ArduinoCli $ArduinoCli             -Targets Basic             -Sketch "01_empty"             -OutputRoot $buildRoot *> $buildLog
    }
    catch {
        Write-Host "=== BUILD LOG TAIL ==="
        if (Test-Path -LiteralPath $buildLog) {
            Get-Content -LiteralPath $buildLog -Tail 220 | ForEach-Object { Write-Host $_ }
        }
        throw
    }

    $buildText = if (Test-Path -LiteralPath $buildLog) {
        [IO.File]::ReadAllText($buildLog)
    } else { "" }

    if (-not $buildText.Contains("CORE_PRECOMPILED_BUILD=PASS")) {
        Write-Host "=== BUILD LOG TAIL ==="
        Get-Content -LiteralPath $buildLog -Tail 220 | ForEach-Object { Write-Host $_ }
        throw "A12_CORE_BUILD_PASS_MARKER_MISSING"
    }

    $newSha = Get-Sha256Lower $corePath
    $newBytes = (Get-Item -LiteralPath $corePath).Length

    Write-Host "CORE_PRECOMPILED_BUILD=PASS"
    Write-Host "NEW_CORE_BYTES=$newBytes"
    Write-Host "NEW_CORE_SHA256=$newSha"
    Write-Host "CORE_ARCHIVE_CHANGED=$(if ($newSha -ne $oldSha) { 'YES' } else { 'NO' })"

    Write-Host ""
    Write-Host "=== VERIFY BASIC USES STUB + NEW CORE.A ==="

    try {
        & $verifyScript             -Target Basic             -ArduinoCli $ArduinoCli *> $verifyBasicLog
    }
    catch {
        Write-Host "=== VERIFY BASIC LOG TAIL ==="
        if (Test-Path -LiteralPath $verifyBasicLog) {
            Get-Content -LiteralPath $verifyBasicLog -Tail 220 | ForEach-Object { Write-Host $_ }
        }
        throw
    }

    $verifyBasicText = [IO.File]::ReadAllText($verifyBasicLog)
    if (-not $verifyBasicText.Contains("CORE_PRECOMPILED_VERIFY_BASIC=PASS")) {
        Get-Content -LiteralPath $verifyBasicLog -Tail 220 | ForEach-Object { Write-Host $_ }
        throw "A12_CORE_VERIFY_BASIC_PASS_MARKER_MISSING"
    }
    Write-Host "CORE_PRECOMPILED_VERIFY_BASIC=PASS"

    Write-Host ""
    Write-Host "=== VERIFY BASIC CORE REMAINS SOURCE CONTROL ==="

    try {
        & $verifyScript             -Target Core             -ArduinoCli $ArduinoCli *> $verifyCoreLog
    }
    catch {
        Write-Host "=== VERIFY CORE LOG TAIL ==="
        if (Test-Path -LiteralPath $verifyCoreLog) {
            Get-Content -LiteralPath $verifyCoreLog -Tail 220 | ForEach-Object { Write-Host $_ }
        }
        throw
    }

    $verifyCoreText = [IO.File]::ReadAllText($verifyCoreLog)
    if (-not $verifyCoreText.Contains("CORE_PRECOMPILED_VERIFY_CORE=PASS")) {
        Get-Content -LiteralPath $verifyCoreLog -Tail 220 | ForEach-Object { Write-Host $_ }
        throw "A12_CORE_VERIFY_CORE_PASS_MARKER_MISSING"
    }
    Write-Host "CORE_PRECOMPILED_VERIFY_CORE=PASS"

    $finalDirty = @(Get-TrackedDirty)
    $finalStaged = @(& git -C $repo diff --cached --name-only)

    Write-Host "FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
    $finalDirty | ForEach-Object { Write-Host "FINAL_TRACKED_DIRTY=$_" }
    Write-Host "FINAL_STAGED_COUNT=$($finalStaged.Count)"

    $normalizedCoreRel = $coreRel.Replace("\", "/")
    $normalizedDirty = @($finalDirty | ForEach-Object { $_.Replace("\", "/") })

    if ($normalizedDirty.Count -ne 1 -or
        $normalizedDirty[0] -ne $normalizedCoreRel) {
        throw "A12_CORE_FINAL_DIRTY_SCOPE_INVALID"
    }
    if ($finalStaged.Count -ne 0) {
        throw "A12_CORE_FINAL_INDEX_NOT_CLEAN"
    }

    @(
        "ALPHA12_CORE_PRECOMPILED_REFRESH=PASS"
        "BRANCH=$branch"
        "HEAD_SOURCE=$head"
        "SOURCE_LAST_COMMIT=$sourceCommit"
        "ARCHIVE_PREVIOUS_COMMIT=$archiveCommit"
        "OLD_CORE_BYTES=$oldBytes"
        "OLD_CORE_SHA256=$oldSha"
        "NEW_CORE_BYTES=$newBytes"
        "NEW_CORE_SHA256=$newSha"
        "CORE_ARCHIVE_CHANGED=$(if ($newSha -ne $oldSha) { 'YES' } else { 'NO' })"
        "CORE_PRECOMPILED_BUILD=PASS"
        "CORE_PRECOMPILED_VERIFY_BASIC=PASS"
        "CORE_PRECOMPILED_VERIFY_CORE=PASS"
        "FINAL_TRACKED_DIRTY=$normalizedCoreRel"
        "PHYSICAL_UPLOAD_PERFORMED=NO"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath $summaryPath -Encoding UTF8

    $success = $true

    Write-Host ""
    Write-Host "=============================================================================="
    Write-Host "ALPHA12_CORE_PRECOMPILED_REFRESH=PASS"
    Write-Host "NEW_CORE_BYTES=$newBytes"
    Write-Host "NEW_CORE_SHA256=$newSha"
    Write-Host "FINAL_TRACKED_DIRTY=$normalizedCoreRel"
    Write-Host "PHYSICAL_UPLOAD_PERFORMED=NO"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
finally {
    if (-not $success) {
        Write-Host "A12_CORE_ROLLBACK=START"

        if (Test-Path -LiteralPath $backup) {
            Copy-Item -LiteralPath $backup -Destination $corePath -Force
        }

        $restoredSha = Get-Sha256Lower $corePath
        Write-Host "A12_CORE_ROLLBACK_SHA256=$restoredSha"

        if ($restoredSha -ne $oldSha) {
            throw "A12_CORE_ROLLBACK_FAILED"
        }

        Write-Host "A12_CORE_ROLLBACK=PASS"
    }

    if (Test-Path -LiteralPath $backup) {
        Remove-Item -LiteralPath $backup -Force
    }
}
