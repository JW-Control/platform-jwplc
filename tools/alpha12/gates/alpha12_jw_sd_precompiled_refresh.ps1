param(
    [string]$ArduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic",
    [string]$ResultRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"

$libraryRel = "JWPLC/2.1.0/libraries/JW_SD"
$libraryRoot = Join-Path $repo $libraryRel
$headerPath = Join-Path $libraryRoot "src\JW_SD.h"
$cppPath = Join-Path $libraryRoot "src\JW_SD.cpp"
$archiveRel = "$libraryRel/src/esp32/libJW_SD.a"
$archivePath = Join-Path $repo $archiveRel
$repoLibraries = Join-Path $repo "JWPLC\2.1.0\libraries"

function Get-Sha256Lower {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Invoke-NativeCaptured {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        [object[]]$output = @(& $FilePath @Arguments 2>&1)
        $exitCode = [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = @($output | ForEach-Object { $_.ToString() })
    }
}

function Resolve-Archiver {
    param([string[]]$Lines)

    foreach ($line in $Lines) {
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"') {
            $candidate = $Matches["exe"]
            foreach ($path in @($candidate, ($candidate + ".exe"))) {
                if (Test-Path -LiteralPath $path) {
                    return (Resolve-Path -LiteralPath $path).Path
                }
            }
        }
    }

    foreach ($line in $Lines) {
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') {
            $toolDir = Split-Path -Parent $Matches["exe"]
            foreach ($name in @("xtensa-esp32-elf-gcc-ar.exe", "xtensa-esp32-elf-gcc-ar")) {
                $candidate = Join-Path $toolDir $name
                if (Test-Path -LiteralPath $candidate) {
                    return (Resolve-Path -LiteralPath $candidate).Path
                }
            }
        }
    }

    throw "A12_JW_SD_ARCHIVER_NOT_FOUND"
}

function Assert-LibrarySelected {
    param(
        [string[]]$Lines,
        [string]$ExpectedRoot,
        [string]$Label
    )

    $normalizedExpected = [IO.Path]::GetFullPath($ExpectedRoot).TrimEnd('\', '/')
    $selected = $false

    foreach ($line in $Lines) {
        if ($line -match '^Using library JW_SD at version .+ in folder: (.+)$') {
            $actual = [IO.Path]::GetFullPath($Matches[1].Trim()).TrimEnd('\', '/')
            if ($actual -ieq $normalizedExpected) {
                $selected = $true
            }
        }
    }

    Write-Host "$($Label)_JW_SD_SELECTED=$selected"

    if (-not $selected) {
        throw "A12_JW_SD_$($Label)_LIBRARY_NOT_SELECTED"
    }
}

function Assert-SourceBuild {
    param(
        [string[]]$Lines,
        [string]$BuildPath,
        [string]$ExpectedRoot,
        [string]$Label
    )

    Assert-LibrarySelected -Lines $Lines -ExpectedRoot $ExpectedRoot -Label $Label

    [int]$sourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JW_SD.cpp.o" }
    ).Count

    $precompiled = @(
        $Lines | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match '\bJW_SD\b')
        }
    ).Count -gt 0

    Write-Host "$($Label)_SOURCE_OBJECT_COUNT=$sourceObjects"
    Write-Host "$($Label)_PRECOMPILED_MARKER=$(if ($precompiled) { 'YES' } else { 'NO' })"

    if ($sourceObjects -ne 1 -or $precompiled) {
        throw "A12_JW_SD_$($Label)_SOURCE_POLICY_FAILED"
    }
}

function Assert-PrecompiledBuild {
    param(
        [string[]]$Lines,
        [string]$BuildPath,
        [string]$ExpectedRoot,
        [string]$Label
    )

    Assert-LibrarySelected -Lines $Lines -ExpectedRoot $ExpectedRoot -Label $Label

    [int]$sourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JW_SD.cpp.o" }
    ).Count

    $precompiled = @(
        $Lines | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match '\bJW_SD\b')
        }
    ).Count -gt 0

    Write-Host "$($Label)_SOURCE_OBJECT_COUNT=$sourceObjects"
    Write-Host "$($Label)_PRECOMPILED_MARKER=$(if ($precompiled) { 'YES' } else { 'NO' })"

    if ($sourceObjects -ne 0 -or -not $precompiled) {
        throw "A12_JW_SD_$($Label)_PRECOMPILED_POLICY_FAILED"
    }
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

