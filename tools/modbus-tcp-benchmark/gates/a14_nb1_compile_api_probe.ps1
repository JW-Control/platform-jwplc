param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$expectedHeaderSha256 = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
$expectedCppSha256 = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
$expectedG3FirmwareSha256 = "08E16E60F008371416859B63F93D7C8B7D2FB88325FB7281AC6C5C72983FEA8F"

$headerRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$cppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"

function Invoke-NB1NativeToLog {
    param(
        [Parameter(Mandatory = $true)] [string]$FilePath,
        [Parameter(Mandatory = $true)] [string[]]$Arguments,
        [Parameter(Mandatory = $true)] [string]$LogPath
    )

    & $FilePath @Arguments *> $LogPath
    return [int]$LASTEXITCODE
}

function Assert-NB1RepoEthernetUsed {
    param(
        [Parameter(Mandatory = $true)] [string]$LogPath,
        [Parameter(Mandatory = $true)] [string]$ExpectedLibraryPath,
        [Parameter(Mandatory = $true)] [string]$Label
    )

    $text = [System.IO.File]::ReadAllText($LogPath)
    $used = $text.IndexOf(
        $ExpectedLibraryPath,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0

    Write-Host ("{0}_REPO_ETHERNET_LIBRARY_USED={1}" -f $Label, $used)
    if (-not $used) {
        throw ("A14_NB1_{0}_DID_NOT_USE_REPO_ETHERNET_LIBRARY" -f $Label)
    }
}

Write-Host "============================================================"
Write-Host " A14 NB1-B - COMPILE + ASYNC API PROBE"
Write-Host "============================================================"

Assert-G2Branch

$head = Get-G2Head
$effectiveHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$expectedDirty = @(
    $script:G2SpiHeaderRelative,
    $script:G2RawFirmwareRelative,
    $headerRelative,
    $cppRelative
) | Sort-Object

Write-Host "HEAD=$head"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "EXPECTED_SPI_HZ=26000000"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }

if ($effectiveHz -ne 26000000) {
    throw "A14_NB1_SPI_FREQUENCY_MISMATCH=$effectiveHz"
}

if ($dirty.Count -ne $expectedDirty.Count) {
    throw "A14_NB1_DIRTY_COUNT_INVALID=$($dirty.Count)"
}
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB1_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0) {
    throw "A14_NB1_CACHED_DIFF_FAILED"
}
Write-Host "STAGED_COUNT=$($staged.Count)"
if ($staged.Count -ne 0) {
    throw "A14_NB1_INDEX_NOT_CLEAN"
}

& git -C $script:G2RepoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw "A14_NB1_DIFF_CHECK_FAILED"
}

Assert-G2ProtectedArtifacts

$headerHash = Get-G2Sha256 $headerRelative
$cppHash = Get-G2Sha256 $cppRelative
$firmwareHash = Get-G2Sha256 $script:G2RawFirmwareRelative
$runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative

Write-Host "HEADER_SHA256=$headerHash"
Write-Host "CPP_SHA256=$cppHash"
Write-Host "G3_FIRMWARE_SHA256=$firmwareHash"
Write-Host "RAW_RUNNER_SHA256=$runnerHash"

if ($headerHash -ne $expectedHeaderSha256) {
    throw "A14_NB1_HEADER_HASH_MISMATCH"
}
if ($cppHash -ne $expectedCppSha256) {
    throw "A14_NB1_CPP_HASH_MISMATCH"
}
if ($firmwareHash -ne $expectedG3FirmwareSha256) {
    throw "A14_NB1_G3_FIRMWARE_HASH_MISMATCH"
}
if ($runnerHash -ne $script:G2RawRunnerSha256) {
    throw "A14_NB1_RUNNER_HASH_MISMATCH"
}

$headerText = [System.IO.File]::ReadAllText((Get-G2Path $headerRelative))
$cppText = [System.IO.File]::ReadAllText((Get-G2Path $cppRelative))

