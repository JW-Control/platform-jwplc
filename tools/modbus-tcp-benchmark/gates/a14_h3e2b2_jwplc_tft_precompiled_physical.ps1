param(
    [string]$MasterPort = "COM14",
    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.2B2 - JWPLC_TFT PRECOMPILED PHYSICAL"
Write-Host "============================================================"

Assert-G2Branch

$fqbn = "jwplc_local:esp32:jwplcbasic"
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$diagDefine = "-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1"

$b1Path = Join-Path $PSScriptRoot "a14_h3e2b1_jwplc_tft_precompiled_candidate.ps1"
$sketchRelative = "tools/modbus-tcp-benchmark/firmware/a14_h3e2b2_jwplc_tft_precompiled_physical"
$sketchPath = Get-G2Path $sketchRelative
$inoPath = Join-Path $sketchPath "a14_h3e2b2_jwplc_tft_precompiled_physical.ino"

foreach ($required in @(
    $arduinoCli,
    $b1Path,
    $sketchPath,
    $inoPath
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E2B2_REQUIRED_PATH_MISSING=$required"
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

if ($normalizedEntryDirty.Count -ne 1 -or $normalizedEntryDirty[0] -ne $expectedCoreDirty) {
    $normalizedEntryDirty |
        ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }

    throw "H3E2B2_ENTRY_DIRTY_SCOPE_INVALID"
}

if ($entryStaged.Count -ne 0) {
    throw "H3E2B2_ENTRY_INDEX_NOT_CLEAN"
}

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E2B2_FQBN=$fqbn"
Write-Host "H3E2B2_MASTER_PORT=$MasterPort"
Write-Host "H3E2B2_DIAGNOSTIC_DEFINE=$diagDefine"
Write-Host "H3E2B2_REPOSITORY_MUTATION=NO"

# -------------------------------------------------------------------------
# 1) Regenerar y calificar candidate B1 en esta misma ejecucion.
# -------------------------------------------------------------------------
Write-Host ""
Write-Host "=== H3E2B2 REGENERATE / QUALIFY B1 CANDIDATE ==="

[object[]]$b1Output = @(& $b1Path *>&1)
$b1Output | ForEach-Object { Write-Host $_ }

$b1Text =
    ($b1Output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

if (-not $b1Text.Contains(
    "A14_H3E2B1_JWPLC_TFT_PRECOMPILED_CANDIDATE_GATE=PASS")) {
    throw "H3E2B2_B1_CANDIDATE_GATE_FAILED"
}

$rootMatch = [regex]::Match(
    $b1Text,
    '(?m)^H3E2B1_CANDIDATE_LIBRARY_ROOT=(.+)\r?$'
)

$archiveMatch = [regex]::Match(
    $b1Text,
    '(?m)^H3E2B1_CANDIDATE_ARCHIVE=(.+)\r?$'
)

$shaMatch = [regex]::Match(
    $b1Text,
    '(?m)^H3E2B1_CANDIDATE_ARCHIVE_SHA256=([0-9A-Fa-f]{64})\r?$'
)

if (-not $rootMatch.Success -or
    -not $archiveMatch.Success -or
    -not $shaMatch.Success) {
    throw "H3E2B2_B1_HANDOFF_MARKERS_MISSING"
}

$candidateLibraryRoot = $rootMatch.Groups[1].Value.Trim()
$candidateArchive = $archiveMatch.Groups[1].Value.Trim()
$candidateArchiveSha = $shaMatch.Groups[1].Value.Trim().ToUpperInvariant()

foreach ($required in @(
    $candidateLibraryRoot,
    $candidateArchive
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E2B2_B1_HANDOFF_PATH_MISSING=$required"
    }
}

$actualArchiveSha =
    (Get-G2Sha256Path $candidateArchive).ToUpperInvariant()

Write-Host "H3E2B2_CANDIDATE_LIBRARY_ROOT=$candidateLibraryRoot"
Write-Host "H3E2B2_CANDIDATE_ARCHIVE=$candidateArchive"
Write-Host "H3E2B2_CANDIDATE_ARCHIVE_SHA256=$actualArchiveSha"
Write-Host "H3E2B2_B1_TO_B2_ARCHIVE_IDENTITY=$(
    if ($actualArchiveSha -eq $candidateArchiveSha) { 'PASS' } else { 'FAIL' }
)"

if ($actualArchiveSha -ne $candidateArchiveSha) {
    throw "H3E2B2_B1_TO_B2_ARCHIVE_IDENTITY_FAILED"
}

# -------------------------------------------------------------------------
# 2) Compile del probe fisico usando solo la libreria precompilada temporal.
# -------------------------------------------------------------------------
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h3e2b2_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $buildPath,
    "--library", $candidateLibraryRoot,
    "--build-property", "compiler.cpp.extra_flags=$diagDefine",
    $sketchPath
)

Write-Host ""
Write-Host "=== H3E2B2 COMPILE PHYSICAL PROBE ==="
Write-Host "H3E2B2_COMPILE_START=YES"

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    [object[]]$compileOutput = @(& $arduinoCli @compileArgs 2>&1)
    $compileExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

$compileOutput |
    ForEach-Object { $_.ToString() } |
    Set-Content -LiteralPath $compileLog -Encoding UTF8

$compileText =
    ($compileOutput | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

Write-Host "H3E2B2_COMPILE_EXIT=$compileExit"
Write-Host "H3E2B2_COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    $compileOutput |
        Select-Object -Last 160 |
        ForEach-Object { Write-Host $_ }

    throw "H3E2B2_COMPILE_FAILED"
}

$normalizedCandidateRoot =
    [IO.Path]::GetFullPath(
        $candidateLibraryRoot
    ).TrimEnd('\', '/')

[string[]]$jwplcSelection = @(
    $compileOutput |
        ForEach-Object { $_.ToString() } |
        Where-Object {
            $_ -match '^Using library JWPLC_TFT at version .+ in folder: (.+)$'
        }
)

$selectedCandidate = $false

foreach ($line in $jwplcSelection) {
    if ($line -match '^Using library JWPLC_TFT at version .+ in folder: (.+)$') {
        $selectedPath =
            [IO.Path]::GetFullPath(
                $Matches[1].Trim()
            ).TrimEnd('\', '/')

        if ($selectedPath -ieq $normalizedCandidateRoot) {
            $selectedCandidate = $true
        }
    }
}

Write-Host "H3E2B2_CANDIDATE_LIBRARY_SELECTED=$(
    if ($selectedCandidate) { 'PASS' } else { 'FAIL' }
)"

if (-not $selectedCandidate) {
    $jwplcSelection |
        ForEach-Object {
            Write-Host "H3E2B2_SELECTION_LINE=$_"
        }

    throw "H3E2B2_TEMP_LIBRARY_NOT_SELECTED"
}

[string[]]$backendSelection = @(
    $compileOutput |
        ForEach-Object { $_.ToString() } |
        Where-Object {
            $_ -match '^Using library TFT_eSPI at version '
        }
)

Write-Host "H3E2B2_EXTERNAL_TFT_ESPI_SELECTION_COUNT=$($backendSelection.Count)"

if ($backendSelection.Count -ne 0) {
    $backendSelection |
        ForEach-Object {
            Write-Host "H3E2B2_TFT_ESPI_SELECTION=$_"
        }

    throw "H3E2B2_EXTERNAL_TFT_ESPI_SELECTED"
}

$compileDbPath = Join-Path $buildPath "compile_commands.json"

if (-not (Test-Path -LiteralPath $compileDbPath)) {
    throw "H3E2B2_COMPILE_DB_MISSING"
}

[object[]]$compileDbEntries = @(
    Get-Content -LiteralPath $compileDbPath -Raw |
        ConvertFrom-Json
)

[string[]]$tuFiles = @(
    foreach ($entry in $compileDbEntries) {
        $fileText = [string]$entry.file

        if ([string]::IsNullOrWhiteSpace($fileText)) {
            continue
        }

        $fileText.
            Trim().
            Trim('"').
            Replace([char]92, [char]47)
    }
)

[int]$jwplcTftSourceCompiles = @(
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

Write-Host "H3E2B2_COMPILE_DB_ENTRY_COUNT=$($compileDbEntries.Count)"
Write-Host "H3E2B2_JWPLC_TFT_SOURCE_COMPILES=$jwplcTftSourceCompiles"
Write-Host "H3E2B2_TFT_ESPI_SOURCE_COMPILES=$backendSourceCompiles"

if ($jwplcTftSourceCompiles -ne 0 -or
    $backendSourceCompiles -ne 0) {
    throw "H3E2B2_PRECOMPILED_SOURCE_COMPILE_POLICY_FAILED"
}

$precompiledObserved = @(
    $compileOutput |
        ForEach-Object { $_.ToString() } |
        Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match 'JWPLC_TFT')
        }
).Count -gt 0

Write-Host "H3E2B2_PRECOMPILED_SELECTION_PROOF=$(
    if ($precompiledObserved) { 'PASS' } else { 'FAIL' }
)"

