param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 NB1-D1 - FLUSH ASYNC COMPILE PROBE"
Write-Host "============================================================"

Assert-G2Branch

$headerRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$cppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$firmwareRelative = $script:G2RawFirmwareRelative

$expectedHeaderSha256 = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
$expectedCppSha256 = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
$expectedFirmwareSha256 = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"

$dirty = @(Get-G2TrackedDirtyPaths)
$expectedDirty = @(
    $headerRelative,
    $cppRelative,
    $script:G2SpiHeaderRelative,
    $firmwareRelative
) | Sort-Object

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }

if ($dirty.Count -ne $expectedDirty.Count) {
    throw "A14_NB1D1_DIRTY_COUNT_INVALID=$($dirty.Count)"
}
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB1D1_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
    throw "A14_NB1D1_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts

$headerHash = Get-G2Sha256 $headerRelative
$cppHash = Get-G2Sha256 $cppRelative
$firmwareHash = Get-G2Sha256 $firmwareRelative
$runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
Write-Host "HEADER_SHA256=$headerHash"
Write-Host "CPP_SHA256=$cppHash"
Write-Host "FIRMWARE_SHA256=$firmwareHash"
Write-Host "RAW_RUNNER_SHA256=$runnerHash"

if ($headerHash -ne $expectedHeaderSha256) { throw "A14_NB1D1_HEADER_HASH_MISMATCH" }
if ($cppHash -ne $expectedCppSha256) { throw "A14_NB1D1_CPP_HASH_MISMATCH" }
if ($firmwareHash -ne $expectedFirmwareSha256) { throw "A14_NB1D1_FIRMWARE_HASH_MISMATCH" }
if ($runnerHash -ne $script:G2RawRunnerSha256) { throw "A14_NB1D1_RUNNER_HASH_MISMATCH" }

