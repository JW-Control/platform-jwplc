param(
    [string]$SerialPort = "COM14",
    [string]$DutIp = "192.168.0.31",
    [int]$NoReadMs = 750,
    [int]$ReceiveBuffer = 4096
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 NB1-D2 - FLUSH ASYNC PHYSICAL DIAGNOSTIC"
Write-Host "============================================================"

Assert-G2Branch

if ($NoReadMs -lt 100 -or $NoReadMs -ge 950) {
    throw "A14_NB1D2_NO_READ_MS_OUT_OF_RANGE=$NoReadMs"
}
if ($ReceiveBuffer -lt 1024) {
    throw "A14_NB1D2_RECEIVE_BUFFER_TOO_SMALL=$ReceiveBuffer"
}

$headerRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$cppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$firmwareRelative = $script:G2RawFirmwareRelative
$probeSketchRelative = "tools/modbus-tcp-benchmark/firmware/a14_nb1_flush_async_probe/a14_nb1_flush_async_probe.ino"
$clientRelative = "tools/modbus-tcp-benchmark/gates/a14_nb1_flush_async_physical_client.py"

$expectedHeaderSha256 = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
$expectedCppSha256 = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
$expectedFirmwareSha256 = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
$hardHoldMaxUs = 10000L
$preferredPollHoldMaxUs = 5000L
$minObservedFlushMs = 100L
$connectionTimeoutMs = 1000L

$dirty = @(Get-G2TrackedDirtyPaths)
$expectedDirty = @(
    $headerRelative,
    $cppRelative,
    $script:G2SpiHeaderRelative,
    $firmwareRelative
) | Sort-Object

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "EXPECTED_SPI_HZ=26000000"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "DUT_IP=$DutIp"
Write-Host "NO_READ_MS=$NoReadMs"
Write-Host "RECEIVE_BUFFER_REQUESTED=$ReceiveBuffer"
Write-Host "CONNECTION_TIMEOUT_MS=$connectionTimeoutMs"
Write-Host "MIN_OBSERVED_FLUSH_MS=$minObservedFlushMs"
Write-Host "PREFERRED_POLL_HOLD_MAX_US=$preferredPollHoldMaxUs"
Write-Host "HARD_HOLD_MAX_US=$hardHoldMaxUs"

if ((Get-G2SpiHz) -ne 26000000) {
    throw "A14_NB1D2_SPI_FREQUENCY_MISMATCH"
}

Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }
if ($dirty.Count -ne $expectedDirty.Count) {
    throw "A14_NB1D2_DIRTY_COUNT_INVALID=$($dirty.Count)"
}
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB1D2_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
    throw "A14_NB1D2_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts

$headerHash = Get-G2Sha256 $headerRelative
$cppHash = Get-G2Sha256 $cppRelative
$firmwareHash = Get-G2Sha256 $firmwareRelative
$runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
Write-Host "HEADER_SHA256=$headerHash"
Write-Host "CPP_SHA256=$cppHash"
Write-Host "G3_FIRMWARE_SHA256=$firmwareHash"
Write-Host "RAW_RUNNER_SHA256=$runnerHash"

if ($headerHash -ne $expectedHeaderSha256) { throw "A14_NB1D2_HEADER_HASH_MISMATCH" }
if ($cppHash -ne $expectedCppSha256) { throw "A14_NB1D2_CPP_HASH_MISMATCH" }
if ($firmwareHash -ne $expectedFirmwareSha256) { throw "A14_NB1D2_FIRMWARE_HASH_MISMATCH" }
if ($runnerHash -ne $script:G2RawRunnerSha256) { throw "A14_NB1D2_RUNNER_HASH_MISMATCH" }

$probeSketchPath = Get-G2Path $probeSketchRelative
$probeSketchDir = Split-Path -Parent $probeSketchPath
$clientPath = Get-G2Path $clientRelative
if (-not (Test-Path -LiteralPath $probeSketchPath)) {
    throw "A14_NB1D2_PROBE_SKETCH_NOT_FOUND"
}
if (-not (Test-Path -LiteralPath $clientPath)) {
    throw "A14_NB1D2_CLIENT_NOT_FOUND"
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB1D2_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "A14_NB1D2_PYTHON_NOT_FOUND"
}
$pythonExe = $pythonCommand.Source

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb1_flush_physical_{0}" -f $timestamp)
$buildDir = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"
$clientLog = Join-Path $tempRoot "client.log"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"
Write-Host "FQBN=$fqbn"
Write-Host "PROBE_SKETCH=$probeSketchPath"
Write-Host "CLIENT=$clientPath"
Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "SOURCE_MUTATION=NO"

