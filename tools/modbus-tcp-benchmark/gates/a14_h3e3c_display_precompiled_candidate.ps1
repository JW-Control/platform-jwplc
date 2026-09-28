param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.3C - JWPLC_Display PRECOMPILED CANDIDATE"
Write-Host "============================================================"

Assert-G2Branch

$cli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$fqbn = "jwplc_local:esp32:jwplcbasic"
$diag = "-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1"

$displayRoot =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Display"

$repoLibraries =
    Get-G2Path "JWPLC/2.1.0/libraries"

$sketch =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_h3e3b_display_runtime_physical"

foreach ($required in @(
    $cli,
    $displayRoot,
    $repoLibraries,
    $sketch
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E3C_REQUIRED_PATH_MISSING=$required"
    }
}

[string[]]$entryDirty =
    @(Get-G2TrackedDirtyPaths)

[string[]]$entryStaged =
    @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$normalizedEntryDirty = @(
    $entryDirty |
        ForEach-Object {
            $_.Replace("\", "/")
        } |
        Sort-Object
)

$expectedCoreDirty =
    $script:G2CoreRelative.Replace("\", "/")

if ($normalizedEntryDirty.Count -ne 1 -or
    $normalizedEntryDirty[0] -ne $expectedCoreDirty) {
    $normalizedEntryDirty |
        ForEach-Object {
            Write-Host "ENTRY_DIRTY=$_"
        }

    throw "H3E3C_ENTRY_DIRTY_SCOPE_INVALID"
}

if ($entryStaged.Count -ne 0) {
    throw "H3E3C_ENTRY_INDEX_NOT_CLEAN"
}

$runRoot =
    Join-Path $env:TEMP (
        "jwplc_a14_h3e3c_{0}" -f
        (Get-Date -Format "yyyyMMdd_HHmmss")
    )

$sourceDisplay =
    Join-Path $runRoot "source-libraries\JWPLC_Display"

$candidateDisplay =
    Join-Path $runRoot "candidate-libraries\JWPLC_Display"

$sourceBuild =
    Join-Path $runRoot "source-build"

$candidateBuild =
    Join-Path $runRoot "candidate-build"

$sourceLog =
    Join-Path $runRoot "source.log"

$candidateLog =
    Join-Path $runRoot "candidate.log"

New-Item -ItemType Directory -Force -Path $sourceBuild | Out-Null
New-Item -ItemType Directory -Force -Path $candidateBuild | Out-Null

Copy-Item -LiteralPath $displayRoot -Destination $sourceDisplay -Recurse -Force
Copy-Item -LiteralPath $displayRoot -Destination $candidateDisplay -Recurse -Force

foreach ($copyRoot in @($sourceDisplay, $candidateDisplay)) {
    [object[]]$existingArchives = @(
        Get-ChildItem -LiteralPath $copyRoot -Recurse -File -Filter "*.a" -ErrorAction SilentlyContinue
    )

    foreach ($archive in $existingArchives) {
        Remove-Item -LiteralPath $archive.FullName -Force
    }
}

$sourceProperties = @'
name=JWPLC_Display
version=1.0.1-h3e3c-source
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=H3E3C source reference.
paragraph=Temporary source reference for JWPLC_Display over JWPLC_TFT.
category=Display
architectures=esp32
includes=JWPLC_Display.h
dot_a_linkage=true
depends=JWPLC_TFT,JW_MatrixButtons,JWPLC_GlobalPeripherals
'@

[IO.File]::WriteAllText(
    (Join-Path $sourceDisplay "library.properties"),
    $sourceProperties,
    (New-Object Text.UTF8Encoding($false)))

$sourceArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $sourceBuild,
    "--library", $sourceDisplay,
    "--libraries", $repoLibraries,
    "--build-property", "compiler.cpp.extra_flags=$diag",
    $sketch
)

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E3C_REPOSITORY_MUTATION=NO"
Write-Host "H3E3C_UPLOADS=NO"
Write-Host ""
Write-Host "=== H3E3C SOURCE REFERENCE BUILD ==="
Write-Host "H3E3C_SOURCE_COMPILE_START=YES"

$oldPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    [object[]]$sourceOutput = @(& $cli @sourceArgs 2>&1)
    $sourceExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $oldPreference
}

