/* A14 S1 - persistent cooperative TCP TX stability soak. */

#include <JWPLC_Ethernet.h>
#include <esp_system.h>

static constexpr uint16_t PAYLOAD_BYTES = 1536;
static constexpr uint32_t API_LIMIT_US = 10000;
static constexpr uint32_t CONNECT_LIMIT_MS = 5000;

enum Phase : uint8_t
{
    PHASE_IDLE = 0,
    PHASE_CONNECT_BEGIN,
    PHASE_CONNECT_POLL,
    PHASE_WRITE_BEGIN,
    PHASE_WRITE_POLL
};

static EthernetClient client;
static Phase phase = PHASE_IDLE;
static IPAddress targetIP;
static uint16_t targetPort = 0;
static uint32_t requestedDurationMs = 0;
static uint8_t payload[PAYLOAD_BYTES];
static char serialLine[96];
static size_t serialLineLength = 0;

static uint32_t bootId = 0;
static uint32_t caseStartedMs = 0;
static uint32_t connectedAtMs = 0;
static uint32_t spiLockErrors = 0;
static uint32_t transportErrors = 0;
static uint32_t connectPolls = 0;
static uint32_t writePolls = 0;
static uint32_t writesCompleted = 0;
static uint32_t bytesCompleted = 0;
static uint32_t beginWriteMaxUs = 0;
static uint32_t pollWriteMaxUs = 0;
static bool writePendingObserved = false;

static bool acquireEthernet()
{
    if (!jwplcSPI_acquire(50))
    {
        ++spiLockErrors;
        return false;
    }
    jwplcSPI_deselectAll();
    return true;
}

static void releaseEthernet()
{
    jwplcSPI_release();
}

