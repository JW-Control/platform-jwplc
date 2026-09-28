param(
    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.3E - ADOPCION PACKAGE JWPLC_Display"
Write-Host "============================================================"

Assert-G2Branch

$cli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$fqbn = "jwplc_local:esp32:jwplcbasic"

$h3e3c =
    Join-Path $PSScriptRoot "a14_h3e3c_display_precompiled_candidate.ps1"

$displayRoot =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Display"

$propertiesPath =
    Join-Path $displayRoot "library.properties"

$autoPath =
    Join-Path $displayRoot "src\JWPLC_Display_Auto.h"

$officialArchive =
    Join-Path $displayRoot "src\esp32\libJWPLC_Display.a"

$repoLibraries =
    Get-G2Path "JWPLC/2.1.0/libraries"

$emptySketch =
    Get-G2Path "tools/build-speed-benchmark/sketches/01_empty"

$hmiSketch =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_h3e3b_display_runtime_physical"

foreach ($required in @(
    $cli,
    $h3e3c,
    $displayRoot,
    $propertiesPath,
    $autoPath,
    $officialArchive,
    $repoLibraries,
    $emptySketch,
    $hmiSketch
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E3E_REQUIRED_PATH_MISSING=$required"
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

    throw "H3E3E_ENTRY_DIRTY_SCOPE_INVALID"
}

if ($entryStaged.Count -ne 0) {
    throw "H3E3E_ENTRY_INDEX_NOT_CLEAN"
}

$mode =
    if ($PreflightOnly) {
        "PREFLIGHT"
    }
    else {
        "ADOPT"
    }

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E3E_MODE=$mode"

Write-Host ""
Write-Host "=== H3E3E REQUIRE H3E3C ==="

[object[]]$cOutput =
    @(& $h3e3c *>&1)

$cOutput |
    ForEach-Object {
        Write-Host $_
    }

[string[]]$cLines = @(
    $cOutput |
        ForEach-Object {
            $_.ToString()
        }
)

$cText =
    $cLines -join [Environment]::NewLine

if (-not $cText.Contains(
    "A14_H3E3C_DISPLAY_PRECOMPILED_CANDIDATE_GATE=PASS")) {
    throw "H3E3E_H3E3C_NOT_PASS"
}

Write-Host "H3E3E_H3E3C_PREREQUISITE=PASS"

function Get-H3E3EMarker {
    param(
        [string[]]$Lines,
        [string]$Prefix
    )

    [string[]]$matches = @(
        $Lines |
            Where-Object {
                $_.StartsWith(
                    $Prefix,
                    [StringComparison]::Ordinal)
            }
    )

    if ($matches.Count -ne 1) {
        throw ("H3E3E_MARKER_COUNT_INVALID={0}:{1}" -f $Prefix, $matches.Count)
    }

    return $matches[0].Substring($Prefix.Length)
}

$candidateArchive =
    Get-H3E3EMarker -Lines $cLines -Prefix "H3E3C_CANDIDATE_ARCHIVE="

$candidateArchiveSha =
    Get-H3E3EMarker -Lines $cLines -Prefix "H3E3C_CANDIDATE_ARCHIVE_SHA256="

if (-not (Test-Path -LiteralPath $candidateArchive)) {
    throw "H3E3E_CANDIDATE_ARCHIVE_MISSING"
}

$actualCandidateSha =
    (Get-G2Sha256Path $candidateArchive).ToUpperInvariant()

if ($actualCandidateSha -ne $candidateArchiveSha.ToUpperInvariant()) {
    throw "H3E3E_CANDIDATE_ARCHIVE_HASH_CHANGED"
}

$candidateDisplay =
    Split-Path -Parent (
        Split-Path -Parent (
            Split-Path -Parent $candidateArchive
        )
    )

if (-not (Test-Path -LiteralPath $candidateDisplay)) {
    throw "H3E3E_CANDIDATE_DISPLAY_ROOT_MISSING"
}

$finalProperties = @'
name=JWPLC_Display
version=1.0.1
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=Display ST7789 e HMI declarativa para JWPLC Basic.
paragraph=Implementa IDLE/USER, HMI declarativa, navegacion, diagnosticos y acceso grafico JWPLC_TFT sin exponer backends externos.
category=Display
url=https://github.com/JW-Control/platform-jwplc
architectures=esp32
includes=JWPLC_Display.h
dot_a_linkage=true
precompiled=full
depends=JWPLC_TFT,JW_MatrixButtons,JWPLC_GlobalPeripherals
'@

$finalAuto = @'
#ifndef JWPLC_DISPLAY_AUTO_H
#define JWPLC_DISPLAY_AUTO_H

// =====================================================
// JWPLC Display autoload
// =====================================================
//
// JWPLC_Display se apoya exclusivamente en la API JWPLC_TFT.
// El usuario no necesita instalar, configurar ni descubrir Adafruit GFX,
// Adafruit ST7789 o TFT_eSPI para la pantalla integrada.
//
// Durante library discovery este header conserva la API liviana y deja que
// JWPLC_Display_API.h descubra JWPLC_TFT como dependencia publica del package.
// JWPLC_GlobalPeripherals_Auto.h mantiene el resto del autoload normal.

#ifndef JWPLC_LIBRARY_DISCOVERY_PHASE
#define JWPLC_LIBRARY_DISCOVERY_PHASE 0
#endif

#include <JWPLC_Display_API.h>
#include <JWPLC_GlobalPeripherals_Auto.h>

#if !JWPLC_LIBRARY_DISCOVERY_PHASE
// Con dot_a_linkage=true / precompiled=full, esta referencia fuerza la
// extraccion del miembro base JWPLC_Display.cpp.o sin usar whole-archive.
// Los TUs HMI opcionales siguen entrando solo cuando la API los requiere.
namespace JWPLCDisplayAutoload
{
    static JWPLC_DisplayClass *const __attribute__((used)) anchor = &JWPLC_Display;
}
#endif

#endif // JWPLC_DISPLAY_AUTO_H
'@

$utf8NoBom =
    New-Object Text.UTF8Encoding($false)

$candidatePropertiesPath =
    Join-Path $candidateDisplay "library.properties"

$candidateAutoPath =
    Join-Path $candidateDisplay "src\JWPLC_Display_Auto.h"

[IO.File]::WriteAllText(
    $candidatePropertiesPath,
    $finalProperties,
    $utf8NoBom)

[IO.File]::WriteAllText(
    $candidateAutoPath,
    $finalAuto,
    $utf8NoBom)

Write-Host "H3E3E_TEMP_METADATA=JWPLC_TFT_ONLY"
Write-Host "H3E3E_TEMP_ADAFRUIT_DISCOVERY_MARKERS=REMOVED"

function Invoke-H3E3ECompile {
    param(
        [string]$LibraryRoot,
        [string]$Sketch,
        [string]$BuildPath,
        [string]$LogPath,
        [string]$Label
    )

    New-Item -ItemType Directory -Force -Path $BuildPath | Out-Null

    $args = @(
        "compile",
        "--fqbn", $fqbn,
        "-j", "0",
        "-v",
        "--clean",
        "--build-path", $BuildPath,
        "--library", $LibraryRoot,
        "--libraries", $repoLibraries,
        $Sketch
    )

    Write-Host ""
    Write-Host ("=== {0} ===" -f $Label)

    $previousPreference =
        $ErrorActionPreference

    try {
        $ErrorActionPreference =
            "Continue"

        [object[]]$output =
            @(& $cli @args 2>&1)

        $exitCode =
            [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference =
            $previousPreference
    }

    $output |
        ForEach-Object {
            $_.ToString()
        } |
        Set-Content -LiteralPath $LogPath -Encoding UTF8

    Write-Host ("{0}_COMPILE_EXIT={1}" -f $Label, $exitCode)
    Write-Host ("{0}_COMPILE_LOG={1}" -f $Label, $LogPath)

    if ($exitCode -ne 0) {
        $output |
            Select-Object -Last 180 |
            ForEach-Object {
                Write-Host $_
            }

        throw ("{0}_COMPILE_FAILED" -f $Label)
    }

    return @(
        $output |
            ForEach-Object {
                $_.ToString()
            }
    )
}

function Assert-H3E3EBuild {
    param(
        [string[]]$Lines,
        [string]$BuildPath,
        [string]$ExpectedDisplayRoot,
        [string]$Label
    )

    $text =
        $Lines -join [Environment]::NewLine

    $normalizedExpected =
        [IO.Path]::GetFullPath(
            $ExpectedDisplayRoot
        ).TrimEnd('\', '/')

    [string[]]$displaySelection = @(
        $Lines |
            Where-Object {
                $_ -match '^Using library JWPLC_Display at version .+ in folder: (.+)$'
            }
    )

    $displaySelected =
        $false

    foreach ($line in $displaySelection) {
        if ($line -match '^Using library JWPLC_Display at version .+ in folder: (.+)$') {
            $selected =
                [IO.Path]::GetFullPath(
                    $Matches[1].Trim()
                ).TrimEnd('\', '/')

            if ($selected -ieq $normalizedExpected) {
                $displaySelected =
                    $true
            }
        }
    }

    [string[]]$graphicsSelections = @(
        $Lines |
            Where-Object {
                $_ -match '^Using library (Adafruit ST7735 and ST7789 Library|Adafruit GFX Library|TFT_eSPI) at version '
            }
    )

    [string[]]$busIoSelections = @(
        $Lines |
            Where-Object {
                $_ -match '^Using library Adafruit BusIO at version '
            }
    )

    $displayPrecompiled =
        @(
            $Lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_Display')
                }
        ).Count -gt 0

    $tftSelected =
        $text.Contains("Using library JWPLC_TFT")

    $tftPrecompiled =
        @(
            $Lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_TFT')
                }
        ).Count -gt 0

    $globalSelected =
        $text.Contains("Using library JWPLC_GlobalPeripherals")

    [int]$displaySourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -match '[\\/]libraries[\\/]JWPLC_Display[\\/].+\.cpp\.o$'
            }
    ).Count

    [int]$tftSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_TFT.cpp.o"
            }
    ).Count

    [int]$backendSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "TFT_eSPI.cpp.o"
            }
    ).Count

    [object[]]$maps = @(
        Get-ChildItem -LiteralPath $BuildPath -File -Filter "*.map" -ErrorAction SilentlyContinue
    )

    if ($maps.Count -ne 1) {
        throw ("{0}_MAP_COUNT_INVALID={1}" -f $Label, $maps.Count)
    }

    $mapText =
        [IO.File]::ReadAllText(
            $maps[0].FullName
        )

    $displayBaseLinked =
        $mapText -match '(?:lib)?JWPLC_Display\.a\(JWPLC_Display\.cpp\.o\)'

    $displaySelectedText =
        if ($displaySelected) { "PASS" } else { "FAIL" }

    $displayPrecompiledText =
        if ($displayPrecompiled) { "PASS" } else { "FAIL" }

    $tftSelectedText =
        if ($tftSelected) { "PASS" } else { "FAIL" }

    $tftPrecompiledText =
        if ($tftPrecompiled) { "PASS" } else { "FAIL" }

    $globalSelectedText =
        if ($globalSelected) { "PASS" } else { "FAIL" }

    $displayBaseLinkedText =
        if ($displayBaseLinked) { "PASS" } else { "FAIL" }

    Write-Host ("{0}_DISPLAY_SELECTED={1}" -f $Label, $displaySelectedText)
    Write-Host ("{0}_DISPLAY_PRECOMPILED={1}" -f $Label, $displayPrecompiledText)
    Write-Host ("{0}_JWPLC_TFT_SELECTED={1}" -f $Label, $tftSelectedText)
    Write-Host ("{0}_JWPLC_TFT_PRECOMPILED={1}" -f $Label, $tftPrecompiledText)
    Write-Host ("{0}_GLOBAL_PERIPHERALS_SELECTED={1}" -f $Label, $globalSelectedText)
    Write-Host ("{0}_EXTERNAL_GRAPHICS_SELECTION_COUNT={1}" -f $Label, $graphicsSelections.Count)
    Write-Host ("{0}_ADAFRUIT_BUSIO_SELECTION_COUNT={1}" -f $Label, $busIoSelections.Count)
    Write-Host ("{0}_ADAFRUIT_BUSIO_CLASSIFICATION=NON_GRAPHICS_DEPENDENCY" -f $Label)
    Write-Host ("{0}_DISPLAY_SOURCE_OBJECT_COUNT={1}" -f $Label, $displaySourceObjects)
    Write-Host ("{0}_JWPLC_TFT_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftSourceObjects)
    Write-Host ("{0}_TFT_ESPI_SOURCE_OBJECT_COUNT={1}" -f $Label, $backendSourceObjects)
    Write-Host ("{0}_DISPLAY_BASE_LINKED={1}" -f $Label, $displayBaseLinkedText)

    if (-not $displaySelected -or
        -not $displayPrecompiled -or
        -not $tftSelected -or
        -not $tftPrecompiled -or
        -not $globalSelected -or
        $graphicsSelections.Count -ne 0 -or
        $displaySourceObjects -ne 0 -or
        $tftSourceObjects -ne 0 -or
        $backendSourceObjects -ne 0 -or
        -not $displayBaseLinked) {
        throw ("{0}_BUILD_POLICY_FAILED" -f $Label)
    }
}