if ($branch -ne $expectedBranch) {
    throw "A12_JW_SD_BRANCH_MISMATCH expected=$expectedBranch actual=$branch"
}

[string[]]$entryDirty = @(& git -C $repo diff --name-only)
[string[]]$entryStaged = @(& git -C $repo diff --cached --name-only)

if ($entryDirty.Count -ne 0) {
    $entryDirty | ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }
    throw "A12_JW_SD_TRACKED_TREE_NOT_CLEAN"
}
if ($entryStaged.Count -ne 0) {
    throw "A12_JW_SD_INDEX_NOT_CLEAN"
}

& git -C $repo diff --check
if ($LASTEXITCODE -ne 0) {
    throw "A12_JW_SD_GIT_DIFF_CHECK_FAILED"
}

foreach ($required in @(
    $libraryRoot,
    $headerPath,
    $cppPath,
    $archivePath,
    $repoLibraries
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "A12_JW_SD_REQUIRED_PATH_MISSING=$required"
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
    throw "A12_JW_SD_ARDUINO_CLI_NOT_FOUND"
}

$headerText = [IO.File]::ReadAllText($headerPath)
$cppText = [IO.File]::ReadAllText($cppPath)

$headerContracts = @(
    "class JWPLCDataLog",
    "void serviceDataLogs()",
    "void setBusLockCallbacks(",
    "static constexpr uint8_t MAX_DATALOGS = 4",
    "static constexpr uint32_t CARD_DEBOUNCE_MS = 300"
)

foreach ($contract in $headerContracts) {
    if (-not $headerText.Contains($contract)) {
        throw "A12_JW_SD_HEADER_CONTRACT_MISSING=$contract"
    }
}

$sourceContracts = @(
    "JWPLCDataLog::commitInternal",
    "JW_SD::serviceDataLogs",
    "JW_SD::serviceCardLifecycle",
    "JW_SD::mountCard",
    "JW_SD::setBusLockCallbacks",
    "JWPLCFile::write"
)

foreach ($contract in $sourceContracts) {
    if (-not $cppText.Contains($contract)) {
        throw "A12_JW_SD_SOURCE_CONTRACT_MISSING=$contract"
    }
}

Write-Host "A12_JW_SD_CURRENT_SOURCE_CONTRACT=PASS"
Write-Host "DATALOG_MANAGER=ON"
Write-Host "CARD_LIFECYCLE_RECOVERY=ON"
Write-Host "SPI_LOCK_CALLBACKS=ON"

$sourceCommit = (& git -C $repo log -1 --format=%H -- "$libraryRel/src").Trim()
$sourceDate = (& git -C $repo log -1 --format=%cI -- "$libraryRel/src").Trim()
$archiveCommit = (& git -C $repo log -1 --format=%H -- $archiveRel).Trim()
$archiveDate = (& git -C $repo log -1 --format=%cI -- $archiveRel).Trim()

$oldSha = Get-Sha256Lower $archivePath
$oldBytes = (Get-Item -LiteralPath $archivePath).Length

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo "tools\alpha12\results\jw_sd_precompiled_refresh_$stamp"
}
New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$runRoot = Join-Path $env:TEMP ("jwplc_a12_jw_sd_refresh_" + $stamp)
$sketchRoot = Join-Path $runRoot "jw_sd_gate"
$sourceRoot = Join-Path $runRoot "source-libraries\JW_SD"
$sourceSrc = Join-Path $sourceRoot "src"
$sourceBuild = Join-Path $runRoot "source-build"
$sourceLog = Join-Path $ResultRoot "source.log"

$candidateRoot = Join-Path $runRoot "candidate-libraries\JW_SD"
$candidateSrc = Join-Path $candidateRoot "src"
$candidateArchiveDir = Join-Path $candidateSrc "esp32"
$candidateArchive = Join-Path $candidateArchiveDir "libJW_SD.a"
$candidateEvidenceArchive = Join-Path $ResultRoot "libJW_SD-candidate.a"
$candidateBuild = Join-Path $runRoot "candidate-build"
$candidateLog = Join-Path $ResultRoot "candidate.log"
$extractDir = Join-Path $runRoot "archive-members"

