param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-NativeToLog {
    param([string]$FilePath, [string[]]$Arguments, [string]$LogPath)
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $FilePath @Arguments *> $LogPath
        return [int]$LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
}

function Get-SubstringCount {
    param([string]$Text, [string]$Needle)
    $count = 0
    $offset = 0
    while ($true) {
        $index = $Text.IndexOf($Needle, $offset, [System.StringComparison]::Ordinal)
        if ($index -lt 0) { break }
        $count++
        $offset = $index + $Needle.Length
    }
    return $count
}

function Assert-ExactCount {
    param([string]$Text, [string]$Needle, [int]$Expected, [string]$Label)
    $count = Get-SubstringCount -Text $Text -Needle $Needle
    Write-Host "$Label=$count"
    if ($count -ne $Expected) { throw "$Label EXPECTED=$Expected ACTUAL=$count" }
}

Write-Host "============================================================"
Write-Host " A14 P5-A - MASTER + SLAVE HMI ON-DEMAND COMPILE"
Write-Host "============================================================"

Assert-G2Branch
$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0) { throw "P5A_CACHED_DIFF_FAILED" }

Write-Host "HEAD=$head"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "UPLOAD=NO"
Write-Host "MASTER_ROLE=RTU_MASTER_PLUS_MODBUS_TCP_FULL_RUNTIME"
Write-Host "SLAVE_ROLE=RTU_SLAVE_ID_2"
Write-Host "DISPLAY_API=HMI_ON_DEMAND_DIRTY"

if ($spiHz -ne 26000000) { throw "P5A_EXPECTED_26MHZ" }
if ($dirty.Count -ne 0) { throw "P5A_TRACKED_TREE_NOT_CLEAN" }
if ($staged.Count -ne 0) { throw "P5A_INDEX_NOT_CLEAN" }
Assert-G2ProtectedArtifacts

$masterDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master"
$slaveDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave"
$masterSketch = Join-Path $masterDir "a14_p5_full_runtime_master.ino"
$slaveSketch = Join-Path $slaveDir "a14_p5_rtu_slave.ino"

foreach ($required in @($masterSketch, $slaveSketch)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "P5A_REQUIRED_SKETCH_MISSING=$required" }
}

$masterText = [System.IO.File]::ReadAllText($masterSketch)
$slaveText = [System.IO.File]::ReadAllText($slaveSketch)

Write-Host ""
Write-Host "=== SOURCE CONTRACT MASTER ==="
Assert-ExactCount -Text $masterText -Needle "void setup()" -Expected 1 -Label "MASTER_SETUP_COUNT"
Assert-ExactCount -Text $masterText -Needle "void loop()" -Expected 1 -Label "MASTER_LOOP_COUNT"
Assert-ExactCount -Text $masterText -Needle "static void printSnapshot()" -Expected 1 -Label "MASTER_SNAPSHOT_FUNCTION_COUNT"
Assert-ExactCount -Text $masterText -Needle "A14_PERF_SNAPSHOT=END" -Expected 1 -Label "MASTER_SNAPSHOT_END_COUNT"
Assert-ExactCount -Text $masterText -Needle "JWPLC_Display.setUserRefreshMode(" -Expected 1 -Label "MASTER_REFRESH_SETTER_COUNT"
Assert-ExactCount -Text $masterText -Needle "USER_REFRESH_ON_DEMAND" -Expected 2 -Label "MASTER_ON_DEMAND_TOKEN_COUNT"
Assert-ExactCount -Text $masterText -Needle "JWPLC_Display.setFields(" -Expected 1 -Label "MASTER_SET_FIELDS_COUNT"
Assert-ExactCount -Text $masterText -Needle "struct RuntimeStats" -Expected 1 -Label "MASTER_RUNTIME_STATS_COUNT"
Assert-ExactCount -Text $masterText -Needle "static RuntimeStats runtimeStats" -Expected 1 -Label "MASTER_RUNTIME_STATS_INSTANCE_COUNT"
Assert-ExactCount -Text $masterText -Needle "static uint32_t lastIoSampleMs" -Expected 1 -Label "MASTER_SCHEDULER_STATE_COUNT"
Assert-ExactCount -Text $masterText -Needle "static void updateMaxU32(" -Expected 1 -Label "MASTER_UPDATE_MAX_COUNT"
Assert-ExactCount -Text $masterText -Needle "static void serviceRealisticWorkload()" -Expected 1 -Label "MASTER_REALISTIC_WORKLOAD_COUNT"
Assert-ExactCount -Text $masterText -Needle "PERIPHERAL_FAILURE_COUNT=" -Expected 1 -Label "MASTER_PERIPHERAL_FAILURE_MARKER_COUNT"
Assert-ExactCount -Text $masterText -Needle "JWPLC_ModbusRTU.requestReadHoldingRegisters(" -Expected 1 -Label "MASTER_COOPERATIVE_FC03_COUNT"
Assert-ExactCount -Text $masterText -Needle "RTU_TARGET_SLAVE_ID = 2" -Expected 1 -Label "MASTER_TARGET_SLAVE2_COUNT"
Assert-ExactCount -Text $masterText -Needle "RTU_PERIOD_MS = 20UL" -Expected 1 -Label "MASTER_RTU_20MS_COUNT"