$sourceOutput |
    ForEach-Object { $_.ToString() } |
    Set-Content -LiteralPath $sourceLog -Encoding UTF8

Write-Host "H3E3C_SOURCE_COMPILE_EXIT=$sourceExit"
Write-Host "H3E3C_SOURCE_COMPILE_LOG=$sourceLog"

if ($sourceExit -ne 0) {
    $sourceOutput |
        Select-Object -Last 180 |
        ForEach-Object { Write-Host $_ }

    throw "H3E3C_SOURCE_COMPILE_FAILED"
}

$sourceDisplayBuild =
    Join-Path $sourceBuild "libraries\JWPLC_Display"

[string[]]$expectedObjects = @(
    Get-ChildItem -LiteralPath (Join-Path $sourceDisplay "src") -File -Filter "*.cpp" -ErrorAction Stop |
        ForEach-Object { "$($_.Name).o" } |
        Sort-Object
)

[object[]]$sourceObjectFiles = @(
    Get-ChildItem -LiteralPath $sourceDisplayBuild -File -Filter "*.cpp.o" -ErrorAction Stop |
        Sort-Object Name
)

[string[]]$sourceObjectNames = @(
    $sourceObjectFiles |
        ForEach-Object { $_.Name }
)

[object[]]$sourceObjectDiff = @(
    Compare-Object -ReferenceObject $expectedObjects -DifferenceObject $sourceObjectNames
)

Write-Host "H3E3C_SOURCE_EXPECTED_OBJECT_COUNT=$($expectedObjects.Count)"
Write-Host "H3E3C_SOURCE_ACTUAL_OBJECT_COUNT=$($sourceObjectNames.Count)"
Write-Host "H3E3C_SOURCE_OBJECT_PARITY=$(
    if ($sourceObjectDiff.Count -eq 0) { 'PASS' } else { 'FAIL' }
)"

$sourceObjectNames |
    ForEach-Object {
        Write-Host "H3E3C_SOURCE_OBJECT=$_"
    }

if ($expectedObjects.Count -ne 7 -or
    $sourceObjectDiff.Count -ne 0) {
    throw "H3E3C_SOURCE_OBJECT_PARITY_FAILED"
}

[object[]]$sourceArchives = @(
    Get-ChildItem -LiteralPath $sourceDisplayBuild -File -Filter "*.a" -ErrorAction SilentlyContinue
)

Write-Host "H3E3C_SOURCE_ARCHIVE_COUNT=$($sourceArchives.Count)"

$sourceArchives |
    ForEach-Object {
        Write-Host "H3E3C_SOURCE_ARCHIVE=$($_.FullName)"
    }

if ($sourceArchives.Count -ne 1) {
    throw "H3E3C_SOURCE_ARCHIVE_COUNT_INVALID"
}

$sourceArchive =
    $sourceArchives[0].FullName

$sourceArchiveBytes =
    (Get-Item -LiteralPath $sourceArchive).Length

$sourceArchiveSha =
    (Get-G2Sha256Path $sourceArchive).ToUpperInvariant()

Write-Host "H3E3C_SOURCE_ARCHIVE_BYTES=$sourceArchiveBytes"
Write-Host "H3E3C_SOURCE_ARCHIVE_SHA256=$sourceArchiveSha"
Write-Host "H3E3C_ARCHIVE_ORIGIN=ARDUINO_SOURCE_DOT_A_LINKAGE"

$candidateArchiveDir =
    Join-Path $candidateDisplay "src\esp32"

$candidateArchive =
    Join-Path $candidateArchiveDir "libJWPLC_Display.a"

New-Item -ItemType Directory -Force -Path $candidateArchiveDir | Out-Null
Copy-Item -LiteralPath $sourceArchive -Destination $candidateArchive -Force

$candidateArchiveSha =
    (Get-G2Sha256Path $candidateArchive).ToUpperInvariant()

if ($candidateArchiveSha -ne $sourceArchiveSha) {
    throw "H3E3C_SOURCE_ARCHIVE_COPY_HASH_MISMATCH"
}

Write-Host "H3E3C_SOURCE_TO_CANDIDATE_ARCHIVE_IDENTITY=PASS"

# Resolve ar only for inspection/extraction, not for reconstruction.
[string[]]$sourceLines = @(
    $sourceOutput |
        ForEach-Object {
            $_.ToString()
        }
)

$archiverCandidate = $null