if (-not $precompiledObserved) {
    throw "H3E2B2_PRECOMPILED_SELECTION_NOT_PROVEN"
}

$displayObjectCount = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.FullName -match 'JWPLC_Display' -or
            $_.Name -match 'JWPLC_Display'
        }
).Count

Write-Host "H3E2B2_DISPLAY_BUILD_ARTIFACT_COUNT=$displayObjectCount"
Write-Host "H3E2B2_DISPLAY_AUTOLOAD_SUPPRESSED=$(
    if ($displayObjectCount -eq 0) { 'PASS' } else { 'FAIL' }
)"

if ($displayObjectCount -ne 0) {
    throw "H3E2B2_DISPLAY_AUTOLOAD_NOT_SUPPRESSED"
}

Write-Host "H3E2B2_COMPILE_LINK=PASS"

if ($PreflightOnly) {
    Write-Host "H3E2B2_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E2B2_PREFLIGHT_PHYSICAL_OBSERVATION=NO"
    Write-Host "A14_H3E2B2_JWPLC_TFT_PRECOMPILED_PHYSICAL_PREFLIGHT=PASS"
    return
}

# -------------------------------------------------------------------------
# 3) Upload exacto del build que enlazo el archive B1.
# -------------------------------------------------------------------------
$uploadArgs = @(
    "upload",
    "-p", $MasterPort,
    "-b", $fqbn,
    "--input-dir", $buildPath,
    $sketchPath
)

