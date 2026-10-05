param(
    [string]$MasterPort = "COM14",
    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.3D - DISPLAY PRECOMPILED PHYSICAL"
Write-Host "============================================================"

Assert-G2Branch

$cli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$fqbn = "jwplc_local:esp32:jwplcbasic"

$h3e3c =
    Join-Path $PSScriptRoot "a14_h3e3c_display_precompiled_candidate.ps1"

$sketch =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_h3e3b_display_runtime_physical"

foreach ($required in @(
    $cli,
    $h3e3c,
    $sketch
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E3D_REQUIRED_PATH_MISSING=$required"
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

    throw "H3E3D_ENTRY_DIRTY_SCOPE_INVALID"
}

if ($entryStaged.Count -ne 0) {
    throw "H3E3D_ENTRY_INDEX_NOT_CLEAN"
}

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E3D_MASTER_PORT=$MasterPort"
Write-Host "H3E3D_REPOSITORY_MUTATION=NO"

Write-Host ""
Write-Host "=== H3E3D REQUIRE H3E3C ==="

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
    throw "H3E3D_H3E3C_NOT_PASS"
}

Write-Host "H3E3D_H3E3C_PREREQUISITE=PASS"

function Get-H3E3DMarker {
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
        throw ("H3E3D_MARKER_COUNT_INVALID={0}:{1}" -f $Prefix, $matches.Count)
    }

    return $matches[0].Substring($Prefix.Length)
}

$candidateArchive =
    Get-H3E3DMarker -Lines $cLines -Prefix "H3E3C_CANDIDATE_ARCHIVE="

$qualifiedArchiveSha =
    Get-H3E3DMarker -Lines $cLines -Prefix "H3E3C_CANDIDATE_ARCHIVE_SHA256="

if (-not (Test-Path -LiteralPath $candidateArchive)) {
    throw "H3E3D_CANDIDATE_ARCHIVE_MISSING=$candidateArchive"
}

$currentArchiveSha =
    (Get-G2Sha256Path $candidateArchive).ToUpperInvariant()

if ($currentArchiveSha -ne $qualifiedArchiveSha.ToUpperInvariant()) {
    throw "H3E3D_CANDIDATE_ARCHIVE_HASH_CHANGED"
}

$candidateDisplay =
    Split-Path -Parent (
        Split-Path -Parent (
            Split-Path -Parent $candidateArchive
        )
    )

$runRoot =
    Split-Path -Parent (
        Split-Path -Parent $candidateDisplay
    )

$candidateBuild =
    Join-Path $runRoot "candidate-build"

if (-not (Test-Path -LiteralPath $candidateBuild)) {
    throw "H3E3D_CANDIDATE_BUILD_MISSING=$candidateBuild"
}

[object[]]$appBins = @(
    Get-ChildItem -LiteralPath $candidateBuild -File -Filter "*.ino.bin" -ErrorAction SilentlyContinue
)

[object[]]$mergedBins = @(
    Get-ChildItem -LiteralPath $candidateBuild -File -Filter "*.merged.bin" -ErrorAction SilentlyContinue
)

Write-Host "H3E3D_APP_BIN_COUNT=$($appBins.Count)"
Write-Host "H3E3D_MERGED_BIN_COUNT=$($mergedBins.Count)"
Write-Host "H3E3D_APP_BIN_SELECTION=EXACT_INO_BIN"

if ($appBins.Count -ne 1) {
    throw "H3E3D_APP_BIN_COUNT_INVALID=$($appBins.Count)"
}

$appBin =
    $appBins[0].FullName

$appBinSha =
    (Get-G2Sha256Path $appBin).ToUpperInvariant()

$appBinBytes =
    (Get-Item -LiteralPath $appBin).Length

Write-Host "H3E3D_CANDIDATE_ARCHIVE=$candidateArchive"
Write-Host "H3E3D_CANDIDATE_ARCHIVE_SHA256=$currentArchiveSha"
Write-Host "H3E3D_CANDIDATE_BUILD=$candidateBuild"
Write-Host "H3E3D_APP_BIN=$appBin"
Write-Host "H3E3D_APP_BIN_BYTES=$appBinBytes"
Write-Host "H3E3D_APP_BIN_SHA256=$appBinSha"
Write-Host "H3E3D_EXACT_QUALIFIED_BUILD=YES"
Write-Host "H3E3D_RECOMPILE_AFTER_H3E3C=NO"

if ($PreflightOnly) {
    Write-Host "H3E3D_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E3D_PREFLIGHT_PHYSICAL=NO"
    Write-Host "A14_H3E3D_DISPLAY_PRECOMPILED_PHYSICAL_PREFLIGHT=PASS"
    return
}

