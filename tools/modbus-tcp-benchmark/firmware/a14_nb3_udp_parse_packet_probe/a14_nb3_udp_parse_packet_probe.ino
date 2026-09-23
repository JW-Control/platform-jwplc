#include <JWPLC_Ethernet.h>
#include <jwplc_spi_bus.h>

static constexpr uint16_t UDP_PORT = 5003;
static constexpr size_t PARTIAL_READ_BYTES = 8;
static constexpr size_t RX_BUFFER_BYTES = 512;

static EthernetUDP udpProbe;
static uint8_t rxBuffer[RX_BUFFER_BYTES];

enum ProbePhase : uint8_t
{
    WAIT_PARTIAL = 0,
    DRAIN_PENDING,
    WAIT_NEXT,
    DONE
};

static ProbePhase phase = WAIT_PARTIAL;
static bool udpStarted = false;
static bool readyPrinted = false;
static bool resultPrinted = false;
static bool probeFailed = false;
static int resultCode = 0;

static int partialPacketSize = 0;
static int partialReadBytes = 0;
static int partialRemainingBeforeDrain = 0;
static int drainParseReturn = -999;
static int drainRemainingAfter = -1;
static uint32_t drainHoldUs = 0;
static bool nextPacketRecovered = false;

static uint32_t spiLockErrors = 0;
static uint32_t loopGapMaxUs = 0;
static uint32_t lastLoopUs = 0;

static void updateLoopGap()
{
    const uint32_t nowUs = micros();

    if (lastLoopUs != 0)
    {
        const uint32_t gapUs =
            (uint32_t)(nowUs - lastLoopUs);

        if (gapUs > loopGapMaxUs)
        {
            loopGapMaxUs = gapUs;
        }
    }

    lastLoopUs = nowUs;
}

static void failProbe(int code)
{
    probeFailed = true;
    resultCode = code;
    phase = DONE;
}

static bool acquireSpi()
{
    if (!jwplcSPI_acquire(50))
    {
        ++spiLockErrors;
        return false;
    }

    jwplcSPI_deselectAll();
    return true;
}

static void releaseSpi()
{
    jwplcSPI_release();
}

static void serviceProbe()
{
    if (!udpStarted || phase == DONE)
    {
        return;
    }

    if (phase == DRAIN_PENDING)
    {
        if (!acquireSpi())
        {
            return;
        }

        const uint32_t startedUs = micros();
        drainParseReturn = udpProbe.parsePacket();
        drainHoldUs = (uint32_t)(micros() - startedUs);
        drainRemainingAfter = udpProbe.available();

        releaseSpi();

        if (drainParseReturn != 0)
        {
            failProbe(-20);
            return;
        }

        if (drainRemainingAfter != 0)
        {
            failProbe(-21);
            return;
        }

        phase = WAIT_NEXT;
        Serial.println("NB3_E2_DRAIN_DONE=YES");
        return;
    }

    if (!acquireSpi())
    {
        return;
    }

    const int packetSize = udpProbe.parsePacket();

    if (packetSize <= 0)
    {
        releaseSpi();
        return;
    }

    if (phase == WAIT_PARTIAL)
    {
        partialPacketSize = packetSize;
        partialReadBytes =
            udpProbe.read(
                rxBuffer,
                PARTIAL_READ_BYTES);
        partialRemainingBeforeDrain =
            udpProbe.available();

        releaseSpi();

        if (partialPacketSize <= (int)PARTIAL_READ_BYTES)
        {
            failProbe(-10);
            return;
        }

        if (partialReadBytes != (int)PARTIAL_READ_BYTES)
        {
            failProbe(-11);
            return;
        }

        if (partialRemainingBeforeDrain <= 0)
        {
            failProbe(-12);
            return;
        }

        phase = DRAIN_PENDING;
        return;
    }

    int totalRead = 0;

    while (
        udpProbe.available() > 0 &&
        totalRead < (int)sizeof(rxBuffer))
    {
        const int got =
            udpProbe.read(
                rxBuffer + totalRead,
                sizeof(rxBuffer) - totalRead);

        if (got <= 0)
        {
            break;
        }

        totalRead += got;
    }

    releaseSpi();

    if (phase == WAIT_NEXT)
    {
        static const char expected[] = "NEXT";

        if (
            totalRead == (int)(sizeof(expected) - 1) &&
            memcmp(
                rxBuffer,
                expected,
                sizeof(expected) - 1) == 0)
        {
            nextPacketRecovered = true;
            resultCode = 1;
            phase = DONE;
            return;
        }

        failProbe(-30);
    }
}

static void startUdpIfReady()
{
    if (udpStarted || !JWPLC_Ethernet.isReady())
    {
        return;
    }

    if (!acquireSpi())
    {
        return;
    }

    const uint8_t started =
        udpProbe.begin(UDP_PORT);

    releaseSpi();

    if (started == 0)
    {
        failProbe(-40);
        return;
    }

    udpStarted = true;
    lastLoopUs = micros();
}

static void announceReady()
{
    if (!udpStarted || readyPrinted)
    {
        return;
    }

    readyPrinted = true;

    Serial.print("NB3_E2_PROBE_READY=YES IP=");
    Serial.println(JWPLC_Ethernet.localIP());
    Serial.print("NB3_E2_UDP_PORT=");
    Serial.println(UDP_PORT);
}

static void printResultIfReady()
{
    if (phase != DONE || resultPrinted)
    {
        return;
    }

    resultPrinted = true;

    Serial.println("NB3_E2_RESULT=BEGIN");
    Serial.print("RESULT_CODE=");
    Serial.println(resultCode);
    Serial.print("PROBE_FAILED=");
    Serial.println(probeFailed ? "YES" : "NO");
    Serial.print("PARTIAL_PACKET_SIZE=");
    Serial.println(partialPacketSize);
    Serial.print("PARTIAL_READ_BYTES=");
    Serial.println(partialReadBytes);
    Serial.print("PARTIAL_REMAINING_BEFORE_DRAIN=");
    Serial.println(partialRemainingBeforeDrain);
    Serial.print("DRAIN_PARSE_RETURN=");
    Serial.println(drainParseReturn);
    Serial.print("DRAIN_REMAINING_AFTER=");
    Serial.println(drainRemainingAfter);
    Serial.print("DRAIN_HOLD_US=");
    Serial.println(drainHoldUs);
    Serial.print("NEXT_PACKET_RECOVERED=");
    Serial.println(nextPacketRecovered ? "YES" : "NO");
    Serial.print("SPI_LOCK_ERRORS=");
    Serial.println(spiLockErrors);
    Serial.print("LOOP_GAP_MAX_US=");
    Serial.println(loopGapMaxUs);
    Serial.println("NB3_E2_RESULT=END");
}

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    updateLoopGap();
    startUdpIfReady();
    announceReady();
    serviceProbe();
    printResultIfReady();
    delay(0);
}