Write-Host ""
Write-Host "=== COMPILE FLUSH ASYNC PHYSICAL PROBE ==="
$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildDir,
    "--libraries", $librariesRoot,
    $probeSketchDir
)
& $arduinoCli @compileArgs *> $compileLog
$compileExit = [int]$LASTEXITCODE
Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"
if ($compileExit -ne 0) {
    Get-Content -LiteralPath $compileLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB1D2_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$repoEthernetUsed = (
    $compileText.IndexOf($expectedEthernetLibrary, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
)
Write-Host "REPO_ETHERNET_LIBRARY_USED=$repoEthernetUsed"
if (-not $repoEthernetUsed) {
    throw "A14_NB1D2_REPO_ETHERNET_NOT_USED"
}

$binCount = @(Get-ChildItem -LiteralPath $buildDir -Recurse -File -Filter "*.bin").Count
Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) {
    throw "A14_NB1D2_NO_BIN_OUTPUT"
}

Write-Host ""
Write-Host "=== UPLOAD FLUSH ASYNC PHYSICAL PROBE ==="
$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildDir,
    $probeSketchDir
)
& $arduinoCli @uploadArgs *> $uploadLog
$uploadExit = [int]$LASTEXITCODE
Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"
if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB1D2_UPLOAD_FAILED"
}

Start-Sleep -Milliseconds 600

Write-Host ""
Write-Host "=== RUN SLOW-READER CLIENT / CAPTURE SERIAL ==="
$clientArgs = @(
    $clientPath,
    "--host", $DutIp,
    "--port", "5003",
    "--serial", $SerialPort,
    "--no-read-ms", $NoReadMs.ToString([System.Globalization.CultureInfo]::InvariantCulture),
    "--recv-buffer", $ReceiveBuffer.ToString([System.Globalization.CultureInfo]::InvariantCulture),
    "--ready-timeout-s", "15",
    "--result-timeout-s", "8"
)
& $pythonExe @clientArgs *> $clientLog
$clientExit = [int]$LASTEXITCODE
Write-Host "CLIENT_EXIT=$clientExit"
Write-Host "CLIENT_LOG=$clientLog"
Get-Content -LiteralPath $clientLog | ForEach-Object { Write-Host $_ }
if ($clientExit -ne 0) {
    throw "A14_NB1D2_CLIENT_FAILED=$clientExit"
}

$logText = [System.IO.File]::ReadAllText($clientLog)

function Get-LogValue {
    param([string]$Key)
    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($logText, $pattern))
    if ($matches.Count -ne 1) {
        throw "A14_NB1D2_LOG_KEY_COUNT_${Key}=$($matches.Count)"
    }
    return $matches[0].Groups[1].Value.Trim()
}

