/*
  Alpha14 final capability - UDP FAST RX ceiling
  - package source only
  - additive jwplcReadPacketFastDeferred()/jwplcCommitRxFast()
  - no Serial output during the measurement window
  - batch2 matches the validated 2 KB W5500 RX topology at payload 1016 B
*/
#include <JWPLC_Ethernet.h>
#include <esp_system.h>

static uint32_t benchmarkBootMarker = 0;

static constexpr uint16_t UDP_PORT = 5002;
static constexpr size_t UDP_PAYLOAD_BYTES = 1016;
static constexpr uint8_t UDP_BATCH = 2;

static EthernetUDP udp;
static uint8_t buffer[UDP_PAYLOAD_BYTES];

static bool started = false;
static bool announced = false;
static uint64_t rxBytes = 0;
static uint32_t rxPackets = 0;
static uint32_t transportErrors = 0;
static uint32_t spiLockErrors = 0;
static uint32_t spiHoldCount = 0;
static uint64_t spiHoldTotalUs = 0;
static uint32_t spiHoldMaxUs = 0;
static uint32_t loopLastUs = 0;
static uint64_t loopGapSumUs = 0;
static uint32_t loopGapSamples = 0;
static uint32_t loopGapMaxUs = 0;

static void resetCounters()
{
    rxBytes = 0;
    rxPackets = 0;
    transportErrors = 0;
    spiLockErrors = 0;
    spiHoldCount = 0;
    spiHoldTotalUs = 0;
    spiHoldMaxUs = 0;
    loopGapSumUs = 0;
    loopGapSamples = 0;
    loopGapMaxUs = 0;
    loopLastUs = micros();
}

static void updateLoop()
{
    const uint32_t nowUs = micros();
    if (loopLastUs != 0)
    {
        const uint32_t gap = (uint32_t)(nowUs - loopLastUs);
        loopGapSumUs += gap;
        ++loopGapSamples;
        if (gap > loopGapMaxUs) loopGapMaxUs = gap;
    }
    loopLastUs = nowUs;
}

static void printSnapshot()
{
    const uint32_t loopAvg =
        loopGapSamples > 0 ? (uint32_t)(loopGapSumUs / loopGapSamples) : 0;
    const uint32_t holdAvg =
        spiHoldCount > 0 ? (uint32_t)(spiHoldTotalUs / spiHoldCount) : 0;

    Serial.println("A14_FINAL_UDP_FAST_SNAPSHOT=BEGIN");
    Serial.print("BOOT_MARKER=");
    Serial.println(benchmarkBootMarker);
    Serial.print("UPTIME_MS=");
    Serial.println(millis());
    Serial.print("UDP_FAST_READY=");
    Serial.println(started ? "YES" : "NO");
    Serial.print("ETH_READY=");
    Serial.println(JWPLC_Ethernet.isReady() ? "YES" : "NO");
    Serial.print("ETH_LINK=");
    Serial.println(JWPLC_Ethernet.linkUp() ? "UP" : "DOWN");
    Serial.print("IP=");
    Serial.println(JWPLC_Ethernet.localIP());
    Serial.print("UDP_PORT=");
    Serial.println(UDP_PORT);
    Serial.print("UDP_PAYLOAD_BYTES=");
    Serial.println(UDP_PAYLOAD_BYTES);
    Serial.print("UDP_BATCH=");
    Serial.println(UDP_BATCH);
    Serial.print("RX_BYTES=");
    Serial.println((unsigned long long)rxBytes);
    Serial.print("RX_PACKETS=");
    Serial.println(rxPackets);
    Serial.print("TRANSPORT_ERRORS=");
    Serial.println(transportErrors);
    Serial.print("SPI_LOCK_ERRORS=");
    Serial.println(spiLockErrors);
    Serial.print("SPI_HOLD_COUNT=");
    Serial.println(spiHoldCount);
    Serial.print("SPI_HOLD_TOTAL_US=");
    Serial.println((unsigned long long)spiHoldTotalUs);
    Serial.print("SPI_HOLD_AVG_US=");
    Serial.println(holdAvg);
    Serial.print("SPI_HOLD_MAX_US=");
    Serial.println(spiHoldMaxUs);
    Serial.print("LOOP_GAP_AVG_US=");
    Serial.println(loopAvg);
    Serial.print("LOOP_GAP_MAX_US=");
    Serial.println(loopGapMaxUs);
    Serial.println("A14_FINAL_UDP_FAST_SNAPSHOT=END");
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();
        if (c == 'R' || c == 'r')
        {
            resetCounters();
            Serial.println("A14_FINAL_UDP_FAST_RESET=PASS");
        }
        else if (c == 'S' || c == 's')
        {
            printSnapshot();
        }
    }
}

static void startIfReady()
{
    if (started || !JWPLC_Ethernet.isReady()) return;

    if (!jwplcSPI_acquire(50))
    {
        ++spiLockErrors;
        return;
    }

    jwplcSPI_deselectAll();
    const bool ok = udp.begin(UDP_PORT) != 0;
    jwplcSPI_release();

    if (ok)
    {
        started = true;
        resetCounters();
    }
}

static void serviceUdpFast()
{
    if (!started) return;

    if (!jwplcSPI_acquire(50))
    {
        ++spiLockErrors;
        return;
    }

    const uint32_t holdStartUs = micros();
    jwplcSPI_deselectAll();

    uint8_t readCount = 0;
    for (uint8_t i = 0; i < UDP_BATCH; ++i)
    {
        const int got = udp.jwplcReadPacketFastDeferred(
            buffer,
            sizeof(buffer));

        if (got <= 0) break;

        rxBytes += (uint32_t)got;
        ++rxPackets;
        ++readCount;
    }

    if (readCount > 0 && !udp.jwplcCommitRxFast())
    {
        ++transportErrors;
    }

    const uint32_t holdUs = (uint32_t)(micros() - holdStartUs);
    ++spiHoldCount;
    spiHoldTotalUs += holdUs;
    if (holdUs > spiHoldMaxUs) spiHoldMaxUs = holdUs;

    jwplcSPI_release();
}

void setup()
{
    Serial.begin(115200);
    benchmarkBootMarker = (uint32_t)esp_random();
    resetCounters();
    Serial.println("A14_FINAL_UDP_FAST_CONFIG=PASS");
}

void loop()
{
    updateLoop();
    serviceSerial();
    startIfReady();

    if (started && !announced)
    {
        announced = true;
        Serial.print("A14_FINAL_UDP_FAST_READY=PASS IP=");
        Serial.println(JWPLC_Ethernet.localIP());
    }

    serviceUdpFast();
}
