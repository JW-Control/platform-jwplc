param(
    [string]$MasterPort = "COM14",
    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.3B - DISPLAY RUNTIME PHYSICAL OVER JWPLC_TFT"
Write-Host "============================================================"

Assert-G2Branch

$cli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$fqbn = "jwplc_local:esp32:jwplcbasic"
$diag = "-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1"

$h3e3aPath =
    Join-Path $PSScriptRoot "a14_h3e3a_display_source_migration.ps1"

$displayRoot =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Display"

$repoLibraries =
    Get-G2Path "JWPLC/2.1.0/libraries"

$sketch =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_h3e3b_display_runtime_physical"

foreach ($required in @(
    $cli,
    $h3e3aPath,
    $displayRoot,
    $repoLibraries,
    $sketch
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E3B_REQUIRED_PATH_MISSING=$required"
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

    throw "H3E3B_ENTRY_DIRTY_SCOPE_INVALID"
}

if ($entryStaged.Count -ne 0) {
    throw "H3E3B_ENTRY_INDEX_NOT_CLEAN"
}

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E3B_MASTER_PORT=$MasterPort"
Write-Host "H3E3B_REPOSITORY_MUTATION=NO"

Write-Host ""
Write-Host "=== H3E3B REQUIRE H3E3A ==="

[object[]]$aOutput =
    @(& $h3e3aPath *>&1)

$aOutput |
    ForEach-Object {
        Write-Host $_
    }

$aText =
    ($aOutput |
        ForEach-Object {
            $_.ToString()
        }) -join [Environment]::NewLine

if (-not $aText.Contains(
    "A14_H3E3A_DISPLAY_SOURCE_MIGRATION_GATE=PASS")) {
    throw "H3E3B_H3E3A_NOT_PASS"
}

Write-Host "H3E3B_H3E3A_PREREQUISITE=PASS"

$runRoot =
    Join-Path $env:TEMP (
        "jwplc_a14_h3e3b_{0}" -f
        (Get-Date -Format "yyyyMMdd_HHmmss")
    )

$tempDisplay =
    Join-Path $runRoot "JWPLC_Display"

$build =
    Join-Path $runRoot "build"

$compileLog =
    Join-Path $runRoot "compile.log"

Copy-Item     -LiteralPath $displayRoot     -Destination $tempDisplay     -Recurse     -Force

New-Item     -ItemType Directory     -Force     -Path $build |
    Out-Null

[object[]]$archives = @(
    Get-ChildItem         -LiteralPath $tempDisplay         -Recurse         -File         -Filter "*.a"         -ErrorAction SilentlyContinue
)

foreach ($archive in $archives) {
    Remove-Item         -LiteralPath $archive.FullName         -Force
}

$properties = @'
name=JWPLC_Display
version=1.0.1-h3e3b
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=H3E3B physical source qualification.
paragraph=Temporary source-only Display runtime over JWPLC_TFT.
category=Display
architectures=esp32
includes=JWPLC_Display.h
dot_a_linkage=true
depends=JWPLC_TFT,JW_MatrixButtons,JWPLC_GlobalPeripherals
'@

[IO.File]::WriteAllText(
    (Join-Path $tempDisplay "library.properties"),
    $properties,
    (New-Object Text.UTF8Encoding($false)))

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $build,
    "--library", $tempDisplay,
    "--libraries", $repoLibraries,
    "--build-property", "compiler.cpp.extra_flags=$diag",
    $sketch
)

Write-Host ""
Write-Host "=== H3E3B COMPILE PHYSICAL SOURCE DISPLAY ==="
Write-Host "H3E3B_COMPILE_START=YES"

$previousPreference =
    $ErrorActionPreference

try {
    $ErrorActionPreference = "Continue"

    [object[]]$compileOutput =
        @(& $cli @compileArgs 2>&1)

    $compileExit =
        [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference =
        $previousPreference
}

$compileOutput |
    ForEach-Object {
        $_.ToString()
    } |
    Set-Content         -LiteralPath $compileLog         -Encoding UTF8

Write-Host "H3E3B_COMPILE_EXIT=$compileExit"
Write-Host "H3E3B_COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    $compileOutput |
        Select-Object -Last 180 |
        ForEach-Object {
            Write-Host $_
        }

    throw "H3E3B_COMPILE_FAILED"
}

$normalizedTempDisplay =
    [IO.Path]::GetFullPath(
        $tempDisplay
    ).TrimEnd('\', '/')

[string[]]$displaySelection = @(
    $compileOutput |
        ForEach-Object {
            $_.ToString()
        } |
        Where-Object {
            $_ -match '^Using library JWPLC_Display at version .+ in folder: (.+)$'
        }
)

$displaySourceSelected =
    $false

foreach ($line in $displaySelection) {
    if ($line -match '^Using library JWPLC_Display at version .+ in folder: (.+)$') {
        $selected =
            [IO.Path]::GetFullPath(
                $Matches[1].Trim()
            ).TrimEnd('\', '/')

        if ($selected -ieq $normalizedTempDisplay) {
            $displaySourceSelected =
                $true
        }
    }
}

Write-Host "H3E3B_TEMP_DISPLAY_SOURCE_SELECTED=$(
    if ($displaySourceSelected) { 'PASS' } else { 'FAIL' }
)"

if (-not $displaySourceSelected) {
    throw "H3E3B_TEMP_DISPLAY_NOT_SELECTED"
}

[string[]]$legacyGraphics = @(
    $compileOutput |
        ForEach-Object {
            $_.ToString()
        } |
        Where-Object {
            $_ -match '^Using library (Adafruit ST7735 and ST7789 Library|Adafruit GFX Library|TFT_eSPI) at version '
        }
)

Write-Host "H3E3B_EXTERNAL_GRAPHICS_BACKEND_SELECTION_COUNT=$($legacyGraphics.Count)"

if ($legacyGraphics.Count -ne 0) {
    $legacyGraphics |
        ForEach-Object {
            Write-Host "H3E3B_UNEXPECTED_GRAPHICS_BACKEND=$_"
        }

    throw "H3E3B_EXTERNAL_GRAPHICS_BACKEND_SELECTED"
}

$compileText =
    ($compileOutput |
        ForEach-Object {
            $_.ToString()
        }) -join [Environment]::NewLine

$jwplcTftSelected =
    $compileText.Contains(
        "Using library JWPLC_TFT")

$jwplcTftPrecompiled = @(
    $compileOutput |
        ForEach-Object {
            $_.ToString()
        } |
        Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match 'JWPLC_TFT')
        }
).Count -gt 0

