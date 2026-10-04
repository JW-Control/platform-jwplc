param(
    [string]$ArduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic",
    [string]$ResultRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"

$libraryRel = "JWPLC/2.1.0/libraries/JWPLC_TFT"
$libraryRoot = Join-Path $repo $libraryRel
$srcRoot = Join-Path $libraryRoot "src"
$headerPath = Join-Path $srcRoot "JWPLC_TFT.h"
$cppPath = Join-Path $srcRoot "JWPLC_TFT.cpp"
$propertiesPath = Join-Path $libraryRoot "library.properties"
$archiveRel = "$libraryRel/src/esp32/libJWPLC_TFT.a"
$archivePath = Join-Path $repo $archiveRel
$repoLibraries = Join-Path $repo "JWPLC\2.1.0\libraries"
$displaySketch = Join-Path $repo "JWPLC\2.1.0\libraries\JWPLC_Display\examples\04.Display_TFT_Direct"
$emptySketch = Join-Path $repo "tools\build-speed-benchmark\sketches\01_empty"

[string[]]$expectedMembers = @(
    "JWPLC_TFT.cpp.o",
    "TFT_eSPI.cpp.o"
) | Sort-Object

function Get-Sha256Lower {
    param([string]$Path)

    $stream = [System.IO.File]::OpenRead($Path)
    $sha256 = [System.Security.Cryptography.SHA256]::Create()

    try {
        return (
            [System.BitConverter]::ToString(
                $sha256.ComputeHash($stream)
            ).Replace("-", "").ToLowerInvariant()
        )
    }
    finally {
        $stream.Dispose()
        $sha256.Dispose()
    }
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

    throw "A12_TFT_REQUAL_ARCHIVER_NOT_FOUND"
}

function Assert-LibrarySelected {
    param(
        [string[]]$Lines,
        [string]$LibraryName,
        [string]$ExpectedRoot,
        [string]$Label
    )

    $normalizedExpected = [IO.Path]::GetFullPath($ExpectedRoot).TrimEnd('\', '/')
    $selected = $false

    foreach ($line in $Lines) {
        if ($line -match ("^Using library " + [regex]::Escape($LibraryName) + " at version .+ in folder: (.+)$")) {
            $actual = [IO.Path]::GetFullPath($Matches[1].Trim()).TrimEnd('\', '/')
            if ($actual -ieq $normalizedExpected) {
                $selected = $true
            }
        }
    }

    Write-Host ($Label + "_" + $LibraryName + "_SELECTED=" + [string]$selected)

    if (-not $selected) {
        throw ("A12_TFT_REQUAL_" + $Label + "_" + $LibraryName + "_NOT_SELECTED")
    }
}

function Get-NamedObjects {
    param(
        [string]$BuildPath,
        [string]$ObjectName
    )

    return @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq $ObjectName }
    )
}

function Test-PrecompiledMarker {
    param(
        [string[]]$Lines,
        [string]$LibraryName
    )

    return @(
        $Lines | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match [regex]::Escape($LibraryName))
        }
    ).Count -gt 0
}

function Get-TftEspiSelection {
    param([string[]]$Lines)

    $selectionLines = @(
        $Lines | Where-Object {
            $_ -match '^Using library TFT_eSPI at version .+ in folder: .+$'
        }
    )

    if ($selectionLines.Count -eq 0) {
        return $null
    }

    if ($selectionLines.Count -ne 1) {
        throw "A12_TFT_REQUAL_TFT_ESPI_SELECTION_AMBIGUOUS"
    }

    $selectionLine = $selectionLines[0]
    if ($selectionLine -notmatch '^Using library TFT_eSPI at version (?<version>\S+) in folder: (?<folder>.+)$') {
        throw "A12_TFT_REQUAL_TFT_ESPI_SELECTION_PARSE_FAILED"
    }

    return [pscustomobject]@{
        Version = $Matches["version"].Trim()
        Folder = $Matches["folder"].Trim()
        Line = $selectionLine
    }
}

