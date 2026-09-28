param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.3A - JWPLC_Display SOURCE -> JWPLC_TFT"
Write-Host "============================================================"

Assert-G2Branch

$cli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$fqbn = "jwplc_local:esp32:jwplcbasic"
$diag = "-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1"
$displayRoot = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Display"
$repoLibraries = Get-G2Path "JWPLC/2.1.0/libraries"
$sketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_h3e3a_display_jwplc_tft_compile_probe"

foreach ($p in @($cli, $displayRoot, $repoLibraries, $sketch)) {
    if (-not (Test-Path -LiteralPath $p)) {
        throw "H3E3A_REQUIRED_PATH_MISSING=$p"
    }
}

[string[]]$dirty = @(Get-G2TrackedDirtyPaths)
[string[]]$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
$expectedCore = $script:G2CoreRelative.Replace("\", "/")
[string[]]$normalizedDirty = @(
    $dirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)

if ($normalizedDirty.Count -ne 1 -or $normalizedDirty[0] -ne $expectedCore) {
    $normalizedDirty | ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }
    throw "H3E3A_ENTRY_DIRTY_SCOPE_INVALID"
}
if ($staged.Count -ne 0) {
    throw "H3E3A_ENTRY_INDEX_NOT_CLEAN"
}

$contractFiles = @(
    "src/JWPLC_Display.h",
    "src/JWPLC_Display_API.h",
    "src/JWPLC_IdleScreen.h",
    "src/JWPLC_IdleScreen.cpp",
    "src/JWPLC_UI.h",
    "src/JWPLC_UI.cpp",
    "src/JWPLC_UI_API.cpp",
    "src/JWPLC_UI_Pages.h",
    "src/JWPLC_UI_Pages.cpp",
    "src/JWPLC_UI_PixelMap.h",
    "src/JWPLC_UI_PixelMap.cpp",
    "src/JWPLC_UI_RuntimeHooks.h",
    "src/JWPLC_Display.cpp"
)

$sourceText = (
    $contractFiles |
        ForEach-Object {
            [IO.File]::ReadAllText(
                (Join-Path $displayRoot $_))
        }
) -join [Environment]::NewLine

$legacySt = ([regex]::Matches($sourceText, "Adafruit_ST7789")).Count
$legacyGfx = ([regex]::Matches($sourceText, "Adafruit_GFX")).Count
$legacyColor = ([regex]::Matches($sourceText, "ST77XX_")).Count
$newType = ([regex]::Matches($sourceText, "JWPLC_TFTClass")).Count
$phantomHeader = ([regex]::Matches($sourceText, "JWPLC_TFTClass\.h")).Count

Write-Host "H3E3A_SOURCE_ADAFRUIT_ST7789_COUNT=$legacySt"
Write-Host "H3E3A_SOURCE_ADAFRUIT_GFX_COUNT=$legacyGfx"
Write-Host "H3E3A_SOURCE_ST77XX_COUNT=$legacyColor"
Write-Host "H3E3A_SOURCE_JWPLC_TFT_TYPE_COUNT=$newType"
Write-Host "H3E3A_SOURCE_PHANTOM_TFT_HEADER_COUNT=$phantomHeader"

if ($legacySt -ne 0 -or
    $legacyGfx -ne 0 -or
    $legacyColor -ne 0 -or
    $phantomHeader -ne 0) {
    throw "H3E3A_SOURCE_LEGACY_OR_PHANTOM_BACKEND_REMAINS"
}
if ($newType -lt 1) {
    throw "H3E3A_SOURCE_JWPLC_TFT_MISSING"
}

$api = [IO.File]::ReadAllText(
    (Join-Path $displayRoot "src\JWPLC_Display_API.h"))

