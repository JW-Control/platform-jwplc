param(
    [string]$ArduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic",
    [string]$ResultRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"

$libraryRel = "JWPLC/2.1.0/libraries/JW_FRAM"
$libraryRoot = Join-Path $repo $libraryRel
$headerPath = Join-Path $libraryRoot "src\JW_FRAM.h"
$cppPath = Join-Path $libraryRoot "src\JW_FRAM.cpp"
$propertiesPath = Join-Path $libraryRoot "library.properties"
$archiveRel = "$libraryRel/src/esp32/libJW_FRAM.a"
$archivePath = Join-Path $repo $archiveRel
$repoLibraries = Join-Path $repo "JWPLC\2.1.0\libraries"
$bundledBusIoRoot = Join-Path $repoLibraries "Adafruit_BusIO"

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

    throw "A12_JW_FRAM_ARCHIVER_NOT_FOUND"
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

    Write-Host "$($Label)_$($LibraryName)_SELECTED=$selected"

    if (-not $selected) {
        throw "A12_JW_FRAM_$($Label)_LIBRARY_NOT_SELECTED:$LibraryName"
    }
}

function Assert-BundledBusIo {
    param(
        [string[]]$Lines,
        [string]$Label
    )

    $expected = [IO.Path]::GetFullPath($bundledBusIoRoot).TrimEnd('\', '/')
    [object[]]$selectionLines = @(
        $Lines | Where-Object {
            $_ -match '^Using library Adafruit BusIO at version .+ in folder: (.+)$'
        }
    )

    Write-Host "$($Label)_BUSIO_SELECTION_COUNT=$($selectionLines.Count)"

    if ($selectionLines.Count -ne 1) {
        throw "A12_JW_FRAM_$($Label)_BUSIO_SELECTION_AMBIGUOUS"
    }

    $line = $selectionLines[0].ToString()
    $selectionMatch = [regex]::Match(
        $line,
        '^Using library Adafruit BusIO at version .+ in folder: (.+)$'
    )

    if (-not $selectionMatch.Success -or $selectionMatch.Groups.Count -lt 2) {
        throw "A12_JW_FRAM_$($Label)_BUSIO_SELECTION_PARSE_FAILED"
    }

    $actual = [IO.Path]::GetFullPath(
        $selectionMatch.Groups[1].Value.Trim()
    ).TrimEnd('\', '/')

    $bundled = $actual -ieq $expected

    Write-Host "$($Label)_BUNDLED_BUSIO_SELECTED=$bundled"
    Write-Host "$($Label)_BUSIO_FOLDER=$actual"

    if (-not $bundled) {
        throw "A12_JW_FRAM_$($Label)_EXTERNAL_BUSIO_SELECTED"
    }
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

Write-Host "=============================================================================="
Write-Host " ALPHA12 - JW_FRAM PRECOMPILED REFRESH"
Write-Host "=============================================================================="
Write-Host "BRANCH=$branch"
Write-Host "HEAD=$head"

if ($branch -ne $expectedBranch) {
    throw "A12_JW_FRAM_BRANCH_MISMATCH"
}

[string[]]$entryDirty = @(& git -C $repo status --porcelain=v1)
Write-Host "ENTRY_DIRTY_COUNT=$($entryDirty.Count)"

if ($entryDirty.Count -ne 0) {
    $entryDirty | ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }
    throw "A12_JW_FRAM_TREE_NOT_CLEAN"
}

foreach ($required in @(
    $libraryRoot,
    $headerPath,
    $cppPath,
    $propertiesPath,
    $archivePath,
    $repoLibraries,
    $bundledBusIoRoot
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "A12_JW_FRAM_REQUIRED_PATH_MISSING=$required"
    }
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "A12_JW_FRAM_ARDUINO_CLI_NOT_FOUND"
}

$headerText = [IO.File]::ReadAllText($headerPath)
$marker = "#include <JWPLC_Bundled_Adafruit_BusIO.h>"
$spiInclude = "#include <Adafruit_SPIDevice.h>"
$markerIndex = $headerText.IndexOf($marker)
$spiIndex = $headerText.IndexOf($spiInclude)

Write-Host "BUNDLED_MARKER_PRESENT=$($markerIndex -ge 0)"
Write-Host "BUNDLED_MARKER_BEFORE_SPI=$($markerIndex -ge 0 -and $spiIndex -gt $markerIndex)"

if ($markerIndex -lt 0 -or $spiIndex -le $markerIndex) {
    throw "A12_JW_FRAM_BUNDLED_MARKER_CONTRACT_FAILED"
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo ("tools\alpha12\results\jw_fram_precompiled_refresh_" + $stamp)
}

New-Item -ItemType Directory -Path $ResultRoot -Force | Out-Null

$runRoot = Join-Path $env:TEMP ("jwplc_a12_jw_fram_refresh_" + $stamp)
$sketchRoot = Join-Path $runRoot "jw_fram_gate"
$sourceRoot = Join-Path $runRoot "source-libraries\JW_FRAM"
$sourceSrc = Join-Path $sourceRoot "src"
$sourceBuild = Join-Path $runRoot "source-build"
$candidateRoot = Join-Path $runRoot "candidate-libraries\JW_FRAM"
$candidateSrc = Join-Path $candidateRoot "src"
$candidateArchiveDir = Join-Path $candidateSrc "esp32"
$candidateArchive = Join-Path $candidateArchiveDir "libJW_FRAM.a"
$candidateBuild = Join-Path $runRoot "candidate-build"
$extractDir = Join-Path $runRoot "archive-members"

foreach ($dir in @(
    $sketchRoot,
    $sourceSrc,
    $sourceBuild,
    $candidateArchiveDir,
    $candidateBuild,
    $extractDir
)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

$utf8NoBom = [Text.UTF8Encoding]::new($false)

$sketchPath = Join-Path $sketchRoot "jw_fram_gate.ino"
$sketchText = @'
#include <Arduino.h>
#include <JW_FRAM.h>

JW_FRAM fram(5);

void setup()
{
    volatile uint32_t sizeBytes = fram.size();
    volatile uint8_t addressBytes = fram.addressSize();
    volatile bool valid = fram.isAddressValid(0, 1);
    (void)sizeBytes;
    (void)addressBytes;
    (void)valid;
}

void loop()
{
}
'@
[IO.File]::WriteAllText($sketchPath, $sketchText, $utf8NoBom)

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $sourceSrc "JW_FRAM.h") -Force
Copy-Item -LiteralPath $cppPath -Destination (Join-Path $sourceSrc "JW_FRAM.cpp") -Force

$sourceProperties = @'
name=JW_FRAM
version=1.0.3-alpha12-source
author=JW Control
maintainer=JW Control
sentence=Alpha12 source qualification for JW_FRAM.
paragraph=Temporary source-only qualification library.
category=Data Storage
architectures=esp32
depends=Adafruit BusIO
'@
[IO.File]::WriteAllText((Join-Path $sourceRoot "library.properties"), $sourceProperties, $utf8NoBom)

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $candidateSrc "JW_FRAM.h") -Force

$candidateProperties = @'
name=JW_FRAM
version=1.0.3-alpha12-candidate
author=JW Control
maintainer=JW Control
sentence=Alpha12 precompiled candidate for JW_FRAM.
paragraph=Temporary precompiled qualification library.
category=Data Storage
architectures=esp32
depends=Adafruit BusIO
precompiled=full
'@
[IO.File]::WriteAllText((Join-Path $candidateRoot "library.properties"), $candidateProperties, $utf8NoBom)

$oldSha = Get-Sha256Lower $archivePath
$oldBytes = (Get-Item -LiteralPath $archivePath).Length
$backup = Join-Path $env:TEMP ("a12_jw_fram_archive_backup_" + $stamp + ".a")
Copy-Item -LiteralPath $archivePath -Destination $backup -Force

Write-Host "OLD_ARCHIVE_BYTES=$oldBytes"
Write-Host "OLD_ARCHIVE_SHA256=$oldSha"
Write-Host "RESULT_ROOT=$ResultRoot"

$success = $false

try {
    Write-Host ""
    Write-Host "=== SOURCE-ONLY JW_FRAM BUILD ==="

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

    $sourceRun.Output | Set-Content -LiteralPath (Join-Path $ResultRoot "source.log") -Encoding UTF8
    Write-Host "SOURCE_COMPILE_EXIT=$($sourceRun.ExitCode)"

    if ($sourceRun.ExitCode -ne 0) {
        $sourceRun.Output | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }
        throw "A12_JW_FRAM_SOURCE_COMPILE_FAILED"
    }

    Assert-LibrarySelected -Lines $sourceRun.Output -LibraryName "JW_FRAM" -ExpectedRoot $sourceRoot -Label "SOURCE"
    Assert-BundledBusIo -Lines $sourceRun.Output -Label "SOURCE"

    [object[]]$sourceObjects = @(
        Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JW_FRAM.cpp.o" }
    )

    Write-Host "SOURCE_OBJECT_COUNT=$($sourceObjects.Count)"

    if ($sourceObjects.Count -ne 1) {
        throw "A12_JW_FRAM_SOURCE_OBJECT_SET_INVALID"
    }

    $sourcePrecompiled = @(
        $sourceRun.Output | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match '\bJW_FRAM\b')
        }
    ).Count -gt 0

    Write-Host "SOURCE_PRECOMPILED_MARKER=$(if ($sourcePrecompiled) { 'YES' } else { 'NO' })"

    if ($sourcePrecompiled) {
        throw "A12_JW_FRAM_SOURCE_UNEXPECTED_PRECOMPILED"
    }

    $archiver = Resolve-Archiver -Lines $sourceRun.Output
    Write-Host "ARCHIVER=$archiver"

    Write-Host ""
    Write-Host "=== CREATE JW_FRAM CANDIDATE ARCHIVE ==="

    $archiveRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @(
        "crs",
        $candidateArchive,
        $sourceObjects[0].FullName
    )

    if ($archiveRun.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $candidateArchive)) {
        throw "A12_JW_FRAM_ARCHIVE_CREATE_FAILED"
    }

    $listRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $candidateArchive)

    if ($listRun.ExitCode -ne 0) {
        throw "A12_JW_FRAM_ARCHIVE_LIST_FAILED"
    }

    [string[]]$members = @(
        $listRun.Output |
            ForEach-Object { $_.Trim() } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )

    Write-Host "ARCHIVE_MEMBER_COUNT=$($members.Count)"
    $members | ForEach-Object { Write-Host "ARCHIVE_MEMBER=$_" }

    if ($members.Count -ne 1 -or $members[0] -ne "JW_FRAM.cpp.o") {
        throw "A12_JW_FRAM_ARCHIVE_MEMBER_SET_INVALID"
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
        throw "A12_JW_FRAM_ARCHIVE_EXTRACT_FAILED"
    }

    $extractedObject = Join-Path $extractDir "JW_FRAM.cpp.o"
    $sourceObjectSha = Get-Sha256Lower $sourceObjects[0].FullName
    $archiveObjectSha = Get-Sha256Lower $extractedObject

    Write-Host "SOURCE_OBJECT_SHA256=$sourceObjectSha"
    Write-Host "ARCHIVE_MEMBER_SHA256=$archiveObjectSha"
    Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=$($sourceObjectSha -eq $archiveObjectSha)"

    if ($sourceObjectSha -ne $archiveObjectSha) {
        throw "A12_JW_FRAM_ARCHIVE_MEMBER_PARITY_FAILED"
    }

    $candidateSha = Get-Sha256Lower $candidateArchive
    $candidateBytes = (Get-Item -LiteralPath $candidateArchive).Length
    $archiveChanged = $candidateSha -ne $oldSha

    Write-Host "CANDIDATE_ARCHIVE_BYTES=$candidateBytes"
    Write-Host "CANDIDATE_ARCHIVE_SHA256=$candidateSha"
    Write-Host "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"

    Write-Host ""
    Write-Host "=== CANDIDATE PRECOMPILED JW_FRAM LINK ==="

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

    $candidateRun.Output | Set-Content -LiteralPath (Join-Path $ResultRoot "candidate.log") -Encoding UTF8
    Write-Host "CANDIDATE_COMPILE_EXIT=$($candidateRun.ExitCode)"

    if ($candidateRun.ExitCode -ne 0) {
        $candidateRun.Output | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }
        throw "A12_JW_FRAM_CANDIDATE_COMPILE_FAILED"
    }

    Assert-LibrarySelected -Lines $candidateRun.Output -LibraryName "JW_FRAM" -ExpectedRoot $candidateRoot -Label "CANDIDATE"
    Assert-BundledBusIo -Lines $candidateRun.Output -Label "CANDIDATE"

    [int]$candidateSourceObjects = @(
        Get-ChildItem -LiteralPath $candidateBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JW_FRAM.cpp.o" }
    ).Count

    $candidatePrecompiled = @(
        $candidateRun.Output | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match '\bJW_FRAM\b')
        }
    ).Count -gt 0

    Write-Host "CANDIDATE_SOURCE_OBJECT_COUNT=$candidateSourceObjects"
    Write-Host "CANDIDATE_PRECOMPILED_MARKER=$(if ($candidatePrecompiled) { 'YES' } else { 'NO' })"

    if ($candidateSourceObjects -ne 0 -or -not $candidatePrecompiled) {
        throw "A12_JW_FRAM_CANDIDATE_PRECOMPILED_POLICY_FAILED"
    }

    Write-Host ""
    Write-Host "=== PLACE CANDIDATE IN OFFICIAL WORKTREE ==="

    Copy-Item -LiteralPath $candidateArchive -Destination $archivePath -Force

    $officialSha = Get-Sha256Lower $archivePath
    $officialBytes = (Get-Item -LiteralPath $archivePath).Length

    if ($officialSha -ne $candidateSha -or $officialBytes -ne $candidateBytes) {
        throw "A12_JW_FRAM_OFFICIAL_CANDIDATE_IDENTITY_FAILED"
    }

    [string[]]$finalDirty = @(
        & git -C $repo diff --name-only |
            ForEach-Object { $_.Replace("\", "/") } |
            Sort-Object
    )

    Write-Host "FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
    $finalDirty | ForEach-Object { Write-Host "FINAL_TRACKED_DIRTY=$_" }

    if ($archiveChanged) {
        if ($finalDirty.Count -ne 1 -or $finalDirty[0] -ne $archiveRel) {
            throw "A12_JW_FRAM_FINAL_DIRTY_SCOPE_INVALID"
        }
    }
    else {
        if ($finalDirty.Count -ne 0) {
            throw "A12_JW_FRAM_IDENTICAL_ARCHIVE_DIRTY_UNEXPECTED"
        }
    }

    $success = $true

    @(
        "ALPHA12_JW_FRAM_PRECOMPILED_REFRESH=PASS"
        "HEAD_SOURCE=$head"
        "OLD_ARCHIVE_BYTES=$oldBytes"
        "OLD_ARCHIVE_SHA256=$oldSha"
        "NEW_ARCHIVE_BYTES=$candidateBytes"
        "NEW_ARCHIVE_SHA256=$candidateSha"
        "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"
        "ARCHIVE_MEMBER_COUNT=1"
        "ARCHIVE_MEMBER=JW_FRAM.cpp.o"
        "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
        "SOURCE_BUNDLED_BUSIO_SELECTED=PASS"
        "CANDIDATE_BUNDLED_BUSIO_SELECTED=PASS"
        "CANDIDATE_PRECOMPILED_LINK=PASS"
        "FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "SUMMARY.txt") -Encoding UTF8

    Write-Host ""
    Write-Host "=============================================================================="
    Write-Host " SUMMARY"
    Write-Host "=============================================================================="
    Write-Host "ALPHA12_JW_FRAM_PRECOMPILED_REFRESH=PASS"
    Write-Host "OLD_ARCHIVE_SHA256=$oldSha"
    Write-Host "NEW_ARCHIVE_SHA256=$candidateSha"
    Write-Host "NEW_ARCHIVE_BYTES=$candidateBytes"
    Write-Host "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"
    Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
    Write-Host "SOURCE_BUNDLED_BUSIO_SELECTED=PASS"
    Write-Host "CANDIDATE_BUNDLED_BUSIO_SELECTED=PASS"
    Write-Host "CANDIDATE_PRECOMPILED_LINK=PASS"
    Write-Host "FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
finally {
    if (-not $success) {
        Write-Host "A12_JW_FRAM_ROLLBACK=START"

        if (Test-Path -LiteralPath $backup) {
            Copy-Item -LiteralPath $backup -Destination $archivePath -Force
        }

        $restoredSha = Get-Sha256Lower $archivePath
        Write-Host "A12_JW_FRAM_ROLLBACK_SHA256=$restoredSha"
        Write-Host "A12_JW_FRAM_ROLLBACK=DONE"
    }
}

        }
    )

    Write-Host "$($Label)_BUSIO_SELECTION_COUNT=$($selectionLines.Count)"

    if ($selectionLines.Count -ne 1) {
        throw "A12_JW_FRAM_$($Label)_BUSIO_SELECTION_AMBIGUOUS"
    }

    $line = $selectionLines[0].ToString()
    $selectionMatch = [regex]::Match(
        $line,
        '^Using library Adafruit BusIO at version .+ in folder: (.+)    $bundled = $actual -ieq $expected

    Write-Host "$($Label)_BUNDLED_BUSIO_SELECTED=$bundled"
    Write-Host "$($Label)_BUSIO_FOLDER=$actual"

    if (-not $bundled) {
        throw "A12_JW_FRAM_$($Label)_EXTERNAL_BUSIO_SELECTED"
    }
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

Write-Host "=============================================================================="
Write-Host " ALPHA12 - JW_FRAM PRECOMPILED REFRESH"
Write-Host "=============================================================================="
Write-Host "BRANCH=$branch"
Write-Host "HEAD=$head"

if ($branch -ne $expectedBranch) {
    throw "A12_JW_FRAM_BRANCH_MISMATCH"
}

[string[]]$entryDirty = @(& git -C $repo status --porcelain=v1)
Write-Host "ENTRY_DIRTY_COUNT=$($entryDirty.Count)"

if ($entryDirty.Count -ne 0) {
    $entryDirty | ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }
    throw "A12_JW_FRAM_TREE_NOT_CLEAN"
}

foreach ($required in @(
    $libraryRoot,
    $headerPath,
    $cppPath,
    $propertiesPath,
    $archivePath,
    $repoLibraries,
    $bundledBusIoRoot
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "A12_JW_FRAM_REQUIRED_PATH_MISSING=$required"
    }
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "A12_JW_FRAM_ARDUINO_CLI_NOT_FOUND"
}

$headerText = [IO.File]::ReadAllText($headerPath)
$marker = "#include <JWPLC_Bundled_Adafruit_BusIO.h>"
$spiInclude = "#include <Adafruit_SPIDevice.h>"
$markerIndex = $headerText.IndexOf($marker)
$spiIndex = $headerText.IndexOf($spiInclude)

Write-Host "BUNDLED_MARKER_PRESENT=$($markerIndex -ge 0)"
Write-Host "BUNDLED_MARKER_BEFORE_SPI=$($markerIndex -ge 0 -and $spiIndex -gt $markerIndex)"

if ($markerIndex -lt 0 -or $spiIndex -le $markerIndex) {
    throw "A12_JW_FRAM_BUNDLED_MARKER_CONTRACT_FAILED"
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo ("tools\alpha12\results\jw_fram_precompiled_refresh_" + $stamp)
}

New-Item -ItemType Directory -Path $ResultRoot -Force | Out-Null

$runRoot = Join-Path $env:TEMP ("jwplc_a12_jw_fram_refresh_" + $stamp)
$sketchRoot = Join-Path $runRoot "jw_fram_gate"
$sourceRoot = Join-Path $runRoot "source-libraries\JW_FRAM"
$sourceSrc = Join-Path $sourceRoot "src"
$sourceBuild = Join-Path $runRoot "source-build"
$candidateRoot = Join-Path $runRoot "candidate-libraries\JW_FRAM"
$candidateSrc = Join-Path $candidateRoot "src"
$candidateArchiveDir = Join-Path $candidateSrc "esp32"
$candidateArchive = Join-Path $candidateArchiveDir "libJW_FRAM.a"
$candidateBuild = Join-Path $runRoot "candidate-build"
$extractDir = Join-Path $runRoot "archive-members"

foreach ($dir in @(
    $sketchRoot,
    $sourceSrc,
    $sourceBuild,
    $candidateArchiveDir,
    $candidateBuild,
    $extractDir
)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

$utf8NoBom = [Text.UTF8Encoding]::new($false)

$sketchPath = Join-Path $sketchRoot "jw_fram_gate.ino"
$sketchText = @'
#include <Arduino.h>
#include <JW_FRAM.h>

JW_FRAM fram(5);

void setup()
{
    volatile uint32_t sizeBytes = fram.size();
    volatile uint8_t addressBytes = fram.addressSize();
    volatile bool valid = fram.isAddressValid(0, 1);
    (void)sizeBytes;
    (void)addressBytes;
    (void)valid;
}

void loop()
{
}
'@
[IO.File]::WriteAllText($sketchPath, $sketchText, $utf8NoBom)

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $sourceSrc "JW_FRAM.h") -Force
Copy-Item -LiteralPath $cppPath -Destination (Join-Path $sourceSrc "JW_FRAM.cpp") -Force

$sourceProperties = @'
name=JW_FRAM
version=1.0.3-alpha12-source
author=JW Control
maintainer=JW Control
sentence=Alpha12 source qualification for JW_FRAM.
paragraph=Temporary source-only qualification library.
category=Data Storage
architectures=esp32
depends=Adafruit BusIO
'@
[IO.File]::WriteAllText((Join-Path $sourceRoot "library.properties"), $sourceProperties, $utf8NoBom)

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $candidateSrc "JW_FRAM.h") -Force

