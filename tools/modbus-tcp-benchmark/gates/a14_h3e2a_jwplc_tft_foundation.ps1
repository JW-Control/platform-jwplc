param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.2A - JWPLC_TFT FOUNDATION COMPILE/API"
Write-Host "============================================================"

Assert-G2Branch

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$fqbn = "jwplc_local:esp32:jwplcbasic"
$diagDefine = "-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1"

$libraryRelative = "JWPLC/2.1.0/libraries/JWPLC_TFT"
$libraryRoot = Get-G2Path $libraryRelative
$headerPath = Join-Path $libraryRoot "src\JWPLC_TFT.h"
$cppPath = Join-Path $libraryRoot "src\JWPLC_TFT.cpp"
$backendSetupPath = Join-Path $libraryRoot "src\tft_setup.h"
$propertiesPath = Join-Path $libraryRoot "library.properties"

$sketchRelative = "tools/modbus-tcp-benchmark/firmware/a14_h3e2a_jwplc_tft_compile_probe"
$sketchPath = Get-G2Path $sketchRelative
$inoPath = Join-Path $sketchPath "a14_h3e2a_jwplc_tft_compile_probe.ino"
$sketchSetupPath = Join-Path $sketchPath "tft_setup.h"

$d0Path = Join-Path $PSScriptRoot "a14_h3e1d0_tft_espi_env_preflight.ps1"
$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"

foreach ($required in @(
    $arduinoCli,
    $libraryRoot,
    $headerPath,
    $cppPath,
    $backendSetupPath,
    $propertiesPath,
    $sketchPath,
    $inoPath,
    $d0Path
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E2A_REQUIRED_PATH_MISSING=$required"
    }
}

if (Test-Path -LiteralPath $sketchSetupPath) {
    throw "H3E2A_SKETCH_LOCAL_TFT_SETUP_FORBIDDEN"
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
    $normalizedDirty | ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }
    throw "H3E2A_ENTRY_DIRTY_SCOPE_INVALID"
}
if ($staged.Count -ne 0) {
    throw "H3E2A_ENTRY_INDEX_NOT_CLEAN"
}

$headerText = [IO.File]::ReadAllText($headerPath)
$cppText = [IO.File]::ReadAllText($cppPath)
$setupText = [IO.File]::ReadAllText($backendSetupPath)
$propertiesText = [IO.File]::ReadAllText($propertiesPath)
$inoText = [IO.File]::ReadAllText($inoPath)

$publicBackendLeak =
    $headerText.Contains("TFT_eSPI") -or
    $headerText.Contains("Adafruit_") -or
    $headerText.Contains("Adafruit ST")

Write-Host "H3E2A_PUBLIC_BACKEND_TYPE_LEAK=$(
    if ($publicBackendLeak) { 'YES' } else { 'NO' }
)"

if ($publicBackendLeak) {
    throw "H3E2A_PUBLIC_BACKEND_TYPE_LEAK"
}

$publicContracts = @(
    "class JWPLC_TFTClass : public Print",
    "extern JWPLC_TFTClass JWPLC_TFT;",
    "bool beginBatch(uint32_t timeoutMs = 50);",
    "void endBatch();",
    "bool fillRect(",
    "bool drawRect(",
    "bool drawFastHLine(",
    "bool drawFastVLine(",
    "void setCursor(int16_t x, int16_t y);",
    "void setTextSize(uint8_t size);",
    "void setTextColor(uint16_t foreground, uint16_t background);",
    "void getTextBounds(",
    "size_t write(uint8_t value) override;"
)

foreach ($contract in $publicContracts) {
    if (-not $headerText.Contains($contract)) {
        Write-Host "H3E2A_PUBLIC_CONTRACT_MISSING=$contract"
        throw "H3E2A_PUBLIC_API_CONTRACT_FAILED"
    }
}

Write-Host "H3E2A_PUBLIC_API_CONTRACT=PASS"