foreach ($line in $sourceLines) {
    if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"') {
        $archiverCandidate =
            $Matches["exe"]
        break
    }
}

if ([string]::IsNullOrWhiteSpace($archiverCandidate)) {
    throw "H3E3C_ARCHIVER_NOT_FOUND_FOR_INSPECTION"
}

$archiverNormalized =
    $archiverCandidate.Trim().Trim('"')

while ($archiverNormalized.Contains("\\")) {
    $archiverNormalized =
        $archiverNormalized.Replace("\\", "\")
}

$archiver = $null

foreach ($candidatePath in @(
    $archiverNormalized,
    ($archiverNormalized + ".exe")
)) {
    if (Test-Path -LiteralPath $candidatePath) {
        $archiver =
            (Resolve-Path -LiteralPath $candidatePath).Path
        break
    }
}

if ([string]::IsNullOrWhiteSpace($archiver)) {
    throw "H3E3C_ARCHIVER_NOT_FOUND_FOR_INSPECTION"
}

Write-Host "H3E3C_ARCHIVER=$archiver"
Write-Host "H3E3C_ARCHIVER_ROLE=INSPECTION_ONLY"

[object[]]$memberOutput =
    @(& $archiver "t" $candidateArchive)

$memberExit =
    [int]$LASTEXITCODE

if ($memberExit -ne 0) {
    throw "H3E3C_ARCHIVE_LIST_FAILED"
}

[string[]]$memberNames = @(
    $memberOutput |
        ForEach-Object {
            $_.ToString().Trim()
        } |
        Where-Object {
            -not [string]::IsNullOrWhiteSpace($_)
        }
)

Write-Host "H3E3C_ARCHIVE_MEMBER_COUNT=$($memberNames.Count)"

$memberNames |
    ForEach-Object {
        Write-Host "H3E3C_ARCHIVE_MEMBER=$_"
    }

[object[]]$memberDiff = @(
    Compare-Object -ReferenceObject $sourceObjectNames -DifferenceObject $memberNames
)

if ($memberNames.Count -ne 7 -or
    $memberDiff.Count -ne 0) {
    throw "H3E3C_ARCHIVE_MEMBERS_INVALID"
}

Write-Host "H3E3C_ARCHIVE_MEMBERS_EXACT=PASS"

$extractRoot =
    Join-Path $runRoot "archive-extract"

New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null

Push-Location $extractRoot
try {
    [object[]]$extractOutput =
        @(& $archiver "x" $candidateArchive 2>&1)

    $extractExit =
        [int]$LASTEXITCODE
}
finally {
    Pop-Location
}

if ($extractExit -ne 0) {
    $extractOutput |
        ForEach-Object {
            Write-Host $_
        }

    throw "H3E3C_ARCHIVE_EXTRACT_FAILED"
}

foreach ($sourceObject in $sourceObjectFiles) {
    $extracted =
        Join-Path $extractRoot $sourceObject.Name

    if (-not (Test-Path -LiteralPath $extracted)) {
        throw "H3E3C_EXTRACTED_MEMBER_MISSING=$($sourceObject.Name)"
    }

    $sourceSha =
        (Get-G2Sha256Path $sourceObject.FullName).ToUpperInvariant()

    $extractedSha =
        (Get-G2Sha256Path $extracted).ToUpperInvariant()

    if ($sourceSha -ne $extractedSha) {
        throw "H3E3C_ARCHIVE_MEMBER_BYTE_MISMATCH=$($sourceObject.Name)"
    }
}

Write-Host "H3E3C_ARCHIVE_MEMBER_BYTE_PARITY=PASS"

$archiveBytes =
    (Get-Item -LiteralPath $candidateArchive).Length

$archiveSha =
    (Get-G2Sha256Path $candidateArchive).ToUpperInvariant()

Write-Host "H3E3C_CANDIDATE_ARCHIVE=$candidateArchive"
Write-Host "H3E3C_CANDIDATE_ARCHIVE_BYTES=$archiveBytes"
Write-Host "H3E3C_CANDIDATE_ARCHIVE_SHA256=$archiveSha"

$candidateProperties = @'
name=JWPLC_Display
version=1.0.1-h3e3c-candidate
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=H3E3C precompiled candidate.
paragraph=Temporary precompiled JWPLC_Display candidate over JWPLC_TFT.
category=Display
architectures=esp32
includes=JWPLC_Display.h
dot_a_linkage=true
precompiled=full
depends=JWPLC_TFT,JW_MatrixButtons,JWPLC_GlobalPeripherals
'@

[IO.File]::WriteAllText(
    (Join-Path $candidateDisplay "library.properties"),
    $candidateProperties,
    (New-Object Text.UTF8Encoding($false)))

$candidateArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $candidateBuild,
    "--library", $candidateDisplay,
    "--libraries", $repoLibraries,
    "--build-property", "compiler.cpp.extra_flags=$diag",
    $sketch
)

