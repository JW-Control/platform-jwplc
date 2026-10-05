#include <JWPLC_Ethernet.h>
#include <Dns.h>
#include <jwplc_spi_bus.h>

static constexpr uint16_t SUCCESS_TIMEOUT_MS = 500;
static constexpr uint16_t SILENT_TIMEOUT_MS = 150;
static constexpr uint8_t EXPECTED_IP[4] = {10, 20, 30, 40};

DNSClient dnsProbe;
IPAddress dnsResult;

enum ProbePhase : uint8_t
{
    WAIT_COMMAND = 0,
    SUCCESS_PENDING,
    TIMEOUT_PENDING,
    DONE
};

static ProbePhase phase = WAIT_COMMAND;
static bool resultPrinted = false;
static bool probeFailed = false;
static int resultCode = 0;
static char serialLine[64];
static size_t serialLineLength = 0;
static uint32_t successStartedMs = 0;
static uint32_t successDurationMs = 0;
static uint32_t successPollCount = 0;
static uint32_t successPollPendingCount = 0;
static uint32_t timeoutStartedMs = 0;
static uint32_t timeoutDurationMs = 0;
static uint32_t timeoutPollCount = 0;
static uint32_t timeoutPollPendingCount = 0;
static uint32_t beginHoldMaxUs = 0;
static uint32_t pollHoldMaxUs = 0;
static uint32_t loopGapMaxUs = 0;
static uint32_t lastLoopUs = 0;
static uint32_t spiLockErrors = 0;

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
    resultCode = code;
    phase = DONE;
}

static bool parseIPv4(const char *text, IPAddress &result)
{
    unsigned int a = 0, b = 0, c = 0, d = 0;
    char tail = '\0';
    const int parsed = sscanf(text, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail);
    if (parsed != 4 || a > 255 || b > 255 || c > 255 || d > 255) return false;
    result = IPAddress((uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d);
    return true;
}

static int beginDnsRequestLocked(const char *hostname, IPAddress &result, uint16_t timeoutMs)
{
    const uint32_t startedUs = micros();
    const int state = dnsProbe.beginResolveAsync(hostname, result, timeoutMs);
    const uint32_t holdUs = (uint32_t)(micros() - startedUs);
    if (holdUs > beginHoldMaxUs) beginHoldMaxUs = holdUs;
    return state;
}

static int pollDnsLocked()
{
    const uint32_t startedUs = micros();
    const int state = dnsProbe.pollResolveAsync();
    const uint32_t holdUs = (uint32_t)(micros() - startedUs);
    if (holdUs > pollHoldMaxUs) pollHoldMaxUs = holdUs;
    return state;
}

static void startSuccess(IPAddress dnsServer)
{
    dnsProbe.begin(dnsServer);
    if (!jwplcSPI_acquire(50))
    {
        ++spiLockErrors;
        failProbe(-10);
        return;
    }
    jwplcSPI_deselectAll();
    const int state = beginDnsRequestLocked("jwplc.test", dnsResult, SUCCESS_TIMEOUT_MS);
    jwplcSPI_release();
    if (state != 0)
    {
        failProbe(-11);
        return;
    }
    successStartedMs = millis();
    loopGapMaxUs = 0;
    lastLoopUs = micros();
    phase = SUCCESS_PENDING;
}

static void startTimeout()
{
    if (!jwplcSPI_acquire(50))
    {
        ++spiLockErrors;
        failProbe(-20);
        return;
    }
    jwplcSPI_deselectAll();
    const int state = beginDnsRequestLocked("timeout.jwplc.test", dnsResult, SILENT_TIMEOUT_MS);
    jwplcSPI_release();
    if (state != 0)
    {
        failProbe(-21);
        return;
    }
    timeoutStartedMs = millis();
    loopGapMaxUs = 0;
    lastLoopUs = micros();
    phase = TIMEOUT_PENDING;
}

static void serviceProbe()
{
    if (phase == SUCCESS_PENDING)
    {
        if (!jwplcSPI_acquire(50))
        {
            ++spiLockErrors;
            return;
        }
        jwplcSPI_deselectAll();
        const int state = pollDnsLocked();
        jwplcSPI_release();
        ++successPollCount;
        if (state == 0)
        {
            ++successPollPendingCount;
            return;
        }
        successDurationMs = (uint32_t)(millis() - successStartedMs);
        if (state != 1)
        {
            failProbe(-12);
            return;
        }
        if (dnsResult[0] != EXPECTED_IP[0] || dnsResult[1] != EXPECTED_IP[1] ||
            dnsResult[2] != EXPECTED_IP[2] || dnsResult[3] != EXPECTED_IP[3])
        {
            failProbe(-13);
            return;
        }
        startTimeout();
        return;
    }

    if (phase == TIMEOUT_PENDING)
    {
        if (!jwplcSPI_acquire(50))
        {
            ++spiLockErrors;
            return;
        }
        jwplcSPI_deselectAll();
        const int state = pollDnsLocked();
        jwplcSPI_release();
        ++timeoutPollCount;
        if (state == 0)
        {
            ++timeoutPollPendingCount;
            return;
        }
        timeoutDurationMs = (uint32_t)(millis() - timeoutStartedMs);
        if (state != -1)
        {
            failProbe(-22);
            return;
        }
        resultCode = 1;
        phase = DONE;
    }
}

static void processCommand(const char *line)
{
    if (phase != WAIT_COMMAND) return;
    static const char prefix[] = "RUN ";
    if (strncmp(line, prefix, sizeof(prefix) - 1) != 0) return;
    IPAddress dnsServer;
    if (!parseIPv4(line + sizeof(prefix) - 1, dnsServer))
    {
        failProbe(-30);
        return;
    }
    startSuccess(dnsServer);
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char ch = (char)Serial.read();
        if (ch == '\r') continue;
        if (ch == '\n')
        {
            serialLine[serialLineLength] = '\0';
            processCommand(serialLine);
            serialLineLength = 0;
            continue;
        }
        if (serialLineLength + 1 < sizeof(serialLine))
            serialLine[serialLineLength++] = ch;
        else
            serialLineLength = 0;
    }
}