$backendContracts = @(
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

foreach ($contract in $backendContracts) {
    $count = ([regex]::Matches(
        $setupText,
        [regex]::Escape($contract))).Count

    if ($count -ne 1) {
        Write-Host "H3E2A_BACKEND_SETUP_CONTRACT_FAIL=$contract"
        throw "H3E2A_BACKEND_SETUP_CONTRACT_INVALID"
    }
}

if (-not $propertiesText.Contains("depends=TFT_eSPI")) {
    throw "H3E2A_LIBRARY_DEPENDENCY_CONTRACT_INVALID"
}

if (-not $cppText.Contains("#include <TFT_eSPI.h>")) {
    throw "H3E2A_PRIVATE_BACKEND_INCLUDE_MISSING"
}

if ($inoText.Contains("TFT_eSPI") -and
    -not $inoText.Contains("#ifdef _TFT_eSPIH_")) {
    throw "H3E2A_PROBE_DIRECT_BACKEND_REFERENCE"
}

Write-Host "H3E2A_BACKEND_SETUP_CONTRACT=PASS"
Write-Host "H3E2A_BACKEND_SCOPE=PRIVATE_CPP_ONLY"
Write-Host "H3E2A_ACTIVE_PANEL=ST7789_170X320"
Write-Host "H3E2A_LOGICAL_GEOMETRY=320x170"
Write-Host "H3E2A_ROTATION=1"
Write-Host "H3E2A_COLOR_ORDER=BGR"
Write-Host "H3E2A_SPI=80MHz_MODE0"
Write-Host "H3E2A_V3_TARGET=ST7789_240X320_RESERVED_NOT_ACTIVE"

[object[]]$d0Output = @(& $d0Path *>&1)
$d0Output | ForEach-Object { Write-Host $_ }
$d0Text =
    ($d0Output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

if (-not $d0Text.Contains("A14_H3E1D0_TFT_ESPI_ENV_PREFLIGHT=PASS")) {
    throw "H3E2A_D0_PREFLIGHT_FAILED"
}

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h3e2a_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
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
    "--libraries", $repoLibrariesRoot,
    "--build-property", "compiler.cpp.extra_flags=$diagDefine",
    $sketchPath
)

Write-Host "H3E2A_COMPILE_START=YES"
Write-Host "H3E2A_UPLOADS=NO"

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

Write-Host "H3E2A_COMPILE_EXIT=$compileExit"
Write-Host "H3E2A_COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    $compileOutput |
        Select-Object -Last 120 |
        ForEach-Object { Write-Host $_ }

    throw "H3E2A_COMPILE_FAILED"
}

$jwplcTftResolved =
    $compileText.Contains("JWPLC_TFT")

$tftEsPiResolved =
    $compileText.Contains("TFT_eSPI") -and
    $compileText.Contains("2.5.43")

Write-Host "H3E2A_JWPLC_TFT_RESOLUTION_PROOF=$(
    if ($jwplcTftResolved) { 'PASS' } else { 'FAIL' }
)"
Write-Host "H3E2A_TFT_ESPI_2_5_43_RESOLUTION_PROOF=$(
    if ($tftEsPiResolved) { 'PASS' } else { 'FAIL' }
)"

if (-not $jwplcTftResolved) {
    throw "H3E2A_JWPLC_TFT_RESOLUTION_NOT_PROVEN"
}
if (-not $tftEsPiResolved) {
    throw "H3E2A_TFT_ESPI_RESOLUTION_NOT_PROVEN"
}

[object[]]$jwplcTftObjects = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "JWPLC_TFT.cpp.o" }
)

[object[]]$tftEsPiObjects = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "TFT_eSPI.cpp.o" }
)

[object[]]$displayObjects = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "JWPLC_Display.cpp.o" }
)

Write-Host "H3E2A_JWPLC_TFT_OBJECT_COUNT=$($jwplcTftObjects.Count)"
Write-Host "H3E2A_TFT_ESPI_OBJECT_COUNT=$($tftEsPiObjects.Count)"
Write-Host "H3E2A_JWPLC_DISPLAY_OBJECT_COUNT=$($displayObjects.Count)"

if ($jwplcTftObjects.Count -ne 1) {
    throw "H3E2A_JWPLC_TFT_OBJECT_COUNT_INVALID"
}
if ($tftEsPiObjects.Count -lt 1) {
    throw "H3E2A_TFT_ESPI_OBJECT_MISSING"
}
if ($displayObjects.Count -ne 0) {
    throw "H3E2A_DISPLAY_AUTOLOAD_NOT_SUPPRESSED"
}

$compileDbPath = Join-Path $buildPath "compile_commands.json"
if (-not (Test-Path -LiteralPath $compileDbPath)) {
    throw "H3E2A_COMPILE_DB_MISSING"
}

$compileDbText = [IO.File]::ReadAllText($compileDbPath)

$privateSetupVisible =
    $compileDbText.Contains("JWPLC_TFT") -and
    $setupText.Contains("JWPLC_TFT_BACKEND_SETUP")

Write-Host "H3E2A_PRIVATE_SETUP_PRESENT=YES"
Write-Host "H3E2A_SKETCH_LOCAL_SETUP_PRESENT=NO"
Write-Host "H3E2A_PRIVATE_SETUP_DISCOVERY=$(
    if ($privateSetupVisible) { 'PASS' } else { 'REVIEW' }
)"

[string[]]$finalDirty = @(Get-G2TrackedDirtyPaths)
[string[]]$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)

if ($normalizedFinalDirty.Count -ne 1 -or $normalizedFinalDirty[0] -ne $expectedCoreDirty) {
    throw "H3E2A_FINAL_DIRTY_SCOPE_INVALID"
}
if ($finalStaged.Count -ne 0) {
    throw "H3E2A_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E2A_PUBLIC_BACKEND_TYPE_LEAK=NO"
Write-Host "H3E2A_COMPILE_LINK=PASS"
Write-Host "H3E2A_REPOSITORY_MUTATION=NO"
Write-Host "H3E2A_UPLOADS=NO"
Write-Host "A14_H3E2A_JWPLC_TFT_FOUNDATION_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E2B_BACKEND_PACKAGING"