function Get-LogInt64 {
    param([string]$Key)
    return [int64]::Parse(
        (Get-LogValue -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

$clientPass = Get-LogValue -Key "NB1_FLUSH_CLIENT_PASS"
$resultCode = Get-LogInt64 -Key "RESULT_CODE"
$probeFailed = Get-LogValue -Key "PROBE_FAILED"
$pendingObserved = Get-LogValue -Key "FLUSH_PENDING_OBSERVED"
$prefillBytes = Get-LogInt64 -Key "TX_COMPLETED_BYTES_BEFORE_FLUSH"
$flushDurationMs = Get-LogInt64 -Key "FLUSH_DURATION_MS"
$flushPollCount = Get-LogInt64 -Key "FLUSH_POLL_COUNT"
$flushPollHoldMaxUs = Get-LogInt64 -Key "FLUSH_POLL_HOLD_MAX_US"
$serviceHoldMaxUs = Get-LogInt64 -Key "SERVICE_SPI_HOLD_MAX_US"
$spiLockErrors = Get-LogInt64 -Key "SPI_LOCK_ERRORS"
$loopGapMaxUs = Get-LogInt64 -Key "LOOP_GAP_MAX_US"
$clientRxBytes = Get-LogInt64 -Key "CLIENT_RX_BYTES"

Write-Host ""
Write-Host "=== NB1-D2 PHYSICAL SUMMARY ==="
Write-Host "RESULT_CODE=$resultCode"
Write-Host "PROBE_FAILED=$probeFailed"
Write-Host "FLUSH_PENDING_OBSERVED=$pendingObserved"
Write-Host "TX_COMPLETED_BYTES_BEFORE_FLUSH=$prefillBytes"
Write-Host "FLUSH_DURATION_MS=$flushDurationMs"
Write-Host "FLUSH_POLL_COUNT=$flushPollCount"
Write-Host "FLUSH_POLL_HOLD_MAX_US=$flushPollHoldMaxUs"
Write-Host "SERVICE_SPI_HOLD_MAX_US=$serviceHoldMaxUs"
Write-Host "SPI_LOCK_ERRORS=$spiLockErrors"
Write-Host "LOOP_GAP_MAX_US=$loopGapMaxUs"
Write-Host "CLIENT_RX_BYTES=$clientRxBytes"

if ($clientPass -ne "YES") { throw "A14_NB1D2_CLIENT_PASS_MISSING" }
if ($resultCode -ne 1 -or $probeFailed -ne "NO") { throw "A14_NB1D2_PROBE_FUNCTIONAL_FAIL" }
if ($pendingObserved -ne "YES") { throw "A14_NB1D2_PENDING_NOT_OBSERVED" }
if ($prefillBytes -lt 4096) { throw "A14_NB1D2_PREFILL_TOO_SMALL=$prefillBytes" }
if ($flushDurationMs -lt $minObservedFlushMs) { throw "A14_NB1D2_FLUSH_DURATION_TOO_SHORT=$flushDurationMs" }
if ($flushDurationMs -ge $connectionTimeoutMs) { throw "A14_NB1D2_FLUSH_TIMEOUT_OR_TOO_LONG=$flushDurationMs" }
if ($flushPollCount -lt 2) { throw "A14_NB1D2_FLUSH_POLL_COUNT_TOO_SMALL=$flushPollCount" }
if ($flushPollHoldMaxUs -gt $preferredPollHoldMaxUs) { throw "A14_NB1D2_FLUSH_POLL_HOLD_TOO_HIGH=$flushPollHoldMaxUs" }
if ($serviceHoldMaxUs -gt $hardHoldMaxUs) { throw "A14_NB1D2_SERVICE_HOLD_TOO_HIGH=$serviceHoldMaxUs" }
if ($spiLockErrors -ne 0) { throw "A14_NB1D2_SPI_LOCK_ERRORS=$spiLockErrors" }
if ($loopGapMaxUs -gt $hardHoldMaxUs) { throw "A14_NB1D2_LOOP_GAP_TOO_HIGH=$loopGapMaxUs" }
if ($clientRxBytes -le 0) { throw "A14_NB1D2_CLIENT_RX_EMPTY" }

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante NB1-D2."
$visualAnswer = ""
while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante NB1-D2? (S/N)').Trim().ToUpperInvariant()
}
$visualEvents = $(if ($visualAnswer -eq "S") { 1 } else { 0 })
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"
if ($visualEvents -ne 0) {
    throw "A14_NB1D2_VISUAL_SPI_FAIL"
}

Assert-G2ProtectedArtifacts
if ((Get-G2Sha256 $headerRelative) -ne $expectedHeaderSha256) { throw "A14_NB1D2_HEADER_CHANGED" }
if ((Get-G2Sha256 $cppRelative) -ne $expectedCppSha256) { throw "A14_NB1D2_CPP_CHANGED" }
if ((Get-G2Sha256 $firmwareRelative) -ne $expectedFirmwareSha256) { throw "A14_NB1D2_FIRMWARE_CHANGED" }
if ((Get-G2Sha256 $script:G2RawRunnerRelative) -ne $script:G2RawRunnerSha256) { throw "A14_NB1D2_RUNNER_CHANGED" }

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)
Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"
if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB1D2_FINAL_WORKTREE_INVALID"
}
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB1D2_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

Write-Host "NB1_FLUSH_ASYNC_BACKPRESSURE=REPRODUCED"
Write-Host "NB1_FLUSH_ASYNC_COOPERATIVE=PASS"
Write-Host "NB1_FLUSH_TIMEOUT_MS=1000_PRESERVED"
Write-Host "NB1_VISUAL_SPI=PASS"
Write-Host "A14_NB1_FLUSH_ASYNC_PHYSICAL_DIAG=PASS"
