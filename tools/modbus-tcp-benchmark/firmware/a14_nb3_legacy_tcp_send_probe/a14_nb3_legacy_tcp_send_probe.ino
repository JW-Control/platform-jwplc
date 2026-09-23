#include <JWPLC_Ethernet.h>
#include <jwplc_spi_bus.h>

static constexpr uint16_t NORMAL_PORT = 5004;
static constexpr uint16_t BACKPRESSURE_PORT = 5005;
static constexpr size_t WRITE_BYTES = 1024;
static constexpr uint8_t NORMAL_WRITE_COUNT = 4;
static constexpr uint16_t WRITE_TIMEOUT_MS = 250;
static constexpr uint16_t CONNECT_TIMEOUT_MS = 1000;
static constexpr uint16_t BACKPRESSURE_MAX_WRITES = 1024;

static EthernetClient normalClient;
static EthernetClient backpressureClient;
static uint8_t payload[WRITE_BYTES];

enum ProbeState : uint8_t
{
    WAIT_COMMAND = 0,
    NORMAL_CONNECT,
    NORMAL_SEND,
    NORMAL_CLOSE,
    BACKPRESSURE_CONNECT,
    BACKPRESSURE_SEND,
    DONE
};

static ProbeState state = WAIT_COMMAND;
static IPAddress targetIp;
static bool commandReceived = false;
static bool resultPrinted = false;
static bool probeFailed = false;
static int resultCode = 0;

static uint32_t lastReadyPrintMs = 0;
static char serialLine[64] = {};
static size_t serialLineLength = 0;

static uint32_t spiLockErrors = 0;

static bool normalConnected = false;
static uint8_t normalWriteCalls = 0;
static uint32_t normalBytes = 0;
static uint32_t normalWriteHoldMaxUs = 0;

static bool backpressureConnected = false;
static uint16_t backpressureSuccessfulWrites = 0;
static uint32_t backpressureBytes = 0;
static bool backpressureTimeoutObserved = false;
static size_t backpressureTimeoutWriteReturn = 9999;
static uint32_t backpressureTimeoutHoldUs = 0;
static bool backpressureConnectedAfterTimeout = false;
static uint32_t backpressureSuccessfulHoldMaxUs = 0;

static uint32_t runStartMs = 0;
static uint32_t runDurationMs = 0;

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

static void failProbe(int code)
{
    probeFailed = true;
    resultCode = code;
    runDurationMs = millis() - runStartMs;
    state = DONE;
}

static bool parseRunCommand(const char *line, IPAddress &ip)
{
    unsigned int a = 0;
    unsigned int b = 0;
    unsigned int c = 0;
    unsigned int d = 0;

    if (
        sscanf(
            line,
            "RUN %u.%u.%u.%u",
            &a,
            &b,
            &c,
            &d) != 4)
    {
        return false;
    }

    if (a > 255 || b > 255 || c > 255 || d > 255)
    {
        return false;
    }

    ip = IPAddress(
        (uint8_t)a,
        (uint8_t)b,
        (uint8_t)c,
        (uint8_t)d);

    return true;
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char ch = (char)Serial.read();

        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            serialLine[serialLineLength] = '\0';

            if (
                state == WAIT_COMMAND &&
                parseRunCommand(serialLine, targetIp))
            {
                commandReceived = true;
                runStartMs = millis();
                state = NORMAL_CONNECT;
            }

            serialLineLength = 0;
            continue;
        }

        if (serialLineLength + 1 < sizeof(serialLine))
        {
            serialLine[serialLineLength++] = ch;
        }
        else
        {
            serialLineLength = 0;
        }
    }
}

static void announceReady()
{
    if (
        commandReceived ||
        !JWPLC_Ethernet.isReady())
    {
        return;
    }

    const uint32_t nowMs = millis();

    if (
        lastReadyPrintMs != 0 &&
        (uint32_t)(nowMs - lastReadyPrintMs) < 500)
    {
        return;
    }

    lastReadyPrintMs = nowMs;

    Serial.print("NB3_F2_PROBE_READY=YES IP=");
    Serial.println(JWPLC_Ethernet.localIP());
    Serial.print("NB3_F2_NORMAL_PORT=");
    Serial.println(NORMAL_PORT);
    Serial.print("NB3_F2_BACKPRESSURE_PORT=");
    Serial.println(BACKPRESSURE_PORT);
}

static bool connectClient(
    EthernetClient &client,
    uint16_t port)
{
    client.setConnectionTimeout(CONNECT_TIMEOUT_MS);

    if (!acquireSpi())
    {
        return false;
    }

    const int connected =
        client.connect(targetIp, port);

    releaseSpi();

    client.setConnectionTimeout(WRITE_TIMEOUT_MS);

    return connected == 1;
}

static size_t writeClient(
    EthernetClient &client,
    uint32_t &holdUs)
{
    if (!acquireSpi())
    {
        holdUs = 0;
        return 0;
    }

    const uint32_t startedUs = micros();
    const size_t written =
        client.write(payload, sizeof(payload));
    holdUs = (uint32_t)(micros() - startedUs);

    releaseSpi();

    return written;
}

static bool clientConnected(
    EthernetClient &client)
{
    if (!acquireSpi())
    {
        return false;
    }

    const bool connected =
        client.connected() != 0;

    releaseSpi();
    return connected;
}

