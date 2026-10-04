param(
    [string]$ArduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic",
    [string]$ResultRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"

$libraryRel = "JWPLC/2.1.0/libraries/JWPLC_Display"
$libraryRoot = Join-Path $repo $libraryRel
$srcRoot = Join-Path $libraryRoot "src"
$headerPath = Join-Path $srcRoot "JWPLC_Display.h"
$apiHeaderPath = Join-Path $srcRoot "JWPLC_Display_API.h"
$cppPath = Join-Path $srcRoot "JWPLC_Display.cpp"
$archiveRel = "$libraryRel/src/esp32/libJWPLC_Display.a"
$archivePath = Join-Path $repo $archiveRel
$repoLibraries = Join-Path $repo "JWPLC\2.1.0\libraries"

[string[]]$expectedMembers = @(
    "JWPLC_Display.cpp.o",
    "JWPLC_Display_H3E1_Profile.cpp.o",
    "JWPLC_IdleScreen.cpp.o",
    "JWPLC_UI.cpp.o",
    "JWPLC_UI_API.cpp.o",
    "JWPLC_UI_Pages.cpp.o",
    "JWPLC_UI_PixelMap.cpp.o"
) | Sort-Object

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

    throw "A12_DISPLAY_ARCHIVER_NOT_FOUND"
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
        if ($line -match '^Using library JWPLC_Display at version .+ in folder: (.+)$') {
            $actual = [IO.Path]::GetFullPath($Matches[1].Trim()).TrimEnd('\', '/')
            if ($actual -ieq $normalizedExpected) {
                $selected = $true
            }
        }
    }

    Write-Host "$($Label)_DISPLAY_SELECTED=$selected"

    if (-not $selected) {
        throw "A12_DISPLAY_$($Label)_LIBRARY_NOT_SELECTED"
    }
}

function Get-DisplayObjects {
    param([string]$BuildPath)

    return @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $expectedMembers -contains $_.Name }
    )
}

function Assert-SourceBuild {
    param(
        [string[]]$Lines,
        [string]$BuildPath,
        [string]$ExpectedRoot,
        [string]$Label
    )

    Assert-LibrarySelected -Lines $Lines -ExpectedRoot $ExpectedRoot -Label $Label

    $objects = @(Get-DisplayObjects -BuildPath $BuildPath)
    [string[]]$actualNames = @($objects | ForEach-Object { $_.Name } | Sort-Object -Unique)

    $precompiled = @(
        $Lines | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match '\bJWPLC_Display\b')
        }
    ).Count -gt 0

    Write-Host "$($Label)_SOURCE_OBJECT_COUNT=$($objects.Count)"
    Write-Host "$($Label)_SOURCE_MEMBER_SET=$($actualNames -join ',')"
    Write-Host "$($Label)_PRECOMPILED_MARKER=$(if ($precompiled) { 'YES' } else { 'NO' })"

    if ($objects.Count -ne $expectedMembers.Count) {
        throw "A12_DISPLAY_$($Label)_SOURCE_OBJECT_COUNT_INVALID"
    }

    if (Compare-Object -ReferenceObject $expectedMembers -DifferenceObject $actualNames) {
        throw "A12_DISPLAY_$($Label)_SOURCE_MEMBER_SET_INVALID"
    }

    if ($precompiled) {
        throw "A12_DISPLAY_$($Label)_SOURCE_POLICY_FAILED"
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

    $objects = @(Get-DisplayObjects -BuildPath $BuildPath)

    $precompiled = @(
        $Lines | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match '\bJWPLC_Display\b')
        }
    ).Count -gt 0

    Write-Host "$($Label)_SOURCE_OBJECT_COUNT=$($objects.Count)"
    Write-Host "$($Label)_PRECOMPILED_MARKER=$(if ($precompiled) { 'YES' } else { 'NO' })"

    if ($objects.Count -ne 0 -or -not $precompiled) {
        throw "A12_DISPLAY_$($Label)_PRECOMPILED_POLICY_FAILED"
    }
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

if ($branch -ne $expectedBranch) {
    throw "A12_DISPLAY_BRANCH_MISMATCH expected=$expectedBranch actual=$branch"
}

[string[]]$entryDirty = @(& git -C $repo diff --name-only)
[string[]]$entryStaged = @(& git -C $repo diff --cached --name-only)

if ($entryDirty.Count -ne 0) {
    $entryDirty | ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }
    throw "A12_DISPLAY_TRACKED_TREE_NOT_CLEAN"
}
if ($entryStaged.Count -ne 0) {
    throw "A12_DISPLAY_INDEX_NOT_CLEAN"
}

