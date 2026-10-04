param(
    [string]$ArduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic",
    [string]$ResultRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-NativeToLog {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$LogPath
    )

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $FilePath @Arguments *> $LogPath
        return [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"
$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

if ($branch -ne $expectedBranch) {
    throw "A12_TFT_BRANCH_MISMATCH expected=$expectedBranch actual=$branch"
}

$dirty = @(& git -C $repo status --porcelain --untracked-files=no)
if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "A12_TFT_TRACKED_TREE_NOT_CLEAN"
}

& git -C $repo diff --check
if ($LASTEXITCODE -ne 0) {
    throw "A12_TFT_GIT_DIFF_CHECK_FAILED"
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\arduino-ide\resources\app\lib\backend\resources\arduino-cli.exe"),
        "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            $ArduinoCli = $candidate
            break
        }
    }
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    $cliCommand = Get-Command "arduino-cli" -ErrorAction SilentlyContinue
    if ($null -ne $cliCommand) {
        $ArduinoCli = $cliCommand.Source
    }
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "A12_TFT_ARDUINO_CLI_NOT_FOUND"
}

$libraries = Join-Path $repo "JWPLC\2.1.0\libraries"
if (-not (Test-Path -LiteralPath $libraries)) {
    throw "A12_TFT_LIBRARIES_NOT_FOUND"
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo "tools\alpha12\results\tft_backend_compile_$stamp"
}
New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$sketches = @(
    [pscustomobject]@{
        Label = "DISPLAY_TFT_DIRECT"
        Path = "JWPLC\2.1.0\libraries\JWPLC_Display\examples\04.Display_TFT_Direct"
        RequireLogicUI = $false
    },
    [pscustomobject]@{
        Label = "DISPLAY_USER_CALLBACKS"
        Path = "JWPLC\2.1.0\libraries\JWPLC_Display\examples\Display_UserUI_Callbacks"
        RequireLogicUI = $false
    },
    [pscustomobject]@{
        Label = "LOGIC_UI_HOME"
        Path = "JWPLC\2.1.0\libraries\JWPLC_LogicRuntime_UI\examples\JWPLC_LogicRuntime_UI_Home"
        RequireLogicUI = $true
    },
    [pscustomobject]@{
        Label = "LOGIC_UI_UNIFIED_FBD"
        Path = "JWPLC\2.1.0\libraries\JWPLC_LogicRuntime_UI\examples\JWPLC_LogicRuntime_UI_FBD_Unified_Map_Detail_RAM"
        RequireLogicUI = $true
    }
)

@(
    "DATE=$(Get-Date -Format o)"
    "BRANCH=$branch"
    "HEAD=$head"
    "FQBN=$Fqbn"
    "ARDUINO_CLI=$ArduinoCli"
    "LIBRARIES=$libraries"
    "SKETCH_COUNT=$($sketches.Count)"
) | Set-Content -LiteralPath (Join-Path $ResultRoot "MANIFEST.txt") -Encoding UTF8

Write-Host "=============================================================================="
Write-Host " ALPHA12 - TFT BACKEND SOURCE-FIRST COMPILE GATE"
Write-Host "=============================================================================="
Write-Host "BRANCH=$branch"
Write-Host "HEAD=$head"
Write-Host "FQBN=$Fqbn"
Write-Host "ARDUINO_CLI=$ArduinoCli"
Write-Host "RESULT_ROOT=$ResultRoot"
Write-Host "SKETCH_COUNT=$($sketches.Count)"

$passCount = 0