$uploadArgs = @(
    "upload",
    "-p", $MasterPort,
    "-b", $fqbn,
    "--input-dir", $candidateBuild,
    $sketch
)

Write-Host ""
Write-Host "=== H3E3D UPLOAD EXACT H3E3C BUILD ==="
Write-Host "H3E3D_UPLOAD_START=YES"

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

Write-Host "H3E3D_UPLOAD_EXIT=$uploadExit"

if ($uploadExit -ne 0) {
    throw "H3E3D_UPLOAD_FAILED"
}

function Read-H3E3DYesNo {
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
Write-Host "=== H3E3D PHYSICAL SEQUENCE ==="
Write-Host "Mismo firmware funcional de H3E3B; cambia solo el linkage de Display."
Write-Host "1. Debe iniciar en la pantalla IDLE normal."
Write-Host "2. Presione OK: debe entrar a H3E3B PAGE 1."
Write-Host "3. Observe COUNT/STATE durante varios segundos."
Write-Host "4. Presione RIGHT: debe aparecer H3E3B PAGE 2."
Write-Host "5. Presione UP dos veces y DOWN una vez."
Write-Host "6. Presione LEFT: debe volver a PAGE 1."
Write-Host "7. Presione ESC: debe volver a IDLE."
Write-Host ""

$idle =
    Read-H3E3DYesNo "Pantalla IDLE inicial correcta y estable? (S/N)"

$page1 =
    Read-H3E3DYesNo "Tras OK, PAGE 1 muestra COUNT, STATE y BTN=OK correctamente? (S/N)"

$dirty =
    Read-H3E3DYesNo "COUNT/STATE cambian sin limpiar toda la pantalla ni flicker visible? (S/N)"

$page2 =
    Read-H3E3DYesNo "Tras RIGHT, PAGE 2 aparece correctamente y BTN muestra RIGHT? (S/N)"

$bar =
    Read-H3E3DYesNo "UP/UP/DOWN modifica LEVEL y la barra, y BTN refleja la tecla? (S/N)"

$page1Return =
    Read-H3E3DYesNo "Tras LEFT, vuelve PAGE 1 y BTN muestra LEFT? (S/N)"

$idleReturn =
    Read-H3E3DYesNo "Tras ESC, JWPLC_Display vuelve correctamente a IDLE? (S/N)"

$stable =
    Read-H3E3DYesNo "Sin corrupcion, congelamientos, resets o inestabilidad durante toda la prueba? (S/N)"

$visualParity =
    Read-H3E3DYesNo "Comportamiento visual equivalente a H3E3B source, incluido IDLE rapido y sin flicker visible? (S/N)"

Write-Host "H3E3D_PHYSICAL_IDLE_INITIAL=$idle"
Write-Host "H3E3D_PHYSICAL_PAGE1=$page1"
Write-Host "H3E3D_PHYSICAL_DIRTY_REDRAW=$dirty"
Write-Host "H3E3D_PHYSICAL_PAGE2=$page2"
Write-Host "H3E3D_PHYSICAL_BAR_INPUT=$bar"
Write-Host "H3E3D_PHYSICAL_PAGE1_RETURN=$page1Return"
Write-Host "H3E3D_PHYSICAL_IDLE_RETURN=$idleReturn"
Write-Host "H3E3D_PHYSICAL_STABILITY=$stable"
Write-Host "H3E3D_PHYSICAL_VISUAL_PARITY_WITH_SOURCE=$visualParity"

if (@(
    $idle,
    $page1,
    $dirty,
    $page2,
    $bar,
    $page1Return,
    $idleReturn,
    $stable,
    $visualParity
) -contains "N") {
    throw "H3E3D_PHYSICAL_OBSERVATION_FAILED"
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
    throw "H3E3D_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E3D_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E3D_RUNTIME_DISPLAY=PRECOMPILED"
Write-Host "H3E3D_RUNTIME_TFT=PRECOMPILED"
Write-Host "H3E3D_EXTERNAL_GRAPHICS_BACKEND=NO"
Write-Host "H3E3D_HMI_FIELDS=PASS"
Write-Host "H3E3D_DIRTY_REDRAW=PASS"
Write-Host "H3E3D_MANUAL_PAGE_CHANGE=PASS"
Write-Host "H3E3D_BUTTONS_USER_OWNED_PATH=PASS"
Write-Host "H3E3D_IDLE_RETURN_ESC=PASS"
Write-Host "H3E3D_SOURCE_PRECOMPILED_PHYSICAL_PARITY=PASS"
Write-Host "H3E3D_REPOSITORY_MUTATION=NO"
Write-Host "A14_H3E3D_DISPLAY_PRECOMPILED_PHYSICAL_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E3E_PACKAGE_ADOPTION"