foreach ($marker in @(
    "beginStopAsync",
    "pollStopAsync",
    "stopAsyncInProgress",
    "cancelStopAsync",
    "beginFlushAsync",
    "pollFlushAsync",
    "flushAsyncInProgress",
    "cancelFlushAsync"
)) {
    $headerCount = ([regex]::Matches($headerText, [regex]::Escape($marker))).Count
    $cppCount = ([regex]::Matches($cppText, [regex]::Escape("EthernetClient::$marker"))).Count
    Write-Host "MARKER=$marker HEADER_COUNT=$headerCount CPP_COUNT=$cppCount"
    if ($headerCount -ne 1 -or $cppCount -ne 1) {
        throw "A14_NB1_MARKER_COUNT_INVALID=$marker"
    }
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB1_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$rawFirmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$rawSketchDir = Split-Path -Parent $rawFirmwarePath

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb1_compile_probe_{0}" -f $timestamp)
$rawBuildPath = Join-Path $tempRoot "raw_build"
$probeSketchDir = Join-Path $tempRoot "nb1_api_probe"
$probeBuildPath = Join-Path $tempRoot "probe_build"
$rawCompileLog = Join-Path $tempRoot "raw_compile.log"
$probeCompileLog = Join-Path $tempRoot "probe_compile.log"

New-Item -ItemType Directory -Force -Path $rawBuildPath | Out-Null
New-Item -ItemType Directory -Force -Path $probeSketchDir | Out-Null
New-Item -ItemType Directory -Force -Path $probeBuildPath | Out-Null

$probeSketchPath = Join-Path $probeSketchDir "nb1_api_probe.ino"
$probeSketch = @'
#include <JWPLC_Ethernet.h>

EthernetClient nb1Client;
volatile int nb1Sink = 0;

static void compileOnlyAsyncLifecycleProbe()
{
    nb1Sink += nb1Client.beginStopAsync();
    nb1Sink += nb1Client.pollStopAsync();
    nb1Sink += nb1Client.stopAsyncInProgress() ? 1 : 0;
    nb1Client.cancelStopAsync();

    nb1Sink += nb1Client.beginFlushAsync();
    nb1Sink += nb1Client.pollFlushAsync();
    nb1Sink += nb1Client.flushAsyncInProgress() ? 1 : 0;
    nb1Client.cancelFlushAsync();
}

void setup()
{
    if (false)
    {
        compileOnlyAsyncLifecycleProbe();
    }
}

void loop()
{
}
'@
$utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
[System.IO.File]::WriteAllText($probeSketchPath, $probeSketch, $utf8NoBom)

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "FQBN=$fqbn"
Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "SOURCE_MUTATION=NO"
Write-Host "UPLOAD=NO"

Write-Host ""
Write-Host "=== COMPILE RAW G3 FIRMWARE WITH NB1 LIBRARY ==="
$rawCompileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $rawBuildPath,
    "--libraries", $librariesRoot,
    $rawSketchDir
)
$rawCompileExit = Invoke-NB1NativeToLog -FilePath $arduinoCli -Arguments $rawCompileArgs -LogPath $rawCompileLog
Write-Host "RAW_COMPILE_EXIT=$rawCompileExit"
Write-Host "RAW_COMPILE_LOG=$rawCompileLog"
if ($rawCompileExit -ne 0) {
    Get-Content -LiteralPath $rawCompileLog -Tail 100 | ForEach-Object { Write-Host $_ }
    throw "A14_NB1_RAW_COMPILE_FAILED"
}
Assert-NB1RepoEthernetUsed -LogPath $rawCompileLog -ExpectedLibraryPath $expectedEthernetLibrary -Label "RAW"
$rawBinCount = @(Get-ChildItem -LiteralPath $rawBuildPath -Recurse -File -Filter "*.bin").Count
Write-Host "RAW_BIN_COUNT=$rawBinCount"
if ($rawBinCount -lt 1) {
    throw "A14_NB1_RAW_COMPILE_NO_BIN"
}

Write-Host ""
Write-Host "=== COMPILE ASYNC API PROBE ==="
$probeCompileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $probeBuildPath,
    "--libraries", $librariesRoot,
    $probeSketchDir
)
$probeCompileExit = Invoke-NB1NativeToLog -FilePath $arduinoCli -Arguments $probeCompileArgs -LogPath $probeCompileLog
Write-Host "API_PROBE_COMPILE_EXIT=$probeCompileExit"
Write-Host "API_PROBE_COMPILE_LOG=$probeCompileLog"
if ($probeCompileExit -ne 0) {
    Get-Content -LiteralPath $probeCompileLog -Tail 100 | ForEach-Object { Write-Host $_ }
    throw "A14_NB1_API_PROBE_COMPILE_FAILED"
}
Assert-NB1RepoEthernetUsed -LogPath $probeCompileLog -ExpectedLibraryPath $expectedEthernetLibrary -Label "API_PROBE"
$probeBinCount = @(Get-ChildItem -LiteralPath $probeBuildPath -Recurse -File -Filter "*.bin").Count
Write-Host "API_PROBE_BIN_COUNT=$probeBinCount"
if ($probeBinCount -lt 1) {
    throw "A14_NB1_API_PROBE_NO_BIN"
}

Write-Host ""
Write-Host "=== FINAL INVARIANTS ==="
Assert-G2ProtectedArtifacts

$headerHashAfter = Get-G2Sha256 $headerRelative
$cppHashAfter = Get-G2Sha256 $cppRelative
$firmwareHashAfter = Get-G2Sha256 $script:G2RawFirmwareRelative
$runnerHashAfter = Get-G2Sha256 $script:G2RawRunnerRelative

Write-Host "HEADER_SHA256_FINAL=$headerHashAfter"
Write-Host "CPP_SHA256_FINAL=$cppHashAfter"
Write-Host "G3_FIRMWARE_SHA256_FINAL=$firmwareHashAfter"
Write-Host "RAW_RUNNER_SHA256_FINAL=$runnerHashAfter"

if ($headerHashAfter -ne $expectedHeaderSha256 -or
    $cppHashAfter -ne $expectedCppSha256 -or
    $firmwareHashAfter -ne $expectedG3FirmwareSha256 -or
    $runnerHashAfter -ne $script:G2RawRunnerSha256) {
    throw "A14_NB1_SOURCE_CHANGED_DURING_COMPILE"
}

$dirtyAfter = @(Get-G2TrackedDirtyPaths)
if ($dirtyAfter.Count -ne $expectedDirty.Count) {
    throw "A14_NB1_DIRTY_COUNT_CHANGED=$($dirtyAfter.Count)"
}
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyAfter[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB1_DIRTY_PATH_CHANGED=$($dirtyAfter[$i])"
    }
}

$stagedAfter = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedAfter.Count -ne 0) {
    throw "A14_NB1_INDEX_CHANGED"
}

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyAfter.Count)"
Write-Host "STAGED_COUNT_FINAL=0"
Write-Host "NB1_RAW_COMPILE=PASS"
Write-Host "NB1_ASYNC_API_COMPILE=PASS"
Write-Host "NB1_UPLOAD=NO"
Write-Host "A14_NB1_COMPILE_API_PROBE=PASS"
