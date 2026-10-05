#include <JWPLC_Ethernet.h>
#include <jwplc_spi_bus.h>
#include <SPI.h>
#include "utility/w5100.h"

static constexpr uint16_t PROBE_PORT = 5003;
static constexpr size_t CONTROLLED_PENDING_BYTES = 1024;
static constexpr uint32_t CONTROLLED_RELEASE_MS = 300;

EthernetServer probeServer(PROBE_PORT);
EthernetClient probeClient;

static uint8_t txBuffer[CONTROLLED_PENDING_BYTES];
static bool serverStarted = false;
static bool commandReceived = false;
static bool flushActive = false;
static bool flushPendingObserved = false;
static bool controlledSendReleased = false;
static bool stopActive = false;
static bool resultReady = false;
static bool resultPrinted = false;
static bool probeFailed = false;
static int probeResult = 0;
static uint32_t txBufferedBytes = 0;
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
    resultReady = true;
}

static void printResultIfReady()
{
    if (resultPrinted || !resultReady)
    {
        return;
    }

    resultPrinted = true;
    Serial.println("NB1_FLUSH_PROBE_RESULT=BEGIN");
    Serial.print("RESULT_CODE=");
    Serial.println(probeResult);
    Serial.print("TX_BUFFERED_BYTES_BEFORE_FLUSH=");
    Serial.println(txBufferedBytes);
    Serial.print("CONTROLLED_RELEASE_MS=");
    Serial.println(CONTROLLED_RELEASE_MS);
    Serial.print("CONTROLLED_SEND_RELEASED=");
    Serial.println(controlledSendReleased ? "YES" : "NO");
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

static uint16_t bufferTxWithoutSend(
    uint8_t socket,
    const uint8_t *data,
    uint16_t length)
{
    if (data == nullptr || length == 0 || length > W5100.SSIZE)
    {
        return 0;
    }

    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

    const uint16_t freeBytes = W5100.readSnTX_FSR(socket);
    if (freeBytes < length)
    {
        SPI.endTransaction();
        return 0;
    }

    uint16_t ptr = W5100.readSnTX_WR(socket);
    const uint16_t offset = ptr & W5100.SMASK;
    const uint16_t destination = offset + W5100.SBASE(socket);

    if (
        W5100.hasOffsetAddressMapping() ||
        (uint16_t)(offset + length) <= W5100.SSIZE
    )
    {
        W5100.write(destination, data, length);
    }
    else
    {
        const uint16_t firstPart = W5100.SSIZE - offset;
        W5100.write(destination, data, firstPart);
        W5100.write(
            W5100.SBASE(socket),
            data + firstPart,
            length - firstPart);
    }

    ptr = (uint16_t)(ptr + length);
    W5100.writeSnTX_WR(socket, ptr);
    SPI.endTransaction();

    return length;
}

static void beginControlledFlush()
{
    const uint8_t socket = probeClient.getSocketNumber();
    if (socket >= MAX_SOCK_NUM)
    {
        failProbe(-20);
        probeClient.cancelStopAsync();
        return;
    }

    const uint16_t buffered = bufferTxWithoutSend(
        socket,
        txBuffer,
        CONTROLLED_PENDING_BYTES);

    if (buffered != CONTROLLED_PENDING_BYTES)
    {
        failProbe(-21);
        probeClient.cancelStopAsync();
        return;
    }

    txBufferedBytes = buffered;
    controlledSendReleased = false;
    flushStartedAtMs = millis();

    // Aísla la métrica de loop a la ventana real de flush/stop.
    loopGapMaxUs = 0;
    lastLoopUs = micros();

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
        failProbe(-30); // El TX buffered no produjo un flush pendiente observable.
        probeClient.cancelStopAsync();
        return;
    }

    failProbe(-31);
    probeClient.cancelStopAsync();
}

static void releaseControlledSendIfDue()
{
    if (controlledSendReleased) return;

    if ((uint32_t)(millis() - flushStartedAtMs) < CONTROLLED_RELEASE_MS)
    {
        return;
    }

    const uint8_t socket = probeClient.getSocketNumber();
    if (socket >= MAX_SOCK_NUM)
    {
        failProbe(-32);
        probeClient.cancelStopAsync();
        flushActive = false;
        return;
    }

    // Dispara el SEND sólo después de una ventana controlada de flush pendiente.
    // El caller ya posee el mutex SPI compartido JWPLC.
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.writeSnIR(socket, (uint8_t)(SnIR::SEND_OK | SnIR::TIMEOUT));
    W5100.execCmdSn(socket, Sock_SEND);
    SPI.endTransaction();

    controlledSendReleased = true;
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
            if (!probeFailed)
            {
                probeResult = 1;
                resultReady = true;
            }
        }
        return;
    }

    if (flushActive)
    {
        releaseControlledSendIfDue();
        if (!flushActive || probeFailed) return;

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

        const int stopState = probeClient.beginStopAsync();
        if (stopState == 0)
        {
            stopActive = true;
        }
        else if (!probeFailed)
        {
            probeResult = 1;
            resultReady = true;
        }
        return;
    }

    if (!probeClient)
    {
        probeClient = probeServer.accept();
        commandReceived = false;
        txBufferedBytes = 0;
        controlledSendReleased = false;
        return;
    }

    if (!probeClient.connected())
    {
        const int stopState = probeClient.beginStopAsync();
        if (stopState == 0)
        {
            stopActive = true;
        }
        else
        {
            resultReady = true;
        }
        return;
    }

    if (!commandReceived)
    {
        if (probeClient.available() <= 0) return;

        const int command = probeClient.read();
        if (command != 'F')
        {
            failProbe(-10);
            probeClient.cancelStopAsync();
            return;
        }

        commandReceived = true;
        beginControlledFlush();
        return;
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