& git -C $repo diff --check
if ($LASTEXITCODE -ne 0) {
    throw "A12_DISPLAY_GIT_DIFF_CHECK_FAILED"
}

foreach ($required in @(
    $libraryRoot,
    $srcRoot,
    $headerPath,
    $apiHeaderPath,
    $cppPath,
    $archivePath,
    $repoLibraries
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "A12_DISPLAY_REQUIRED_PATH_MISSING=$required"
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
    throw "A12_DISPLAY_ARDUINO_CLI_NOT_FOUND"
}

[string[]]$officialCpp = @(
    Get-ChildItem -LiteralPath $srcRoot -File |
        Where-Object { $_.Extension -eq ".cpp" } |
        ForEach-Object { "$($_.Name).o" } |
        Sort-Object
)

Write-Host "DISPLAY_CURRENT_CPP_COUNT=$($officialCpp.Count)"
Write-Host "DISPLAY_EXPECTED_ARCHIVE_MEMBER_COUNT=$($expectedMembers.Count)"

if (Compare-Object -ReferenceObject $expectedMembers -DifferenceObject $officialCpp) {
    $officialCpp | ForEach-Object { Write-Host "DISPLAY_CURRENT_MEMBER=$_" }
    throw "A12_DISPLAY_SOURCE_TU_SET_CHANGED"
}

$legacyHits = @()
Get-ChildItem -LiteralPath $srcRoot -File |
    Where-Object { $_.Extension -in @(".h", ".cpp") } |
    ForEach-Object {
        $file = $_
        $lineNo = 0
        Get-Content -LiteralPath $file.FullName | ForEach-Object {
            $lineNo++
            if ($_ -match 'Adafruit_ST7789|ST77XX_') {
                $legacyHits += ($file.Name + ":" + [string]$lineNo + ":" + $_.Trim())
            }
        }
    }

Write-Host "DISPLAY_LEGACY_BACKEND_REF_COUNT=$($legacyHits.Count)"
if ($legacyHits.Count -ne 0) {
    $legacyHits | ForEach-Object { Write-Host "DISPLAY_LEGACY_BACKEND_REF=$_" }
    throw "A12_DISPLAY_LEGACY_BACKEND_REF_FOUND"
}

$headerText = [IO.File]::ReadAllText($headerPath)
$apiHeaderText = [IO.File]::ReadAllText($apiHeaderPath)
$cppText = [IO.File]::ReadAllText($cppPath)

foreach ($contract in @(
    "#include <JWPLC_TFT.h>",
    "JWPLC_TFTClass &display()"
)) {
    if (-not $headerText.Contains($contract)) {
        throw "A12_DISPLAY_HEADER_CONTRACT_MISSING=$contract"
    }
}

foreach ($contract in @(
    "JWPLC_TFTClass &tft()",
    "JWPLC_TFTClass &display()",
    "bool setErrCode(const char *code)"
)) {
    if (-not $apiHeaderText.Contains($contract)) {
        throw "A12_DISPLAY_API_CONTRACT_MISSING=$contract"
    }
}

foreach ($contract in @(
    "JWPLC_TFT.beginBatch",
    "JWPLC_TFT.endBatch",
    "JWPLC_TFT.fillScreen",
    "JWPLC_TFTClass &JWPLC_DisplayClass::tft()",
    "JWPLC_TFTClass &JWPLC_DisplayClass::display()"
)) {
    if (-not $cppText.Contains($contract)) {
        throw "A12_DISPLAY_SOURCE_CONTRACT_MISSING=$contract"
    }
}

Write-Host "A12_DISPLAY_CURRENT_SOURCE_CONTRACT=PASS"
Write-Host "TFT_BACKEND=JWPLC_TFT"
Write-Host "LEGACY_ADAFRUIT_ST7789_REFS=0"
Write-Host "LEGACY_ST77XX_REFS=0"

$sourceCommit = (& git -C $repo log -1 --format=%H -- "$libraryRel/src").Trim()
$sourceDate = (& git -C $repo log -1 --format=%cI -- "$libraryRel/src").Trim()
$archiveCommit = (& git -C $repo log -1 --format=%H -- $archiveRel).Trim()
$archiveDate = (& git -C $repo log -1 --format=%cI -- $archiveRel).Trim()

$oldSha = Get-Sha256Lower $archivePath
$oldBytes = (Get-Item -LiteralPath $archivePath).Length

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo "tools\alpha12\results\display_precompiled_refresh_$stamp"
}
New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$runRoot = Join-Path $env:TEMP ("jwplc_a12_display_refresh_" + $stamp)
$sketchRoot = Join-Path $runRoot "display_gate"
$sourceRoot = Join-Path $runRoot "source-libraries\JWPLC_Display"
$sourceSrc = Join-Path $sourceRoot "src"
$sourceBuild = Join-Path $runRoot "source-build"
$sourceLog = Join-Path $ResultRoot "source.log"