foreach ($dir in @(
    $sketchRoot,
    $sourceSrc,
    $sourceBuild,
    $candidateArchiveDir,
    $candidateBuild,
    $extractDir
)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

$sketchPath = Join-Path $sketchRoot "jw_sd_gate.ino"
$sketchText = @'
#include <Arduino.h>
#include <JW_SD.h>

JW_SD storage(5);
JWPLCDataLog dataLog;

void setup()
{
    storage.setOperationTimeout(250);
    storage.setEnabled(true);

    volatile bool enabled = storage.isEnabled();
    volatile uint32_t timeoutMs = storage.operationTimeout();
    volatile uint8_t activeLogs = storage.activeDataLogs();

    JW_SDDataLogConfig cfg(1024, 256, 1000);
    volatile size_t configuredBuffer = cfg.bufferSize;

    JW_SDDataLogStatus status = dataLog.status();
    volatile size_t pending = status.pendingBytes;
    volatile uint32_t commits = status.commitCount;

    (void)enabled;
    (void)timeoutMs;
    (void)activeLogs;
    (void)configuredBuffer;
    (void)pending;
    (void)commits;
}

void loop()
{
    storage.serviceDataLogs();
}
'@

$utf8NoBom = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($sketchPath, $sketchText, $utf8NoBom)

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $sourceSrc "JW_SD.h") -Force
Copy-Item -LiteralPath $cppPath -Destination (Join-Path $sourceSrc "JW_SD.cpp") -Force

$sourceProperties = @'
name=JW_SD
version=1.0.2-alpha12-source
author=JW Control
maintainer=JW Control
sentence=Alpha12 source qualification for JW_SD.
paragraph=Temporary source-only qualification library.
category=Data Storage
architectures=esp32
'@
[IO.File]::WriteAllText(
    (Join-Path $sourceRoot "library.properties"),
    $sourceProperties,
    $utf8NoBom
)

$candidateProperties = @'
name=JW_SD
version=1.0.2-alpha12-candidate
author=JW Control
maintainer=JW Control
sentence=Alpha12 precompiled candidate for JW_SD.
paragraph=Temporary precompiled qualification library.
category=Data Storage
architectures=esp32
precompiled=full
'@
[IO.File]::WriteAllText(
    (Join-Path $candidateRoot "library.properties"),
    $candidateProperties,
    $utf8NoBom
)

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $candidateSrc "JW_SD.h") -Force

$backup = Join-Path $env:TEMP ("a12_jw_sd_archive_backup_" + $stamp + ".a")
Copy-Item -LiteralPath $archivePath -Destination $backup -Force

@(
    "DATE=$(Get-Date -Format o)"
    "BRANCH=$branch"
    "HEAD=$head"
    "SOURCE_LAST_COMMIT=$sourceCommit"
    "SOURCE_LAST_DATE=$sourceDate"
    "ARCHIVE_LAST_COMMIT=$archiveCommit"
    "ARCHIVE_LAST_DATE=$archiveDate"
    "OLD_ARCHIVE_BYTES=$oldBytes"
    "OLD_ARCHIVE_SHA256=$oldSha"
    "FQBN=$Fqbn"
    "ARDUINO_CLI=$ArduinoCli"
    "DATALOG_MANAGER=ON"
    "CARD_LIFECYCLE_RECOVERY=ON"
    "SPI_LOCK_CALLBACKS=ON"
) | Set-Content -LiteralPath (Join-Path $ResultRoot "MANIFEST.txt") -Encoding UTF8

Write-Host "=============================================================================="
Write-Host " ALPHA12 - JW_SD PRECOMPILED REFRESH"
Write-Host "=============================================================================="
Write-Host "BRANCH=$branch"
Write-Host "HEAD=$head"
Write-Host "SOURCE_LAST_COMMIT=$sourceCommit"
Write-Host "SOURCE_LAST_DATE=$sourceDate"
Write-Host "ARCHIVE_LAST_COMMIT=$archiveCommit"
Write-Host "ARCHIVE_LAST_DATE=$archiveDate"
Write-Host "OLD_ARCHIVE_BYTES=$oldBytes"
Write-Host "OLD_ARCHIVE_SHA256=$oldSha"
Write-Host "RESULT_ROOT=$ResultRoot"

$success = $false