foreach ($entry in $sketches) {
    $sketchPath = Join-Path $repo $entry.Path
    if (-not (Test-Path -LiteralPath $sketchPath)) {
        throw "A12_TFT_SKETCH_NOT_FOUND_$($entry.Label)"
    }

    $buildPath = Join-Path $env:TEMP ("a12_tft_" + $entry.Label.ToLowerInvariant() + "_" + $stamp)
    New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

    $logPath = Join-Path $ResultRoot ("compile_" + $entry.Label.ToLowerInvariant() + ".log")

    Write-Host "CASE_BEGIN=$($entry.Label)"

    $exitCode = Invoke-NativeToLog $ArduinoCli @(
        "compile", "--verbose",
        "--fqbn", $Fqbn,
        "--build-path", $buildPath,
        "--libraries", $libraries,
        $sketchPath
    ) $logPath

    Write-Host "COMPILE_EXIT=$exitCode"

    if ($exitCode -ne 0) {
        Write-Host "=== COMPILE LOG TAIL: $($entry.Label) ==="
        Get-Content -LiteralPath $logPath -Tail 220 | ForEach-Object { Write-Host $_ }
        throw "A12_TFT_COMPILE_FAILED_$($entry.Label)"
    }

    $tftObjects = @(
        Get-ChildItem -LiteralPath $buildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JWPLC_TFT.cpp.o" }
    )
    $displayObjects = @(
        Get-ChildItem -LiteralPath $buildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JWPLC_Display.cpp.o" }
    )
    $logicUiObjects = @(
        Get-ChildItem -LiteralPath $buildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JWPLC_LogicRuntime_UI.cpp.o" }
    )
    $widgetObjects = @(
        Get-ChildItem -LiteralPath $buildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "RuntimeUIWidgets.cpp.o" }
    )

    $compileText = [IO.File]::ReadAllText($logPath)

    $precompiledTft = [regex]::IsMatch(
        $compileText,
        '(?im)using precompiled library .*JWPLC_TFT|usando .*precompilad.*JWPLC_TFT'
    )
    $precompiledDisplay = [regex]::IsMatch(
        $compileText,
        '(?im)using precompiled library .*JWPLC_Display|usando .*precompilad.*JWPLC_Display'
    )
    $precompiledLogicUi = [regex]::IsMatch(
        $compileText,
        '(?im)using precompiled library .*JWPLC_LogicRuntime_UI|usando .*precompilad.*JWPLC_LogicRuntime_UI'
    )

    Write-Host "TFT_SOURCE_OBJECT_COUNT=$($tftObjects.Count)"
    Write-Host "DISPLAY_SOURCE_OBJECT_COUNT=$($displayObjects.Count)"
    Write-Host "LOGIC_UI_SOURCE_OBJECT_COUNT=$($logicUiObjects.Count)"
    Write-Host "LOGIC_UI_WIDGET_OBJECT_COUNT=$($widgetObjects.Count)"
    Write-Host "TFT_PRECOMPILED_MARKER=$(if ($precompiledTft) { 'YES' } else { 'NO' })"
    Write-Host "DISPLAY_PRECOMPILED_MARKER=$(if ($precompiledDisplay) { 'YES' } else { 'NO' })"
    Write-Host "LOGIC_UI_PRECOMPILED_MARKER=$(if ($precompiledLogicUi) { 'YES' } else { 'NO' })"

    if ($tftObjects.Count -ne 1 -or
        $displayObjects.Count -ne 1 -or
        $precompiledTft -or
        $precompiledDisplay) {
        throw "A12_TFT_SOURCE_FIRST_NOT_PROVEN_$($entry.Label)"
    }

    if ($entry.RequireLogicUI) {
        if ($logicUiObjects.Count -ne 1 -or
            $widgetObjects.Count -ne 1 -or
            $precompiledLogicUi) {
            throw "A12_TFT_LOGIC_UI_SOURCE_FIRST_NOT_PROVEN_$($entry.Label)"
        }
    }

    $passCount++
    Write-Host "CASE_END=$($entry.Label) CLASS=PASS"
}

@(
    "ALPHA12_TFT_BACKEND_COMPILE=PASS"
    "BRANCH=$branch"
    "HEAD=$head"
    "PASS=$passCount"
    "FAIL=0"
    "SOURCE_FIRST=PASS"
    "PHYSICAL_UPLOAD_PERFORMED=NO"
    "RESULT_ROOT=$ResultRoot"
) | Set-Content -LiteralPath (Join-Path $ResultRoot "SUMMARY.txt") -Encoding UTF8

Write-Host "=============================================================================="
Write-Host "ALPHA12_TFT_BACKEND_COMPILE=PASS"
Write-Host "PASS=$passCount"
Write-Host "FAIL=0"
Write-Host "SOURCE_FIRST=PASS"
Write-Host "PHYSICAL_UPLOAD_PERFORMED=NO"
Write-Host "RESULT_ROOT=$ResultRoot"
