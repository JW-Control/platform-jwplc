param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.2B3 - ADOPT JWPLC_TFT PRECOMPILED IN PACKAGE"
Write-Host "============================================================"

Assert-G2Branch

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$fqbn = "jwplc_local:esp32:jwplcbasic"
$diagDefine = "-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1"

$expectedArchiveSha =
    "6CFC76EE5089311483B971442806BEDC9179C33A924EC53FCBC5FD1A84C1013E"
$expectedArchiveBytes = 1081796

$b1Path =
    Join-Path $PSScriptRoot "a14_h3e2b1_jwplc_tft_precompiled_candidate.ps1"

$libraryRelative = "JWPLC/2.1.0/libraries/JWPLC_TFT"
$libraryRoot = Get-G2Path $libraryRelative
$propertiesRelative = "$libraryRelative/library.properties"
$readmeRelative = "$libraryRelative/README.md"
$setupRelative = "$libraryRelative/src/tft_setup.h"
$archiveRelative = "$libraryRelative/src/esp32/libJWPLC_TFT.a"

$propertiesPath = Get-G2Path $propertiesRelative
$readmePath = Get-G2Path $readmeRelative
$setupPath = Get-G2Path $setupRelative
$archivePath = Get-G2Path $archiveRelative
$archiveDir = Split-Path -Parent $archivePath

$licensePath =
    Get-G2Path "$libraryRelative/licenses/TFT_eSPI-2.5.43-license.txt"

$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"

$sketchRelative =
    "tools/modbus-tcp-benchmark/firmware/a14_h3e2a_jwplc_tft_compile_probe"
$sketchPath = Get-G2Path $sketchRelative

foreach ($required in @(
    $arduinoCli,
    $b1Path,
    $libraryRoot,
    $propertiesPath,
    $readmePath,
    $setupPath,
    $licensePath,
    $sketchPath
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E2B3_REQUIRED_PATH_MISSING=$required"
    }
}

[string[]]$entryDirty = @(Get-G2TrackedDirtyPaths)
[string[]]$entryStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
[string[]]$normalizedEntryDirty = @(
    $entryDirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)