$candidateRoot = Join-Path $runRoot "candidate-libraries\JWPLC_Display"
$candidateSrc = Join-Path $candidateRoot "src"
$candidateArchiveDir = Join-Path $candidateSrc "esp32"
$candidateArchive = Join-Path $candidateArchiveDir "libJWPLC_Display.a"
$candidateEvidenceArchive = Join-Path $ResultRoot "libJWPLC_Display-candidate.a"
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

$sketchPath = Join-Path $sketchRoot "display_gate.ino"
$sketchText = @'
#include <Arduino.h>
#include <JWPLC_Display.h>

void setup()
{
    JWPLC_Display.setIdleTimeoutMs(15000);
    JWPLC_Display.setUserRefreshPeriodMs(40);
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
    JWPLC_Display.setUserPageCount(2);
    JWPLC_Display.clearFields();
    JWPLC_Display.clearPixelMaps();
    JWPLC_Display.setRunLed(true);
    (void)JWPLC_Display.setErrCode("E12");

    JWPLC_TFTClass &tft = JWPLC_Display.tft();
    tft.fillScreen(JWPLC_TFT_BLACK);

    volatile uint32_t idleMs = JWPLC_Display.idleTimeoutMs();
    volatile uint8_t pages = JWPLC_Display.userPageCount();
    volatile bool err = JWPLC_Display.errLed();

    (void)idleMs;
    (void)pages;
    (void)err;
}

void loop()
{
}
'@

$utf8NoBom = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($sketchPath, $sketchText, $utf8NoBom)

Get-ChildItem -LiteralPath $srcRoot -File |
    Copy-Item -Destination $sourceSrc -Force

Get-ChildItem -LiteralPath $srcRoot -File |
    Where-Object { $_.Extension -eq ".h" } |
    Copy-Item -Destination $candidateSrc -Force

$sourceProperties = @'
name=JWPLC_Display
version=1.0.1-alpha12-source
author=JW Control
maintainer=JW Control
sentence=Alpha12 source qualification for JWPLC_Display.
paragraph=Temporary source-only qualification library.
category=Display
architectures=esp32
includes=JWPLC_Display.h
depends=JWPLC_TFT,JW_MatrixButtons,JWPLC_GlobalPeripherals
'@
[IO.File]::WriteAllText(
    (Join-Path $sourceRoot "library.properties"),
    $sourceProperties,
    $utf8NoBom
)

$candidateProperties = @'
name=JWPLC_Display
version=1.0.1-alpha12-candidate
author=JW Control
maintainer=JW Control
sentence=Alpha12 precompiled candidate for JWPLC_Display.
paragraph=Temporary precompiled qualification library.
category=Display
architectures=esp32
includes=JWPLC_Display.h
depends=JWPLC_TFT,JW_MatrixButtons,JWPLC_GlobalPeripherals
precompiled=full
'@
[IO.File]::WriteAllText(
    (Join-Path $candidateRoot "library.properties"),
    $candidateProperties,
    $utf8NoBom
)

$backup = Join-Path $env:TEMP ("a12_display_archive_backup_" + $stamp + ".a")
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
    "EXPECTED_MEMBER_COUNT=$($expectedMembers.Count)"
    "EXPECTED_MEMBERS=$($expectedMembers -join ',')"
    "TFT_BACKEND=JWPLC_TFT"
    "LEGACY_ADAFRUIT_ST7789_REFS=0"
    "LEGACY_ST77XX_REFS=0"
) | Set-Content -LiteralPath (Join-Path $ResultRoot "MANIFEST.txt") -Encoding UTF8

Write-Host "=============================================================================="
Write-Host " ALPHA12 - JWPLC_DISPLAY PRECOMPILED REFRESH"
Write-Host "=============================================================================="
Write-Host "BRANCH=$branch"
Write-Host "HEAD=$head"
Write-Host "SOURCE_LAST_COMMIT=$sourceCommit"
Write-Host "SOURCE_LAST_DATE=$sourceDate"
Write-Host "ARCHIVE_LAST_COMMIT=$archiveCommit"
Write-Host "ARCHIVE_LAST_DATE=$archiveDate"
Write-Host "OLD_ARCHIVE_BYTES=$oldBytes"
Write-Host "OLD_ARCHIVE_SHA256=$oldSha"
Write-Host "EXPECTED_MEMBER_COUNT=$($expectedMembers.Count)"
Write-Host "RESULT_ROOT=$ResultRoot"