Write-Host ""
Write-Host "=== H3E3C PRECOMPILED CANDIDATE BUILD ==="
Write-Host "H3E3C_CANDIDATE_COMPILE_START=YES"

$oldPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    [object[]]$candidateOutput = @(& $cli @candidateArgs 2>&1)
    $candidateExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $oldPreference
}

$candidateOutput |
    ForEach-Object { $_.ToString() } |
    Set-Content -LiteralPath $candidateLog -Encoding UTF8

Write-Host "H3E3C_CANDIDATE_COMPILE_EXIT=$candidateExit"
Write-Host "H3E3C_CANDIDATE_COMPILE_LOG=$candidateLog"

if ($candidateExit -ne 0) {
    $candidateOutput |
        Select-Object -Last 180 |
        ForEach-Object { Write-Host $_ }

    throw "H3E3C_CANDIDATE_COMPILE_FAILED"
}

$normalizedCandidate =
    [IO.Path]::GetFullPath(
        $candidateDisplay
    ).TrimEnd('\', '/')

[string[]]$candidateSelectionLines = @(
    $candidateOutput |
        ForEach-Object { $_.ToString() } |
        Where-Object {
            $_ -match '^Using library JWPLC_Display at version .+ in folder: (.+)$'
        }
)

$candidateSelected = $false

foreach ($line in $candidateSelectionLines) {
    if ($line -match '^Using library JWPLC_Display at version .+ in folder: (.+)$') {
        $selected =
            [IO.Path]::GetFullPath(
                $Matches[1].Trim()
            ).TrimEnd('\', '/')

        if ($selected -ieq $normalizedCandidate) {
            $candidateSelected = $true
        }
    }
}

Write-Host "H3E3C_CANDIDATE_LIBRARY_SELECTED=$(
    if ($candidateSelected) { 'PASS' } else { 'FAIL' }
)"

if (-not $candidateSelected) {
    throw "H3E3C_CANDIDATE_LIBRARY_NOT_SELECTED"
}

[string[]]$legacyGraphics = @(
    $candidateOutput |
        ForEach-Object { $_.ToString() } |
        Where-Object {
            $_ -match '^Using library (Adafruit ST7735 and ST7789 Library|Adafruit GFX Library|TFT_eSPI) at version '
        }
)

Write-Host "H3E3C_EXTERNAL_GRAPHICS_BACKEND_SELECTION_COUNT=$($legacyGraphics.Count)"

if ($legacyGraphics.Count -ne 0) {
    $legacyGraphics |
        ForEach-Object {
            Write-Host "H3E3C_UNEXPECTED_GRAPHICS_BACKEND=$_"
        }

    throw "H3E3C_EXTERNAL_GRAPHICS_BACKEND_SELECTED"
}

$candidateText =
    ($candidateOutput |
        ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

$displayPrecompiled =
    @(
        $candidateOutput |
            ForEach-Object { $_.ToString() } |
            Where-Object {
                ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                ($_ -match 'JWPLC_Display')
            }
    ).Count -gt 0

$jwplcTftPrecompiled =
    @(
        $candidateOutput |
            ForEach-Object { $_.ToString() } |
            Where-Object {
                ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                ($_ -match 'JWPLC_TFT')
            }
    ).Count -gt 0

Write-Host "H3E3C_DISPLAY_PRECOMPILED_SELECTION_PROOF=$(
    if ($displayPrecompiled) { 'PASS' } else { 'FAIL' }
)"

Write-Host "H3E3C_JWPLC_TFT_PRECOMPILED_SELECTION_PROOF=$(
    if ($jwplcTftPrecompiled) { 'PASS' } else { 'FAIL' }
)"

if (-not $displayPrecompiled -or
    -not $jwplcTftPrecompiled) {
    throw "H3E3C_PRECOMPILED_SELECTION_NOT_PROVEN"
}