$headerText = [System.IO.File]::ReadAllText((Get-G2Path $headerRelative))
$cppText = [System.IO.File]::ReadAllText((Get-G2Path $cppRelative))
foreach ($marker in @(
    "beginFlushAsync",
    "pollFlushAsync",
    "flushAsyncInProgress",
    "cancelFlushAsync",
    "beginStopAsync",
    "pollStopAsync"
)) {
    $headerCount = ([regex]::Matches($headerText, [regex]::Escape($marker))).Count
    $cppCount = ([regex]::Matches($cppText, [regex]::Escape("EthernetClient::$marker"))).Count
    Write-Host "MARKER=$marker HEADER_COUNT=$headerCount CPP_COUNT=$cppCount"
    if ($headerCount -ne 1 -or $cppCount -ne 1) {
        throw "A14_NB1D1_MARKER_COUNT_INVALID=$marker"
    }
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB1D1_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb1_flush_compile_{0}" -f $timestamp)
$sketchDir = Join-Path $tempRoot "a14_nb1_flush_async_probe"
$buildDir = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
New-Item -ItemType Directory -Force -Path $sketchDir | Out-Null
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$sketchPath = Join-Path $sketchDir "a14_nb1_flush_async_probe.ino"
$sketch = @'
#include <JWPLC_Ethernet.h>
#include <jwplc_ethernet_async_tx.h>
#include <jwplc_spi_bus.h>

static constexpr uint16_t PROBE_PORT = 5003;
static constexpr size_t TX_CHUNK_BYTES = 1024;
static constexpr uint32_t BACKPRESSURE_CONFIRM_MS = 50;
static constexpr uint32_t MIN_PREFILL_BYTES = 4096;

EthernetServer probeServer(PROBE_PORT);
EthernetClient probeClient;
JWPLC_EthernetAsyncTx asyncTx;

static uint8_t txBuffer[TX_CHUNK_BYTES];
static bool serverStarted = false;
static bool commandReceived = false;
static bool flushActive = false;
static bool stopActive = false;
static bool resultPrinted = false;
static bool probeFailed = false;
static int probeResult = 0;
static uint32_t txCompletedBytes = 0;
static uint32_t txPendingSinceMs = 0;
static uint32_t flushStartedAtMs = 0;
static uint32_t flushCompletedAtMs = 0;
static uint32_t flushPollCount = 0;
static uint32_t flushPollHoldMaxUs = 0;
static uint32_t loopGapMaxUs = 0;
static uint32_t lastLoopUs = 0;

static void updateLoopGap()
{
    const uint32_t nowUs = micros();
    if (lastLoopUs != 0)
    {
        const uint32_t gapUs = (uint32_t)(nowUs - lastLoopUs);
        if (gapUs > loopGapMaxUs) loopGapMaxUs = gapUs;
    }
    lastLoopUs = nowUs;
}

static void failProbe(int code)
{
    probeFailed = true;
    probeResult = code;
}

static void printResultIfReady()
{
    if (resultPrinted || (!probeFailed && !stopActive && probeResult == 0)) return;

    resultPrinted = true;
    Serial.println("NB1_FLUSH_PROBE_RESULT=BEGIN");
    Serial.print("RESULT_CODE=");
    Serial.println(probeResult);
    Serial.print("TX_COMPLETED_BYTES_BEFORE_FLUSH=");
    Serial.println(txCompletedBytes);
    Serial.print("FLUSH_DURATION_MS=");
    Serial.println((uint32_t)(flushCompletedAtMs - flushStartedAtMs));
    Serial.print("FLUSH_POLL_COUNT=");
    Serial.println(flushPollCount);
    Serial.print("FLUSH_POLL_HOLD_MAX_US=");
    Serial.println(flushPollHoldMaxUs);
    Serial.print("LOOP_GAP_MAX_US=");
    Serial.println(loopGapMaxUs);
    Serial.print("PROBE_FAILED=");
    Serial.println(probeFailed ? "YES" : "NO");
    Serial.println("NB1_FLUSH_PROBE_RESULT=END");
}

static void startServerIfReadyLocked()
{
    if (serverStarted || !JWPLC_Ethernet.isReady()) return;
    probeServer.begin();
    if (probeServer)
    {
        serverStarted = true;
    }
}

static void serviceProbeLocked()
{
    startServerIfReadyLocked();
    if (!serverStarted || resultPrinted) return;

    if (stopActive)
    {
        const int state = probeClient.pollStopAsync();
        if (state != 0)
        {
            stopActive = false;
            if (!probeFailed) probeResult = 1;
        }
        return;
    }

    if (flushActive)
    {
        const uint32_t pollStartUs = micros();
        const int state = probeClient.pollFlushAsync();
        const uint32_t pollHoldUs = (uint32_t)(micros() - pollStartUs);
        ++flushPollCount;
        if (pollHoldUs > flushPollHoldMaxUs) flushPollHoldMaxUs = pollHoldUs;

        if (state == 0) return;

        flushCompletedAtMs = millis();
        flushActive = false;

        if (state < 0)
        {
            failProbe(-40);
            probeClient.cancelStopAsync();
            return;
        }

        if (asyncTx.inProgress())
        {
            const int txState = asyncTx.poll(probeClient);
            if (txState < 0)
            {
                failProbe(-41);
                probeClient.cancelStopAsync();
                return;
            }
        }

        const int stopState = probeClient.beginStopAsync();
        if (stopState == 0)
        {
            stopActive = true;
        }
        else
        {
            if (!probeFailed) probeResult = 1;
        }
        return;
    }

    if (!probeClient)
    {
        probeClient = probeServer.accept();
        commandReceived = false;
        txCompletedBytes = 0;
        txPendingSinceMs = 0;
        return;
    }

    if (!probeClient.connected())
    {
        const int stopState = probeClient.beginStopAsync();
        if (stopState == 0) stopActive = true;
        return;
    }

    if (!commandReceived)
    {
        if (probeClient.available() <= 0) return;
        const int command = probeClient.read();
        if (command != 'F')
        {
            failProbe(-10);
            const int stopState = probeClient.beginStopAsync();
            if (stopState == 0) stopActive = true;
            return;
        }
        commandReceived = true;
        return;
    }

    if (asyncTx.inProgress())
    {
        const int txState = asyncTx.poll(probeClient);
        if (txState > 0)
        {
            txCompletedBytes += TX_CHUNK_BYTES;
            txPendingSinceMs = 0;
            return;
        }
        if (txState < 0)
        {
            failProbe(-20);
            probeClient.cancelStopAsync();
            return;
        }

        if (txPendingSinceMs == 0) txPendingSinceMs = millis();
        if (
            txCompletedBytes >= MIN_PREFILL_BYTES &&
            (uint32_t)(millis() - txPendingSinceMs) >= BACKPRESSURE_CONFIRM_MS)
        {
            flushStartedAtMs = millis();
            const uint32_t pollStartUs = micros();
            const int flushState = probeClient.beginFlushAsync();
            const uint32_t pollHoldUs = (uint32_t)(micros() - pollStartUs);
            ++flushPollCount;
            if (pollHoldUs > flushPollHoldMaxUs) flushPollHoldMaxUs = pollHoldUs;

            if (flushState == 0)
            {
                flushActive = true;
                return;
            }
            if (flushState > 0)
            {
                flushCompletedAtMs = millis();
                failProbe(-30); // No se logró observar flush pendiente real.
                probeClient.cancelStopAsync();
                return;
            }

            failProbe(-31);
            probeClient.cancelStopAsync();
            return;
        }
        return;
    }

    const int beginState = asyncTx.begin(probeClient, txBuffer, TX_CHUNK_BYTES);
    if (beginState < 0)
    {
        failProbe(-21);
        probeClient.cancelStopAsync();
        return;
    }
    if (asyncTx.inProgress())
    {
        txPendingSinceMs = millis();
    }
}

void setup()
{
    Serial.begin(115200);
    for (size_t i = 0; i < sizeof(txBuffer); ++i)
    {
        txBuffer[i] = (uint8_t)(i & 0xFFU);
    }
}

void loop()
{
    updateLoopGap();

    if (jwplcSPI_acquire(50))
    {
        jwplcSPI_deselectAll();
        serviceProbeLocked();
        jwplcSPI_release();
    }

    if (serverStarted && !resultPrinted)
    {
        static bool announced = false;
        if (!announced)
        {
            announced = true;
            Serial.print("NB1_FLUSH_PROBE_READY=YES IP=");
            Serial.print(JWPLC_Ethernet.localIP());
            Serial.print(" PORT=");
            Serial.println(PROBE_PORT);
        }
    }

    printResultIfReady();
    delay(0);
}
'@

$utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
[System.IO.File]::WriteAllText($sketchPath, $sketch, $utf8NoBom)

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "FQBN=$fqbn"
Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "SKETCH_PATH=$sketchPath"
Write-Host "SOURCE_MUTATION=NO"
Write-Host "UPLOAD=NO"

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildDir,
    "--libraries", $librariesRoot,
    $sketchDir
)
& $arduinoCli @compileArgs *> $compileLog
$compileExit = [int]$LASTEXITCODE
Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"
if ($compileExit -ne 0) {
    Get-Content -LiteralPath $compileLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB1D1_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$repoEthernetUsed = (
    $compileText.IndexOf($expectedEthernetLibrary, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
)
Write-Host "REPO_ETHERNET_LIBRARY_USED=$repoEthernetUsed"
if (-not $repoEthernetUsed) {
    throw "A14_NB1D1_REPO_ETHERNET_NOT_USED"
}

$binCount = @(Get-ChildItem -LiteralPath $buildDir -Recurse -File -Filter "*.bin").Count
Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) {
    throw "A14_NB1D1_NO_BIN_OUTPUT"
}

Assert-G2ProtectedArtifacts
if ((Get-G2Sha256 $headerRelative) -ne $expectedHeaderSha256) { throw "A14_NB1D1_HEADER_CHANGED" }
if ((Get-G2Sha256 $cppRelative) -ne $expectedCppSha256) { throw "A14_NB1D1_CPP_CHANGED" }
if ((Get-G2Sha256 $firmwareRelative) -ne $expectedFirmwareSha256) { throw "A14_NB1D1_FIRMWARE_CHANGED" }
if ((Get-G2Sha256 $script:G2RawRunnerRelative) -ne $script:G2RawRunnerSha256) { throw "A14_NB1D1_RUNNER_CHANGED" }

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)
Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"
if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB1D1_FINAL_TREE_STATE_INVALID"
}

Write-Host "NB1_FLUSH_ASYNC_STATE_MACHINE_COMPILE=PASS"
Write-Host "NB1_FLUSH_PENDING_TEST_DESIGN=READY"
Write-Host "NB1_UPLOAD=NO"
Write-Host "A14_NB1_FLUSH_ASYNC_COMPILE_PROBE=PASS"