function Invoke-CandidateCase {
    param(
        [string]$Label,
        [string]$SketchPath,
        [string]$CandidateRoot,
        [string]$BuildPath,
        [string]$LogPath
    )

    Write-Host ""
    Write-Host ("=== CANDIDATE CASE " + $Label + " ===")

    $run = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments @(
        "compile",
        "--fqbn", $Fqbn,
        "-j", "0",
        "-v",
        "--clean",
        "--build-path", $BuildPath,
        "--library", $CandidateRoot,
        "--libraries", $repoLibraries,
        $SketchPath
    )

    $run.Output | Set-Content -LiteralPath $LogPath -Encoding UTF8
    Write-Host ($Label + "_COMPILE_EXIT=" + [string]$run.ExitCode)

    if ($run.ExitCode -ne 0) {
        $run.Output | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }
        throw ("A12_TFT_REQUAL_" + $Label + "_COMPILE_FAILED")
    }

    Assert-LibrarySelected -Lines $run.Output -LibraryName "JWPLC_TFT" -ExpectedRoot $CandidateRoot -Label $Label

    $precompiled = Test-PrecompiledMarker -Lines $run.Output -LibraryName "JWPLC_TFT"
    $tftObjects = @(Get-NamedObjects -BuildPath $BuildPath -ObjectName "JWPLC_TFT.cpp.o")
    $backendObjects = @(Get-NamedObjects -BuildPath $BuildPath -ObjectName "TFT_eSPI.cpp.o")
    $tftEspiSelection = Get-TftEspiSelection -Lines $run.Output

    $warningCount = @($run.Output | Where-Object { $_ -match '(?i)\bwarning:' }).Count
    $errorCount = @($run.Output | Where-Object { $_ -match '(?i)\berror:' }).Count

    Write-Host ($Label + "_TFT_SOURCE_OBJECT_COUNT=" + [string]$tftObjects.Count)
    Write-Host ($Label + "_TFT_ESPI_SOURCE_OBJECT_COUNT=" + [string]$backendObjects.Count)
    Write-Host ($Label + "_TFT_PRECOMPILED_MARKER=" + $(if ($precompiled) { "YES" } else { "NO" }))
    Write-Host ($Label + "_GLOBAL_TFT_ESPI_SELECTED=" + $(if ($null -eq $tftEspiSelection) { "NO" } else { "YES" }))
    Write-Host ($Label + "_WARNING_LINES=" + [string]$warningCount)
    Write-Host ($Label + "_ERROR_LINES=" + [string]$errorCount)

    if (-not $precompiled) {
        throw ("A12_TFT_REQUAL_" + $Label + "_PRECOMPILED_MARKER_MISSING")
    }
    if ($tftObjects.Count -ne 0) {
        throw ("A12_TFT_REQUAL_" + $Label + "_TFT_SOURCE_RECOMPILED")
    }
    if ($backendObjects.Count -ne 0) {
        throw ("A12_TFT_REQUAL_" + $Label + "_TFT_ESPI_SOURCE_RECOMPILED")
    }
    if ($null -ne $tftEspiSelection) {
        Write-Host ($Label + "_TFT_ESPI_SELECTION_LINE=" + $tftEspiSelection.Line)
        throw ("A12_TFT_REQUAL_" + $Label + "_GLOBAL_TFT_ESPI_DEPENDENCY")
    }
    if ($warningCount -ne 0 -or $errorCount -ne 0) {
        throw ("A12_TFT_REQUAL_" + $Label + "_COMPILER_DIAGNOSTICS")
    }

    return [pscustomobject]@{
        Label = $Label
        WarningCount = $warningCount
        ErrorCount = $errorCount
    }
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

if ($branch -ne $expectedBranch) {
    throw "A12_TFT_REQUAL_BRANCH_MISMATCH"
}

[string[]]$entryDirty = @(& git -C $repo diff --name-only)
[string[]]$entryStaged = @(& git -C $repo diff --cached --name-only)

if ($entryDirty.Count -ne 0) {
    $entryDirty | ForEach-Object { Write-Host ("ENTRY_DIRTY=" + $_) }
    throw "A12_TFT_REQUAL_TRACKED_TREE_NOT_CLEAN"
}
if ($entryStaged.Count -ne 0) {
    throw "A12_TFT_REQUAL_INDEX_NOT_CLEAN"
}