$tempRoot =
    Join-Path $env:TEMP (
        "jwplc_a14_h3e3e_{0}" -f
        (Get-Date -Format "yyyyMMdd_HHmmss")
    )

$tempEmptyBuild =
    Join-Path $tempRoot "temp-empty"

$tempEmptyLog =
    Join-Path $tempRoot "temp-empty.log"

$tempHmiBuild =
    Join-Path $tempRoot "temp-hmi"

$tempHmiLog =
    Join-Path $tempRoot "temp-hmi.log"

$tempEmptyCompileArgs = @{
    LibraryRoot = $candidateDisplay
    Sketch = $emptySketch
    BuildPath = $tempEmptyBuild
    LogPath = $tempEmptyLog
    Label = "H3E3E_TEMP_AUTOLOAD"
}

[string[]]$tempEmptyOutput =
    @(Invoke-H3E3ECompile @tempEmptyCompileArgs)

$tempEmptyAssertArgs = @{
    Lines = $tempEmptyOutput
    BuildPath = $tempEmptyBuild
    ExpectedDisplayRoot = $candidateDisplay
    Label = "H3E3E_TEMP_AUTOLOAD"
}

Assert-H3E3EBuild @tempEmptyAssertArgs

$tempHmiCompileArgs = @{
    LibraryRoot = $candidateDisplay
    Sketch = $hmiSketch
    BuildPath = $tempHmiBuild
    LogPath = $tempHmiLog
    Label = "H3E3E_TEMP_HMI"
}

