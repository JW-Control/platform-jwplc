/* A14 Gate A2 - additive cooperative TCP TX qualification. */

#include <JWPLC_Ethernet.h>
#include <esp_system.h>

static constexpr uint32_t API_LIMIT_US = 10000;
static constexpr uint32_t CASE_LIMIT_MS = 12000;
static constexpr uint16_t PAYLOAD_BYTES = 1536;
static constexpr uint16_t TIMEOUT_PAYLOAD_BYTES = 2048;

enum CaseKind : uint8_t
{
    CASE_NONE = 0,
    CASE_NORMAL,
    CASE_FLUSH,
    CASE_CANCEL,
    CASE_TIMEOUT,
    CASE_PEER_CLOSE
};

enum Phase : uint8_t
{
    PHASE_IDLE = 0,
    PHASE_CONNECT_BEGIN,
    PHASE_CONNECT_POLL,
    PHASE_WAIT_PEER_CLOSE,
    PHASE_WRITE_BEGIN,
    PHASE_WRITE_POLL,
    PHASE_FLUSH_POLL
};

static EthernetClient client;
static CaseKind activeCase = CASE_NONE;
static Phase phase = PHASE_IDLE;
static IPAddress targetIP;
static uint16_t targetPort = 0;
static uint8_t payload[TIMEOUT_PAYLOAD_BYTES];
static char serialLine[96];
static size_t serialLineLength = 0;

static uint32_t bootId = 0;
static uint32_t caseStartedMs = 0;
static uint32_t phaseStartedMs = 0;
static uint32_t writeStartedMs = 0;
static uint32_t payloadFNV = 0;
static uint32_t spiLockErrors = 0;
static uint32_t connectPolls = 0;
static uint32_t writePolls = 0;
static uint32_t flushPolls = 0;
static uint32_t writesCompleted = 0;
static uint32_t bytesCompleted = 0;
static uint32_t beginWriteMaxUs = 0;
static uint32_t pollWriteMaxUs = 0;
static uint32_t pollFlushMaxUs = 0;
static uint32_t timeoutElapsedMs = 0;
static bool writePendingObserved = false;
static bool flushPendingObserved = false;
static bool cancelClosedSocket = false;

static const char *caseName()
{
    switch (activeCase)
    {
        case CASE_NORMAL: return "NORMAL";
        case CASE_FLUSH: return "FLUSH";
        case CASE_CANCEL: return "CANCEL";
        case CASE_TIMEOUT: return "TIMEOUT";
        case CASE_PEER_CLOSE: return "PEER_CLOSE";
        default: return "NONE";
    }
}

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
    connectPolls = 0;
    writePolls = 0;
    flushPolls = 0;
    writesCompleted = 0;
    bytesCompleted = 0;
    beginWriteMaxUs = 0;
    pollWriteMaxUs = 0;
    pollFlushMaxUs = 0;
    timeoutElapsedMs = 0;
    writePendingObserved = false;
    flushPendingObserved = false;
    cancelClosedSocket = false;
}

static void preparePayload()
{
    payloadFNV = 2166136261UL;
    const uint16_t hashedLength = activeCase == CASE_TIMEOUT
        ? TIMEOUT_PAYLOAD_BYTES
        : PAYLOAD_BYTES;
    for (uint16_t i = 0; i < sizeof(payload); ++i)
    {
        payload[i] = (uint8_t)((i * 29U + (uint16_t)activeCase * 17U + 0x53U) & 0xFFU);
        if (i < hashedLength)
        {
            payloadFNV ^= payload[i];
            payloadFNV *= 16777619UL;
        }
    }
}

static void printSnapshot()
{
    Serial.print("A2_BOOT_ID=");
    Serial.println(bootId);
    Serial.print("A2_ETH_READY=");
    Serial.println(JWPLC_Ethernet.isReady() ? "YES" : "NO");
    Serial.print("A2_ETH_LINK=");
    Serial.println(JWPLC_Ethernet.linkUp() ? "UP" : "DOWN");
    Serial.print("A2_IP=");
    Serial.println(JWPLC_Ethernet.localIP());
    Serial.print("A2_IDLE=");
    Serial.println(phase == PHASE_IDLE ? "YES" : "NO");
    Serial.println("A14_A2_SNAPSHOT=END");
}