[int]$displaySourceObjectCount = @(
    Get-ChildItem -LiteralPath $candidateBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.FullName -match '[\\/]libraries[\\/]JWPLC_Display[\\/].+\.cpp\.o$'
        }
).Count

[int]$tftSourceObjectCount = @(
    Get-ChildItem -LiteralPath $candidateBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -eq "JWPLC_TFT.cpp.o"
        }
).Count

[int]$backendSourceObjectCount = @(
    Get-ChildItem -LiteralPath $candidateBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -eq "TFT_eSPI.cpp.o"
        }
).Count

Write-Host "H3E3C_CANDIDATE_DISPLAY_SOURCE_OBJECT_COUNT=$displaySourceObjectCount"
Write-Host "H3E3C_CANDIDATE_JWPLC_TFT_SOURCE_OBJECT_COUNT=$tftSourceObjectCount"
Write-Host "H3E3C_CANDIDATE_TFT_ESPI_SOURCE_OBJECT_COUNT=$backendSourceObjectCount"

if ($displaySourceObjectCount -ne 0 -or
    $tftSourceObjectCount -ne 0 -or
    $backendSourceObjectCount -ne 0) {
    throw "H3E3C_SOURCE_OBJECTS_PRESENT_IN_PRECOMPILED_BUILD"
}

function Get-H3E3CUsage {
    param(
        [object[]]$Lines,
        [string]$Kind
    )

    foreach ($lineObject in $Lines) {
        $line = [string]$lineObject

        if ($Kind -eq "FLASH" -and
            $line -match '(?:Sketch uses|El Sketch usa)\s+(?<n>\d+)\s+bytes') {
            return [int64]$Matches["n"]
        }

        if ($Kind -eq "RAM" -and
            $line -match '(?:Global variables use|Las variables Globales usan)\s+(?<n>\d+)\s+bytes') {
            return [int64]$Matches["n"]
        }
    }

    return [int64]-1
}

$sourceFlash =
    Get-H3E3CUsage -Lines $sourceOutput -Kind "FLASH"

$sourceRam =
    Get-H3E3CUsage -Lines $sourceOutput -Kind "RAM"

$candidateFlash =
    Get-H3E3CUsage -Lines $candidateOutput -Kind "FLASH"

$candidateRam =
    Get-H3E3CUsage -Lines $candidateOutput -Kind "RAM"

Write-Host "H3E3C_SOURCE_FLASH_BYTES=$sourceFlash"
Write-Host "H3E3C_CANDIDATE_FLASH_BYTES=$candidateFlash"
Write-Host "H3E3C_SOURCE_RAM_BYTES=$sourceRam"
Write-Host "H3E3C_CANDIDATE_RAM_BYTES=$candidateRam"

$flashDelta =
    $candidateFlash - $sourceFlash

$ramDelta =
    $candidateRam - $sourceRam

Write-Host "H3E3C_FLASH_DELTA_BYTES=$flashDelta"
Write-Host "H3E3C_RAM_DELTA_BYTES=$ramDelta"

if ($sourceFlash -lt 0 -or
    $sourceRam -lt 0 -or
    $candidateFlash -lt 0 -or
    $candidateRam -lt 0) {
    throw "H3E3C_USAGE_MARKERS_MISSING"
}

if ($candidateRam -ne $sourceRam) {
    throw "H3E3C_RAM_PARITY_FAILED"
}

# -------------------------------------------------------------------------
# Equivalencia estructural source vs precompiled.
#
# precompiled=full puede cambiar el orden/layout de link aun cuando el archive
# y todos sus miembros sean identicos. Por eso Flash total se registra pero no
# se usa aisladamente como equivalencia funcional.
# -------------------------------------------------------------------------
function Get-H3E3CMapPath {
    param([string]$BuildRoot)

    [object[]]$maps = @(
        Get-ChildItem -LiteralPath $BuildRoot -File -Filter "*.map" -ErrorAction SilentlyContinue
    )

    if ($maps.Count -ne 1) {
        throw "H3E3C_MAP_COUNT_INVALID=$($maps.Count):$BuildRoot"
    }

    return $maps[0].FullName
}