foreach ($required in @(
    "JWPLC_TFTClass &tft();",
    "JWPLC_TFTClass &display();",
    "bool setFields(",
    "void setUserPageCount(",
    "void setUserRefreshMode("
)) {
    if (-not $api.Contains($required)) {
        Write-Host "H3E3A_API_CONTRACT_MISSING=$required"
        throw "H3E3A_PUBLIC_API_CONTRACT_FAILED"
    }
}

Write-Host "H3E3A_PUBLIC_API_CONTRACT=PASS"

$root = Join-Path $env:TEMP (
    "jwplc_a14_h3e3a_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$tempDisplay = Join-Path $root "JWPLC_Display"
$build = Join-Path $root "build"
$log = Join-Path $root "compile.log"

Copy-Item -LiteralPath $displayRoot -Destination $tempDisplay -Recurse -Force
New-Item -ItemType Directory -Force -Path $build | Out-Null

[object[]]$archives = @(
    Get-ChildItem -LiteralPath $tempDisplay -Recurse -File -Filter "*.a" -ErrorAction SilentlyContinue
)
foreach ($a in $archives) {
    Remove-Item -LiteralPath $a.FullName -Force
}

$props = @'
name=JWPLC_Display
version=1.0.1-h3e3a
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=H3E3A source qualification.
paragraph=Temporary source-only Display over JWPLC_TFT.
category=Display
architectures=esp32
includes=JWPLC_Display.h
dot_a_linkage=true
depends=JWPLC_TFT,JW_MatrixButtons,JWPLC_GlobalPeripherals
'@

[IO.File]::WriteAllText(
    (Join-Path $tempDisplay "library.properties"),
    $props,
    (New-Object Text.UTF8Encoding($false)))

$args = @(
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

Write-Host "H3E3A_OFFICIAL_DISPLAY_ARCHIVE_MUTATION=NO"
Write-Host "H3E3A_UPLOADS=NO"
Write-Host "H3E3A_COMPILE_START=YES"

$oldPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    [object[]]$output = @(& $cli @args 2>&1)
    $exitCode = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $oldPreference
}

$output |
    ForEach-Object { $_.ToString() } |
    Set-Content -LiteralPath $log -Encoding UTF8

Write-Host "H3E3A_COMPILE_EXIT=$exitCode"
Write-Host "H3E3A_COMPILE_LOG=$log"

if ($exitCode -ne 0) {
    $output | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }
    throw "H3E3A_COMPILE_FAILED"
}