static void finishCase(bool pass, const char *reason)
{
    if (acquireEthernet())
    {
        client.cancelFlushAsync();
        client.cancelWriteAsync();
        client.cancelConnectAsync();
        releaseEthernet();
    }

    pass = pass && spiLockErrors == 0;
    Serial.print("A2_CASE="); Serial.println(caseName());
    Serial.print("A2_RESULT="); Serial.println(pass ? "PASS" : "FAIL");
    Serial.print("A2_REASON="); Serial.println(reason);
    Serial.print("A2_BOOT_ID="); Serial.println(bootId);
    Serial.print("A2_SPI_LOCK_ERRORS="); Serial.println(spiLockErrors);
    Serial.print("A2_CONNECT_POLLS="); Serial.println(connectPolls);
    Serial.print("A2_WRITE_POLLS="); Serial.println(writePolls);
    Serial.print("A2_FLUSH_POLLS="); Serial.println(flushPolls);
    Serial.print("A2_WRITES_COMPLETED="); Serial.println(writesCompleted);
    Serial.print("A2_BYTES_COMPLETED="); Serial.println(bytesCompleted);
    Serial.print("A2_PAYLOAD_BYTES=");
    Serial.println(activeCase == CASE_TIMEOUT ? TIMEOUT_PAYLOAD_BYTES : PAYLOAD_BYTES);
    Serial.print("A2_PAYLOAD_FNV="); Serial.println(payloadFNV);
    Serial.print("A2_BEGIN_WRITE_MAX_US="); Serial.println(beginWriteMaxUs);
    Serial.print("A2_POLL_WRITE_MAX_US="); Serial.println(pollWriteMaxUs);
    Serial.print("A2_POLL_FLUSH_MAX_US="); Serial.println(pollFlushMaxUs);
    Serial.print("A2_WRITE_PENDING_OBSERVED=");
    Serial.println(writePendingObserved ? "YES" : "NO");
    Serial.print("A2_FLUSH_PENDING_OBSERVED=");
    Serial.println(flushPendingObserved ? "YES" : "NO");
    Serial.print("A2_CANCEL_CLOSED_SOCKET=");
    Serial.println(cancelClosedSocket ? "YES" : "NO");
    Serial.print("A2_TIMEOUT_ELAPSED_MS="); Serial.println(timeoutElapsedMs);
    Serial.print("A2_CASE_ELAPSED_MS="); Serial.println((uint32_t)(millis() - caseStartedMs));
    Serial.print("A2_API_LIMIT_US="); Serial.println(API_LIMIT_US);
    Serial.println("A14_A2_CASE=END");

    client.setConnectionTimeout(3000);
    activeCase = CASE_NONE;
    phase = PHASE_IDLE;
}

static void afterConnected()
{
    if (activeCase == CASE_TIMEOUT)
    {
        client.setConnectionTimeout(100);
    }
    if (activeCase == CASE_PEER_CLOSE)
    {
        phaseStartedMs = millis();
        phase = PHASE_WAIT_PEER_CLOSE;
    }
    else
    {
        phase = PHASE_WRITE_BEGIN;
    }
}

