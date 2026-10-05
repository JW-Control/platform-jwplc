param(
    [string]$ArduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$libraries = Join-Path $repo "JWPLC\2.1.0\libraries"
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"
$resultRoot = Join-Path $env:TEMP ("jwplc_alpha12_autocontainment_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
$buildPath = Join-Path $resultRoot "build"
$logPath = Join-Path $resultRoot "compile.log"

function Invoke-Captured {
    param([string]$FilePath,[string[]]$Arguments)
    $old = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        [object[]]$out = @(& $FilePath @Arguments 2>&1)
        $code = [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $old
    }
    return [pscustomobject]@{
        ExitCode = $code
        Output = @($out | ForEach-Object { $_.ToString() })
    }
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

Write-Host "=============================================================================="
Write-Host " ALPHA12 - AUTOCONTAINMENT GATE"
Write-Host "=============================================================================="
Write-Host ("BRANCH=" + $branch)
Write-Host ("HEAD=" + $head)

if ($branch -ne $expectedBranch) {
    throw "A12_AUTOCONTAINMENT_BRANCH_MISMATCH"
}

[string[]]$dirty = @(& git -C $repo status --short)
Write-Host ("ENTRY_DIRTY_COUNT=" + [string]$dirty.Count)
if ($dirty.Count -ne 0) {
    throw "A12_AUTOCONTAINMENT_TREE_NOT_CLEAN"
}

$gfxPath = Join-Path $libraries "Adafruit_GFX_Library"
$stPath = Join-Path $libraries "Adafruit_ST7735_and_ST7789_Library"
$busioPath = Join-Path $libraries "Adafruit_BusIO"
$markerPath = Join-Path $busioPath "src\JWPLC_Bundled_Adafruit_BusIO.h"
$framHeader = Join-Path $libraries "JW_FRAM\src\JW_FRAM.h"

$gfxAbsent = -not (Test-Path -LiteralPath $gfxPath)
$stAbsent = -not (Test-Path -LiteralPath $stPath)
$busioPresent = Test-Path -LiteralPath $busioPath
$markerPresent = Test-Path -LiteralPath $markerPath

$framText = [IO.File]::ReadAllText($framHeader)
$markerIndex = $framText.IndexOf("#include <JWPLC_Bundled_Adafruit_BusIO.h>")
$spiIndex = $framText.IndexOf("#include <Adafruit_SPIDevice.h>")
$markerBeforeSpi = $markerIndex -ge 0 -and $spiIndex -gt $markerIndex

Write-Host ("LEGACY_GFX_ABSENT=" + [string]$gfxAbsent)
Write-Host ("LEGACY_ST77XX_ABSENT=" + [string]$stAbsent)
Write-Host ("BUNDLED_BUSIO_PRESENT=" + [string]$busioPresent)
Write-Host ("BUNDLED_BUSIO_MARKER_PRESENT=" + [string]$markerPresent)
Write-Host ("FRAM_BUNDLED_MARKER_BEFORE_SPI_DEVICE=" + [string]$markerBeforeSpi)

if (-not $gfxAbsent -or -not $stAbsent -or -not $busioPresent -or -not $markerPresent -or -not $markerBeforeSpi) {
    throw "A12_AUTOCONTAINMENT_STATIC_POLICY_FAILED"
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "A12_AUTOCONTAINMENT_ARDUINO_CLI_NOT_FOUND"
}

$sketch = Join-Path $repo "tools\build-speed-benchmark\sketches\06_alpha4_local_physical_gate"

New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null

$args = @(
    "compile",
    "--fqbn", $Fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $buildPath,
    "--libraries", $libraries,
    $sketch
)

$run = Invoke-Captured -FilePath $ArduinoCli -Arguments $args
$run.Output | Set-Content -LiteralPath $logPath -Encoding UTF8

Write-Host ("COMPILE_EXIT=" + [string]$run.ExitCode)

if ($run.ExitCode -ne 0) {
    $run.Output | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }
    throw "A12_AUTOCONTAINMENT_COMPILE_FAILED"
}

[object[]]$busioSelections = @(
    $run.Output | Where-Object {
        $_ -match '^Using library Adafruit BusIO at version .+ in folder: (.+)$' -or
        $_ -match '^Usando libreria Adafruit BusIO con version .+ en la carpeta: (.+)$' -or
        $_ -match '^Usando librería Adafruit BusIO con versión .+ en la carpeta: (.+)$'
    }
)

Write-Host ("BUSIO_SELECTION_COUNT=" + [string]$busioSelections.Count)
$busioSelections | ForEach-Object { Write-Host ("BUSIO_SELECTION=" + $_) }

if ($busioSelections.Count -ne 1) {
    throw "A12_AUTOCONTAINMENT_BUSIO_SELECTION_AMBIGUOUS"
}

$normalizedBundled = [IO.Path]::GetFullPath($busioPath).TrimEnd('\')
$selectedLine = $busioSelections[0].ToString()
$bundledSelected = $selectedLine.IndexOf($normalizedBundled,[StringComparison]::OrdinalIgnoreCase) -ge 0

$gfxSelected = @($run.Output | Where-Object { $_ -match 'Using library Adafruit GFX|Usando librer.*Adafruit GFX' }).Count -gt 0
$stSelected = @($run.Output | Where-Object { $_ -match 'Using library Adafruit ST7735|Usando librer.*Adafruit ST7735' }).Count -gt 0

Write-Host ("BUNDLED_BUSIO_SELECTED=" + [string]$bundledSelected)
Write-Host ("LEGACY_GFX_SELECTED=" + [string]$gfxSelected)
Write-Host ("LEGACY_ST77XX_SELECTED=" + [string]$stSelected)

if (-not $bundledSelected) {
    throw "A12_AUTOCONTAINMENT_EXTERNAL_BUSIO_SELECTED"
}

if ($gfxSelected -or $stSelected) {
    throw "A12_AUTOCONTAINMENT_LEGACY_GRAPHICS_SELECTED"
}

$warnings = @($run.Output | Where-Object { $_ -match '(?i)\bwarning:' }).Count
$errors = @($run.Output | Where-Object { $_ -match '(?i)\berror:' }).Count

Write-Host ("WARNING_LINES=" + [string]$warnings)
Write-Host ("ERROR_LINES=" + [string]$errors)

[string[]]$finalDirty = @(& git -C $repo status --short)
Write-Host ("FINAL_DIRTY_COUNT=" + [string]$finalDirty.Count)

if ($finalDirty.Count -ne 0) {
    throw "A12_AUTOCONTAINMENT_REPOSITORY_MUTATED"
}

Write-Host "ALPHA12_AUTOCONTAINMENT_GATE=PASS"
Write-Host ("RESULT_ROOT=" + $resultRoot)
