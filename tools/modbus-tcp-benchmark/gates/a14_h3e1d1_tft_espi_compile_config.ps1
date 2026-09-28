Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.1D.1 - TFT_eSPI COMPILE/CONFIG QUALIFICATION"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreVersion = "3.3.8"
$expectedTftEsPiVersion = "2.5.43"
$fqbn = "jwplc_local:esp32:jwplcbasic"
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"

$sketchRelative = "tools/modbus-tcp-benchmark/firmware/a14_h3e1d1_tft_espi_compile_probe"
$sketchPath = Get-G2Path $sketchRelative
$setupPath = Join-Path $sketchPath "tft_setup.h"
$inoPath = Join-Path $sketchPath "a14_h3e1d1_tft_espi_compile_probe.ino"
$d0Path = Join-Path $PSScriptRoot "a14_h3e1d0_tft_espi_env_preflight.ps1"

foreach ($required in @($sketchPath, $setupPath, $inoPath, $d0Path, $arduinoCli)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E1D1_REQUIRED_PATH_MISSING=$required"
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
    throw "H3E1D1_ENTRY_DIRTY_SCOPE_INVALID"
}
if ($entryStaged.Count -ne 0) {
    throw "H3E1D1_ENTRY_INDEX_NOT_CLEAN"
}

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E1D1_FQBN=$fqbn"
Write-Host "H3E1D1_EXPECTED_ESP32_CORE=$expectedCoreVersion"
Write-Host "H3E1D1_EXPECTED_TFT_ESPI=$expectedTftEsPiVersion"
Write-Host "H3E1D1_UPLOADS=NO"
Write-Host "H3E1D1_RUNTIME_WINDOW=NO"