Write-Host "H3E3B_JWPLC_TFT_SELECTED=$(
    if ($jwplcTftSelected) { 'PASS' } else { 'FAIL' }
)"

Write-Host "H3E3B_JWPLC_TFT_PRECOMPILED=$(
    if ($jwplcTftPrecompiled) { 'PASS' } else { 'FAIL' }
)"

if (-not $jwplcTftSelected -or
    -not $jwplcTftPrecompiled) {
    throw "H3E3B_JWPLC_TFT_LINKAGE_NOT_PROVEN"
}

$displayBuildRoot =
    Join-Path $build "libraries\JWPLC_Display"

[string[]]$expectedDisplayObjects = @(
    Get-ChildItem         -LiteralPath (Join-Path $tempDisplay "src")         -File         -Filter "*.cpp"         -ErrorAction Stop |
        ForEach-Object {
            "$($_.Name).o"
        } |
        Sort-Object
)

[string[]]$actualDisplayObjects = @(
    if (Test-Path -LiteralPath $displayBuildRoot) {
        Get-ChildItem             -LiteralPath $displayBuildRoot             -File             -Filter "*.cpp.o"             -ErrorAction Stop |
            ForEach-Object {
                $_.Name
            } |
            Sort-Object
    }
)

[object[]]$displayObjectDiff = @(
    Compare-Object         -ReferenceObject $expectedDisplayObjects         -DifferenceObject $actualDisplayObjects
)

Write-Host "H3E3B_DISPLAY_EXPECTED_OBJECT_COUNT=$($expectedDisplayObjects.Count)"
Write-Host "H3E3B_DISPLAY_ACTUAL_OBJECT_COUNT=$($actualDisplayObjects.Count)"
Write-Host "H3E3B_DISPLAY_OBJECT_PARITY=$(
    if ($displayObjectDiff.Count -eq 0) { 'PASS' } else { 'FAIL' }
)"

if ($expectedDisplayObjects.Count -lt 6 -or
    $displayObjectDiff.Count -ne 0) {
    throw "H3E3B_DISPLAY_OBJECT_PARITY_FAILED"
}

[int]$tftSourceObjects = @(
    Get-ChildItem         -LiteralPath $build         -Recurse         -File         -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -eq "JWPLC_TFT.cpp.o"
        }
).Count

[int]$backendSourceObjects = @(
    Get-ChildItem         -LiteralPath $build         -Recurse         -File         -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -eq "TFT_eSPI.cpp.o"
        }
).Count

Write-Host "H3E3B_JWPLC_TFT_SOURCE_OBJECT_COUNT=$tftSourceObjects"
Write-Host "H3E3B_TFT_ESPI_SOURCE_OBJECT_COUNT=$backendSourceObjects"

if ($tftSourceObjects -ne 0 -or
    $backendSourceObjects -ne 0) {
    throw "H3E3B_TFT_PRECOMPILED_POLICY_FAILED"
}

Write-Host "H3E3B_COMPILE_LINK=PASS"

if ($PreflightOnly) {
    Write-Host "H3E3B_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E3B_PREFLIGHT_PHYSICAL=NO"
    Write-Host "A14_H3E3B_DISPLAY_RUNTIME_PHYSICAL_PREFLIGHT=PASS"
    return
}