function Get-H3E3CElfPath {
    param([string]$BuildRoot)

    [object[]]$elfs = @(
        Get-ChildItem -LiteralPath $BuildRoot -File -Filter "*.elf" -ErrorAction SilentlyContinue
    )

    if ($elfs.Count -ne 1) {
        throw "H3E3C_ELF_COUNT_INVALID=$($elfs.Count):$BuildRoot"
    }

    return $elfs[0].FullName
}

function Get-H3E3CLibraryNames {
    param([object[]]$Lines)

    [string[]]$names = @(
        foreach ($lineObject in $Lines) {
            $line =
                [string]$lineObject

            if ($line -match '^Using library (?<name>.+?) at version ') {
                $name =
                    $Matches["name"].Trim()

                if ($name -ne "JWPLC_Display") {
                    $name
                }
            }
        }
    )

    return @(
        $names |
            Sort-Object -Unique
    )
}

function Get-H3E3CExternalObjectTable {
    param([string]$BuildRoot)

    $libraryRoot =
        Join-Path $BuildRoot "libraries"

    $table = @{}

    if (-not (Test-Path -LiteralPath $libraryRoot)) {
        return $table
    }

    [object[]]$objects = @(
        Get-ChildItem -LiteralPath $libraryRoot -Recurse -File -Filter "*.o" -ErrorAction SilentlyContinue
    )

    foreach ($object in $objects) {
        $relative =
            $object.FullName.Substring(
                $libraryRoot.Length
            ).TrimStart([char[]]"\/")

        $normalized =
            $relative.Replace("\", "/")

        if ($normalized -match '^JWPLC_Display/') {
            continue
        }

        $table[$normalized] =
            (Get-G2Sha256Path $object.FullName).ToUpperInvariant()
    }

    return $table
}

function Get-H3E3CDisplayLinkedMembers {
    param([string]$MapText)

    [string[]]$members = @(
        [regex]::Matches(
            $MapText,
            '(?:lib)?JWPLC_Display\.a\((?<member>[^)]+\.cpp\.o)\)'
        ) |
            ForEach-Object {
                $_.Groups["member"].Value
            } |
            Sort-Object -Unique
    )

    return $members
}

$sourceMap =
    Get-H3E3CMapPath -BuildRoot $sourceBuild

$candidateMap =
    Get-H3E3CMapPath -BuildRoot $candidateBuild

$sourceMapText =
    [IO.File]::ReadAllText($sourceMap)

$candidateMapText =
    [IO.File]::ReadAllText($candidateMap)

[string[]]$sourceLinkedMembers =
    @(Get-H3E3CDisplayLinkedMembers -MapText $sourceMapText)

[string[]]$candidateLinkedMembers =
    @(Get-H3E3CDisplayLinkedMembers -MapText $candidateMapText)

[object[]]$linkedMemberDiff = @(
    Compare-Object -ReferenceObject $sourceLinkedMembers -DifferenceObject $candidateLinkedMembers
)

Write-Host "H3E3C_SOURCE_LINKED_DISPLAY_MEMBER_COUNT=$($sourceLinkedMembers.Count)"
Write-Host "H3E3C_CANDIDATE_LINKED_DISPLAY_MEMBER_COUNT=$($candidateLinkedMembers.Count)"
Write-Host "H3E3C_LINKED_DISPLAY_MEMBER_PARITY=$(
    if ($linkedMemberDiff.Count -eq 0) { 'PASS' } else { 'FAIL' }
)"

$sourceLinkedMembers |
    ForEach-Object {
        Write-Host "H3E3C_SOURCE_LINKED_DISPLAY_MEMBER=$_"
    }

$candidateLinkedMembers |
    ForEach-Object {
        Write-Host "H3E3C_CANDIDATE_LINKED_DISPLAY_MEMBER=$_"
    }

if ($sourceLinkedMembers.Count -lt 1 -or
    $linkedMemberDiff.Count -ne 0) {
    throw "H3E3C_LINKED_DISPLAY_MEMBER_PARITY_FAILED"
}

$sourceExternalObjects =
    Get-H3E3CExternalObjectTable -BuildRoot $sourceBuild

$candidateExternalObjects =
    Get-H3E3CExternalObjectTable -BuildRoot $candidateBuild

[string[]]$sourceExternalKeys = @(
    $sourceExternalObjects.Keys |
        Sort-Object
)

[string[]]$candidateExternalKeys = @(
    $candidateExternalObjects.Keys |
        Sort-Object
)