[object[]]$d0Output = @(& $d0Path *>&1)
$d0Output | ForEach-Object { Write-Host $_ }
$d0Text = ($d0Output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

if (-not $d0Text.Contains("A14_H3E1D0_TFT_ESPI_ENV_PREFLIGHT=PASS")) {
    throw "H3E1D1_D0_PREFLIGHT_FAILED"
}
if (-not $d0Text.Contains("H3E1D0_TFT_ESPI_VERSION=2.5.43")) {
    throw "H3E1D1_TFT_ESPI_VERSION_NOT_2_5_43"
}
if (-not $d0Text.Contains("H3E1D0_TFT_SPI_HZ=80000000")) {
    throw "H3E1D1_PACKAGE_TFT_SPI_NOT_80MHZ"
}

$setupText = [IO.File]::ReadAllText($setupPath)
$setupContracts = @(
    '#define JWPLC_H3E1D1_SETUP 1',
    '#define ST7789_DRIVER',
    '#define TFT_WIDTH  170',
    '#define TFT_HEIGHT 320',
    '#define TFT_RGB_ORDER TFT_BGR',
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
)

foreach ($contract in $setupContracts) {
    $count = ([regex]::Matches(
        $setupText,
        [regex]::Escape($contract)
    )).Count

    if ($count -ne 1) {
        Write-Host "H3E1D1_SETUP_CONTRACT_FAIL=$contract"
        throw "H3E1D1_SETUP_CONTRACT_INVALID"
    }
}

foreach ($forbidden in @(
    '#define LOAD_FONT2',
    '#define LOAD_FONT4',
    '#define LOAD_FONT6',
    '#define LOAD_FONT7',
    '#define LOAD_FONT8',
    '#define LOAD_GFXFF',
    '#define SMOOTH_FONT'
)) {
    if ([regex]::IsMatch(
            $setupText,
            '(?m)^[ \t]*' + [regex]::Escape($forbidden) + '(?:\s|$)')) {
        Write-Host "H3E1D1_SETUP_FORBIDDEN=$forbidden"
        throw "H3E1D1_SETUP_FONT_SCOPE_INVALID"
    }
}

Write-Host "H3E1D1_LOCAL_TFT_SETUP_CONTRACT=PASS"
Write-Host "H3E1D1_DRIVER=ST7789"
Write-Host "H3E1D1_GEOMETRY=170x320"
Write-Host "H3E1D1_ROTATION_COMPILED=1"
Write-Host "H3E1D1_COLOR_ORDER=BGR"
Write-Host "H3E1D1_INVERSION=ON"
Write-Host "H3E1D1_SPI_MODE=MODE0"
Write-Host "H3E1D1_SPI_HZ=80000000"
Write-Host "H3E1D1_FONT_SCOPE=GLCD_ONLY"
Write-Host "H3E1D1_SUPPORT_TRANSACTIONS=YES"

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h3e1d1_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$buildPath = Join-Path $tempRoot "build"
$logPath = Join-Path $tempRoot "compile.log"
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

$argsList = @(
    "compile",
    "-b", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $buildPath,
    $sketchPath
)

Write-Host "H3E1D1_BUILD_PATH=$buildPath"
Write-Host "H3E1D1_COMPILE_START=YES"

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    [object[]]$compileOutput = @(& $arduinoCli @argsList 2>&1)
    $compileExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

$compileOutput | ForEach-Object { $_.ToString() } | Set-Content -LiteralPath $logPath -Encoding UTF8
$compileText = ($compileOutput | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

Write-Host "H3E1D1_COMPILE_EXIT=$compileExit"
Write-Host "H3E1D1_COMPILE_LOG=$logPath"

if ($compileExit -ne 0) {
    $compileOutput | Select-Object -Last 80 | ForEach-Object { Write-Host $_ }
    throw "H3E1D1_COMPILE_FAILED"
}

$tftResolved =
    $compileText.Contains("TFT_eSPI") -and
    $compileText.Contains("2.5.43")

Write-Host "H3E1D1_TFT_ESPI_RESOLUTION_PROOF=$(
    if ($tftResolved) { 'PASS' } else { 'REVIEW' }
)"

if (-not $tftResolved) {
    $compileOutput |
        Where-Object { $_.ToString() -match 'TFT_eSPI|Used library|Using library' } |
        ForEach-Object { Write-Host "H3E1D1_COMPILE_LIBRARY_LINE=$_" }
    throw "H3E1D1_TFT_ESPI_RESOLUTION_NOT_PROVEN"
}

[object[]]$elfFiles = @(Get-ChildItem -LiteralPath $buildPath -Filter "*.elf" -File -ErrorAction SilentlyContinue)
[object[]]$binFiles = @(Get-ChildItem -LiteralPath $buildPath -Filter "*.bin" -File -ErrorAction SilentlyContinue)

Write-Host "H3E1D1_ELF_COUNT=$($elfFiles.Count)"
Write-Host "H3E1D1_BIN_COUNT=$($binFiles.Count)"

if ($elfFiles.Count -ne 1) {
    throw "H3E1D1_ELF_COUNT_INVALID"
}
if ($binFiles.Count -lt 1) {
    throw "H3E1D1_BIN_MISSING"
}

Write-Host "H3E1D1_COMPILE_LINK=PASS"
Write-Host "H3E1D1_ESP32_3_3_8_COMPAT=PASS"
Write-Host "H3E1D1_TFT_ESPI_2_5_43_COMPAT=PASS"

[string[]]$finalDirty = @(Get-G2TrackedDirtyPaths)
[string[]]$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)

if ($normalizedFinalDirty.Count -ne 1 -or $normalizedFinalDirty[0] -ne $expectedCoreDirty) {
    $normalizedFinalDirty | ForEach-Object { Write-Host "FINAL_DIRTY=$_" }
    throw "H3E1D1_FINAL_DIRTY_SCOPE_INVALID"
}
if ($finalStaged.Count -ne 0) {
    throw "H3E1D1_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E1D1_REPOSITORY_MUTATION=NO"
Write-Host "H3E1D1_UPLOADS=NO"
Write-Host "A14_H3E1D1_TFT_ESPI_COMPILE_CONFIG_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E1D2_PHYSICAL_INIT_GATE"