static void printResultIfReady()
{
    if (phase != DONE || resultPrinted) return;
    resultPrinted = true;
    Serial.println("NB2_DNS_PHYSICAL_RESULT=BEGIN");
    Serial.print("RESULT_CODE="); Serial.println(resultCode);
    Serial.print("PROBE_FAILED="); Serial.println(probeFailed ? "YES" : "NO");
    Serial.print("SUCCESS_RESULT_IP="); Serial.println(dnsResult);
    Serial.print("SUCCESS_DURATION_MS="); Serial.println(successDurationMs);
    Serial.print("SUCCESS_POLL_COUNT="); Serial.println(successPollCount);
    Serial.print("SUCCESS_POLL_PENDING_COUNT="); Serial.println(successPollPendingCount);
    Serial.print("TIMEOUT_DURATION_MS="); Serial.println(timeoutDurationMs);
    Serial.print("TIMEOUT_POLL_COUNT="); Serial.println(timeoutPollCount);
    Serial.print("TIMEOUT_POLL_PENDING_COUNT="); Serial.println(timeoutPollPendingCount);
    Serial.print("DNS_BEGIN_HOLD_MAX_US="); Serial.println(beginHoldMaxUs);
    Serial.print("DNS_POLL_HOLD_MAX_US="); Serial.println(pollHoldMaxUs);
    Serial.print("LOOP_GAP_MAX_US="); Serial.println(loopGapMaxUs);
    Serial.print("SPI_LOCK_ERRORS="); Serial.println(spiLockErrors);
    Serial.println("NB2_DNS_PHYSICAL_RESULT=END");
}

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    updateLoopGap();
    serviceSerial();
    if (JWPLC_Ethernet.isReady())
    {
        static bool announced = false;
        if (!announced)
        {
            announced = true;
            Serial.print("NB2_DNS_PROBE_READY=YES IP=");
            Serial.println(JWPLC_Ethernet.localIP());
        }
        serviceProbe();
    }
    printResultIfReady();
    delay(0);
}