& git -C $repo diff --check
if ($LASTEXITCODE -ne 0) {
    throw "A12_TFT_REQUAL_GIT_DIFF_CHECK_FAILED"
}

foreach ($required in @(
    $libraryRoot,
    $srcRoot,
    $headerPath,
    $cppPath,
    $propertiesPath,
    $archivePath,
    $repoLibraries,
    $displaySketch,
    $emptySketch
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw ("A12_TFT_REQUAL_REQUIRED_PATH_MISSING=" + $required)
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
    $cliCommand = Get-Command "arduino-cli" -ErrorAction SilentlyContinue
    if ($null -ne $cliCommand) {
        $ArduinoCli = $cliCommand.Source
    }
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "A12_TFT_REQUAL_ARDUINO_CLI_NOT_FOUND"
}

$officialProperties = [IO.File]::ReadAllText($propertiesPath)
if ($officialProperties -match '(?m)^\s*precompiled\s*=\s*full\s*$') {
    throw "A12_TFT_REQUAL_OFFICIAL_LIBRARY_NOT_SOURCE_FIRST"
}

$sourceCommit = (& git -C $repo log -1 --format=%H -- "$libraryRel/src").Trim()
$sourceDate = (& git -C $repo log -1 --format=%cI -- "$libraryRel/src").Trim()
$archiveCommit = (& git -C $repo log -1 --format=%H -- $archiveRel).Trim()
$archiveDate = (& git -C $repo log -1 --format=%cI -- $archiveRel).Trim()

$archiveSha = Get-Sha256Lower $archivePath
$archiveBytes = (Get-Item -LiteralPath $archivePath).Length

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo "tools\alpha12\results\tft_precompiled_requalify_$stamp"
}
New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$runRoot = Join-Path $env:TEMP ("jwplc_a12_tft_requal_" + $stamp)
$sourceBuild = Join-Path $runRoot "source-build"
$sourceLog = Join-Path $ResultRoot "source.log"
$extractDir = Join-Path $runRoot "archive-members"
$candidateRoot = Join-Path $runRoot "candidate-libraries\JWPLC_TFT"
$candidateSrc = Join-Path $candidateRoot "src"
$candidateArchiveDir = Join-Path $candidateSrc "esp32"
$directSketch = Join-Path $runRoot "tft_direct"
$directBuild = Join-Path $runRoot "candidate-direct-build"
$displayBuild = Join-Path $runRoot "candidate-display-build"
$autoloadBuild = Join-Path $runRoot "candidate-autoload-build"

foreach ($dir in @(
    $sourceBuild,
    $extractDir,
    $candidateArchiveDir,
    $directSketch,
    $directBuild,
    $displayBuild,
    $autoloadBuild
)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

$utf8NoBom = [Text.UTF8Encoding]::new($false)

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $candidateSrc "JWPLC_TFT.h") -Force
Copy-Item -LiteralPath $archivePath -Destination (Join-Path $candidateArchiveDir "libJWPLC_TFT.a") -Force

$candidateProperties = @'
name=JWPLC_TFT
version=0.1.0-alpha12-requal
author=JW Control
maintainer=JW Control
sentence=Alpha12 self-contained precompiled TFT requalification.
paragraph=Temporary qualification library with no backend source files.
category=Display
architectures=esp32
includes=JWPLC_TFT.h
depends=SPI
precompiled=full
'@
[IO.File]::WriteAllText(
    (Join-Path $candidateRoot "library.properties"),
    $candidateProperties,
    $utf8NoBom
)

$directSketchText = @'
#include <Arduino.h>
#include <JWPLC_TFT.h>

void setup()
{
    JWPLC_TFTPanelInfo info = JWPLC_TFT.panelInfo();
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE, JWPLC_TFT_BLACK);
    JWPLC_TFT.setCursor(0, 0);
    (void)JWPLC_TFT.width();
    (void)JWPLC_TFT.height();
    (void)JWPLC_TFT.rotation();
    (void)JWPLC_TFT.textWidth("P6");
    (void)JWPLC_TFT.fontHeight();
    (void)info.spiHz;
}

void loop()
{
}
'@
[IO.File]::WriteAllText(
    (Join-Path $directSketch "tft_direct.ino"),
    $directSketchText,
    $utf8NoBom
)