$expectedCoreDirty = $script:G2CoreRelative.Replace("\", "/")

if ($normalizedEntryDirty.Count -ne 1 -or
    $normalizedEntryDirty[0] -ne $expectedCoreDirty) {
    $normalizedEntryDirty |
        ForEach-Object {
            Write-Host "ENTRY_DIRTY=$_"
        }

    throw "H3E2B3_ENTRY_DIRTY_SCOPE_INVALID"
}

if ($entryStaged.Count -ne 0) {
    throw "H3E2B3_ENTRY_INDEX_NOT_CLEAN"
}

[object[]]$entryUntrackedInLibrary = @(
    & git -C $script:G2RepoRoot ls-files --others --exclude-standard -- $libraryRelative
)

if ($LASTEXITCODE -ne 0) {
    throw "H3E2B3_ENTRY_UNTRACKED_QUERY_FAILED"
}

if ($entryUntrackedInLibrary.Count -ne 0) {
    $entryUntrackedInLibrary |
        ForEach-Object {
            Write-Host "ENTRY_UNTRACKED_JWPLC_TFT=$_"
        }

    throw "H3E2B3_ENTRY_UNTRACKED_LIBRARY_NOT_EMPTY"
}

$originalPropertiesBytes =
    [IO.File]::ReadAllBytes($propertiesPath)
$originalReadmeBytes =
    [IO.File]::ReadAllBytes($readmePath)
$originalSetupBytes =
    [IO.File]::ReadAllBytes($setupPath)

$hadArchive = Test-Path -LiteralPath $archivePath
$archiveBackup = $null

if ($hadArchive) {
    $archiveBackup =
        Join-Path $env:TEMP (
            "jwplc_h3e2b3_archive_backup_{0}.a" -f
            (Get-Date -Format "yyyyMMdd_HHmmss")
        )

    Copy-Item -LiteralPath $archivePath -Destination $archiveBackup -Force
}

$success = $false

try {
    Write-Host "HEAD=$(Get-G2Head)"
    Write-Host "H3E2B3_EXPECTED_ARCHIVE_SHA256=$expectedArchiveSha"
    Write-Host "H3E2B3_EXPECTED_ARCHIVE_BYTES=$expectedArchiveBytes"
    Write-Host "H3E2B3_ENTRY_REPOSITORY_MUTATION=NO"

    Write-Host ""
    Write-Host "=== H3E2B3 REGENERATE / QUALIFY B1 ==="

    [object[]]$b1Output = @(& $b1Path *>&1)
    $b1Output |
        ForEach-Object {
            Write-Host $_
        }

    $b1Text =
        ($b1Output | ForEach-Object { $_.ToString() }) -join
        [Environment]::NewLine

    if (-not $b1Text.Contains(
        "A14_H3E2B1_JWPLC_TFT_PRECOMPILED_CANDIDATE_GATE=PASS")) {
        throw "H3E2B3_B1_GATE_FAILED"
    }

    $archiveMatch = [regex]::Match(
        $b1Text,
        '(?m)^H3E2B1_CANDIDATE_ARCHIVE=(.+)\r?$'
    )

    $shaMatch = [regex]::Match(
        $b1Text,
        '(?m)^H3E2B1_CANDIDATE_ARCHIVE_SHA256=([0-9A-Fa-f]{64})\r?$'
    )

    $sourceFlashMatch = [regex]::Match(
        $b1Text,
        '(?m)^H3E2B1_SOURCE_FLASH_BYTES=(\d+)\r?$'
    )

    $sourceRamMatch = [regex]::Match(
        $b1Text,
        '(?m)^H3E2B1_SOURCE_RAM_BYTES=(\d+)\r?$'
    )

    if (-not $archiveMatch.Success -or
        -not $shaMatch.Success -or
        -not $sourceFlashMatch.Success -or
        -not $sourceRamMatch.Success) {
        throw "H3E2B3_B1_HANDOFF_MARKERS_MISSING"
    }

    $candidateArchive =
        $archiveMatch.Groups[1].Value.Trim()

    $candidateSha =
        $shaMatch.Groups[1].Value.Trim().ToUpperInvariant()

    $sourceFlash =
        [int64]$sourceFlashMatch.Groups[1].Value

    $sourceRam =
        [int64]$sourceRamMatch.Groups[1].Value

    if (-not (Test-Path -LiteralPath $candidateArchive)) {
        throw "H3E2B3_B1_ARCHIVE_MISSING"
    }

    $candidateBytes =
        (Get-Item -LiteralPath $candidateArchive).Length

    $candidateShaActual =
        (Get-G2Sha256Path $candidateArchive).ToUpperInvariant()

    Write-Host "H3E2B3_B1_ARCHIVE_SHA256=$candidateShaActual"
    Write-Host "H3E2B3_B1_ARCHIVE_BYTES=$candidateBytes"
    Write-Host "H3E2B3_B1_REFERENCE_FLASH_BYTES=$sourceFlash"
    Write-Host "H3E2B3_B1_REFERENCE_RAM_BYTES=$sourceRam"

    if ($candidateSha -ne $expectedArchiveSha -or
        $candidateShaActual -ne $expectedArchiveSha) {
        throw "H3E2B3_ARCHIVE_HASH_NOT_QUALIFIED"
    }

    if ($candidateBytes -ne $expectedArchiveBytes) {
        throw "H3E2B3_ARCHIVE_SIZE_NOT_QUALIFIED"
    }

    New-Item -ItemType Directory -Force -Path $archiveDir | Out-Null
    Copy-Item -LiteralPath $candidateArchive -Destination $archivePath -Force

    $utf8NoBom =
        New-Object System.Text.UTF8Encoding($false)

    $propertiesText = @"
name=JWPLC_TFT
version=0.1.0
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=Backend grafico ST7789 encapsulado para el ecosistema JWPLC.
paragraph=Expone una API grafica propia JWPLC con backend ST7789 precompilado y controlado por el package. El usuario no requiere instalar ni configurar TFT_eSPI.
category=Display
url=https://github.com/JW-Control/platform-jwplc
architectures=esp32
includes=JWPLC_TFT.h
dot_a_linkage=true
precompiled=full
depends=SPI
"@

    $readmeText = @"
# JWPLC_TFT

Backend grafico propio del ecosistema JWPLC.

## Arquitectura

`JWPLC_TFT` expone una API publica independiente del motor grafico. El
backend calificado para JWPLC Basic v2 se distribuye precompilado dentro del
package, por lo que el usuario no necesita instalar ni configurar TFT_eSPI.

La API publica no expone tipos de TFT_eSPI ni Adafruit.

## Hardware activo — JWPLC Basic v2

- controlador: ST7789;
- panel fisico: 170x320;
- geometria logica landscape: 320x170;
- rotation: 1;
- orden de color: BGR;
- inversion: ON;
- SPI: MODE0;
- frecuencia TFT: 80 MHz;
- bus compartido protegido mediante el mutex SPI del JWPLC.

## API

El objeto global es:

```cpp
JWPLC_TFT
```

La API incluye primitivas de dibujo, texto, geometria y batching. El batching
`beginBatch()/endBatch()` permite que `JWPLC_Display` agrupe un dirty pass
completo bajo una sola transaccion del backend.

## Backend

Para ESP32 el package usa:

```text
src/esp32/libJWPLC_TFT.a
```

El archive contiene el wrapper `JWPLC_TFT` y el backend TFT_eSPI 2.5.43
calificado. `library.properties` no declara TFT_eSPI como dependencia de
usuario.

Los sources `JWPLC_TFT.cpp` y `tft_setup.h` permanecen versionados para
mantenimiento y regeneracion del archive. Los builds normales del package
deben seleccionar `precompiled=full` y no compilar esos sources.

## JWPLC Basic v3

El target planificado usa tambien ST7789 con panel 240x320. Ese perfil no se
activa en 2.1.x hasta fijar y calificar board target, pinout, offsets,
orientacion y configuracion fisica final.

## Relacion con JWPLC_Display

`JWPLC_TFT` es la capa de hardware/renderer. La HMI declarativa, paginas,
modo IDLE, dirty cache y contrato del JWPLC HMI Designer pertenecen a
`JWPLC_Display`.

La migracion de `JWPLC_Display` a este backend se realiza en H3E.3.

## Licencias de terceros

El backend precompilado incorpora TFT_eSPI 2.5.43. Los avisos originales se
conservan en:

```text
licenses/TFT_eSPI-2.5.43-license.txt
```
"@

    $setupText = @"
#pragma once

// Configuracion de mantenimiento para regenerar el backend JWPLC_TFT.
// Los builds normales del package usan src/esp32/libJWPLC_TFT.a y no
// requieren TFT_eSPI instalado ni User_Setup.h del usuario.

#define USER_SETUP_INFO "JWPLC_TFT Basic v2"
#define JWPLC_TFT_BACKEND_SETUP 1

#define ST7789_DRIVER

#define TFT_WIDTH  170
#define TFT_HEIGHT 320

#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_ON

#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18

#define TFT_CS   33
#define TFT_DC   25
#define TFT_RST  14

#define TFT_SPI_MODE SPI_MODE0
#define SPI_FREQUENCY 80000000

#define LOAD_GLCD
#define SUPPORT_TRANSACTIONS
"@

    [IO.File]::WriteAllText(
        $propertiesPath,
        $propertiesText,
        $utf8NoBom)

    [IO.File]::WriteAllText(
        $readmePath,
        $readmeText,
        $utf8NoBom)

    [IO.File]::WriteAllText(
        $setupPath,
        $setupText,
        $utf8NoBom)

    $adoptedSha =
        (Get-G2Sha256Path $archivePath).ToUpperInvariant()

    Write-Host "H3E2B3_ADOPTED_ARCHIVE_SHA256=$adoptedSha"

    if ($adoptedSha -ne $expectedArchiveSha) {
        throw "H3E2B3_ADOPTED_ARCHIVE_IDENTITY_FAILED"
    }

    $propertiesAfter =
        [IO.File]::ReadAllText($propertiesPath)

    foreach ($requiredLine in @(
        "dot_a_linkage=true",
        "precompiled=full",
        "depends=SPI"
    )) {
        if (-not $propertiesAfter.Contains($requiredLine)) {
            throw "H3E2B3_PROPERTIES_CONTRACT_MISSING=$requiredLine"
        }
    }

    if ($propertiesAfter.Contains("depends=TFT_eSPI")) {
        throw "H3E2B3_PROPERTIES_EXTERNAL_BACKEND_DEPENDENCY_PRESENT"
    }

    Write-Host "H3E2B3_PACKAGE_PROPERTIES_CONTRACT=PASS"

    $runRoot =
        Join-Path $env:TEMP (
            "jwplc_a14_h3e2b3_{0}" -f
            (Get-Date -Format "yyyyMMdd_HHmmss")
        )

    $buildPath = Join-Path $runRoot "build"
    $compileLog = Join-Path $runRoot "compile.log"

    New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

    $compileArgs = @(
        "compile",
        "--fqbn", $fqbn,
        "-j", "0",
        "-v",
        "--clean",
        "--build-path", $buildPath,
        "--libraries", $repoLibrariesRoot,
        "--build-property", "compiler.cpp.extra_flags=$diagDefine",
        $sketchPath
    )

    Write-Host ""
    Write-Host "=== H3E2B3 COMPILE ADOPTED PACKAGE ==="
    Write-Host "H3E2B3_COMPILE_START=YES"

    $previousPreference = $ErrorActionPreference

    try {
        $ErrorActionPreference = "Continue"
        [object[]]$compileOutput =
            @(& $arduinoCli @compileArgs 2>&1)

        $compileExit =
            [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }

    $compileOutput |
        ForEach-Object {
            $_.ToString()
        } |
        Set-Content -LiteralPath $compileLog -Encoding UTF8

    Write-Host "H3E2B3_COMPILE_EXIT=$compileExit"
    Write-Host "H3E2B3_COMPILE_LOG=$compileLog"

    if ($compileExit -ne 0) {
        $compileOutput |
            Select-Object -Last 180 |
            ForEach-Object {
                Write-Host $_
            }

        throw "H3E2B3_COMPILE_FAILED"
    }

    $normalizedLibraryRoot =
        [IO.Path]::GetFullPath(
            $libraryRoot
        ).TrimEnd('\', '/')

    [string[]]$selectionLines = @(
        $compileOutput |
            ForEach-Object {
                $_.ToString()
            } |
            Where-Object {
                $_ -match '^Using library JWPLC_TFT at version .+ in folder: (.+)$'
            }
    )

    $packageSelected = $false

    foreach ($line in $selectionLines) {
        if ($line -match '^Using library JWPLC_TFT at version .+ in folder: (.+)$') {
            $selectedPath =
                [IO.Path]::GetFullPath(
                    $Matches[1].Trim()
                ).TrimEnd('\', '/')

            if ($selectedPath -ieq $normalizedLibraryRoot) {
                $packageSelected = $true
            }
        }
    }

    Write-Host "H3E2B3_PACKAGE_LIBRARY_SELECTED=$(
        if ($packageSelected) { 'PASS' } else { 'FAIL' }
    )"

    if (-not $packageSelected) {
        $selectionLines |
            ForEach-Object {
                Write-Host "H3E2B3_SELECTION_LINE=$_"
            }

        throw "H3E2B3_PACKAGE_LIBRARY_NOT_SELECTED"
    }

    [string[]]$backendSelection = @(
        $compileOutput |
            ForEach-Object {
                $_.ToString()
            } |
            Where-Object {
                $_ -match '^Using library TFT_eSPI at version '
            }
    )

    Write-Host "H3E2B3_EXTERNAL_TFT_ESPI_SELECTION_COUNT=$($backendSelection.Count)"

    if ($backendSelection.Count -ne 0) {
        $backendSelection |
            ForEach-Object {
                Write-Host "H3E2B3_TFT_ESPI_SELECTION=$_"
            }

        throw "H3E2B3_EXTERNAL_TFT_ESPI_SELECTED_AFTER_ADOPTION"
    }

    $compileDbPath =
        Join-Path $buildPath "compile_commands.json"

    if (-not (Test-Path -LiteralPath $compileDbPath)) {
        throw "H3E2B3_COMPILE_DB_MISSING"
    }

    [object[]]$compileDbEntries = @(
        Get-Content -LiteralPath $compileDbPath -Raw |
            ConvertFrom-Json
    )

    [string[]]$tuFiles = @(
        foreach ($entry in $compileDbEntries) {
            $fileText =
                [string]$entry.file

            if ([string]::IsNullOrWhiteSpace($fileText)) {
                continue
            }

            $fileText.
                Trim().
                Trim('"').
                Replace([char]92, [char]47)
        }
    )

    [int]$jwplcSourceCompiles = @(
        $tuFiles |
            Where-Object {
                $_.EndsWith(
                    "/JWPLC_TFT.cpp",
                    [StringComparison]::OrdinalIgnoreCase)
            }
    ).Count

    [int]$backendSourceCompiles = @(
        $tuFiles |
            Where-Object {
                $_.EndsWith(
                    "/TFT_eSPI.cpp",
                    [StringComparison]::OrdinalIgnoreCase)
            }
    ).Count

    Write-Host "H3E2B3_COMPILE_DB_ENTRY_COUNT=$($compileDbEntries.Count)"
    Write-Host "H3E2B3_JWPLC_TFT_SOURCE_COMPILES=$jwplcSourceCompiles"
    Write-Host "H3E2B3_TFT_ESPI_SOURCE_COMPILES=$backendSourceCompiles"

    if ($jwplcSourceCompiles -ne 0 -or
        $backendSourceCompiles -ne 0) {
        throw "H3E2B3_SOURCE_COMPILED_AFTER_PRECOMPILED_ADOPTION"
    }

    $precompiledObserved = @(
        $compileOutput |
            ForEach-Object {
                $_.ToString()
            } |
            Where-Object {
                ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                ($_ -match 'JWPLC_TFT')
            }
    ).Count -gt 0

    Write-Host "H3E2B3_PRECOMPILED_SELECTION_PROOF=$(
        if ($precompiledObserved) { 'PASS' } else { 'FAIL' }
    )"

    if (-not $precompiledObserved) {
        throw "H3E2B3_PRECOMPILED_SELECTION_NOT_PROVEN"
    }

    function Get-H3E2B3Usage {
        param(
            [object[]]$Lines,
            [string]$Kind
        )

        foreach ($lineObject in $Lines) {
            $line =
                [string]$lineObject

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

    $packageFlash =
        Get-H3E2B3Usage -Lines $compileOutput -Kind "FLASH"

    $packageRam =
        Get-H3E2B3Usage -Lines $compileOutput -Kind "RAM"

    Write-Host "H3E2B3_REFERENCE_FLASH_BYTES=$sourceFlash"
    Write-Host "H3E2B3_PACKAGE_FLASH_BYTES=$packageFlash"
    Write-Host "H3E2B3_REFERENCE_RAM_BYTES=$sourceRam"
    Write-Host "H3E2B3_PACKAGE_RAM_BYTES=$packageRam"

    if ($packageFlash -ne $sourceFlash) {
        throw "H3E2B3_FLASH_PARITY_FAILED"
    }

    if ($packageRam -ne $sourceRam) {
        throw "H3E2B3_RAM_PARITY_FAILED"
    }

    $archiveShaAfterCompile =
        (Get-G2Sha256Path $archivePath).ToUpperInvariant()

    Write-Host "H3E2B3_ARCHIVE_SHA256_AFTER_COMPILE=$archiveShaAfterCompile"

    if ($archiveShaAfterCompile -ne $expectedArchiveSha) {
        throw "H3E2B3_ARCHIVE_MUTATED_DURING_BUILD"
    }

    [string[]]$trackedDirty = @(Get-G2TrackedDirtyPaths)
    [string[]]$normalizedTrackedDirty = @(
        $trackedDirty |
            ForEach-Object {
                $_.Replace("\", "/")
            } |
            Sort-Object
    )

    [string[]]$expectedTrackedDirty = @(
        $expectedCoreDirty,
        $propertiesRelative,
        $readmeRelative,
        $setupRelative
    ) | Sort-Object

    [object[]]$trackedDiff = @(
        Compare-Object -ReferenceObject $expectedTrackedDirty -DifferenceObject $normalizedTrackedDirty
    )

    if ($trackedDiff.Count -ne 0) {
        $normalizedTrackedDirty |
            ForEach-Object {
                Write-Host "H3E2B3_TRACKED_DIRTY=$_"
            }

        throw "H3E2B3_TRACKED_DIRTY_SCOPE_INVALID"
    }

    [string[]]$untrackedInLibrary = @(
        & git -C $script:G2RepoRoot ls-files --others --exclude-standard -- $libraryRelative
    )

    if ($LASTEXITCODE -ne 0) {
        throw "H3E2B3_UNTRACKED_QUERY_FAILED"
    }

    [string[]]$normalizedUntracked = @(
        $untrackedInLibrary |
            ForEach-Object {
                $_.Replace("\", "/")
            } |
            Sort-Object
    )

    if ($normalizedUntracked.Count -ne 1 -or
        $normalizedUntracked[0] -ne $archiveRelative) {
        $normalizedUntracked |
            ForEach-Object {
                Write-Host "H3E2B3_UNTRACKED=$_"
            }

        throw "H3E2B3_UNTRACKED_SCOPE_INVALID"
    }

    [object[]]$diffCheck = @(
        & git -C $script:G2RepoRoot diff --check -- $propertiesRelative $readmeRelative $setupRelative
    )

    $diffCheckExit =
        [int]$LASTEXITCODE

    $diffCheck |
        ForEach-Object {
            Write-Host "H3E2B3_DIFF_CHECK=$_"
        }

    Write-Host "H3E2B3_DIFF_CHECK_EXIT=$diffCheckExit"

    if ($diffCheckExit -ne 0) {
        throw "H3E2B3_DIFF_CHECK_FAILED"
    }

    Write-Host "H3E2B3_TRACKED_DIRTY_SCOPE=PASS"
    Write-Host "H3E2B3_UNTRACKED_ARCHIVE_SCOPE=PASS"
    Write-Host "H3E2B3_EXTERNAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO"
    Write-Host "H3E2B3_PACKAGE_PRECOMPILED_FULL=YES"
    Write-Host "H3E2B3_PACKAGE_ADOPTION=PASS"
    Write-Host "H3E2B3_REPOSITORY_MUTATION=YES_EXPECTED_UNCOMMITTED"
    Write-Host "A14_H3E2B3_JWPLC_TFT_PACKAGE_ADOPTION_GATE=PASS"
    Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_EXPLICIT_COMMIT"

    $success = $true
}
finally {
    if (-not $success) {
        Write-Host "H3E2B3_ROLLBACK_START=YES"

        [IO.File]::WriteAllBytes(
            $propertiesPath,
            $originalPropertiesBytes)

        [IO.File]::WriteAllBytes(
            $readmePath,
            $originalReadmeBytes)

        [IO.File]::WriteAllBytes(
            $setupPath,
            $originalSetupBytes)

        if (Test-Path -LiteralPath $archivePath) {
            Remove-Item -LiteralPath $archivePath -Force
        }

        if ($hadArchive -and
            $null -ne $archiveBackup -and
            (Test-Path -LiteralPath $archiveBackup)) {
            New-Item -ItemType Directory -Force -Path $archiveDir |
                Out-Null

            Copy-Item -LiteralPath $archiveBackup -Destination $archivePath -Force
        }

        Write-Host "H3E2B3_ROLLBACK_COMPLETE=YES"
    }

    if ($null -ne $archiveBackup -and
        (Test-Path -LiteralPath $archiveBackup)) {
        Remove-Item -LiteralPath $archiveBackup -Force
    }
}