$candidateProperties = @'
name=JW_FRAM
version=1.0.3-alpha12-candidate
author=JW Control
maintainer=JW Control
sentence=Alpha12 precompiled candidate for JW_FRAM.
paragraph=Temporary precompiled qualification library.
category=Data Storage
architectures=esp32
depends=Adafruit BusIO
precompiled=full
'@
[IO.File]::WriteAllText((Join-Path $candidateRoot "library.properties"), $candidateProperties, $utf8NoBom)

$oldSha = Get-Sha256Lower $archivePath
$oldBytes = (Get-Item -LiteralPath $archivePath).Length
$backup = Join-Path $env:TEMP ("a12_jw_fram_archive_backup_" + $stamp + ".a")
Copy-Item -LiteralPath $archivePath -Destination $backup -Force

Write-Host "OLD_ARCHIVE_BYTES=$oldBytes"
Write-Host "OLD_ARCHIVE_SHA256=$oldSha"
Write-Host "RESULT_ROOT=$ResultRoot"

$success = $false

try {
    Write-Host ""
    Write-Host "=== SOURCE-ONLY JW_FRAM BUILD ==="

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

    $sourceRun.Output | Set-Content -LiteralPath (Join-Path $ResultRoot "source.log") -Encoding UTF8
    Write-Host "SOURCE_COMPILE_EXIT=$($sourceRun.ExitCode)"

    if ($sourceRun.ExitCode -ne 0) {
        $sourceRun.Output | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }
        throw "A12_JW_FRAM_SOURCE_COMPILE_FAILED"
    }

    Assert-LibrarySelected -Lines $sourceRun.Output -LibraryName "JW_FRAM" -ExpectedRoot $sourceRoot -Label "SOURCE"
    Assert-BundledBusIo -Lines $sourceRun.Output -Label "SOURCE"

    [object[]]$sourceObjects = @(
        Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JW_FRAM.cpp.o" }
    )

    Write-Host "SOURCE_OBJECT_COUNT=$($sourceObjects.Count)"

    if ($sourceObjects.Count -ne 1) {
        throw "A12_JW_FRAM_SOURCE_OBJECT_SET_INVALID"
    }

    $sourcePrecompiled = @(
        $sourceRun.Output | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match '\bJW_FRAM\b')
        }
    ).Count -gt 0

    Write-Host "SOURCE_PRECOMPILED_MARKER=$(if ($sourcePrecompiled) { 'YES' } else { 'NO' })"

    if ($sourcePrecompiled) {
        throw "A12_JW_FRAM_SOURCE_UNEXPECTED_PRECOMPILED"
    }

    $archiver = Resolve-Archiver -Lines $sourceRun.Output
    Write-Host "ARCHIVER=$archiver"

    Write-Host ""
    Write-Host "=== CREATE JW_FRAM CANDIDATE ARCHIVE ==="

    $archiveRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @(
        "crs",
        $candidateArchive,
        $sourceObjects[0].FullName
    )

    if ($archiveRun.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $candidateArchive)) {
        throw "A12_JW_FRAM_ARCHIVE_CREATE_FAILED"
    }

    $listRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $candidateArchive)

    if ($listRun.ExitCode -ne 0) {
        throw "A12_JW_FRAM_ARCHIVE_LIST_FAILED"
    }

    [string[]]$members = @(
        $listRun.Output |
            ForEach-Object { $_.Trim() } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )

    Write-Host "ARCHIVE_MEMBER_COUNT=$($members.Count)"
    $members | ForEach-Object { Write-Host "ARCHIVE_MEMBER=$_" }

    if ($members.Count -ne 1 -or $members[0] -ne "JW_FRAM.cpp.o") {
        throw "A12_JW_FRAM_ARCHIVE_MEMBER_SET_INVALID"
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
        throw "A12_JW_FRAM_ARCHIVE_EXTRACT_FAILED"
    }

    $extractedObject = Join-Path $extractDir "JW_FRAM.cpp.o"
    $sourceObjectSha = Get-Sha256Lower $sourceObjects[0].FullName
    $archiveObjectSha = Get-Sha256Lower $extractedObject

    Write-Host "SOURCE_OBJECT_SHA256=$sourceObjectSha"
    Write-Host "ARCHIVE_MEMBER_SHA256=$archiveObjectSha"
    Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=$($sourceObjectSha -eq $archiveObjectSha)"

    if ($sourceObjectSha -ne $archiveObjectSha) {
        throw "A12_JW_FRAM_ARCHIVE_MEMBER_PARITY_FAILED"
    }

    $candidateSha = Get-Sha256Lower $candidateArchive
    $candidateBytes = (Get-Item -LiteralPath $candidateArchive).Length
    $archiveChanged = $candidateSha -ne $oldSha

    Write-Host "CANDIDATE_ARCHIVE_BYTES=$candidateBytes"
    Write-Host "CANDIDATE_ARCHIVE_SHA256=$candidateSha"
    Write-Host "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"

    Write-Host ""
    Write-Host "=== CANDIDATE PRECOMPILED JW_FRAM LINK ==="

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

    $candidateRun.Output | Set-Content -LiteralPath (Join-Path $ResultRoot "candidate.log") -Encoding UTF8
    Write-Host "CANDIDATE_COMPILE_EXIT=$($candidateRun.ExitCode)"

    if ($candidateRun.ExitCode -ne 0) {
        $candidateRun.Output | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }
        throw "A12_JW_FRAM_CANDIDATE_COMPILE_FAILED"
    }

    Assert-LibrarySelected -Lines $candidateRun.Output -LibraryName "JW_FRAM" -ExpectedRoot $candidateRoot -Label "CANDIDATE"
    Assert-BundledBusIo -Lines $candidateRun.Output -Label "CANDIDATE"

    [int]$candidateSourceObjects = @(
        Get-ChildItem -LiteralPath $candidateBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JW_FRAM.cpp.o" }
    ).Count

    $candidatePrecompiled = @(
        $candidateRun.Output | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match '\bJW_FRAM\b')
        }
    ).Count -gt 0

    Write-Host "CANDIDATE_SOURCE_OBJECT_COUNT=$candidateSourceObjects"
    Write-Host "CANDIDATE_PRECOMPILED_MARKER=$(if ($candidatePrecompiled) { 'YES' } else { 'NO' })"

    if ($candidateSourceObjects -ne 0 -or -not $candidatePrecompiled) {
        throw "A12_JW_FRAM_CANDIDATE_PRECOMPILED_POLICY_FAILED"
    }

    Write-Host ""
    Write-Host "=== PLACE CANDIDATE IN OFFICIAL WORKTREE ==="

    Copy-Item -LiteralPath $candidateArchive -Destination $archivePath -Force

    $officialSha = Get-Sha256Lower $archivePath
    $officialBytes = (Get-Item -LiteralPath $archivePath).Length

    if ($officialSha -ne $candidateSha -or $officialBytes -ne $candidateBytes) {
        throw "A12_JW_FRAM_OFFICIAL_CANDIDATE_IDENTITY_FAILED"
    }

    [string[]]$finalDirty = @(
        & git -C $repo diff --name-only |
            ForEach-Object { $_.Replace("\", "/") } |
            Sort-Object
    )

    Write-Host "FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
    $finalDirty | ForEach-Object { Write-Host "FINAL_TRACKED_DIRTY=$_" }

    if ($archiveChanged) {
        if ($finalDirty.Count -ne 1 -or $finalDirty[0] -ne $archiveRel) {
            throw "A12_JW_FRAM_FINAL_DIRTY_SCOPE_INVALID"
        }
    }
    else {
        if ($finalDirty.Count -ne 0) {
            throw "A12_JW_FRAM_IDENTICAL_ARCHIVE_DIRTY_UNEXPECTED"
        }
    }

    $success = $true

    @(
        "ALPHA12_JW_FRAM_PRECOMPILED_REFRESH=PASS"
        "HEAD_SOURCE=$head"
        "OLD_ARCHIVE_BYTES=$oldBytes"
        "OLD_ARCHIVE_SHA256=$oldSha"
        "NEW_ARCHIVE_BYTES=$candidateBytes"
        "NEW_ARCHIVE_SHA256=$candidateSha"
        "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"
        "ARCHIVE_MEMBER_COUNT=1"
        "ARCHIVE_MEMBER=JW_FRAM.cpp.o"
        "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
        "SOURCE_BUNDLED_BUSIO_SELECTED=PASS"
        "CANDIDATE_BUNDLED_BUSIO_SELECTED=PASS"
        "CANDIDATE_PRECOMPILED_LINK=PASS"
        "FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "SUMMARY.txt") -Encoding UTF8

    Write-Host ""
    Write-Host "=============================================================================="
    Write-Host " SUMMARY"
    Write-Host "=============================================================================="
    Write-Host "ALPHA12_JW_FRAM_PRECOMPILED_REFRESH=PASS"
    Write-Host "OLD_ARCHIVE_SHA256=$oldSha"
    Write-Host "NEW_ARCHIVE_SHA256=$candidateSha"
    Write-Host "NEW_ARCHIVE_BYTES=$candidateBytes"
    Write-Host "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"
    Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
    Write-Host "SOURCE_BUNDLED_BUSIO_SELECTED=PASS"
    Write-Host "CANDIDATE_BUNDLED_BUSIO_SELECTED=PASS"
    Write-Host "CANDIDATE_PRECOMPILED_LINK=PASS"
    Write-Host "FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
finally {
    if (-not $success) {
        Write-Host "A12_JW_FRAM_ROLLBACK=START"

        if (Test-Path -LiteralPath $backup) {
            Copy-Item -LiteralPath $backup -Destination $archivePath -Force
        }

        $restoredSha = Get-Sha256Lower $archivePath
        Write-Host "A12_JW_FRAM_ROLLBACK_SHA256=$restoredSha"
        Write-Host "A12_JW_FRAM_ROLLBACK=DONE"
    }
}

    )

    if (-not $selectionMatch.Success -or $selectionMatch.Groups.Count -lt 2) {
        throw "A12_JW_FRAM_$($Label)_BUSIO_SELECTION_PARSE_FAILED"
    }

    $actual = [IO.Path]::GetFullPath($selectionMatch.Groups[1].Value.Trim()).TrimEnd('\', '/')
    $bundled = $actual -ieq $expected

    Write-Host "$($Label)_BUNDLED_BUSIO_SELECTED=$bundled"
    Write-Host "$($Label)_BUSIO_FOLDER=$actual"

    if (-not $bundled) {
        throw "A12_JW_FRAM_$($Label)_EXTERNAL_BUSIO_SELECTED"
    }
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

Write-Host "=============================================================================="
Write-Host " ALPHA12 - JW_FRAM PRECOMPILED REFRESH"
Write-Host "=============================================================================="
Write-Host "BRANCH=$branch"
Write-Host "HEAD=$head"

if ($branch -ne $expectedBranch) {
    throw "A12_JW_FRAM_BRANCH_MISMATCH"
}

[string[]]$entryDirty = @(& git -C $repo status --porcelain=v1)
Write-Host "ENTRY_DIRTY_COUNT=$($entryDirty.Count)"

if ($entryDirty.Count -ne 0) {
    $entryDirty | ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }
    throw "A12_JW_FRAM_TREE_NOT_CLEAN"
}

foreach ($required in @(
    $libraryRoot,
    $headerPath,
    $cppPath,
    $propertiesPath,
    $archivePath,
    $repoLibraries,
    $bundledBusIoRoot
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "A12_JW_FRAM_REQUIRED_PATH_MISSING=$required"
    }
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "A12_JW_FRAM_ARDUINO_CLI_NOT_FOUND"
}

$headerText = [IO.File]::ReadAllText($headerPath)
$marker = "#include <JWPLC_Bundled_Adafruit_BusIO.h>"
$spiInclude = "#include <Adafruit_SPIDevice.h>"
$markerIndex = $headerText.IndexOf($marker)
$spiIndex = $headerText.IndexOf($spiInclude)

Write-Host "BUNDLED_MARKER_PRESENT=$($markerIndex -ge 0)"
Write-Host "BUNDLED_MARKER_BEFORE_SPI=$($markerIndex -ge 0 -and $spiIndex -gt $markerIndex)"

if ($markerIndex -lt 0 -or $spiIndex -le $markerIndex) {
    throw "A12_JW_FRAM_BUNDLED_MARKER_CONTRACT_FAILED"
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo ("tools\alpha12\results\jw_fram_precompiled_refresh_" + $stamp)
}

New-Item -ItemType Directory -Path $ResultRoot -Force | Out-Null

$runRoot = Join-Path $env:TEMP ("jwplc_a12_jw_fram_refresh_" + $stamp)
$sketchRoot = Join-Path $runRoot "jw_fram_gate"
$sourceRoot = Join-Path $runRoot "source-libraries\JW_FRAM"
$sourceSrc = Join-Path $sourceRoot "src"
$sourceBuild = Join-Path $runRoot "source-build"
$candidateRoot = Join-Path $runRoot "candidate-libraries\JW_FRAM"
$candidateSrc = Join-Path $candidateRoot "src"
$candidateArchiveDir = Join-Path $candidateSrc "esp32"
$candidateArchive = Join-Path $candidateArchiveDir "libJW_FRAM.a"
$candidateBuild = Join-Path $runRoot "candidate-build"
$extractDir = Join-Path $runRoot "archive-members"

foreach ($dir in @(
    $sketchRoot,
    $sourceSrc,
    $sourceBuild,
    $candidateArchiveDir,
    $candidateBuild,
    $extractDir
)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

$utf8NoBom = [Text.UTF8Encoding]::new($false)

$sketchPath = Join-Path $sketchRoot "jw_fram_gate.ino"
$sketchText = @'
#include <Arduino.h>
#include <JW_FRAM.h>

JW_FRAM fram(5);

void setup()
{
    volatile uint32_t sizeBytes = fram.size();
    volatile uint8_t addressBytes = fram.addressSize();
    volatile bool valid = fram.isAddressValid(0, 1);
    (void)sizeBytes;
    (void)addressBytes;
    (void)valid;
}

void loop()
{
}
'@
[IO.File]::WriteAllText($sketchPath, $sketchText, $utf8NoBom)

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $sourceSrc "JW_FRAM.h") -Force
Copy-Item -LiteralPath $cppPath -Destination (Join-Path $sourceSrc "JW_FRAM.cpp") -Force

$sourceProperties = @'
name=JW_FRAM
version=1.0.3-alpha12-source
author=JW Control
maintainer=JW Control
sentence=Alpha12 source qualification for JW_FRAM.
paragraph=Temporary source-only qualification library.
category=Data Storage
architectures=esp32
depends=Adafruit BusIO
'@
[IO.File]::WriteAllText((Join-Path $sourceRoot "library.properties"), $sourceProperties, $utf8NoBom)

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $candidateSrc "JW_FRAM.h") -Force

$candidateProperties = @'
name=JW_FRAM
version=1.0.3-alpha12-candidate
author=JW Control
maintainer=JW Control
sentence=Alpha12 precompiled candidate for JW_FRAM.
paragraph=Temporary precompiled qualification library.
category=Data Storage
architectures=esp32
depends=Adafruit BusIO
precompiled=full
'@
[IO.File]::WriteAllText((Join-Path $candidateRoot "library.properties"), $candidateProperties, $utf8NoBom)

$oldSha = Get-Sha256Lower $archivePath
$oldBytes = (Get-Item -LiteralPath $archivePath).Length
$backup = Join-Path $env:TEMP ("a12_jw_fram_archive_backup_" + $stamp + ".a")
Copy-Item -LiteralPath $archivePath -Destination $backup -Force

Write-Host "OLD_ARCHIVE_BYTES=$oldBytes"
Write-Host "OLD_ARCHIVE_SHA256=$oldSha"
Write-Host "RESULT_ROOT=$ResultRoot"

$success = $false

try {
    Write-Host ""
    Write-Host "=== SOURCE-ONLY JW_FRAM BUILD ==="

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

    $sourceRun.Output | Set-Content -LiteralPath (Join-Path $ResultRoot "source.log") -Encoding UTF8
    Write-Host "SOURCE_COMPILE_EXIT=$($sourceRun.ExitCode)"

    if ($sourceRun.ExitCode -ne 0) {
        $sourceRun.Output | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }
        throw "A12_JW_FRAM_SOURCE_COMPILE_FAILED"
    }

    Assert-LibrarySelected -Lines $sourceRun.Output -LibraryName "JW_FRAM" -ExpectedRoot $sourceRoot -Label "SOURCE"
    Assert-BundledBusIo -Lines $sourceRun.Output -Label "SOURCE"

    [object[]]$sourceObjects = @(
        Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JW_FRAM.cpp.o" }
    )

    Write-Host "SOURCE_OBJECT_COUNT=$($sourceObjects.Count)"

    if ($sourceObjects.Count -ne 1) {
        throw "A12_JW_FRAM_SOURCE_OBJECT_SET_INVALID"
    }

    $sourcePrecompiled = @(
        $sourceRun.Output | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match '\bJW_FRAM\b')
        }
    ).Count -gt 0

    Write-Host "SOURCE_PRECOMPILED_MARKER=$(if ($sourcePrecompiled) { 'YES' } else { 'NO' })"

    if ($sourcePrecompiled) {
        throw "A12_JW_FRAM_SOURCE_UNEXPECTED_PRECOMPILED"
    }

    $archiver = Resolve-Archiver -Lines $sourceRun.Output
    Write-Host "ARCHIVER=$archiver"

    Write-Host ""
    Write-Host "=== CREATE JW_FRAM CANDIDATE ARCHIVE ==="

    $archiveRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @(
        "crs",
        $candidateArchive,
        $sourceObjects[0].FullName
    )

    if ($archiveRun.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $candidateArchive)) {
        throw "A12_JW_FRAM_ARCHIVE_CREATE_FAILED"
    }

    $listRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $candidateArchive)

    if ($listRun.ExitCode -ne 0) {
        throw "A12_JW_FRAM_ARCHIVE_LIST_FAILED"
    }

    [string[]]$members = @(
        $listRun.Output |
            ForEach-Object { $_.Trim() } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )

    Write-Host "ARCHIVE_MEMBER_COUNT=$($members.Count)"
    $members | ForEach-Object { Write-Host "ARCHIVE_MEMBER=$_" }

    if ($members.Count -ne 1 -or $members[0] -ne "JW_FRAM.cpp.o") {
        throw "A12_JW_FRAM_ARCHIVE_MEMBER_SET_INVALID"
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
        throw "A12_JW_FRAM_ARCHIVE_EXTRACT_FAILED"
    }

    $extractedObject = Join-Path $extractDir "JW_FRAM.cpp.o"
    $sourceObjectSha = Get-Sha256Lower $sourceObjects[0].FullName
    $archiveObjectSha = Get-Sha256Lower $extractedObject

    Write-Host "SOURCE_OBJECT_SHA256=$sourceObjectSha"
    Write-Host "ARCHIVE_MEMBER_SHA256=$archiveObjectSha"
    Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=$($sourceObjectSha -eq $archiveObjectSha)"

    if ($sourceObjectSha -ne $archiveObjectSha) {
        throw "A12_JW_FRAM_ARCHIVE_MEMBER_PARITY_FAILED"
    }

    $candidateSha = Get-Sha256Lower $candidateArchive
    $candidateBytes = (Get-Item -LiteralPath $candidateArchive).Length
    $archiveChanged = $candidateSha -ne $oldSha

    Write-Host "CANDIDATE_ARCHIVE_BYTES=$candidateBytes"
    Write-Host "CANDIDATE_ARCHIVE_SHA256=$candidateSha"
    Write-Host "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"

    Write-Host ""
    Write-Host "=== CANDIDATE PRECOMPILED JW_FRAM LINK ==="

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

    $candidateRun.Output | Set-Content -LiteralPath (Join-Path $ResultRoot "candidate.log") -Encoding UTF8
    Write-Host "CANDIDATE_COMPILE_EXIT=$($candidateRun.ExitCode)"

    if ($candidateRun.ExitCode -ne 0) {
        $candidateRun.Output | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }
        throw "A12_JW_FRAM_CANDIDATE_COMPILE_FAILED"
    }

    Assert-LibrarySelected -Lines $candidateRun.Output -LibraryName "JW_FRAM" -ExpectedRoot $candidateRoot -Label "CANDIDATE"
    Assert-BundledBusIo -Lines $candidateRun.Output -Label "CANDIDATE"

    [int]$candidateSourceObjects = @(
        Get-ChildItem -LiteralPath $candidateBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JW_FRAM.cpp.o" }
    ).Count

    $candidatePrecompiled = @(
        $candidateRun.Output | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match '\bJW_FRAM\b')
        }
    ).Count -gt 0

    Write-Host "CANDIDATE_SOURCE_OBJECT_COUNT=$candidateSourceObjects"
    Write-Host "CANDIDATE_PRECOMPILED_MARKER=$(if ($candidatePrecompiled) { 'YES' } else { 'NO' })"

    if ($candidateSourceObjects -ne 0 -or -not $candidatePrecompiled) {
        throw "A12_JW_FRAM_CANDIDATE_PRECOMPILED_POLICY_FAILED"
    }

    Write-Host ""
    Write-Host "=== PLACE CANDIDATE IN OFFICIAL WORKTREE ==="

    Copy-Item -LiteralPath $candidateArchive -Destination $archivePath -Force

    $officialSha = Get-Sha256Lower $archivePath
    $officialBytes = (Get-Item -LiteralPath $archivePath).Length

    if ($officialSha -ne $candidateSha -or $officialBytes -ne $candidateBytes) {
        throw "A12_JW_FRAM_OFFICIAL_CANDIDATE_IDENTITY_FAILED"
    }

    [string[]]$finalDirty = @(
        & git -C $repo diff --name-only |
            ForEach-Object { $_.Replace("\", "/") } |
            Sort-Object
    )

    Write-Host "FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
    $finalDirty | ForEach-Object { Write-Host "FINAL_TRACKED_DIRTY=$_" }

    if ($archiveChanged) {
        if ($finalDirty.Count -ne 1 -or $finalDirty[0] -ne $archiveRel) {
            throw "A12_JW_FRAM_FINAL_DIRTY_SCOPE_INVALID"
        }
    }
    else {
        if ($finalDirty.Count -ne 0) {
            throw "A12_JW_FRAM_IDENTICAL_ARCHIVE_DIRTY_UNEXPECTED"
        }
    }

    $success = $true

    @(
        "ALPHA12_JW_FRAM_PRECOMPILED_REFRESH=PASS"
        "HEAD_SOURCE=$head"
        "OLD_ARCHIVE_BYTES=$oldBytes"
        "OLD_ARCHIVE_SHA256=$oldSha"
        "NEW_ARCHIVE_BYTES=$candidateBytes"
        "NEW_ARCHIVE_SHA256=$candidateSha"
        "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"
        "ARCHIVE_MEMBER_COUNT=1"
        "ARCHIVE_MEMBER=JW_FRAM.cpp.o"
        "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
        "SOURCE_BUNDLED_BUSIO_SELECTED=PASS"
        "CANDIDATE_BUNDLED_BUSIO_SELECTED=PASS"
        "CANDIDATE_PRECOMPILED_LINK=PASS"
        "FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "SUMMARY.txt") -Encoding UTF8

    Write-Host ""
    Write-Host "=============================================================================="
    Write-Host " SUMMARY"
    Write-Host "=============================================================================="
    Write-Host "ALPHA12_JW_FRAM_PRECOMPILED_REFRESH=PASS"
    Write-Host "OLD_ARCHIVE_SHA256=$oldSha"
    Write-Host "NEW_ARCHIVE_SHA256=$candidateSha"
    Write-Host "NEW_ARCHIVE_BYTES=$candidateBytes"
    Write-Host "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"
    Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
    Write-Host "SOURCE_BUNDLED_BUSIO_SELECTED=PASS"
    Write-Host "CANDIDATE_BUNDLED_BUSIO_SELECTED=PASS"
    Write-Host "CANDIDATE_PRECOMPILED_LINK=PASS"
    Write-Host "FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
finally {
    if (-not $success) {
        Write-Host "A12_JW_FRAM_ROLLBACK=START"

        if (Test-Path -LiteralPath $backup) {
            Copy-Item -LiteralPath $backup -Destination $archivePath -Force
        }

        $restoredSha = Get-Sha256Lower $archivePath
        Write-Host "A12_JW_FRAM_ROLLBACK_SHA256=$restoredSha"
        Write-Host "A12_JW_FRAM_ROLLBACK=DONE"
    }
}