Write-Host ""
Write-Host "=== H3E2B2 UPLOAD PRECOMPILED PHYSICAL PROBE ==="
Write-Host "H3E2B2_UPLOAD_START=YES"

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    [object[]]$uploadOutput = @(& $arduinoCli @uploadArgs 2>&1)
    $uploadExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

$uploadOutput |
    ForEach-Object {
        Write-Host $_
    }

Write-Host "H3E2B2_UPLOAD_EXIT=$uploadExit"

if ($uploadExit -ne 0) {
    throw "H3E2B2_UPLOAD_FAILED"
}

Write-Host ""
Write-Host "Observe la TFT del Master antes de responder."
Write-Host "Debe verse 'JWPLC_TFT PRECOMP' en landscape 320x170,"
Write-Host "con marco blanco completo, esquinas TL/TR/BL/BR correctas"
Write-Host "y barras centrales ROJO - VERDE - AZUL."
Write-Host ""

function Read-H3E2B2YesNo {
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

$orientation =
    Read-H3E2B2YesNo "¿Texto horizontal y orientacion landscape correctos? (S/N)"

$border =
    Read-H3E2B2YesNo "¿Marco blanco completo y cuatro esquinas sin recorte/desfase? (S/N)"

$colors =
    Read-H3E2B2YesNo "¿Barras centrales se ven ROJO - VERDE - AZUL en ese orden? (S/N)"

$labels =
    Read-H3E2B2YesNo "¿TL/TR/BL/BR corresponden a sus esquinas correctas? (S/N)"

$stable =
    Read-H3E2B2YesNo "¿Pantalla estable, sin flicker/cortes/reinicios visibles? (S/N)"

Write-Host "H3E2B2_PHYSICAL_ORIENTATION=$orientation"
Write-Host "H3E2B2_PHYSICAL_BORDER_OFFSETS=$border"
Write-Host "H3E2B2_PHYSICAL_RGB=$colors"
Write-Host "H3E2B2_PHYSICAL_CORNER_LABELS=$labels"
Write-Host "H3E2B2_PHYSICAL_STABILITY=$stable"

if (@(
    $orientation,
    $border,
    $colors,
    $labels,
    $stable
) -contains "N") {
    throw "H3E2B2_PHYSICAL_OBSERVATION_FAILED"
}

[string[]]$finalDirty = @(Get-G2TrackedDirtyPaths)
[string[]]$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)

if ($normalizedFinalDirty.Count -ne 1 -or
    $normalizedFinalDirty[0] -ne $expectedCoreDirty) {
    throw "H3E2B2_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E2B2_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E2B2_RUNTIME_API=JWPLC_TFT"
Write-Host "H3E2B2_RUNTIME_LINKAGE=PRECOMPILED_ARCHIVE"
Write-Host "H3E2B2_EXTERNAL_TFT_ESPI_REQUIRED=NO"
Write-Host "H3E2B2_ROTATION=1"
Write-Host "H3E2B2_COLOR_ORDER=BGR"
Write-Host "H3E2B2_LOGICAL_GEOMETRY=320x170"
Write-Host "H3E2B2_SPI=80MHz_MODE0"
Write-Host "H3E2B2_ARCHIVE_SHA256=$actualArchiveSha"
Write-Host "H3E2B2_REPOSITORY_MUTATION=NO"
Write-Host "A14_H3E2B2_JWPLC_TFT_PRECOMPILED_PHYSICAL_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E2B3_PACKAGE_ADOPTION"
