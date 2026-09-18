#include <JWPLC_Ethernet.h>
#include <jwplc_spi_bus.h>
#include <SPI.h>
#include "utility/w5100.h"

static constexpr uint16_t PROBE_PORT = 5005;
static constexpr uint16_t PAYLOAD_BYTES = 1024;
static constexpr uint16_t TARGET_CYCLES = 20;

EthernetServer probeServer(PROBE_PORT);
EthernetClient probeClient;
static uint8_t payload[PAYLOAD_BYTES];

enum ProbeState : uint8_t
{
    WAIT_CLIENT,
    WAIT_COMMAND,
    START_CYCLE,
    POLL_FLUSH,
    STOP_CLIENT,
    DONE
};

static ProbeState probeState = WAIT_CLIENT;
static bool serverStarted = false;
static bool resultPrinted = false;
static bool probeFailed = false;
static int resultCode = 0;

static uint16_t cyclesCompleted = 0;
static uint16_t flushBeginPendingCount = 0;
static uint16_t flushBeginImmediateCount = 0;
static uint32_t flushPollCountTotal = 0;
static uint32_t flushPollPendingTotal = 0;
static uint32_t flushTimeoutCount = 0;

static uint32_t cycleStartedUs = 0;
static uint32_t flushDurationMinUs = 0xFFFFFFFFUL;
static uint32_t flushDurationMaxUs = 0;
static uint64_t flushDurationSumUs = 0;
static uint32_t flushPollHoldMaxUs = 0;
static uint32_t serviceHoldMaxUs = 0;
static uint32_t loopGapMaxUs = 0;
static uint32_t lastLoopUs = 0;
static uint32_t spiLockErrors = 0;

static uint16_t firstAfterSendWr = 0;
static uint16_t firstAfterSendRd = 0;
static uint16_t firstAfterSendFsr = 0;
static uint16_t firstFlushDoneWr = 0;
static uint16_t firstFlushDoneRd = 0;
static uint16_t firstFlushDoneFsr = 0;

static void failProbe(int code)
{
    probeFailed = true;
    resultCode = code;
    probeState = DONE;
}

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

static uint16_t readFsrStableBounded(uint8_t s, bool &stable)
{
    uint16_t previous = W5100.readSnTX_FSR(s);
    for (uint8_t i = 0; i < 8; ++i)
    {
        const uint16_t current = W5100.readSnTX_FSR(s);
        if (current == previous)
        {
            stable = true;
            return current;
        }
        previous = current;
    }
    stable = false;
    return previous;
}

static bool readTxRegs(
    uint8_t s,
    uint16_t &wr,
    uint16_t &rd,
    uint16_t &fsr)
{
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    wr = W5100.readSnTX_WR(s);
    rd = W5100.readSnTX_RD(s);
    bool stable = false;
    fsr = readFsrStableBounded(s, stable);
    SPI.endTransaction();
    return stable;
}

static bool bufferAndSend(uint8_t s)
{
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

    bool stable = false;
    const uint16_t freeBytes = readFsrStableBounded(s, stable);
    if (!stable || freeBytes < PAYLOAD_BYTES)
    {
        SPI.endTransaction();
        return false;
    }

    uint16_t ptr = W5100.readSnTX_WR(s);
    const uint16_t offset = ptr & W5100.SMASK;
    const uint16_t destination = offset + W5100.SBASE(s);

    if (W5100.hasOffsetAddressMapping() ||
        (uint16_t)(offset + PAYLOAD_BYTES) <= W5100.SSIZE)
    {
        W5100.write(destination, payload, PAYLOAD_BYTES);
    }
    else
    {
        const uint16_t firstPart = W5100.SSIZE - offset;
        W5100.write(destination, payload, firstPart);
        W5100.write(
            W5100.SBASE(s),
            payload + firstPart,
            PAYLOAD_BYTES - firstPart);
    }

    W5100.writeSnTX_WR(s, (uint16_t)(ptr + PAYLOAD_BYTES));
    W5100.writeSnIR(s, (uint8_t)(SnIR::SEND_OK | SnIR::TIMEOUT));
    W5100.execCmdSn(s, Sock_SEND);
    SPI.endTransaction();
    return true;
}