@(
    "DATE=$(Get-Date -Format o)"
    "BRANCH=$branch"
    "HEAD=$head"
    "SOURCE_LAST_COMMIT=$sourceCommit"
    "SOURCE_LAST_DATE=$sourceDate"
    "ARCHIVE_LAST_COMMIT=$archiveCommit"
    "ARCHIVE_LAST_DATE=$archiveDate"
    "ARCHIVE_BYTES=$archiveBytes"
    "ARCHIVE_SHA256=$archiveSha"
    "EXPECTED_MEMBERS=$($expectedMembers -join ',')"
    "FQBN=$Fqbn"
    "ARDUINO_CLI=$ArduinoCli"
) | Set-Content -LiteralPath (Join-Path $ResultRoot "MANIFEST.txt") -Encoding UTF8

Write-Host "=============================================================================="
Write-Host " ALPHA12 - JWPLC_TFT PRECOMPILED REQUALIFICATION"
Write-Host "=============================================================================="
Write-Host ("BRANCH=" + $branch)
Write-Host ("HEAD=" + $head)
Write-Host ("SOURCE_LAST_COMMIT=" + $sourceCommit)
Write-Host ("SOURCE_LAST_DATE=" + $sourceDate)
Write-Host ("ARCHIVE_LAST_COMMIT=" + $archiveCommit)
Write-Host ("ARCHIVE_LAST_DATE=" + $archiveDate)
Write-Host ("ARCHIVE_BYTES=" + [string]$archiveBytes)
Write-Host ("ARCHIVE_SHA256=" + $archiveSha)
Write-Host ("RESULT_ROOT=" + $ResultRoot)

Write-Host ""
Write-Host "=== SOURCE-FIRST REFERENCE BUILD ==="

$sourceRun = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments @(
    "compile",
    "--fqbn", $Fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $sourceBuild,
    "--libraries", $repoLibraries,
    $displaySketch
)

$sourceRun.Output | Set-Content -LiteralPath $sourceLog -Encoding UTF8
Write-Host ("SOURCE_COMPILE_EXIT=" + [string]$sourceRun.ExitCode)

if ($sourceRun.ExitCode -ne 0) {
    $sourceRun.Output | Select-Object -Last 220 | ForEach-Object { Write-Host $_ }
    throw "A12_TFT_REQUAL_SOURCE_COMPILE_FAILED"
}

Assert-LibrarySelected -Lines $sourceRun.Output -LibraryName "JWPLC_TFT" -ExpectedRoot $libraryRoot -Label "SOURCE"

$sourcePrecompiled = Test-PrecompiledMarker -Lines $sourceRun.Output -LibraryName "JWPLC_TFT"
$sourceTftObjects = @(Get-NamedObjects -BuildPath $sourceBuild -ObjectName "JWPLC_TFT.cpp.o")
$sourceBackendObjects = @(Get-NamedObjects -BuildPath $sourceBuild -ObjectName "TFT_eSPI.cpp.o")
$tftEspiSelection = Get-TftEspiSelection -Lines $sourceRun.Output
$sourceWarnings = @($sourceRun.Output | Where-Object { $_ -match '(?i)\bwarning:' }).Count
$sourceErrors = @($sourceRun.Output | Where-Object { $_ -match '(?i)\berror:' }).Count

Write-Host ("SOURCE_TFT_OBJECT_COUNT=" + [string]$sourceTftObjects.Count)
Write-Host ("SOURCE_TFT_ESPI_OBJECT_COUNT=" + [string]$sourceBackendObjects.Count)
Write-Host ("SOURCE_TFT_PRECOMPILED_MARKER=" + $(if ($sourcePrecompiled) { "YES" } else { "NO" }))
Write-Host ("SOURCE_WARNING_LINES=" + [string]$sourceWarnings)
Write-Host ("SOURCE_ERROR_LINES=" + [string]$sourceErrors)