[string[]]$tempHmiOutput =
    @(Invoke-H3E3ECompile @tempHmiCompileArgs)

$tempHmiAssertArgs = @{
    Lines = $tempHmiOutput
    BuildPath = $tempHmiBuild
    ExpectedDisplayRoot = $candidateDisplay
    Label = "H3E3E_TEMP_HMI"
}

Assert-H3E3EBuild @tempHmiAssertArgs

Write-Host "H3E3E_TEMP_RELEASE_LIKE=PASS"
Write-Host "H3E3E_NORMAL_AUTOLOAD_WITHOUT_ADAFRUIT=PASS"

if ($PreflightOnly) {
    Write-Host "H3E3E_PREFLIGHT_REPOSITORY_MUTATION=NO"
    Write-Host "A14_H3E3E_DISPLAY_PACKAGE_ADOPTION_PREFLIGHT=PASS"
    return
}

$originalProperties =
    [IO.File]::ReadAllBytes(
        $propertiesPath
    )

$originalAuto =
    [IO.File]::ReadAllBytes(
        $autoPath
    )

$originalArchive =
    [IO.File]::ReadAllBytes(
        $officialArchive
    )

$adoptionSucceeded =
    $false

try {
    Write-Host ""
    Write-Host "=== H3E3E ADOPT OFFICIAL PACKAGE ==="

    Copy-Item -LiteralPath $candidateArchive -Destination $officialArchive -Force

    [IO.File]::WriteAllText(
        $propertiesPath,
        $finalProperties,
        $utf8NoBom)

    [IO.File]::WriteAllText(
        $autoPath,
        $finalAuto,
        $utf8NoBom)

    $officialArchiveSha =
        (Get-G2Sha256Path $officialArchive).ToUpperInvariant()

    if ($officialArchiveSha -ne $actualCandidateSha) {
        throw "H3E3E_OFFICIAL_ARCHIVE_HASH_MISMATCH"
    }

    Write-Host "H3E3E_OFFICIAL_ARCHIVE_SHA256=$officialArchiveSha"
    Write-Host "H3E3E_ARCHIVE_IDENTITY_WITH_H3E3C=PASS"

    $officialEmptyBuild =
        Join-Path $tempRoot "official-empty"

    $officialEmptyLog =
        Join-Path $tempRoot "official-empty.log"

    $officialHmiBuild =
        Join-Path $tempRoot "official-hmi"

    $officialHmiLog =
        Join-Path $tempRoot "official-hmi.log"

    $officialEmptyCompileArgs = @{
        LibraryRoot = $displayRoot
        Sketch = $emptySketch
        BuildPath = $officialEmptyBuild
        LogPath = $officialEmptyLog
        Label = "H3E3E_OFFICIAL_AUTOLOAD"
    }

    [string[]]$officialEmptyOutput =
        @(Invoke-H3E3ECompile @officialEmptyCompileArgs)

    $officialEmptyAssertArgs = @{
        Lines = $officialEmptyOutput
        BuildPath = $officialEmptyBuild
        ExpectedDisplayRoot = $displayRoot
        Label = "H3E3E_OFFICIAL_AUTOLOAD"
    }

    Assert-H3E3EBuild @officialEmptyAssertArgs

    $officialHmiCompileArgs = @{
        LibraryRoot = $displayRoot
        Sketch = $hmiSketch
        BuildPath = $officialHmiBuild
        LogPath = $officialHmiLog
        Label = "H3E3E_OFFICIAL_HMI"
    }

    [string[]]$officialHmiOutput =
        @(Invoke-H3E3ECompile @officialHmiCompileArgs)

    $officialHmiAssertArgs = @{
        Lines = $officialHmiOutput
        BuildPath = $officialHmiBuild
        ExpectedDisplayRoot = $displayRoot
        Label = "H3E3E_OFFICIAL_HMI"
    }

    Assert-H3E3EBuild @officialHmiAssertArgs

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

    [string[]]$expectedDirty = @(
        $expectedCoreDirty,
        "JWPLC/2.1.0/libraries/JWPLC_Display/library.properties",
        "JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_Display_Auto.h",
        "JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a"
    ) |
        Sort-Object

    [object[]]$dirtyDiff = @(
        Compare-Object -ReferenceObject $expectedDirty -DifferenceObject $normalizedFinalDirty
    )

    Write-Host "H3E3E_FINAL_DIRTY_COUNT=$($normalizedFinalDirty.Count)"

    $normalizedFinalDirty |
        ForEach-Object {
            Write-Host "H3E3E_FINAL_DIRTY=$_"
        }

    if ($dirtyDiff.Count -ne 0) {
        throw "H3E3E_FINAL_DIRTY_SCOPE_INVALID"
    }

    if ($finalStaged.Count -ne 0) {
        throw "H3E3E_FINAL_INDEX_NOT_CLEAN"
    }

    $adoptionSucceeded =
        $true

    Write-Host "H3E3E_DISPLAY_DEPENDS=JWPLC_TFT,JW_MatrixButtons,JWPLC_GlobalPeripherals"
    Write-Host "H3E3E_ADAFRUIT_GFX_PUBLIC_DEPENDENCY=REMOVED"
    Write-Host "H3E3E_ADAFRUIT_ST7789_PUBLIC_DEPENDENCY=REMOVED"
    Write-Host "H3E3E_ADAFRUIT_DISCOVERY_MARKERS=REMOVED"
    Write-Host "H3E3E_AUTOLOAD_PERIPHERALS_REMOVED=NO"
    Write-Host "H3E3E_DISPLAY_ARCHIVE=OFFICIAL_ADOPTED"
    Write-Host "H3E3E_DISPLAY_ARCHIVE_SOURCE=H3E3C_QUALIFIED"
    Write-Host "H3E3E_PACKAGE_BUILD_AUTOLOAD=PASS"
    Write-Host "H3E3E_PACKAGE_BUILD_HMI=PASS"
    Write-Host "H3E3E_REPOSITORY_MUTATION=PRODUCT_FILES_ONLY"
    Write-Host "A14_H3E3E_DISPLAY_PACKAGE_ADOPTION_GATE=PASS"
    Write-Host "NEXT=REVIEW_DIFF_AND_COMMIT_H3E3E_PRODUCT_FILES"
}
finally {
    if (-not $adoptionSucceeded) {
        Write-Host "H3E3E_ROLLBACK=START"

        [IO.File]::WriteAllBytes(
            $propertiesPath,
            $originalProperties)

        [IO.File]::WriteAllBytes(
            $autoPath,
            $originalAuto)

        [IO.File]::WriteAllBytes(
            $officialArchive,
            $originalArchive)

        Write-Host "H3E3E_ROLLBACK=PASS"
    }
}