static void startServerIfReadyLocked()
{
    if (serverStarted || !JWPLC_Ethernet.isReady()) return;
    probeServer.begin();
    if (probeServer) serverStarted = true;
}

static void finishCycleLocked(uint8_t s)
{
    const uint32_t durationUs = (uint32_t)(micros() - cycleStartedUs);
    if (durationUs < flushDurationMinUs) flushDurationMinUs = durationUs;
    if (durationUs > flushDurationMaxUs) flushDurationMaxUs = durationUs;
    flushDurationSumUs += durationUs;

    if (cyclesCompleted == 0)
    {
        if (!readTxRegs(
                s,
                firstFlushDoneWr,
                firstFlushDoneRd,
                firstFlushDoneFsr))
        {
            failProbe(-31);
            return;
        }
    }

    ++cyclesCompleted;
    if (cyclesCompleted >= TARGET_CYCLES)
    {
        const int stopState = probeClient.beginStopAsync();
        if (stopState == 0)
        {
            probeState = STOP_CLIENT;
        }
        else
        {
            resultCode = 1;
            probeState = DONE;
        }
        return;
    }

    probeState = START_CYCLE;
}

static void serviceProbeLocked()
{
    startServerIfReadyLocked();
    if (!serverStarted || probeState == DONE) return;

    if (probeState == WAIT_CLIENT)
    {
        if (!probeClient)
        {
            probeClient = probeServer.accept();
            return;
        }
        probeState = WAIT_COMMAND;
        return;
    }

    if (probeState == WAIT_COMMAND)
    {
        if (!probeClient.connected())
        {
            failProbe(-10);
            return;
        }
        if (probeClient.available() <= 0) return;
        if (probeClient.read() != 'F')
        {
            failProbe(-11);
            return;
        }

        loopGapMaxUs = 0;
        lastLoopUs = micros();
        probeState = START_CYCLE;
        return;
    }

    if (probeState == START_CYCLE)
    {
        if (!probeClient.connected())
        {
            failProbe(-12);
            return;
        }

        const uint8_t s = probeClient.getSocketNumber();
        if (s >= MAX_SOCK_NUM)
        {
            failProbe(-13);
            return;
        }

        if (!bufferAndSend(s))
        {
            failProbe(-20);
            return;
        }

        if (cyclesCompleted == 0)
        {
            if (!readTxRegs(
                    s,
                    firstAfterSendWr,
                    firstAfterSendRd,
                    firstAfterSendFsr))
            {
                failProbe(-21);
                return;
            }
        }

        cycleStartedUs = micros();
        const uint32_t pollStartedUs = micros();
        const int flushState = probeClient.beginFlushAsync();
        const uint32_t pollHoldUs =
            (uint32_t)(micros() - pollStartedUs);

        ++flushPollCountTotal;
        if (pollHoldUs > flushPollHoldMaxUs)
        {
            flushPollHoldMaxUs = pollHoldUs;
        }

        if (flushState == 0)
        {
            ++flushBeginPendingCount;
            probeState = POLL_FLUSH;
            return;
        }

        if (flushState > 0)
        {
            ++flushBeginImmediateCount;
            finishCycleLocked(s);
            return;
        }

        ++flushTimeoutCount;
        failProbe(-22);
        return;
    }

    if (probeState == POLL_FLUSH)
    {
        const uint8_t s = probeClient.getSocketNumber();
        if (s >= MAX_SOCK_NUM)
        {
            failProbe(-23);
            return;
        }

        const uint32_t pollStartedUs = micros();
        const int flushState = probeClient.pollFlushAsync();
        const uint32_t pollHoldUs =
            (uint32_t)(micros() - pollStartedUs);

        ++flushPollCountTotal;
        if (pollHoldUs > flushPollHoldMaxUs)
        {
            flushPollHoldMaxUs = pollHoldUs;
        }

        if (flushState == 0)
        {
            ++flushPollPendingTotal;
            return;
        }

        if (flushState > 0)
        {
            finishCycleLocked(s);
            return;
        }

        ++flushTimeoutCount;
        failProbe(-24);
        return;
    }

    if (probeState == STOP_CLIENT)
    {
        const int state = probeClient.pollStopAsync();
        if (state != 0)
        {
            resultCode = 1;
            probeState = DONE;
        }
    }
}