if ($sourcePrecompiled) {
    throw "A12_TFT_REQUAL_SOURCE_UNEXPECTED_PRECOMPILED"
}
if ($sourceTftObjects.Count -ne 1 -or $sourceBackendObjects.Count -ne 1) {
    throw "A12_TFT_REQUAL_SOURCE_OBJECT_SET_INVALID"
}
if ($null -eq $tftEspiSelection) {
    throw "A12_TFT_REQUAL_SOURCE_TFT_ESPI_NOT_SELECTED"
}
if ($tftEspiSelection.Version -ne "2.5.43") {
    Write-Host ("SOURCE_TFT_ESPI_VERSION=" + $tftEspiSelection.Version)
    Write-Host ("SOURCE_TFT_ESPI_FOLDER=" + $tftEspiSelection.Folder)
    throw "A12_TFT_REQUAL_MAINTAINER_TFT_ESPI_VERSION_MISMATCH"
}
if ($sourceWarnings -ne 0 -or $sourceErrors -ne 0) {
    throw "A12_TFT_REQUAL_SOURCE_COMPILER_DIAGNOSTICS"
}

Write-Host ("SOURCE_TFT_ESPI_VERSION=" + $tftEspiSelection.Version)
Write-Host ("SOURCE_TFT_ESPI_FOLDER=" + $tftEspiSelection.Folder)

$archiver = Resolve-Archiver -Lines $sourceRun.Output
Write-Host ("ARCHIVER=" + $archiver)

Write-Host ""
Write-Host "=== OFFICIAL ARCHIVE STRUCTURE / PARITY ==="

$listRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $archivePath)
if ($listRun.ExitCode -ne 0) {
    throw "A12_TFT_REQUAL_ARCHIVE_LIST_FAILED"
}

[string[]]$members = @(
    $listRun.Output |
        ForEach-Object { $_.Trim() } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Sort-Object
)

Write-Host ("ARCHIVE_MEMBER_COUNT=" + [string]$members.Count)
$members | ForEach-Object { Write-Host ("ARCHIVE_MEMBER=" + $_) }

if ($members.Count -ne $expectedMembers.Count) {
    throw "A12_TFT_REQUAL_ARCHIVE_MEMBER_COUNT_INVALID"
}
if (Compare-Object -ReferenceObject $expectedMembers -DifferenceObject $members) {
    throw "A12_TFT_REQUAL_ARCHIVE_MEMBER_SET_INVALID"
}

$oldLocation = Get-Location
try {
    Set-Location $extractDir
    $extractRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @("x", $archivePath)
}
finally {
    Set-Location $oldLocation
}

if ($extractRun.ExitCode -ne 0) {
    throw "A12_TFT_REQUAL_ARCHIVE_EXTRACT_FAILED"
}

$sourceTftSha = Get-Sha256Lower $sourceTftObjects[0].FullName
$sourceBackendSha = Get-Sha256Lower $sourceBackendObjects[0].FullName
$archiveTftObject = Join-Path $extractDir "JWPLC_TFT.cpp.o"
$archiveBackendObject = Join-Path $extractDir "TFT_eSPI.cpp.o"

if (-not (Test-Path -LiteralPath $archiveTftObject) -or -not (Test-Path -LiteralPath $archiveBackendObject)) {
    throw "A12_TFT_REQUAL_EXTRACTED_MEMBER_MISSING"
}

$archiveTftSha = Get-Sha256Lower $archiveTftObject
$archiveBackendSha = Get-Sha256Lower $archiveBackendObject

Write-Host ("JWPLC_TFT_OBJECT_SOURCE_SHA256=" + $sourceTftSha)
Write-Host ("JWPLC_TFT_OBJECT_ARCHIVE_SHA256=" + $archiveTftSha)
Write-Host ("TFT_ESPI_OBJECT_SOURCE_SHA256=" + $sourceBackendSha)
Write-Host ("TFT_ESPI_OBJECT_ARCHIVE_SHA256=" + $archiveBackendSha)

if ($sourceTftSha -ne $archiveTftSha) {
    throw "A12_TFT_REQUAL_JWPLC_TFT_OBJECT_PARITY_FAILED"
}
if ($sourceBackendSha -ne $archiveBackendSha) {
    throw "A12_TFT_REQUAL_TFT_ESPI_OBJECT_PARITY_FAILED"
}

Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=PASS"

Write-Host ""
Write-Host "=== SELF-CONTAINED PRECOMPILED CASES ==="

