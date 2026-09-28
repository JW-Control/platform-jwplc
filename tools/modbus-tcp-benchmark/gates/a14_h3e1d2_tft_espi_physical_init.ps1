param(
    [string]$MasterPort = "COM14",
    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.1D.2 - TFT_eSPI PHYSICAL INIT / GEOMETRY"
Write-Host "============================================================"

Assert-G2Branch

$fqbn = "jwplc_local:esp32:jwplcbasic"
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$diagDefine = "-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1"

$sketchRelative = "tools/modbus-tcp-benchmark/firmware/a14_h3e1d2_tft_espi_physical_init"
$sketchPath = Get-G2Path $sketchRelative
$setupPath = Join-Path $sketchPath "tft_setup.h"
$inoPath = Join-Path $sketchPath "a14_h3e1d2_tft_espi_physical_init.ino"
$d0Path = Join-Path $PSScriptRoot "a14_h3e1d0_tft_espi_env_preflight.ps1"
$arduinoHeaderPath = Get-G2Path "JWPLC/2.1.0/cores/jwcontrol/Arduino.h"

foreach ($required in @(
    $arduinoCli,
    $sketchPath,
    $setupPath,
    $inoPath,
    $d0Path,
    $arduinoHeaderPath
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E1D2_REQUIRED_PATH_MISSING=$required"
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
    $normalizedEntryDirty | ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }
    throw "H3E1D2_ENTRY_DIRTY_SCOPE_INVALID"
}
if ($entryStaged.Count -ne 0) {
    throw "H3E1D2_ENTRY_INDEX_NOT_CLEAN"
}

$arduinoHeaderText = [IO.File]::ReadAllText($arduinoHeaderPath)
$diagContract =
    $arduinoHeaderText.Contains("JWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD") -and
    $arduinoHeaderText.Contains("#include <JWPLC_GlobalPeripherals_Auto.h>") -and
    $arduinoHeaderText.Contains("#include <JWPLC_Display_Auto.h>")

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E1D2_FQBN=$fqbn"
Write-Host "H3E1D2_MASTER_PORT=$MasterPort"
Write-Host "H3E1D2_DIAGNOSTIC_DEFINE=$diagDefine"
Write-Host "H3E1D2_NORMAL_AUTOLOAD_POLICY=UNCHANGED"
Write-Host "H3E1D2_DIAGNOSTIC_AUTOLOAD_CONTRACT=$(
    if ($diagContract) { 'PASS' } else { 'FAIL' }
)"

if (-not $diagContract) {
    throw "H3E1D2_DIAGNOSTIC_AUTOLOAD_CONTRACT_FAILED"
}

[object[]]$d0Output = @(& $d0Path *>&1)
$d0Output | ForEach-Object { Write-Host $_ }
$d0Text = ($d0Output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
if (-not $d0Text.Contains("A14_H3E1D0_TFT_ESPI_ENV_PREFLIGHT=PASS")) {
    throw "H3E1D2_D0_PREFLIGHT_FAILED"
}

$setupText = [IO.File]::ReadAllText($setupPath)
foreach ($contract in @(
    '#define ST7789_DRIVER',
    '#define TFT_WIDTH  170',
    '#define TFT_HEIGHT 320',
    '#define TFT_RGB_ORDER TFT_RGB',
    '#define TFT_INVERSION_ON',
    '#define TFT_MOSI 23',
    '#define TFT_MISO 19',
    '#define TFT_SCLK 18',
    '#define TFT_CS   33',
    '#define TFT_DC   25',
    '#define TFT_RST  14',
    '#define TFT_SPI_MODE SPI_MODE0',
    '#define SPI_FREQUENCY 80000000',
    '#define LOAD_GLCD',
    '#define SUPPORT_TRANSACTIONS'
)) {
    $count = ([regex]::Matches($setupText, [regex]::Escape($contract))).Count
    if ($count -ne 1) {
        Write-Host "H3E1D2_SETUP_CONTRACT_FAIL=$contract"
        throw "H3E1D2_SETUP_CONTRACT_INVALID"
    }
}

Write-Host "H3E1D2_SETUP_CONTRACT=PASS"

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h3e1d2_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

$compileArgs = @(
    "compile",
    "-b", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $buildPath,
    "--build-property", "compiler.cpp.extra_flags=$diagDefine",
    $sketchPath
)

Write-Host "H3E1D2_COMPILE_START=YES"

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    [object[]]$compileOutput = @(& $arduinoCli @compileArgs 2>&1)
    $compileExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

$compileOutput | ForEach-Object { $_.ToString() } | Set-Content -LiteralPath $compileLog -Encoding UTF8
$compileText = ($compileOutput | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

Write-Host "H3E1D2_COMPILE_EXIT=$compileExit"
Write-Host "H3E1D2_COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    $compileOutput | Select-Object -Last 100 | ForEach-Object { Write-Host $_ }
    throw "H3E1D2_COMPILE_FAILED"
}

$tftResolved =
    $compileText.Contains("TFT_eSPI") -and
    $compileText.Contains("2.5.43")

$displayObjectCount = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.FullName -match 'JWPLC_Display' -or
            $_.Name -match 'JWPLC_Display'
        }
).Count