static void serviceCase()
{
    if (phase == PHASE_IDLE) return;
    if ((uint32_t)(millis() - caseStartedMs) > CASE_LIMIT_MS)
    {
        finishCase(false, "CASE_TIMEOUT");
        return;
    }

    if (phase == PHASE_WAIT_PEER_CLOSE)
    {
        if ((uint32_t)(millis() - phaseStartedMs) >= 500)
        {
            phase = PHASE_WRITE_BEGIN;
        }
        return;
    }

    if (!acquireEthernet()) return;

    if (phase == PHASE_CONNECT_BEGIN)
    {
        const int state = client.beginConnectAsync(targetIP, targetPort);
        releaseEthernet();
        if (state < 0) finishCase(false, "CONNECT_BEGIN_FAILED");
        else if (state > 0) afterConnected();
        else phase = PHASE_CONNECT_POLL;
        return;
    }

    if (phase == PHASE_CONNECT_POLL)
    {
        const int state = client.pollConnectAsync();
        ++connectPolls;
        releaseEthernet();
        if (state < 0) finishCase(false, "CONNECT_POLL_FAILED");
        else if (state > 0) afterConnected();
        return;
    }

    if (phase == PHASE_WRITE_BEGIN)
    {
        const uint16_t length = activeCase == CASE_TIMEOUT
            ? TIMEOUT_PAYLOAD_BYTES
            : PAYLOAD_BYTES;
        writeStartedMs = millis();
        const uint32_t startedUs = micros();
        const int state = client.beginWriteAsync(payload, length);
        const uint32_t durationUs = (uint32_t)(micros() - startedUs);
        if (durationUs > beginWriteMaxUs) beginWriteMaxUs = durationUs;
        const bool pending = state == 0 && client.writeAsyncInProgress();
        writePendingObserved = writePendingObserved || pending;

        if (activeCase == CASE_CANCEL && state == 0)
        {
            client.cancelWriteAsync();
            cancelClosedSocket = !client.writeAsyncInProgress() && !client;
            releaseEthernet();
            finishCase(pending && cancelClosedSocket && durationUs <= API_LIMIT_US, "CANCEL_COMPLETE");
            return;
        }

        if (activeCase == CASE_FLUSH && state == 0)
        {
            const int flushState = client.beginFlushAsync();
            flushPendingObserved = flushState == 0 && client.flushAsyncInProgress();
            releaseEthernet();
            if (!pending || !flushPendingObserved || durationUs > API_LIMIT_US)
                finishCase(false, "FLUSH_NOT_PENDING");
            else
                phase = PHASE_FLUSH_POLL;
            return;
        }

        releaseEthernet();
        if (durationUs > API_LIMIT_US)
        {
            finishCase(false, "WRITE_BEGIN_TOO_SLOW");
        }
        else if (state < 0)
        {
            finishCase(activeCase == CASE_PEER_CLOSE, "WRITE_BEGIN_ERROR");
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
        else if (state > 0)
        {
            ++writesCompleted;
            bytesCompleted += activeCase == CASE_TIMEOUT
                ? TIMEOUT_PAYLOAD_BYTES
                : PAYLOAD_BYTES;
            if (activeCase == CASE_TIMEOUT)
                phase = PHASE_WRITE_BEGIN;
            else
                finishCase(activeCase == CASE_NORMAL, "WRITE_COMPLETE");
        }
        else if (state < 0)
        {
            timeoutElapsedMs = (uint32_t)(millis() - writeStartedMs);
            const bool expected =
                activeCase == CASE_PEER_CLOSE ||
                (activeCase == CASE_TIMEOUT && writesCompleted > 0 && timeoutElapsedMs >= 90);
            finishCase(expected, activeCase == CASE_TIMEOUT ? "WRITE_TIMEOUT_OBSERVED" : "PEER_CLOSE_OBSERVED");
        }
        return;
    }

    if (phase == PHASE_FLUSH_POLL)
    {
        const uint32_t startedUs = micros();
        const int state = client.pollFlushAsync();
        const uint32_t durationUs = (uint32_t)(micros() - startedUs);
        if (durationUs > pollFlushMaxUs) pollFlushMaxUs = durationUs;
        ++flushPolls;
        releaseEthernet();
        if (durationUs > API_LIMIT_US) finishCase(false, "FLUSH_POLL_TOO_SLOW");
        else if (state < 0) finishCase(false, "FLUSH_FAILED");
        else if (state > 0)
        {
            ++writesCompleted;
            bytesCompleted += PAYLOAD_BYTES;
            finishCase(true, "FLUSH_COMPLETE");
        }
        return;
    }

    releaseEthernet();
}

static bool decodeCase(const char *text, CaseKind &kind)
{
    if (strcmp(text, "NORMAL") == 0) kind = CASE_NORMAL;
    else if (strcmp(text, "FLUSH") == 0) kind = CASE_FLUSH;
    else if (strcmp(text, "CANCEL") == 0) kind = CASE_CANCEL;
    else if (strcmp(text, "TIMEOUT") == 0) kind = CASE_TIMEOUT;
    else if (strcmp(text, "PEER_CLOSE") == 0) kind = CASE_PEER_CLOSE;
    else return false;
    return true;
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
        Serial.println("A2_BUSY=YES");
        return;
    }

    char caseText[20] = {0};
    char ipText[24] = {0};
    unsigned int port = 0;
    CaseKind requested = CASE_NONE;
    if (sscanf(line, "RUN %19s %23s %u", caseText, ipText, &port) != 3 ||
        !decodeCase(caseText, requested) || !parseIPv4(ipText, targetIP) ||
        port == 0 || port > 65535)
    {
        Serial.println("A2_COMMAND=INVALID");
        return;
    }

    activeCase = requested;
    targetPort = (uint16_t)port;
    resetMetrics();
    preparePayload();
    caseStartedMs = millis();
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
    Serial.print("A14_A2_BOOT=");
    Serial.println(bootId);
}

void loop()
{
    serviceSerial();
    serviceCase();
}