$directResult = Invoke-CandidateCase -Label "DIRECT_TFT" -SketchPath $directSketch -CandidateRoot $candidateRoot -BuildPath $directBuild -LogPath (Join-Path $ResultRoot "candidate_direct_tft.log")
$displayResult = Invoke-CandidateCase -Label "DISPLAY_INTEGRATION" -SketchPath $displaySketch -CandidateRoot $candidateRoot -BuildPath $displayBuild -LogPath (Join-Path $ResultRoot "candidate_display_integration.log")
$autoloadResult = Invoke-CandidateCase -Label "NORMAL_AUTOLOAD" -SketchPath $emptySketch -CandidateRoot $candidateRoot -BuildPath $autoloadBuild -LogPath (Join-Path $ResultRoot "candidate_normal_autoload.log")

[string[]]$finalDirty = @(& git -C $repo diff --name-only)
[string[]]$finalStaged = @(& git -C $repo diff --cached --name-only)

Write-Host ("FINAL_TRACKED_DIRTY_COUNT=" + [string]$finalDirty.Count)
Write-Host ("FINAL_STAGED_COUNT=" + [string]$finalStaged.Count)

if ($finalDirty.Count -ne 0 -or $finalStaged.Count -ne 0) {
    $finalDirty | ForEach-Object { Write-Host ("FINAL_TRACKED_DIRTY=" + $_) }
    $finalStaged | ForEach-Object { Write-Host ("FINAL_STAGED=" + $_) }
    throw "A12_TFT_REQUAL_WORKTREE_MUTATED"
}

@(
    "ALPHA12_TFT_PRECOMPILED_REQUALIFICATION=PASS"
    "BRANCH=$branch"
    "HEAD=$head"
    "SOURCE_LAST_COMMIT=$sourceCommit"
    "ARCHIVE_LAST_COMMIT=$archiveCommit"
    "ARCHIVE_BYTES=$archiveBytes"
    "ARCHIVE_SHA256=$archiveSha"
    "ARCHIVE_MEMBER_COUNT=2"
    "ARCHIVE_MEMBERS=$($expectedMembers -join ',')"
    "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
    "JWPLC_TFT_OBJECT_SHA256=$sourceTftSha"
    "TFT_ESPI_OBJECT_SHA256=$sourceBackendSha"
    "MAINTAINER_TFT_ESPI_VERSION=$($tftEspiSelection.Version)"
    "MAINTAINER_TFT_ESPI_FOLDER=$($tftEspiSelection.Folder)"
    "DIRECT_TFT_PRECOMPILED_LINK=PASS"
    "DISPLAY_INTEGRATION_PRECOMPILED_LINK=PASS"
    "NORMAL_AUTOLOAD_PRECOMPILED_LINK=PASS"
    "GLOBAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO"
    "OFFICIAL_ARCHIVE_CHANGED=NO"
    "PHYSICAL_UPLOAD_PERFORMED=NO"
    "FINAL_TRACKED_DIRTY_COUNT=0"
    "RESULT_ROOT=$ResultRoot"
) | Set-Content -LiteralPath (Join-Path $ResultRoot "SUMMARY.txt") -Encoding UTF8

Write-Host ""
Write-Host "=============================================================================="
Write-Host "ALPHA12_TFT_PRECOMPILED_REQUALIFICATION=PASS"
Write-Host ("ARCHIVE_BYTES=" + [string]$archiveBytes)
Write-Host ("ARCHIVE_SHA256=" + $archiveSha)
Write-Host "ARCHIVE_MEMBER_COUNT=2"
Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
Write-Host ("MAINTAINER_TFT_ESPI_VERSION=" + $tftEspiSelection.Version)
Write-Host "DIRECT_TFT_PRECOMPILED_LINK=PASS"
Write-Host "DISPLAY_INTEGRATION_PRECOMPILED_LINK=PASS"
Write-Host "NORMAL_AUTOLOAD_PRECOMPILED_LINK=PASS"
Write-Host "GLOBAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO"
Write-Host "OFFICIAL_ARCHIVE_CHANGED=NO"
Write-Host "PHYSICAL_UPLOAD_PERFORMED=NO"
Write-Host "FINAL_TRACKED_DIRTY_COUNT=0"
Write-Host ("RESULT_ROOT=" + $ResultRoot)