$success = $false

try {
    Write-Host ""
    Write-Host "=== SOURCE-ONLY JWPLC_DISPLAY BUILD ==="

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
        $sourceRun.Output | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }
        throw "A12_DISPLAY_SOURCE_COMPILE_FAILED"
    }

    Assert-SourceBuild -Lines $sourceRun.Output -BuildPath $sourceBuild -ExpectedRoot $sourceRoot -Label "SOURCE"

    $archiver = Resolve-Archiver -Lines $sourceRun.Output
    Write-Host "ARCHIVER=$archiver"

    $sourceObjects = @(Get-DisplayObjects -BuildPath $sourceBuild | Sort-Object Name)

    Write-Host ""
    Write-Host "=== CREATE JWPLC_DISPLAY CANDIDATE ARCHIVE ==="

    [string[]]$archiveArgs = @("crs", $candidateArchive) + @(
        $sourceObjects | ForEach-Object { $_.FullName }
    )

    $archiveRun = Invoke-NativeCaptured -FilePath $archiver -Arguments $archiveArgs

    if ($archiveRun.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $candidateArchive)) {
        throw "A12_DISPLAY_ARCHIVE_CREATE_FAILED"
    }

    $listRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $candidateArchive)
    if ($listRun.ExitCode -ne 0) {
        throw "A12_DISPLAY_ARCHIVE_LIST_FAILED"
    }

    [string[]]$members = @(
        $listRun.Output |
            ForEach-Object { $_.Trim() } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object
    )

    Write-Host "ARCHIVE_MEMBER_COUNT=$($members.Count)"
    $members | ForEach-Object { Write-Host "ARCHIVE_MEMBER=$_" }

    if ($members.Count -ne $expectedMembers.Count) {
        throw "A12_DISPLAY_ARCHIVE_MEMBER_COUNT_INVALID"
    }

    if (Compare-Object -ReferenceObject $expectedMembers -DifferenceObject $members) {
        throw "A12_DISPLAY_ARCHIVE_MEMBER_SET_INVALID"
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
        throw "A12_DISPLAY_ARCHIVE_EXTRACT_FAILED"
    }

    $parityPass = $true
    foreach ($member in $expectedMembers) {
        $sourceObject = @($sourceObjects | Where-Object { $_.Name -eq $member })
        if ($sourceObject.Count -ne 1) {
            throw "A12_DISPLAY_SOURCE_OBJECT_LOOKUP_FAILED=$member"
        }

        $extractedObject = Join-Path $extractDir $member
        if (-not (Test-Path -LiteralPath $extractedObject)) {
            throw "A12_DISPLAY_EXTRACTED_OBJECT_MISSING=$member"
        }

        $sourceSha = Get-Sha256Lower $sourceObject[0].FullName
        $archiveSha = Get-Sha256Lower $extractedObject

        Write-Host "MEMBER_PARITY=$member source=$sourceSha archive=$archiveSha"

        if ($sourceSha -ne $archiveSha) {
            $parityPass = $false
        }
    }

    if (-not $parityPass) {
        throw "A12_DISPLAY_ARCHIVE_MEMBER_BYTE_PARITY_FAILED"
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
    Write-Host "=== CANDIDATE PRECOMPILED JWPLC_DISPLAY LINK ==="

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
        $candidateRun.Output | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }
        throw "A12_DISPLAY_CANDIDATE_COMPILE_FAILED"
    }

    Assert-PrecompiledBuild -Lines $candidateRun.Output -BuildPath $candidateBuild -ExpectedRoot $candidateRoot -Label "CANDIDATE"

    $sourceWarnings = @($sourceRun.Output | Where-Object { $_ -match '(?i)\bwarning:' }).Count
    $candidateWarnings = @($candidateRun.Output | Where-Object { $_ -match '(?i)\bwarning:' }).Count
    $sourceErrors = @($sourceRun.Output | Where-Object { $_ -match '(?i)\berror:' }).Count
    $candidateErrors = @($candidateRun.Output | Where-Object { $_ -match '(?i)\berror:' }).Count

    Write-Host "SOURCE_WARNING_LINES=$sourceWarnings"
    Write-Host "CANDIDATE_WARNING_LINES=$candidateWarnings"
    Write-Host "SOURCE_ERROR_LINES=$sourceErrors"
    Write-Host "CANDIDATE_ERROR_LINES=$candidateErrors"

    Write-Host ""
    Write-Host "=== PLACE JWPLC_DISPLAY CANDIDATE IN OFFICIAL WORKTREE ==="

    Copy-Item -LiteralPath $candidateArchive -Destination $archivePath -Force

    $officialSha = Get-Sha256Lower $archivePath
    $officialBytes = (Get-Item -LiteralPath $archivePath).Length

    Write-Host "OFFICIAL_CANDIDATE_BYTES=$officialBytes"
    Write-Host "OFFICIAL_CANDIDATE_SHA256=$officialSha"

    if ($officialSha -ne $candidateSha -or $officialBytes -ne $candidateBytes) {
        throw "A12_DISPLAY_OFFICIAL_CANDIDATE_IDENTITY_FAILED"
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
        throw "A12_DISPLAY_FINAL_DIRTY_SCOPE_INVALID"
    }
    if ($finalStaged.Count -ne 0) {
        throw "A12_DISPLAY_FINAL_INDEX_NOT_CLEAN"
    }

    @(
        "ALPHA12_DISPLAY_PRECOMPILED_REFRESH=PASS"
        "BRANCH=$branch"
        "HEAD_SOURCE=$head"
        "SOURCE_LAST_COMMIT=$sourceCommit"
        "ARCHIVE_PREVIOUS_COMMIT=$archiveCommit"
        "OLD_ARCHIVE_BYTES=$oldBytes"
        "OLD_ARCHIVE_SHA256=$oldSha"
        "NEW_ARCHIVE_BYTES=$candidateBytes"
        "NEW_ARCHIVE_SHA256=$candidateSha"
        "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"
        "ARCHIVE_MEMBER_COUNT=$($expectedMembers.Count)"
        "ARCHIVE_MEMBERS=$($expectedMembers -join ',')"
        "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
        "TFT_BACKEND=JWPLC_TFT"
        "LEGACY_ADAFRUIT_ST7789_REFS=0"
        "LEGACY_ST77XX_REFS=0"
        "CANDIDATE_LINK=PASS"
        "SOURCE_WARNING_LINES=$sourceWarnings"
        "CANDIDATE_WARNING_LINES=$candidateWarnings"
        "SOURCE_ERROR_LINES=$sourceErrors"
        "CANDIDATE_ERROR_LINES=$candidateErrors"
        "FINAL_TRACKED_DIRTY=$normalizedArchiveRel"
        "PHYSICAL_UPLOAD_PERFORMED=NO"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "SUMMARY.txt") -Encoding UTF8

    $success = $true

    Write-Host ""
    Write-Host "=============================================================================="
    Write-Host "ALPHA12_DISPLAY_PRECOMPILED_REFRESH=PASS"
    Write-Host "NEW_ARCHIVE_BYTES=$candidateBytes"
    Write-Host "NEW_ARCHIVE_SHA256=$candidateSha"
    Write-Host "ARCHIVE_MEMBER_COUNT=$($expectedMembers.Count)"
    Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
    Write-Host "TFT_BACKEND=JWPLC_TFT"
    Write-Host "LEGACY_BACKEND_REFS=0"
    Write-Host "CANDIDATE_LINK=PASS"
    Write-Host "SOURCE_WARNING_LINES=$sourceWarnings"
    Write-Host "CANDIDATE_WARNING_LINES=$candidateWarnings"
    Write-Host "SOURCE_ERROR_LINES=$sourceErrors"
    Write-Host "CANDIDATE_ERROR_LINES=$candidateErrors"
    Write-Host "FINAL_TRACKED_DIRTY=$normalizedArchiveRel"
    Write-Host "PHYSICAL_UPLOAD_PERFORMED=NO"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
finally {
    if (-not $success) {
        Write-Host "A12_DISPLAY_ROLLBACK=START"

        if (Test-Path -LiteralPath $backup) {
            Copy-Item -LiteralPath $backup -Destination $archivePath -Force
        }

        $restoredSha = Get-Sha256Lower $archivePath
        Write-Host "A12_DISPLAY_ROLLBACK_SHA256=$restoredSha"

        if ($restoredSha -ne $oldSha) {
            throw "A12_DISPLAY_ROLLBACK_FAILED"
        }

        Write-Host "A12_DISPLAY_ROLLBACK=PASS"
    }

    if (Test-Path -LiteralPath $backup) {
        Remove-Item -LiteralPath $backup -Force
    }
}
