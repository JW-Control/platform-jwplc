param(
    [string]$MasterPort = "COM14",
    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.4A2 - JWPLC_TFT SHAPES PHYSICAL"
Write-Host "============================================================"

Assert-G2Branch

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$fqbn = "jwplc_local:esp32:jwplcbasic"

$h3e4a1 =
    Join-Path $PSScriptRoot "a14_h3e4a1_jwplc_tft_shapes_candidate.ps1"

$probePath =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_h3e4a1_jwplc_tft_shapes_probe"

foreach ($required in @(
    $arduinoCli,
    $h3e4a1,
    $probePath
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E4A2_REQUIRED_PATH_MISSING=$required"
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

    throw "H3E4A2_ENTRY_DIRTY_SCOPE_INVALID"
}

if ($entryStaged.Count -ne 0) {
    throw "H3E4A2_ENTRY_INDEX_NOT_CLEAN"
}

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E4A2_MASTER_PORT=$MasterPort"
Write-Host "H3E4A2_REPOSITORY_MUTATION=NO"

Write-Host ""
Write-Host "=== H3E4A2 REQUIRE H3E4A1 ==="

[object[]]$a1Output =
    @(& $h3e4a1 *>&1)

$a1Output |
    ForEach-Object {
        Write-Host $_
    }

[string[]]$a1Lines = @(
    $a1Output |
        ForEach-Object {
            $_.ToString()
        }
)

$a1Text =
    $a1Lines -join [Environment]::NewLine

if (-not $a1Text.Contains(
    "A14_H3E4A1_JWPLC_TFT_SHAPES_CANDIDATE_GATE=PASS")) {
    throw "H3E4A2_H3E4A1_NOT_PASS"
}

Write-Host "H3E4A2_H3E4A1_PREREQUISITE=PASS"

function Get-H3E4A2Marker {
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
        throw ("H3E4A2_MARKER_COUNT_INVALID={0}:{1}" -f $Prefix, $matches.Count)
    }

    return $matches[0].Substring($Prefix.Length)
}

$candidateRoot =
    Get-H3E4A2Marker -Lines $a1Lines -Prefix "H3E4A1_CANDIDATE_LIBRARY_ROOT="

$candidateArchive =
    Get-H3E4A2Marker -Lines $a1Lines -Prefix "H3E4A1_CANDIDATE_ARCHIVE="

$qualifiedArchiveSha =
    Get-H3E4A2Marker -Lines $a1Lines -Prefix "H3E4A1_CANDIDATE_ARCHIVE_SHA256="

foreach ($required in @(
    $candidateRoot,
    $candidateArchive
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E4A2_CANDIDATE_PATH_MISSING=$required"
    }
}

$currentArchiveSha =
    (Get-G2Sha256Path $candidateArchive).ToUpperInvariant()

if ($currentArchiveSha -ne $qualifiedArchiveSha.ToUpperInvariant()) {
    throw "H3E4A2_CANDIDATE_ARCHIVE_HASH_CHANGED"
}

$candidateLibraries =
    Split-Path -Parent $candidateRoot

$runRoot =
    Split-Path -Parent $candidateLibraries

$candidateBuild =
    Join-Path $runRoot "candidate-build"

if (-not (Test-Path -LiteralPath $candidateBuild)) {
    throw "H3E4A2_CANDIDATE_BUILD_MISSING=$candidateBuild"
}

[object[]]$appBins = @(
    Get-ChildItem -LiteralPath $candidateBuild -File -Filter "*.ino.bin" -ErrorAction SilentlyContinue
)

if ($appBins.Count -ne 1) {
    throw "H3E4A2_APP_BIN_COUNT_INVALID=$($appBins.Count)"
}

$appBin =
    $appBins[0].FullName

$appBinSha =
    (Get-G2Sha256Path $appBin).ToUpperInvariant()

$appBinBytes =
    (Get-Item -LiteralPath $appBin).Length

Write-Host "H3E4A2_CANDIDATE_ARCHIVE=$candidateArchive"
Write-Host "H3E4A2_CANDIDATE_ARCHIVE_SHA256=$currentArchiveSha"
Write-Host "H3E4A2_CANDIDATE_BUILD=$candidateBuild"
Write-Host "H3E4A2_APP_BIN=$appBin"
Write-Host "H3E4A2_APP_BIN_BYTES=$appBinBytes"
Write-Host "H3E4A2_APP_BIN_SHA256=$appBinSha"
Write-Host "H3E4A2_EXACT_QUALIFIED_BUILD=YES"
Write-Host "H3E4A2_RECOMPILE_AFTER_H3E4A1=NO"

if ($PreflightOnly) {
    Write-Host "H3E4A2_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E4A2_PREFLIGHT_PHYSICAL=NO"
    Write-Host "A14_H3E4A2_JWPLC_TFT_SHAPES_PHYSICAL_PREFLIGHT=PASS"
    return
}

$uploadArgs = @(
    "upload",
    "-p", $MasterPort,
    "-b", $fqbn,
    "--input-dir", $candidateBuild,
    $probePath
)

Write-Host ""
Write-Host "=== H3E4A2 UPLOAD EXACT H3E4A1 BUILD ==="
Write-Host "H3E4A2_UPLOAD_START=YES"

$previousPreference =
    $ErrorActionPreference

try {
    $ErrorActionPreference =
        "Continue"

    [object[]]$uploadOutput =
        @(& $arduinoCli @uploadArgs 2>&1)

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

Write-Host "H3E4A2_UPLOAD_EXIT=$uploadExit"

if ($uploadExit -ne 0) {
    throw "H3E4A2_UPLOAD_FAILED"
}

function Read-H3E4A2YesNo {
    param([string]$PromptText)

    while ($true) {
        $answer =
            (Read-Host $PromptText).Trim().ToUpperInvariant()

        if ($answer -eq "S" -or
            $answer -eq "N") {
            return $answer
        }

        Write-Host "Responda S o N."
    }
}

Write-Host ""
Write-Host "=== H3E4A2 PHYSICAL SEQUENCE ==="
Write-Host "Debe verse fondo negro y tres grupos graficos en landscape 320x170:"
Write-Host "1. Izquierda: rectangulo redondeado azul relleno con borde blanco redondeado."
Write-Host "2. Centro: circulo verde relleno."
Write-Host "3. Derecha: circulo amarillo solo de contorno."
Write-Host "4. No debe haber corrupcion, flicker sostenido, freeze ni reset."
Write-Host ""

$background =
    Read-H3E4A2YesNo "Fondo negro, orientacion landscape y offsets correctos? (S/N)"

$roundFill =
    Read-H3E4A2YesNo "A la izquierda se ve un rectangulo redondeado AZUL relleno? (S/N)"

$roundOutline =
    Read-H3E4A2YesNo "Ese rectangulo tiene un borde BLANCO redondeado claramente visible? (S/N)"

$circleFill =
    Read-H3E4A2YesNo "Al centro se ve un circulo VERDE relleno y correctamente redondo? (S/N)"

$circleOutline =
    Read-H3E4A2YesNo "A la derecha se ve un circulo AMARILLO solo de contorno? (S/N)"

$stability =
    Read-H3E4A2YesNo "La pantalla permanece estable, sin corrupcion, freeze, reset o flicker sostenido? (S/N)"

Write-Host "H3E4A2_PHYSICAL_BACKGROUND=$background"
Write-Host "H3E4A2_PHYSICAL_FILL_ROUND_RECT=$roundFill"
Write-Host "H3E4A2_PHYSICAL_DRAW_ROUND_RECT=$roundOutline"
Write-Host "H3E4A2_PHYSICAL_FILL_CIRCLE=$circleFill"
Write-Host "H3E4A2_PHYSICAL_DRAW_CIRCLE=$circleOutline"
Write-Host "H3E4A2_PHYSICAL_STABILITY=$stability"

if (@(
    $background,
    $roundFill,
    $roundOutline,
    $circleFill,
    $circleOutline,
    $stability
) -contains "N") {
    throw "H3E4A2_PHYSICAL_OBSERVATION_FAILED"
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
    throw "H3E4A2_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E4A2_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E4A2_FILL_ROUND_RECT=PASS"
Write-Host "H3E4A2_DRAW_ROUND_RECT=PASS"
Write-Host "H3E4A2_FILL_CIRCLE=PASS"
Write-Host "H3E4A2_DRAW_CIRCLE=PASS"
Write-Host "H3E4A2_RUNTIME_TFT=PRECOMPILED_CANDIDATE"
Write-Host "H3E4A2_EXTERNAL_TFT_ESPI_AT_USER_BUILD=NO"
Write-Host "H3E4A2_REPOSITORY_MUTATION=NO"
Write-Host "A14_H3E4A2_JWPLC_TFT_SHAPES_PHYSICAL_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E4A3_TFT_PACKAGE_ADOPTION"