static void printResultIfReady()
{
    if (resultPrinted || probeState != DONE) return;
    resultPrinted = true;

    const uint32_t avgUs =
        cyclesCompleted == 0
            ? 0
            : (uint32_t)(flushDurationSumUs / cyclesCompleted);

    Serial.println("NB1_D2S_RESULT=BEGIN");
    Serial.print("RESULT_CODE="); Serial.println(resultCode);
    Serial.print("PROBE_FAILED="); Serial.println(probeFailed ? "YES" : "NO");
    Serial.print("CYCLES_TARGET="); Serial.println(TARGET_CYCLES);
    Serial.print("CYCLES_COMPLETED="); Serial.println(cyclesCompleted);
    Serial.print("FLUSH_BEGIN_PENDING_COUNT="); Serial.println(flushBeginPendingCount);
    Serial.print("FLUSH_BEGIN_IMMEDIATE_COUNT="); Serial.println(flushBeginImmediateCount);
    Serial.print("FLUSH_POLL_COUNT_TOTAL="); Serial.println(flushPollCountTotal);
    Serial.print("FLUSH_POLL_PENDING_TOTAL="); Serial.println(flushPollPendingTotal);
    Serial.print("FLUSH_TIMEOUT_COUNT="); Serial.println(flushTimeoutCount);
    Serial.print("FLUSH_DURATION_MIN_US="); Serial.println(flushDurationMinUs == 0xFFFFFFFFUL ? 0 : flushDurationMinUs);
    Serial.print("FLUSH_DURATION_AVG_US="); Serial.println(avgUs);
    Serial.print("FLUSH_DURATION_MAX_US="); Serial.println(flushDurationMaxUs);
    Serial.print("FLUSH_POLL_HOLD_MAX_US="); Serial.println(flushPollHoldMaxUs);
    Serial.print("SERVICE_SPI_HOLD_MAX_US="); Serial.println(serviceHoldMaxUs);
    Serial.print("LOOP_GAP_MAX_US="); Serial.println(loopGapMaxUs);
    Serial.print("SPI_LOCK_ERRORS="); Serial.println(spiLockErrors);
    Serial.print("FIRST_AFTER_SEND_TX_WR="); Serial.println(firstAfterSendWr);
    Serial.print("FIRST_AFTER_SEND_TX_RD="); Serial.println(firstAfterSendRd);
    Serial.print("FIRST_AFTER_SEND_TX_FSR="); Serial.println(firstAfterSendFsr);
    Serial.print("FIRST_FLUSH_DONE_TX_WR="); Serial.println(firstFlushDoneWr);
    Serial.print("FIRST_FLUSH_DONE_TX_RD="); Serial.println(firstFlushDoneRd);
    Serial.print("FIRST_FLUSH_DONE_TX_FSR="); Serial.println(firstFlushDoneFsr);
    Serial.println("NB1_D2S_RESULT=END");
}

void setup()
{
    Serial.begin(115200);
    for (uint16_t i = 0; i < PAYLOAD_BYTES; ++i)
    {
        payload[i] = (uint8_t)(i & 0xFFU);
    }
}

void loop()
{
    updateLoopGap();

    if (jwplcSPI_acquire(50))
    {
        const uint32_t serviceStartedUs = micros();
        jwplcSPI_deselectAll();
        serviceProbeLocked();
        const uint32_t serviceHoldUs =
            (uint32_t)(micros() - serviceStartedUs);
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

    if (serverStarted)
    {
        static bool announced = false;
        if (!announced)
        {
            announced = true;
            Serial.print("NB1_D2S_READY=YES IP=");
            Serial.print(JWPLC_Ethernet.localIP());
            Serial.print(" PORT=");
            Serial.println(PROBE_PORT);
        }
    }

    printResultIfReady();
    delay(0);
}
