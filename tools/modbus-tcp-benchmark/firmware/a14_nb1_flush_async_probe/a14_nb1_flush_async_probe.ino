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
static bool flushPendingObserved = false;
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
static uint32_t serviceHoldMaxUs = 0;
static uint32_t spiLockErrors = 0;
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
    if (resultPrinted || (!probeFailed && !stopActive && probeResult == 0))
    {
        return;
    }

    resultPrinted = true;
    Serial.println("NB1_FLUSH_PROBE_RESULT=BEGIN");
    Serial.print("RESULT_CODE=");
    Serial.println(probeResult);
    Serial.print("TX_COMPLETED_BYTES_BEFORE_FLUSH=");
    Serial.println(txCompletedBytes);
    Serial.print("FLUSH_PENDING_OBSERVED=");
    Serial.println(flushPendingObserved ? "YES" : "NO");
    Serial.print("FLUSH_DURATION_MS=");
    Serial.println((uint32_t)(flushCompletedAtMs - flushStartedAtMs));
    Serial.print("FLUSH_POLL_COUNT=");
    Serial.println(flushPollCount);
    Serial.print("FLUSH_POLL_HOLD_MAX_US=");
    Serial.println(flushPollHoldMaxUs);
    Serial.print("SERVICE_SPI_HOLD_MAX_US=");
    Serial.println(serviceHoldMaxUs);
    Serial.print("SPI_LOCK_ERRORS=");
    Serial.println(spiLockErrors);
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
        if (pollHoldUs > flushPollHoldMaxUs)
        {
            flushPollHoldMaxUs = pollHoldUs;
        }

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
        else if (!probeFailed)
        {
            probeResult = 1;
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
            if (pollHoldUs > flushPollHoldMaxUs)
            {
                flushPollHoldMaxUs = pollHoldUs;
            }

            if (flushState == 0)
            {
                flushPendingObserved = true;
                flushActive = true;
                return;
            }

            if (flushState > 0)
            {
                flushCompletedAtMs = millis();
                failProbe(-30);
                probeClient.cancelStopAsync();
                return;
            }

            failProbe(-31);
            probeClient.cancelStopAsync();
            return;
        }

        return;
    }

    const int beginState = asyncTx.begin(
        probeClient,
        txBuffer,
        TX_CHUNK_BYTES);

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
        const uint32_t serviceStartUs = micros();
        jwplcSPI_deselectAll();
        serviceProbeLocked();
        const uint32_t serviceHoldUs =
            (uint32_t)(micros() - serviceStartUs);
        if (serviceHoldUs > serviceHoldMaxUs)
        {
            serviceHoldMaxUs = serviceHoldUs;
        }
        jwplcSPI_release();
    }
    else
    {
        ++spiLockErrors;
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