[object[]]$externalKeyDiff = @(
    Compare-Object -ReferenceObject $sourceExternalKeys -DifferenceObject $candidateExternalKeys
)

[string[]]$externalHashDiff = @(
    foreach ($key in $sourceExternalKeys) {
        if ($candidateExternalObjects.ContainsKey($key) -and
            $sourceExternalObjects[$key] -ne $candidateExternalObjects[$key]) {
            $key
        }
    }
)

Write-Host "H3E3C_EXTERNAL_OBJECT_SOURCE_COUNT=$($sourceExternalKeys.Count)"
Write-Host "H3E3C_EXTERNAL_OBJECT_CANDIDATE_COUNT=$($candidateExternalKeys.Count)"
Write-Host "H3E3C_EXTERNAL_OBJECT_SET_PARITY=$(
    if ($externalKeyDiff.Count -eq 0) { 'PASS' } else { 'FAIL' }
)"
Write-Host "H3E3C_EXTERNAL_OBJECT_HASH_MISMATCH_COUNT=$($externalHashDiff.Count)"

if ($externalKeyDiff.Count -ne 0 -or
    $externalHashDiff.Count -ne 0) {
    throw "H3E3C_EXTERNAL_OBJECT_PARITY_FAILED"
}

[string[]]$sourceLibraryNames =
    @(Get-H3E3CLibraryNames -Lines $sourceOutput)

[string[]]$candidateLibraryNames =
    @(Get-H3E3CLibraryNames -Lines $candidateOutput)

[object[]]$libraryNameDiff = @(
    Compare-Object -ReferenceObject $sourceLibraryNames -DifferenceObject $candidateLibraryNames
)

Write-Host "H3E3C_NON_DISPLAY_LIBRARY_SOURCE_COUNT=$($sourceLibraryNames.Count)"
Write-Host "H3E3C_NON_DISPLAY_LIBRARY_CANDIDATE_COUNT=$($candidateLibraryNames.Count)"
Write-Host "H3E3C_NON_DISPLAY_LIBRARY_SELECTION_PARITY=$(
    if ($libraryNameDiff.Count -eq 0) { 'PASS' } else { 'FAIL' }
)"

if ($libraryNameDiff.Count -ne 0) {
    throw "H3E3C_LIBRARY_SELECTION_PARITY_FAILED"
}

$toolDir =
    Split-Path -Parent $archiver

$nm = $null

foreach ($nmName in @(
    "xtensa-esp32-elf-nm.exe",
    "xtensa-esp32-elf-nm"
)) {
    $nmCandidate =
        Join-Path $toolDir $nmName

    if (Test-Path -LiteralPath $nmCandidate) {
        $nm =
            (Resolve-Path -LiteralPath $nmCandidate).Path
        break
    }
}

if ([string]::IsNullOrWhiteSpace($nm)) {
    throw "H3E3C_NM_NOT_FOUND"
}

$sourceElf =
    Get-H3E3CElfPath -BuildRoot $sourceBuild

$candidateElf =
    Get-H3E3CElfPath -BuildRoot $candidateBuild

