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

function Resolve-H3E3CNativeTool {
    param([string]$Candidate)

    if ([string]::IsNullOrWhiteSpace($Candidate)) {
        return $null
    }

    $normalized =
        $Candidate.Trim().Trim('"')

    while ($normalized.Contains("\\")) {
        $normalized =
            $normalized.Replace("\\", "\")
    }

    [string[]]$paths = @(
        $normalized
    )

    if (-not [IO.Path]::HasExtension($normalized)) {
        $paths += @(
            ($normalized + ".exe"),
            ($normalized + ".cmd"),
            ($normalized + ".bat")
        )
    }

    foreach ($candidatePath in $paths) {
        if (Test-Path -LiteralPath $candidatePath) {
            return (Resolve-Path -LiteralPath $candidatePath).Path
        }
    }

    return $null
}

function Resolve-H3E3CArchiver {
    param([string[]]$Lines)

    foreach ($line in $Lines) {
        $candidate = $null

        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"') {
            $candidate =
                $Matches["exe"]
        }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-gcc-ar(?:\.exe)?)\s+(?:cr|crs)') {
            $candidate =
                $Matches["exe"]
        }

        if (-not [string]::IsNullOrWhiteSpace($candidate)) {
            $resolved =
                Resolve-H3E3CNativeTool -Candidate $candidate

            if (-not [string]::IsNullOrWhiteSpace($resolved)) {
                return $resolved
            }
        }
    }

    foreach ($line in $Lines) {
        $compilerCandidate = $null

        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') {
            $compilerCandidate =
                $Matches["exe"]
        }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-g\+\+(?:\.exe)?)\s+') {
            $compilerCandidate =
                $Matches["exe"]
        }

        if ([string]::IsNullOrWhiteSpace($compilerCandidate)) {
            continue
        }

        $resolvedCompiler =
            Resolve-H3E3CNativeTool -Candidate $compilerCandidate

        if ([string]::IsNullOrWhiteSpace($resolvedCompiler)) {
            continue
        }

        $toolDir =
            Split-Path -Parent $resolvedCompiler

        foreach ($name in @(
            "xtensa-esp32-elf-gcc-ar.exe",
            "xtensa-esp32-elf-gcc-ar"
        )) {
            $candidatePath =
                Join-Path $toolDir $name

            $resolved =
                Resolve-H3E3CNativeTool -Candidate $candidatePath

            if (-not [string]::IsNullOrWhiteSpace($resolved)) {
                return $resolved
            }
        }
    }

    throw "H3E3C_ARCHIVER_NOT_FOUND"
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

[string[]]$sourceLines = @(
    $sourceOutput |
        ForEach-Object {
            $_.ToString()
        }
)

$archiver =
    Resolve-H3E3CArchiver -Lines $sourceLines

Write-Host "H3E3C_ARCHIVER=$archiver"
Write-Host "H3E3C_ARCHIVER_RESOLUTION=NORMALIZED_NATIVE_TOOL"

$candidateArchiveDir =
    Join-Path $candidateDisplay "src\esp32"

$candidateArchive =
    Join-Path $candidateArchiveDir "libJWPLC_Display.a"

New-Item -ItemType Directory -Force -Path $candidateArchiveDir | Out-Null

$archiveArgs = @("crs", $candidateArchive) +
    @($sourceObjectFiles | ForEach-Object { $_.FullName })

$oldPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    [object[]]$archiveOutput = @(& $archiver @archiveArgs 2>&1)
    $archiveExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $oldPreference
}

$archiveOutput |
    ForEach-Object { Write-Host $_ }

Write-Host "H3E3C_ARCHIVE_CREATE_EXIT=$archiveExit"

if ($archiveExit -ne 0 -or
    -not (Test-Path -LiteralPath $candidateArchive)) {
    throw "H3E3C_ARCHIVE_CREATE_FAILED"
}

[object[]]$memberOutput = @(& $archiver "t" $candidateArchive)
$memberExit = [int]$LASTEXITCODE

if ($memberExit -ne 0) {
    throw "H3E3C_ARCHIVE_LIST_FAILED"
}

[string[]]$memberNames = @(
    $memberOutput |
        ForEach-Object { $_.ToString().Trim() } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
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
        ForEach-Object { Write-Host $_ }

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

Write-Host "H3E3C_FLASH_DELTA_BYTES=$($candidateFlash - $sourceFlash)"
Write-Host "H3E3C_RAM_DELTA_BYTES=$($candidateRam - $sourceRam)"

if ($sourceFlash -lt 0 -or
    $sourceRam -lt 0 -or
    $candidateFlash -lt 0 -or
    $candidateRam -lt 0) {
    throw "H3E3C_USAGE_MARKERS_MISSING"
}

if ($candidateFlash -ne $sourceFlash) {
    throw "H3E3C_FLASH_PARITY_FAILED"
}

if ($candidateRam -ne $sourceRam) {
    throw "H3E3C_RAM_PARITY_FAILED"
}

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