$uploadArgs = @(
    "upload",
    "-p", $MasterPort,
    "-b", $fqbn,
    "--input-dir", $build,
    $sketch
)

Write-Host ""
Write-Host "=== H3E3B UPLOAD EXACT QUALIFIED BUILD ==="
Write-Host "H3E3B_UPLOAD_START=YES"

$previousPreference =
    $ErrorActionPreference

try {
    $ErrorActionPreference =
        "Continue"

    [object[]]$uploadOutput =
        @(& $cli @uploadArgs 2>&1)

    $uploadExit =
        [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference =
        $previousPreference
}

$uploadOutput |
    ForEach-Object {
        Write-Host $_
    }

Write-Host "H3E3B_UPLOAD_EXIT=$uploadExit"

if ($uploadExit -ne 0) {
    throw "H3E3B_UPLOAD_FAILED"
}

function Read-H3E3BYesNo {
    param([string]$PromptText)

    while ($true) {
        $answer =
            (Read-Host $PromptText).
                Trim().
                ToUpperInvariant()

        if ($answer -eq "S" -or
            $answer -eq "N") {
            return $answer
        }

        Write-Host "Responda S o N."
    }
}

Write-Host ""
Write-Host "=== H3E3B PHYSICAL SEQUENCE ==="
Write-Host "1. Debe iniciar en la pantalla IDLE normal."
Write-Host "2. Presione OK: el sketch debe entrar a H3E3B PAGE 1."
Write-Host "3. Observe COUNT/STATE durante varios segundos."
Write-Host "4. Presione RIGHT: debe aparecer H3E3B PAGE 2."
Write-Host "5. Presione UP dos veces y DOWN una vez: LEVEL debe subir y bajar."
Write-Host "6. Presione LEFT: debe volver a H3E3B PAGE 1."
Write-Host "7. Presione ESC: debe volver a IDLE."
Write-Host ""

$idle =
    Read-H3E3BYesNo         "Pantalla IDLE inicial correcta y estable? (S/N)"

$page1 =
    Read-H3E3BYesNo         "Tras OK, PAGE 1 muestra COUNT, STATE y BTN=OK correctamente? (S/N)"

$dirty =
    Read-H3E3BYesNo         "COUNT/STATE cambian sin limpiar toda la pantalla ni flicker visible? (S/N)"

$page2 =
    Read-H3E3BYesNo         "Tras RIGHT, PAGE 2 aparece correctamente y BTN muestra RIGHT? (S/N)"

$bar =
    Read-H3E3BYesNo         "UP/UP/DOWN modifica LEVEL y la barra, y BTN refleja la tecla? (S/N)"

$page1Return =
    Read-H3E3BYesNo         "Tras LEFT, vuelve PAGE 1 y BTN muestra LEFT? (S/N)"

$idleReturn =
    Read-H3E3BYesNo         "Tras ESC, JWPLC_Display vuelve correctamente a IDLE? (S/N)"

$stable =
    Read-H3E3BYesNo         "Sin corrupcion, congelamientos, resets o inestabilidad durante toda la prueba? (S/N)"

Write-Host "H3E3B_PHYSICAL_IDLE_INITIAL=$idle"
Write-Host "H3E3B_PHYSICAL_PAGE1=$page1"
Write-Host "H3E3B_PHYSICAL_DIRTY_REDRAW=$dirty"
Write-Host "H3E3B_PHYSICAL_PAGE2=$page2"
Write-Host "H3E3B_PHYSICAL_BAR_INPUT=$bar"
Write-Host "H3E3B_PHYSICAL_PAGE1_RETURN=$page1Return"
Write-Host "H3E3B_PHYSICAL_IDLE_RETURN=$idleReturn"
Write-Host "H3E3B_PHYSICAL_STABILITY=$stable"

if (@(
    $idle,
    $page1,
    $dirty,
    $page2,
    $bar,
    $page1Return,
    $idleReturn,
    $stable
) -contains "N") {
    throw "H3E3B_PHYSICAL_OBSERVATION_FAILED"
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
    throw "H3E3B_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E3B_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E3B_RUNTIME_DISPLAY=SOURCE_MIGRATED"
Write-Host "H3E3B_RUNTIME_TFT=JWPLC_TFT_PRECOMPILED"
Write-Host "H3E3B_EXTERNAL_GRAPHICS_BACKEND=NO"
Write-Host "H3E3B_HMI_FIELDS=PASS"
Write-Host "H3E3B_DIRTY_REDRAW=PASS"
Write-Host "H3E3B_MANUAL_PAGE_CHANGE=PASS"
Write-Host "H3E3B_BUTTONS_USER_OWNED_PATH=PASS"
Write-Host "H3E3B_IDLE_RETURN_ESC=PASS"
Write-Host "H3E3B_REPOSITORY_MUTATION=NO"
Write-Host "A14_H3E3B_DISPLAY_RUNTIME_PHYSICAL_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E3C_DISPLAY_PRECOMPILED"