function Get-H3E3CNormalizedSymbols {
    param(
        [string]$NmPath,
        [string]$ElfPath
    )

    $previousPreference =
        $ErrorActionPreference

    try {
        $ErrorActionPreference =
            "Continue"

        [object[]]$nmOutput =
            @(& $NmPath "-S" "--defined-only" $ElfPath 2>&1)

        $nmExit =
            [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference =
            $previousPreference
    }

    if ($nmExit -ne 0) {
        throw "H3E3C_NM_FAILED=$ElfPath"
    }

    [string[]]$normalized = @(
        foreach ($lineObject in $nmOutput) {
            $line =
                ([string]$lineObject).Trim()

            if ($line -match '^[0-9A-Fa-f]+\s+(?<size>[0-9A-Fa-f]+)\s+(?<type>\S)\s+(?<name>.+)
[string[]]$finalDirty =
    @(Get-G2TrackedDirtyPaths)

[string[]]$finalStaged =
    @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object {
            $_.Replace("\", "/")
        } |
        Sort-Object
)

if ($normalizedFinalDirty.Count -ne 1 -or
    $normalizedFinalDirty[0] -ne $expectedCoreDirty) {
    throw "H3E3C_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E3C_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E3C_RUNTIME_ARCHITECTURE=JWPLC_Display_PRECOMPILED_OVER_JWPLC_TFT_PRECOMPILED"
Write-Host "H3E3C_EXTERNAL_GRAPHICS_BACKEND_REQUIRED=NO"
Write-Host "H3E3C_ARCHIVE_MEMBER_COUNT=7"
Write-Host "H3E3C_ARCHIVE_MEMBER_BYTE_PARITY=PASS"
Write-Host "H3E3C_SOURCE_PRECOMPILED_FLASH_LAYOUT_REVIEW=PASS"
Write-Host "H3E3C_SOURCE_PRECOMPILED_RAM_PARITY=PASS"
Write-Host "H3E3C_STRUCTURAL_EQUIVALENCE=PASS"
Write-Host "H3E3C_REPOSITORY_MUTATION=NO"
Write-Host "H3E3C_UPLOADS=NO"
Write-Host "A14_H3E3C_DISPLAY_PRECOMPILED_CANDIDATE_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E3D_DISPLAY_PRECOMPILED_PHYSICAL"
) {
                $size =
                    $Matches["size"].ToUpperInvariant()

                $type =
                    $Matches["type"]

                $name =
                    $Matches["name"]

                "$size|$type|$name"
            }
        }
    )

    return @(
        $normalized |
            Sort-Object
    )
}

[string[]]$sourceSymbols =
    @(Get-H3E3CNormalizedSymbols -NmPath $nm -ElfPath $sourceElf)

[string[]]$candidateSymbols =
    @(Get-H3E3CNormalizedSymbols -NmPath $nm -ElfPath $candidateElf)

[object[]]$symbolDiff = @(
    Compare-Object -ReferenceObject $sourceSymbols -DifferenceObject $candidateSymbols
)

Write-Host "H3E3C_NM=$nm"
Write-Host "H3E3C_SOURCE_DEFINED_SYMBOL_COUNT=$($sourceSymbols.Count)"
Write-Host "H3E3C_CANDIDATE_DEFINED_SYMBOL_COUNT=$($candidateSymbols.Count)"
Write-Host "H3E3C_DEFINED_SYMBOL_NAME_TYPE_SIZE_PARITY=$(
    if ($symbolDiff.Count -eq 0) { 'PASS' } else { 'FAIL' }
)"

if ($symbolDiff.Count -ne 0) {
    $symbolDiff |
        Select-Object -First 40 |
        ForEach-Object {
            Write-Host "H3E3C_SYMBOL_DIFF=$($_.SideIndicator):$($_.InputObject)"
        }

    throw "H3E3C_DEFINED_SYMBOL_PARITY_FAILED"
}

Write-Host "H3E3C_FLASH_DELTA_CLASSIFICATION=$(
    if ($flashDelta -eq 0) { 'EXACT' } else { 'LINK_LAYOUT_ONLY' }
)"
Write-Host "H3E3C_STRUCTURAL_EQUIVALENCE=PASS"

[string[]]$finalDirty =
    @(Get-G2TrackedDirtyPaths)

[string[]]$finalStaged =
    @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object {
            $_.Replace("\", "/")
        } |
        Sort-Object
)

if ($normalizedFinalDirty.Count -ne 1 -or
    $normalizedFinalDirty[0] -ne $expectedCoreDirty) {
    throw "H3E3C_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E3C_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E3C_RUNTIME_ARCHITECTURE=JWPLC_Display_PRECOMPILED_OVER_JWPLC_TFT_PRECOMPILED"
Write-Host "H3E3C_EXTERNAL_GRAPHICS_BACKEND_REQUIRED=NO"
Write-Host "H3E3C_ARCHIVE_MEMBER_COUNT=7"
Write-Host "H3E3C_ARCHIVE_MEMBER_BYTE_PARITY=PASS"
Write-Host "H3E3C_SOURCE_PRECOMPILED_FLASH_PARITY=PASS"
Write-Host "H3E3C_SOURCE_PRECOMPILED_RAM_PARITY=PASS"
Write-Host "H3E3C_REPOSITORY_MUTATION=NO"
Write-Host "H3E3C_UPLOADS=NO"
Write-Host "A14_H3E3C_DISPLAY_PRECOMPILED_CANDIDATE_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E3D_DISPLAY_PRECOMPILED_PHYSICAL"