$globalPeripheralEvidence =
    $compileText.Contains("JWPLC_GlobalPeripherals")

Write-Host "H3E1D2_TFT_ESPI_RESOLUTION_PROOF=$(
    if ($tftResolved) { 'PASS' } else { 'FAIL' }
)"
Write-Host "H3E1D2_DISPLAY_BUILD_ARTIFACT_COUNT=$displayObjectCount"
Write-Host "H3E1D2_DISPLAY_AUTOLOAD_SUPPRESSED=$(
    if ($displayObjectCount -eq 0) { 'PASS' } else { 'FAIL' }
)"
Write-Host "H3E1D2_GLOBAL_PERIPHERALS_EVIDENCE=$(
    if ($globalPeripheralEvidence) { 'PASS' } else { 'REVIEW' }
)"

if (-not $tftResolved) {
    throw "H3E1D2_TFT_ESPI_RESOLUTION_NOT_PROVEN"
}
if ($displayObjectCount -ne 0) {
    throw "H3E1D2_DISPLAY_AUTOLOAD_NOT_SUPPRESSED"
}
if (-not $globalPeripheralEvidence) {
    throw "H3E1D2_GLOBAL_PERIPHERALS_NOT_PROVEN"
}

Write-Host "H3E1D2_COMPILE_LINK=PASS"

if ($PreflightOnly) {
    Write-Host "H3E1D2_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E1D2_PREFLIGHT_PHYSICAL_OBSERVATION=NO"
    Write-Host "A14_H3E1D2_TFT_ESPI_PHYSICAL_INIT_PREFLIGHT=PASS"
    return
}

$uploadArgs = @(
    "upload",
    "-p", $MasterPort,
    "-b", $fqbn,
    "--input-dir", $buildPath,
    $sketchPath
)

Write-Host "H3E1D2_UPLOAD_START=YES"
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    [object[]]$uploadOutput = @(& $arduinoCli @uploadArgs 2>&1)
    $uploadExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

$uploadOutput | ForEach-Object { Write-Host $_ }
Write-Host "H3E1D2_UPLOAD_EXIT=$uploadExit"

if ($uploadExit -ne 0) {
    throw "H3E1D2_UPLOAD_FAILED"
}

Write-Host ""
Write-Host "Observe la TFT del Master antes de responder."
Write-Host "Debe verse en landscape 320x170 con marco blanco completo,"
Write-Host "esquinas TL/TR/BL/BR y barras centrales R-G-B."
Write-Host ""

function Read-H3E1D2YesNo {
    param([string]$PromptText)

    while ($true) {
        $answer = (Read-Host $PromptText).Trim().ToUpperInvariant()
        if ($answer -eq "S" -or $answer -eq "N") {
            return $answer
        }
        Write-Host "Responda S o N."
    }
}

$orientation = Read-H3E1D2YesNo "¿Texto horizontal y orientacion landscape correctos? (S/N)"
$border = Read-H3E1D2YesNo "¿Marco blanco completo y cuatro esquinas sin recorte/desfase? (S/N)"
$colors = Read-H3E1D2YesNo "¿Barras centrales se ven ROJO - VERDE - AZUL en ese orden? (S/N)"
$labels = Read-H3E1D2YesNo "¿TL/TR/BL/BR corresponden a sus esquinas correctas? (S/N)"
$stable = Read-H3E1D2YesNo "¿Pantalla estable, sin flicker/cortes/reinicios visibles? (S/N)"

Write-Host "H3E1D2_PHYSICAL_ORIENTATION=$orientation"
Write-Host "H3E1D2_PHYSICAL_BORDER_OFFSETS=$border"
Write-Host "H3E1D2_PHYSICAL_RGB=$colors"
Write-Host "H3E1D2_PHYSICAL_CORNER_LABELS=$labels"
Write-Host "H3E1D2_PHYSICAL_STABILITY=$stable"

if (@($orientation, $border, $colors, $labels, $stable) -contains "N") {
    throw "H3E1D2_PHYSICAL_OBSERVATION_FAILED"
}

[string[]]$finalDirty = @(Get-G2TrackedDirtyPaths)
[string[]]$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)

if ($normalizedFinalDirty.Count -ne 1 -or $normalizedFinalDirty[0] -ne $expectedCoreDirty) {
    throw "H3E1D2_FINAL_DIRTY_SCOPE_INVALID"
}
if ($finalStaged.Count -ne 0) {
    throw "H3E1D2_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E1D2_RUNTIME_BACKEND=TFT_eSPI_2.5.43"
Write-Host "H3E1D2_LOGICAL_GEOMETRY_EXPECTED=320x170"
Write-Host "H3E1D2_SPI=80MHz_MODE0"
Write-Host "H3E1D2_DISPLAY_AUTOLOAD_SUPPRESSED=PASS"
Write-Host "H3E1D2_NORMAL_AUTOLOAD_POLICY=UNCHANGED"
Write-Host "A14_H3E1D2_TFT_ESPI_PHYSICAL_INIT_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E1D3_FULL_RUNTIME_AB"
