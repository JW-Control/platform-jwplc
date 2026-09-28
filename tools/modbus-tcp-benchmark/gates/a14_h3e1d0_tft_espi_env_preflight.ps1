Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.1D.0 - TFT_eSPI ENVIRONMENT PREFLIGHT"
Write-Host "============================================================"

Assert-G2Branch

$expectedTftHz = 80000000
$expectedTftEsPiVersion = [Version]"2.5.43"
$expectedTftEsPiSource = "ARDUINO_LIBRARY_MANAGER_RELEASE"

$spiHeaderRelative = "JWPLC/2.1.0/cores/jwcontrol/peripherals/include/jwplc_spi_bus.h"
$displayHeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_Display.h"
$displayApiRelative = "JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_Display_API.h"
$tftEsPiBundledRelative = "JWPLC/2.1.0/libraries/TFT_eSPI"

$spiHeaderPath = Get-G2Path $spiHeaderRelative
$displayHeaderPath = Get-G2Path $displayHeaderRelative
$displayApiPath = Get-G2Path $displayApiRelative
$tftEsPiBundledPath = Get-G2Path $tftEsPiBundledRelative

foreach ($required in @($spiHeaderPath, $displayHeaderPath, $displayApiPath)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E1D0_REQUIRED_PATH_MISSING=$required"
    }
}

[string[]]$dirty = @(Get-G2TrackedDirtyPaths)
[string[]]$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
[string[]]$normalizedDirty = @(
    $dirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)
$expectedCoreDirty = $script:G2CoreRelative.Replace("\", "/")

if ($normalizedDirty.Count -ne 1 -or $normalizedDirty[0] -ne $expectedCoreDirty) {
    $normalizedDirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "H3E1D0_DIRTY_SCOPE_INVALID"
}
if ($staged.Count -ne 0) {
    throw "H3E1D0_INDEX_NOT_CLEAN"
}

$spiHeaderText = [IO.File]::ReadAllText($spiHeaderPath)
$spiMatch = [regex]::Match(
    $spiHeaderText,
    '(?m)^[ \t]*#define[ \t]+JWPLC_SPI_TFT_HZ[ \t]+(\d+)UL[ \t]*(?=\r?$)'
)
if (-not $spiMatch.Success) {
    throw "H3E1D0_TFT_SPI_DEFINE_MISSING"
}
$actualTftHz = [int]$spiMatch.Groups[1].Value
if ($actualTftHz -ne $expectedTftHz) {
    throw "H3E1D0_TFT_SPI_NOT_80MHZ"
}

$displayHeaderText = [IO.File]::ReadAllText($displayHeaderPath)
$displayApiText = [IO.File]::ReadAllText($displayApiPath)

$publicAdafruitInclude =
    $displayHeaderText.Contains("#include <Adafruit_ST7789.h>")
$publicDisplayReturn =
    $displayHeaderText.Contains("Adafruit_ST7789 &display();")
$publicTftReference =
    $displayApiText.Contains("Adafruit_ST7789 &tft()")

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E1D0_TFT_SPI_HZ=$actualTftHz"
Write-Host "H3E1D0_PUBLIC_ADAFRUIT_INCLUDE=$publicAdafruitInclude"
Write-Host "H3E1D0_PUBLIC_DISPLAY_RETURNS_ADAFRUIT=$publicDisplayReturn"
Write-Host "H3E1D0_PUBLIC_TFT_RETURNS_ADAFRUIT=$publicTftReference"

if (-not $publicAdafruitInclude -or -not $publicDisplayReturn -or -not $publicTftReference) {
    throw "H3E1D0_PUBLIC_API_CONTRACT_CHANGED"
}

$bundledTftEsPi = Test-Path -LiteralPath $tftEsPiBundledPath
Write-Host "H3E1D0_TFT_ESPI_BUNDLED_IN_PACKAGE=$(
    if ($bundledTftEsPi) { 'YES' } else { 'NO' }
)"

if ($bundledTftEsPi) {
    throw "H3E1D0_UNEXPECTED_BUNDLED_TFT_ESPI"
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "H3E1D0_ARDUINO_CLI_MISSING"
}

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    [object[]]$libList = @(& $arduinoCli lib list 2>&1)
    $libListExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "H3E1D0_ARDUINO_LIB_LIST_EXIT=$libListExit"
if ($libListExit -ne 0) {
    throw "H3E1D0_ARDUINO_LIB_LIST_FAILED"
}

$tftLine = @(
    $libList |
        ForEach-Object { $_.ToString() } |
        Where-Object { $_ -match '(^|\s)TFT_eSPI(\s|$)' }
) | Select-Object -First 1

if ($null -eq $tftLine) {
    Write-Host "H3E1D0_TFT_ESPI_FOUND=NO"
    Write-Host "H3E1D0_TFT_ESPI_EXPECTED_VERSION=$expectedTftEsPiVersion"
Write-Host "H3E1D0_TFT_ESPI_EXPECTED_SOURCE=$expectedTftEsPiSource"
    throw "H3E1D0_TFT_ESPI_NOT_INSTALLED"
}

$versionMatch = [regex]::Match(
    $tftLine,
    '(?<!\d)(\d+\.\d+\.\d+)(?!\d)'
)
if (-not $versionMatch.Success) {
    Write-Host "H3E1D0_TFT_ESPI_LINE=$tftLine"
    throw "H3E1D0_TFT_ESPI_VERSION_PARSE_FAILED"
}

$actualVersion = [Version]$versionMatch.Groups[1].Value
$pinMatch = ($actualVersion -eq $expectedTftEsPiVersion)

Write-Host "H3E1D0_TFT_ESPI_FOUND=YES"
Write-Host "H3E1D0_TFT_ESPI_VERSION=$actualVersion"
Write-Host "H3E1D0_TFT_ESPI_EXPECTED_VERSION=$expectedTftEsPiVersion"
Write-Host "H3E1D0_TFT_ESPI_VERSION_PIN_MATCH=$(
    if ($pinMatch) { 'YES' } else { 'NO' }
)"
Write-Host "H3E1D0_TFT_ESPI_LIB_LINE=$tftLine"

if (-not $pinMatch) {
    throw "H3E1D0_TFT_ESPI_VERSION_PIN_MISMATCH"
}

Write-Host "H3E1D0_MUTATES_REPOSITORY=NO"
Write-Host "H3E1D0_COMPILES=NO"
Write-Host "H3E1D0_UPLOADS=NO"
Write-Host "H3E1D0_RUNTIME_WINDOW=NO"
Write-Host "A14_H3E1D0_TFT_ESPI_ENV_PREFLIGHT=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E1D_RENDERER_POC"