$masterLegacyCount = 0
foreach ($needle in @("jwplcUserDisplayEnterCallback","jwplcUserDisplayRefreshCallback","jwplcUserDisplayRefreshNeededCallback")) {
    if ($masterText.Contains($needle)) { $masterLegacyCount++ }
}
$masterFillScreen = $masterText.Contains(".fillScreen(")
$masterSyncRead = $masterText.Contains("JWPLC_ModbusRTU.readHoldingRegisters(")
Write-Host "MASTER_LEGACY_DISPLAY_SYMBOLS=$masterLegacyCount"
Write-Host "MASTER_DIRECT_FILL_SCREEN=$masterFillScreen"
Write-Host "MASTER_BLOCKING_RTU_READ=$masterSyncRead"
if ($masterLegacyCount -ne 0) { throw "P5A_MASTER_LEGACY_DISPLAY_REMAINS" }
if ($masterFillScreen) { throw "P5A_MASTER_DIRECT_FILL_SCREEN_REMAINS" }
if ($masterSyncRead) { throw "P5A_MASTER_BLOCKING_RTU_PATH_REMAINS" }
Write-Host "MASTER_SOURCE_CONTRACT=PASS"

Write-Host ""
Write-Host "=== SOURCE CONTRACT SLAVE ==="
Assert-ExactCount -Text $slaveText -Needle "void setup()" -Expected 1 -Label "SLAVE_SETUP_COUNT"
Assert-ExactCount -Text $slaveText -Needle "void loop()" -Expected 1 -Label "SLAVE_LOOP_COUNT"
Assert-ExactCount -Text $slaveText -Needle "JWPLC_Display.setUserRefreshMode(" -Expected 1 -Label "SLAVE_REFRESH_SETTER_COUNT"
Assert-ExactCount -Text $slaveText -Needle "USER_REFRESH_ON_DEMAND" -Expected 2 -Label "SLAVE_ON_DEMAND_TOKEN_COUNT"
Assert-ExactCount -Text $slaveText -Needle "JWPLC_Display.setFields(" -Expected 1 -Label "SLAVE_SET_FIELDS_COUNT"
Assert-ExactCount -Text $slaveText -Needle "SLAVE_ID = 2" -Expected 1 -Label "SLAVE_ID2_COUNT"
Assert-ExactCount -Text $slaveText -Needle "VERIFY_MAGIC = 0x55AA" -Expected 1 -Label "SLAVE_VERIFY_MAGIC_COUNT"

$slaveLegacyCount = 0
foreach ($needle in @("jwplcUserDisplayEnterCallback","jwplcUserDisplayRefreshCallback","jwplcUserDisplayRefreshNeededCallback")) {
    if ($slaveText.Contains($needle)) { $slaveLegacyCount++ }
}
$slaveFillScreen = $slaveText.Contains(".fillScreen(")
Write-Host "SLAVE_LEGACY_DISPLAY_SYMBOLS=$slaveLegacyCount"
Write-Host "SLAVE_DIRECT_FILL_SCREEN=$slaveFillScreen"
if ($slaveLegacyCount -ne 0) { throw "P5A_SLAVE_LEGACY_DISPLAY_REMAINS" }
if ($slaveFillScreen) { throw "P5A_SLAVE_DIRECT_FILL_SCREEN_REMAINS" }
Write-Host "SLAVE_SOURCE_CONTRACT=PASS"

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) { throw "P5A_ARDUINO_CLI_NOT_FOUND" }
$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$fqbn = "jwplc_local:esp32:jwplcbasic"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p5a_hmi_compile_{0}" -f $timestamp)
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
Write-Host ""
Write-Host "TEMP_ROOT=$tempRoot"

$cases = @(
    [PSCustomObject]@{ Name = "MASTER"; Dir = $masterDir },
    [PSCustomObject]@{ Name = "SLAVE"; Dir = $slaveDir }
)

foreach ($case in $cases) {
    $name = $case.Name
    $buildPath = Join-Path $tempRoot ("build_" + $name.ToLowerInvariant())
    $logPath = Join-Path $tempRoot ("compile_" + $name.ToLowerInvariant() + ".log")
    New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
    Write-Host ""
    Write-Host "=== COMPILE $name ==="
    $args = @("compile","--fqbn",$fqbn,"--build-path",$buildPath,"--libraries",$repoLibrariesRoot,$case.Dir)
    $exitCode = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $args -LogPath $logPath
    Write-Host "${name}_COMPILE_EXIT=$exitCode"
    Write-Host "${name}_COMPILE_LOG=$logPath"
    if ($exitCode -ne 0) {
        Invoke-G2CompileFinishedSound -Success $false
        Get-Content -LiteralPath $logPath -Tail 240 | ForEach-Object { Write-Host $_ }
        throw "P5A_${name}_COMPILE_FAILED"
    }
    $binCount = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin").Count
    Write-Host "${name}_BIN_COUNT=$binCount"
    if ($binCount -lt 1) { throw "P5A_${name}_BIN_MISSING" }
}

Invoke-G2CompileFinishedSound -Success $true

Write-Host ""
Write-Host "=== FINAL INVARIANTS ==="
Assert-G2ProtectedArtifacts
$finalHz = Get-G2SpiHz
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
Write-Host "FINAL_SPI_HZ=$finalHz"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"
if ($finalHz -ne 26000000) { throw "P5A_FINAL_SPI_CHANGED" }
if ($finalDirty.Count -ne 0) { throw "P5A_FINAL_TREE_DIRTY" }
if ($finalStaged.Count -ne 0) { throw "P5A_FINAL_INDEX_DIRTY" }

Write-Host ""
Write-Host "A14_P5A_MASTER_HMI=USER_REFRESH_ON_DEMAND"
Write-Host "A14_P5A_SLAVE_HMI=USER_REFRESH_ON_DEMAND"
Write-Host "A14_P5A_MASTER_RTU=COOPERATIVE_50HZ_TARGET"
Write-Host "A14_P5A_SLAVE_ID=2"
Write-Host "A14_P5A_UPLOAD=NO"
Write-Host "A14_P5A_MASTER_SLAVE_HMI_COMPILE=PASS"
Write-Host "NEXT=P5B_PHYSICAL_MASTER_SLAVE_COMBINED"