$normalizedTemp = [IO.Path]::GetFullPath($tempDisplay).TrimEnd('\', '/')
[string[]]$displayLines = @(
    $output |
        ForEach-Object { $_.ToString() } |
        Where-Object {
            $_ -match '^Using library JWPLC_Display at version .+ in folder: (.+)$'
        }
)

$displaySelected = $false
foreach ($line in $displayLines) {
    if ($line -match '^Using library JWPLC_Display at version .+ in folder: (.+)$') {
        $selected = [IO.Path]::GetFullPath(
            $Matches[1].Trim()).TrimEnd('\', '/')
        if ($selected -ieq $normalizedTemp) {
            $displaySelected = $true
        }
    }
}

Write-Host "H3E3A_TEMP_DISPLAY_SOURCE_SELECTED=$(
    if ($displaySelected) { 'PASS' } else { 'FAIL' })"

if (-not $displaySelected) {
    throw "H3E3A_TEMP_DISPLAY_NOT_SELECTED"
}

[string[]]$legacySelections = @(
    $output |
        ForEach-Object { $_.ToString() } |
        Where-Object {
            $_ -match '^Using library (Adafruit ST7735 and ST7789 Library|Adafruit GFX Library|Adafruit BusIO|TFT_eSPI) at version '
        }
)

Write-Host "H3E3A_EXTERNAL_GRAPHICS_BACKEND_SELECTION_COUNT=$($legacySelections.Count)"
if ($legacySelections.Count -ne 0) {
    $legacySelections | ForEach-Object { Write-Host "H3E3A_UNEXPECTED_BACKEND=$_" }
    throw "H3E3A_EXTERNAL_GRAPHICS_BACKEND_SELECTED"
}

$db = Join-Path $build "compile_commands.json"
if (-not (Test-Path -LiteralPath $db)) {
    throw "H3E3A_COMPILE_DB_MISSING"
}

[object[]]$entries = @(
    Get-Content -LiteralPath $db -Raw | ConvertFrom-Json
)
[string[]]$tuFiles = @(
    foreach ($entry in $entries) {
        $f = [string]$entry.file
        if (-not [string]::IsNullOrWhiteSpace($f)) {
            $f.Trim().Trim('"').Replace([char]92, [char]47)
        }
    }
)

[int]$displayTus = @(
    $tuFiles |
        Where-Object { $_ -match '/JWPLC_Display/src/.+\.cpp$' }
).Count
[int]$jwplcTftTus = @(
    $tuFiles |
        Where-Object {
            $_.EndsWith("/JWPLC_TFT.cpp", [StringComparison]::OrdinalIgnoreCase)
        }
).Count
[int]$tftEspiTus = @(
    $tuFiles |
        Where-Object {
            $_.EndsWith("/TFT_eSPI.cpp", [StringComparison]::OrdinalIgnoreCase)
        }
).Count

Write-Host "H3E3A_COMPILE_DB_ENTRY_COUNT=$($entries.Count)"
Write-Host "H3E3A_DISPLAY_SOURCE_TU_COUNT=$displayTus"
Write-Host "H3E3A_JWPLC_TFT_SOURCE_TU_COUNT=$jwplcTftTus"
Write-Host "H3E3A_TFT_ESPI_SOURCE_TU_COUNT=$tftEspiTus"

if ($displayTus -lt 6) {
    throw "H3E3A_DISPLAY_SOURCE_TU_COUNT_INVALID"
}
if ($jwplcTftTus -ne 0 -or $tftEspiTus -ne 0) {
    throw "H3E3A_JWPLC_TFT_PRECOMPILED_POLICY_FAILED"
}

$outputText = ($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
$jwplcTftSelected = $outputText.Contains("Using library JWPLC_TFT")
$jwplcTftPrecompiled = @(
    $output |
        ForEach-Object { $_.ToString() } |
        Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match 'JWPLC_TFT')
        }
).Count -gt 0

Write-Host "H3E3A_JWPLC_TFT_SELECTED=$(
    if ($jwplcTftSelected) { 'PASS' } else { 'FAIL' })"
Write-Host "H3E3A_JWPLC_TFT_PRECOMPILED=$(
    if ($jwplcTftPrecompiled) { 'PASS' } else { 'FAIL' })"

if (-not $jwplcTftSelected -or -not $jwplcTftPrecompiled) {
    throw "H3E3A_JWPLC_TFT_LINKAGE_NOT_PROVEN"
}

[string[]]$finalDirty = @(Get-G2TrackedDirtyPaths)
[string[]]$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
[string[]]$normalizedFinal = @(
    $finalDirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)

if ($normalizedFinal.Count -ne 1 -or $normalizedFinal[0] -ne $expectedCore) {
    throw "H3E3A_FINAL_DIRTY_SCOPE_INVALID"
}
if ($finalStaged.Count -ne 0) {
    throw "H3E3A_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E3A_DISPLAY_SOURCE_BACKEND=JWPLC_TFT"
Write-Host "H3E3A_ADAFRUIT_PUBLIC_TYPE=REMOVED"
Write-Host "H3E3A_HMI_HIGH_LEVEL_API=PRESERVED"
Write-Host "H3E3A_REPOSITORY_MUTATION=NO"
Write-Host "H3E3A_UPLOADS=NO"
Write-Host "A14_H3E3A_DISPLAY_SOURCE_MIGRATION_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E3B_DISPLAY_RUNTIME"