try {
    Write-Host ""
    Write-Host "=== SOURCE-ONLY JW_SD BUILD ==="

    $sourceRun = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments @(
        "compile",
        "--fqbn", $Fqbn,
        "-j", "0",
        "-v",
        "--clean",
        "--build-path", $sourceBuild,
        "--library", $sourceRoot,
        "--libraries", $repoLibraries,
        $sketchRoot
    )

    $sourceRun.Output | Set-Content -LiteralPath $sourceLog -Encoding UTF8
    Write-Host "SOURCE_COMPILE_EXIT=$($sourceRun.ExitCode)"

    if ($sourceRun.ExitCode -ne 0) {
        $sourceRun.Output | Select-Object -Last 160 | ForEach-Object { Write-Host $_ }
        throw "A12_JW_SD_SOURCE_COMPILE_FAILED"
    }

    Assert-SourceBuild -Lines $sourceRun.Output -BuildPath $sourceBuild -ExpectedRoot $sourceRoot -Label "SOURCE"

    $archiver = Resolve-Archiver -Lines $sourceRun.Output
    Write-Host "ARCHIVER=$archiver"

    $sourceObjects = @(
        Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JW_SD.cpp.o" }
    )

    if ($sourceObjects.Count -ne 1) {
        throw "A12_JW_SD_SOURCE_OBJECT_SET_INVALID"
    }

    $sourceObject = $sourceObjects[0]

    Write-Host ""
    Write-Host "=== CREATE JW_SD CANDIDATE ARCHIVE ==="

    $archiveRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @(
        "crs",
        $candidateArchive,
        $sourceObject.FullName
    )

    if ($archiveRun.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $candidateArchive)) {
        throw "A12_JW_SD_ARCHIVE_CREATE_FAILED"
    }

    $listRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $candidateArchive)
    if ($listRun.ExitCode -ne 0) {
        throw "A12_JW_SD_ARCHIVE_LIST_FAILED"
    }

    [string[]]$members = @(
        $listRun.Output |
            ForEach-Object { $_.Trim() } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )

    Write-Host "ARCHIVE_MEMBER_COUNT=$($members.Count)"
    $members | ForEach-Object { Write-Host "ARCHIVE_MEMBER=$_" }

    if ($members.Count -ne 1 -or $members[0] -ne "JW_SD.cpp.o") {
        throw "A12_JW_SD_ARCHIVE_MEMBER_SET_INVALID"
    }

    $oldLocation = Get-Location
    try {
        Set-Location $extractDir
        $extractRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @("x", $candidateArchive)
    }
    finally {
        Set-Location $oldLocation
    }

    if ($extractRun.ExitCode -ne 0) {
        throw "A12_JW_SD_ARCHIVE_EXTRACT_FAILED"
    }

    $extractedObject = Join-Path $extractDir "JW_SD.cpp.o"
    if (-not (Test-Path -LiteralPath $extractedObject)) {
        throw "A12_JW_SD_EXTRACTED_OBJECT_MISSING"
    }

    $sourceObjectSha = Get-Sha256Lower $sourceObject.FullName
    $extractedObjectSha = Get-Sha256Lower $extractedObject

    Write-Host "SOURCE_OBJECT_SHA256=$sourceObjectSha"
    Write-Host "ARCHIVE_MEMBER_SHA256=$extractedObjectSha"

    if ($sourceObjectSha -ne $extractedObjectSha) {
        throw "A12_JW_SD_ARCHIVE_MEMBER_BYTE_PARITY_FAILED"
    }

    Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=PASS"

    $candidateSha = Get-Sha256Lower $candidateArchive
    $candidateBytes = (Get-Item -LiteralPath $candidateArchive).Length
    $archiveChanged = $candidateSha -ne $oldSha

    Copy-Item -LiteralPath $candidateArchive -Destination $candidateEvidenceArchive -Force

    Write-Host "CANDIDATE_ARCHIVE_BYTES=$candidateBytes"
    Write-Host "CANDIDATE_ARCHIVE_SHA256=$candidateSha"
    Write-Host "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"

    Write-Host ""
    Write-Host "=== CANDIDATE PRECOMPILED JW_SD LINK ==="

    $candidateRun = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments @(
        "compile",
        "--fqbn", $Fqbn,
        "-j", "0",
        "-v",
        "--clean",
        "--build-path", $candidateBuild,
        "--library", $candidateRoot,
        "--libraries", $repoLibraries,
        $sketchRoot
    )

    $candidateRun.Output | Set-Content -LiteralPath $candidateLog -Encoding UTF8
    Write-Host "CANDIDATE_COMPILE_EXIT=$($candidateRun.ExitCode)"

    if ($candidateRun.ExitCode -ne 0) {
        $candidateRun.Output | Select-Object -Last 160 | ForEach-Object { Write-Host $_ }
        throw "A12_JW_SD_CANDIDATE_COMPILE_FAILED"
    }

    Assert-PrecompiledBuild -Lines $candidateRun.Output -BuildPath $candidateBuild -ExpectedRoot $candidateRoot -Label "CANDIDATE"

    Write-Host ""
    Write-Host "=== PLACE JW_SD CANDIDATE IN OFFICIAL WORKTREE ==="

    Copy-Item -LiteralPath $candidateArchive -Destination $archivePath -Force

    $officialSha = Get-Sha256Lower $archivePath
    $officialBytes = (Get-Item -LiteralPath $archivePath).Length

    Write-Host "OFFICIAL_CANDIDATE_BYTES=$officialBytes"
    Write-Host "OFFICIAL_CANDIDATE_SHA256=$officialSha"

    if ($officialSha -ne $candidateSha -or $officialBytes -ne $candidateBytes) {
        throw "A12_JW_SD_OFFICIAL_CANDIDATE_IDENTITY_FAILED"
    }

    [string[]]$finalDirty = @(
        & git -C $repo diff --name-only |
            ForEach-Object { $_.Replace("\", "/") } |
            Sort-Object
    )
    [string[]]$finalStaged = @(& git -C $repo diff --cached --name-only)

    Write-Host "FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
    $finalDirty | ForEach-Object { Write-Host "FINAL_TRACKED_DIRTY=$_" }
    Write-Host "FINAL_STAGED_COUNT=$($finalStaged.Count)"

    $normalizedArchiveRel = $archiveRel.Replace("\", "/")

    if ($finalDirty.Count -ne 1 -or $finalDirty[0] -ne $normalizedArchiveRel) {
        throw "A12_JW_SD_FINAL_DIRTY_SCOPE_INVALID"
    }
    if ($finalStaged.Count -ne 0) {
        throw "A12_JW_SD_FINAL_INDEX_NOT_CLEAN"
    }

    @(
        "ALPHA12_JW_SD_PRECOMPILED_REFRESH=PASS"
        "BRANCH=$branch"
        "HEAD_SOURCE=$head"
        "SOURCE_LAST_COMMIT=$sourceCommit"
        "ARCHIVE_PREVIOUS_COMMIT=$archiveCommit"
        "OLD_ARCHIVE_BYTES=$oldBytes"
        "OLD_ARCHIVE_SHA256=$oldSha"
        "NEW_ARCHIVE_BYTES=$candidateBytes"
        "NEW_ARCHIVE_SHA256=$candidateSha"
        "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"
        "ARCHIVE_MEMBER_COUNT=1"
        "ARCHIVE_MEMBER=JW_SD.cpp.o"
        "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
        "DATALOG_MANAGER=ON"
        "CARD_LIFECYCLE_RECOVERY=ON"
        "SPI_LOCK_CALLBACKS=ON"
        "CANDIDATE_LINK=PASS"
        "FINAL_TRACKED_DIRTY=$normalizedArchiveRel"
        "PHYSICAL_UPLOAD_PERFORMED=NO"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "SUMMARY.txt") -Encoding UTF8

    $success = $true

    Write-Host ""
    Write-Host "=============================================================================="
    Write-Host "ALPHA12_JW_SD_PRECOMPILED_REFRESH=PASS"
    Write-Host "NEW_ARCHIVE_BYTES=$candidateBytes"
    Write-Host "NEW_ARCHIVE_SHA256=$candidateSha"
    Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
    Write-Host "DATALOG_MANAGER=ON"
    Write-Host "CARD_LIFECYCLE_RECOVERY=ON"
    Write-Host "SPI_LOCK_CALLBACKS=ON"
    Write-Host "CANDIDATE_LINK=PASS"
    Write-Host "FINAL_TRACKED_DIRTY=$normalizedArchiveRel"
    Write-Host "PHYSICAL_UPLOAD_PERFORMED=NO"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
finally {
    if (-not $success) {
        Write-Host "A12_JW_SD_ROLLBACK=START"

        if (Test-Path -LiteralPath $backup) {
            Copy-Item -LiteralPath $backup -Destination $archivePath -Force
        }

        $restoredSha = Get-Sha256Lower $archivePath
        Write-Host "A12_JW_SD_ROLLBACK_SHA256=$restoredSha"

        if ($restoredSha -ne $oldSha) {
            throw "A12_JW_SD_ROLLBACK_FAILED"
        }

        Write-Host "A12_JW_SD_ROLLBACK=PASS"
    }

    if (Test-Path -LiteralPath $backup) {
        Remove-Item -LiteralPath $backup -Force
    }
}