static void stopClient(EthernetClient &client)
{
    if (!client)
    {
        return;
    }

    if (!acquireSpi())
    {
        return;
    }

    client.stop();
    releaseSpi();
}

static void serviceProbe()
{
    if (state == WAIT_COMMAND || state == DONE)
    {
        return;
    }

    if (state == NORMAL_CONNECT)
    {
        normalConnected =
            connectClient(normalClient, NORMAL_PORT);

        if (!normalConnected)
        {
            failProbe(-10);
            return;
        }

        state = NORMAL_SEND;
        return;
    }

    if (state == NORMAL_SEND)
    {
        uint32_t holdUs = 0;
        const size_t written =
            writeClient(normalClient, holdUs);

        if (holdUs > normalWriteHoldMaxUs)
        {
            normalWriteHoldMaxUs = holdUs;
        }

        if (written != sizeof(payload))
        {
            failProbe(-11);
            return;
        }

        ++normalWriteCalls;
        normalBytes += (uint32_t)written;

        if (normalWriteCalls >= NORMAL_WRITE_COUNT)
        {
            state = NORMAL_CLOSE;
        }

        return;
    }

    if (state == NORMAL_CLOSE)
    {
        stopClient(normalClient);
        state = BACKPRESSURE_CONNECT;
        return;
    }

    if (state == BACKPRESSURE_CONNECT)
    {
        backpressureConnected =
            connectClient(
                backpressureClient,
                BACKPRESSURE_PORT);

        if (!backpressureConnected)
        {
            failProbe(-20);
            return;
        }

        state = BACKPRESSURE_SEND;
        return;
    }

    if (state == BACKPRESSURE_SEND)
    {
        uint32_t holdUs = 0;
        const size_t written =
            writeClient(
                backpressureClient,
                holdUs);

        if (written == sizeof(payload))
        {
            ++backpressureSuccessfulWrites;
            backpressureBytes += (uint32_t)written;

            if (holdUs > backpressureSuccessfulHoldMaxUs)
            {
                backpressureSuccessfulHoldMaxUs = holdUs;
            }

            if (
                backpressureSuccessfulWrites >=
                BACKPRESSURE_MAX_WRITES)
            {
                failProbe(-30);
            }

            return;
        }

        backpressureTimeoutObserved = true;
        backpressureTimeoutWriteReturn = written;
        backpressureTimeoutHoldUs = holdUs;
        backpressureConnectedAfterTimeout =
            clientConnected(backpressureClient);

        runDurationMs = millis() - runStartMs;
        resultCode = 1;
        state = DONE;
        return;
    }
}

static void printResultIfDone()
{
    if (state != DONE || resultPrinted)
    {
        return;
    }

    resultPrinted = true;

    Serial.println("NB3_F2_RESULT=BEGIN");
    Serial.print("RESULT_CODE=");
    Serial.println(resultCode);
    Serial.print("PROBE_FAILED=");
    Serial.println(probeFailed ? "YES" : "NO");
    Serial.print("NORMAL_CONNECTED=");
    Serial.println(normalConnected ? "YES" : "NO");
    Serial.print("NORMAL_WRITE_CALLS=");
    Serial.println(normalWriteCalls);
    Serial.print("NORMAL_BYTES=");
    Serial.println(normalBytes);
    Serial.print("NORMAL_WRITE_HOLD_MAX_US=");
    Serial.println(normalWriteHoldMaxUs);
    Serial.print("BACKPRESSURE_CONNECTED=");
    Serial.println(backpressureConnected ? "YES" : "NO");
    Serial.print("BACKPRESSURE_SUCCESSFUL_WRITES=");
    Serial.println(backpressureSuccessfulWrites);
    Serial.print("BACKPRESSURE_BYTES=");
    Serial.println(backpressureBytes);
    Serial.print("BACKPRESSURE_SUCCESSFUL_HOLD_MAX_US=");
    Serial.println(backpressureSuccessfulHoldMaxUs);
    Serial.print("BACKPRESSURE_TIMEOUT_OBSERVED=");
    Serial.println(
        backpressureTimeoutObserved ? "YES" : "NO");
    Serial.print("BACKPRESSURE_TIMEOUT_WRITE_RETURN=");
    Serial.println(
        (unsigned long)backpressureTimeoutWriteReturn);
    Serial.print("BACKPRESSURE_TIMEOUT_HOLD_US=");
    Serial.println(backpressureTimeoutHoldUs);
    Serial.print("BACKPRESSURE_CONNECTED_AFTER_TIMEOUT=");
    Serial.println(
        backpressureConnectedAfterTimeout
            ? "YES"
            : "NO");
    Serial.print("WRITE_TIMEOUT_MS=");
    Serial.println(WRITE_TIMEOUT_MS);
    Serial.print("SPI_LOCK_ERRORS=");
    Serial.println(spiLockErrors);
    Serial.print("RUN_DURATION_MS=");
    Serial.println(runDurationMs);
    Serial.println("NB3_F2_RESULT=END");
}

void setup()
{
    Serial.begin(115200);

    for (size_t i = 0; i < sizeof(payload); ++i)
    {
        payload[i] = (uint8_t)(i & 0xFFU);
    }
}

void loop()
{
    serviceSerial();
    announceReady();
    serviceProbe();
    printResultIfDone();
    delay(0);
}