static bool parseIPv4(const char *text, IPAddress &ip)
{
    unsigned int a = 0, b = 0, c = 0, d = 0;
    if (sscanf(text, "%u.%u.%u.%u", &a, &b, &c, &d) != 4 ||
        a > 255 || b > 255 || c > 255 || d > 255)
    {
        return false;
    }
    ip = IPAddress((uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d);
    return true;
}

static void resetMetrics()
{
    spiLockErrors = 0;
    transportErrors = 0;
    connectPolls = 0;
    writePolls = 0;
    writesCompleted = 0;
    bytesCompleted = 0;
    beginWriteMaxUs = 0;
    pollWriteMaxUs = 0;
    writePendingObserved = false;
}

static void preparePayload()
{
    for (uint16_t i = 0; i < PAYLOAD_BYTES; ++i)
    {
        payload[i] = (uint8_t)((i * 29U + 0xA7U) & 0xFFU);
    }
}

static void printSnapshot()
{
    Serial.print("S1_BOOT_ID="); Serial.println(bootId);
    Serial.print("S1_ETH_READY="); Serial.println(JWPLC_Ethernet.isReady() ? "YES" : "NO");
    Serial.print("S1_ETH_LINK="); Serial.println(JWPLC_Ethernet.linkUp() ? "UP" : "DOWN");
    Serial.print("S1_IP="); Serial.println(JWPLC_Ethernet.localIP());
    Serial.print("S1_IDLE="); Serial.println(phase == PHASE_IDLE ? "YES" : "NO");
    Serial.println("A14_S1_SNAPSHOT=END");
}

static void finishCase(bool pass, const char *reason)
{
    const uint32_t trafficDurationMs = connectedAtMs == 0
        ? 0
        : (uint32_t)(millis() - connectedAtMs);
    bool linkUp = false;
    if (acquireEthernet())
    {
        client.cancelWriteAsync();
        client.cancelConnectAsync();
        releaseEthernet();
    }
    linkUp = JWPLC_Ethernet.linkUp();
    pass = pass && spiLockErrors == 0 && transportErrors == 0 && linkUp;

    Serial.print("S1_RESULT="); Serial.println(pass ? "PASS" : "FAIL");
    Serial.print("S1_REASON="); Serial.println(reason);
    Serial.print("S1_BOOT_ID="); Serial.println(bootId);
    Serial.print("S1_REQUESTED_DURATION_MS="); Serial.println(requestedDurationMs);
    Serial.print("S1_TRAFFIC_DURATION_MS="); Serial.println(trafficDurationMs);
    Serial.print("S1_CASE_DURATION_MS="); Serial.println((uint32_t)(millis() - caseStartedMs));
    Serial.print("S1_WRITES_COMPLETED="); Serial.println(writesCompleted);
    Serial.print("S1_BYTES_COMPLETED="); Serial.println(bytesCompleted);
    Serial.print("S1_PAYLOAD_BYTES="); Serial.println(PAYLOAD_BYTES);
    Serial.print("S1_CONNECT_POLLS="); Serial.println(connectPolls);
    Serial.print("S1_WRITE_POLLS="); Serial.println(writePolls);
    Serial.print("S1_BEGIN_WRITE_MAX_US="); Serial.println(beginWriteMaxUs);
    Serial.print("S1_POLL_WRITE_MAX_US="); Serial.println(pollWriteMaxUs);
    Serial.print("S1_WRITE_PENDING_OBSERVED="); Serial.println(writePendingObserved ? "YES" : "NO");
    Serial.print("S1_SPI_LOCK_ERRORS="); Serial.println(spiLockErrors);
    Serial.print("S1_TRANSPORT_ERRORS="); Serial.println(transportErrors);
    Serial.print("S1_LINK_FINAL="); Serial.println(linkUp ? "UP" : "DOWN");
    Serial.print("S1_API_LIMIT_US="); Serial.println(API_LIMIT_US);
    Serial.println("A14_S1_CASE=END");

    client.setConnectionTimeout(3000);
    connectedAtMs = 0;
    phase = PHASE_IDLE;
}

static void serviceCase()
{
    if (phase == PHASE_IDLE) return;

    if (phase == PHASE_CONNECT_POLL &&
        (uint32_t)(millis() - caseStartedMs) > CONNECT_LIMIT_MS)
    {
        ++transportErrors;
        finishCase(false, "CONNECT_TIMEOUT");
        return;
    }

    if (!acquireEthernet()) return;

    if (phase == PHASE_CONNECT_BEGIN)
    {
        const int state = client.beginConnectAsync(targetIP, targetPort);
        releaseEthernet();
        if (state < 0)
        {
            ++transportErrors;
            finishCase(false, "CONNECT_BEGIN_FAILED");
        }
        else if (state > 0)
        {
            connectedAtMs = millis();
            phase = PHASE_WRITE_BEGIN;
        }
        else
        {
            phase = PHASE_CONNECT_POLL;
        }
        return;
    }

    if (phase == PHASE_CONNECT_POLL)
    {
        const int state = client.pollConnectAsync();
        ++connectPolls;
        releaseEthernet();
        if (state < 0)
        {
            ++transportErrors;
            finishCase(false, "CONNECT_POLL_FAILED");
        }
        else if (state > 0)
        {
            connectedAtMs = millis();
            phase = PHASE_WRITE_BEGIN;
        }
        return;
    }

    if (phase == PHASE_WRITE_BEGIN)
    {
        const uint32_t startedUs = micros();
        const int state = client.beginWriteAsync(payload, PAYLOAD_BYTES);
        const uint32_t durationUs = (uint32_t)(micros() - startedUs);
        if (durationUs > beginWriteMaxUs) beginWriteMaxUs = durationUs;
        if (state == 0 && client.writeAsyncInProgress()) writePendingObserved = true;
        releaseEthernet();

        if (durationUs > API_LIMIT_US)
        {
            finishCase(false, "WRITE_BEGIN_TOO_SLOW");
        }
        else if (state < 0)
        {
            ++transportErrors;
            finishCase(false, "WRITE_BEGIN_FAILED");
        }
        else if (state > 0)
        {
            finishCase(false, "WRITE_BEGIN_UNEXPECTED_COMPLETE");
        }
        else
        {
            phase = PHASE_WRITE_POLL;
        }
        return;
    }

    if (phase == PHASE_WRITE_POLL)
    {
        const uint32_t startedUs = micros();
        const int state = client.pollWriteAsync();
        const uint32_t durationUs = (uint32_t)(micros() - startedUs);
        if (durationUs > pollWriteMaxUs) pollWriteMaxUs = durationUs;
        ++writePolls;
        releaseEthernet();

        if (durationUs > API_LIMIT_US)
        {
            finishCase(false, "WRITE_POLL_TOO_SLOW");
        }
        else if (state < 0)
        {
            ++transportErrors;
            finishCase(false, "WRITE_POLL_FAILED");
        }
        else if (state > 0)
        {
            ++writesCompleted;
            bytesCompleted += PAYLOAD_BYTES;
            if ((uint32_t)(millis() - connectedAtMs) >= requestedDurationMs)
                finishCase(true, "DURATION_COMPLETE");
            else
                phase = PHASE_WRITE_BEGIN;
        }
        return;
    }

    releaseEthernet();
}

static void handleLine(char *line)
{
    if (strcmp(line, "S") == 0 || strcmp(line, "s") == 0)
    {
        printSnapshot();
        return;
    }
    if (phase != PHASE_IDLE)
    {
        Serial.println("S1_BUSY=YES");
        return;
    }

    char ipText[24] = {0};
    unsigned int port = 0;
    unsigned long seconds = 0;
    if (sscanf(line, "SOAK %23s %u %lu", ipText, &port, &seconds) != 3 ||
        !parseIPv4(ipText, targetIP) || port == 0 || port > 65535 ||
        seconds == 0 || seconds > 3600)
    {
        Serial.println("S1_COMMAND=INVALID");
        return;
    }

    targetPort = (uint16_t)port;
    requestedDurationMs = (uint32_t)seconds * 1000UL;
    resetMetrics();
    caseStartedMs = millis();
    connectedAtMs = 0;
    client.setConnectionTimeout(3000);
    phase = PHASE_CONNECT_BEGIN;
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n')
        {
            serialLine[serialLineLength] = '\0';
            if (serialLineLength > 0) handleLine(serialLine);
            serialLineLength = 0;
        }
        else if (serialLineLength + 1 < sizeof(serialLine))
        {
            serialLine[serialLineLength++] = c;
        }
    }
}

void setup()
{
    Serial.begin(115200);
    bootId = esp_random();
    preparePayload();
    Serial.print("A14_S1_BOOT="); Serial.println(bootId);
}

void loop()
{
    serviceSerial();
    serviceCase();
}
