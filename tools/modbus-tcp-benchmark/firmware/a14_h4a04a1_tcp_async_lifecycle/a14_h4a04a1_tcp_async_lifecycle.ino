/*
  A14 H4A0.4-A1 - TCP cooperative lifecycle qualification.

  Product source is the canonical JWPLC/2.1.0 package.
  No temporary product patches.

  This firmware explicitly exercises:
  - beginConnectAsync / pollConnectAsync / connectAsyncInProgress / cancelConnectAsync
  - beginFlushAsync / pollFlushAsync / flushAsyncInProgress / cancelFlushAsync
  - beginStopAsync / pollStopAsync / stopAsyncInProgress / cancelStopAsync

  Serial commands:
    RUN <IPv4> <port>
      Connect async -> flush async -> stop async.

    CANCEL_CONNECT <IPv4> <port>
      Start async connect, verify pending when possible, then cancel.

    CANCEL_STOP <IPv4> <port>
      Connect async, begin async stop, then force cancelStopAsync.

    S
      Print readiness snapshot.
*/

#include <JWPLC_Ethernet.h>

static constexpr uint32_t OP_TIMEOUT_MS = 3000;
static constexpr uint32_t API_CALL_LIMIT_US = 10000;

enum ProbeMode : uint8_t
{
    PROBE_IDLE = 0,
    PROBE_RUN_CONNECT_BEGIN,
    PROBE_RUN_CONNECT_POLL,
    PROBE_RUN_FLUSH_BEGIN,
    PROBE_RUN_FLUSH_POLL,
    PROBE_RUN_STOP_BEGIN,
    PROBE_RUN_STOP_POLL,
    PROBE_CANCEL_CONNECT_BEGIN,
    PROBE_CANCEL_STOP_CONNECT_BEGIN,
    PROBE_CANCEL_STOP_CONNECT_POLL,
    PROBE_CANCEL_STOP_BEGIN
};

static EthernetClient probeClient;
static ProbeMode probeMode = PROBE_IDLE;

static IPAddress targetIP;
static uint16_t targetPort = 0;

static char serialLine[96];
static size_t serialLineLen = 0;

static uint32_t phaseStartedMs = 0;
static uint32_t spiLockErrors = 0;

static uint32_t connectBeginUs = 0;
static uint32_t connectPollCount = 0;
static uint32_t connectPollMaxUs = 0;
static uint32_t connectTotalMs = 0;
static bool connectPendingObserved = false;

static uint32_t flushBeginUs = 0;
static uint32_t flushPollCount = 0;
static uint32_t flushPollMaxUs = 0;
static uint32_t flushTotalMs = 0;
static bool flushPendingObserved = false;

static uint32_t stopBeginUs = 0;
static uint32_t stopPollCount = 0;
static uint32_t stopPollMaxUs = 0;
static uint32_t stopTotalMs = 0;
static bool stopPendingObserved = false;

static uint32_t loopTicks = 0;
static uint32_t loopTicksAtProbeStart = 0;

static uint32_t cancelCallUs = 0;
static bool cancelPendingObserved = false;

static void resetMetrics()
{
    connectBeginUs = 0;
    connectPollCount = 0;
    connectPollMaxUs = 0;
    connectTotalMs = 0;
    connectPendingObserved = false;

    flushBeginUs = 0;
    flushPollCount = 0;
    flushPollMaxUs = 0;
    flushTotalMs = 0;
    flushPendingObserved = false;

    stopBeginUs = 0;
    stopPollCount = 0;
    stopPollMaxUs = 0;
    stopTotalMs = 0;
    stopPendingObserved = false;

    cancelCallUs = 0;
    cancelPendingObserved = false;

    spiLockErrors = 0;
    loopTicksAtProbeStart = loopTicks;
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

static bool parseIPv4(
    const char *text,
    IPAddress &ip)
{
    unsigned int a = 0;
    unsigned int b = 0;
    unsigned int c = 0;
    unsigned int d = 0;

    if (sscanf(
            text,
            "%u.%u.%u.%u",
            &a,
            &b,
            &c,
            &d) != 4)
    {
        return false;
    }

    if (
        a > 255 ||
        b > 255 ||
        c > 255 ||
        d > 255)
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

static void printReady()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println(" A14 H4A0.4-A1 TCP ASYNC SNAPSHOT");
    Serial.println("========================================");

    Serial.print("ASYNC_ETH_READY=");
    Serial.println(
        JWPLC_Ethernet.isReady()
            ? "YES"
            : "NO");

    Serial.print("ASYNC_ETH_LINK=");
    Serial.println(
        JWPLC_Ethernet.linkUp()
            ? "UP"
            : "DOWN");

    Serial.print("ASYNC_IP=");
    Serial.println(
        JWPLC_Ethernet.localIP());

    Serial.print("ASYNC_PROBE_IDLE=");
    Serial.println(
        probeMode == PROBE_IDLE
            ? "YES"
            : "NO");

    Serial.println("H4A04A1_SNAPSHOT=END");
}

static void printCommonResult(
    const char *caseName,
    bool pass)
{
    Serial.print("ASYNC_CASE=");
    Serial.println(caseName);

    Serial.print("ASYNC_CONNECT_BEGIN_US=");
    Serial.println(connectBeginUs);
    Serial.print("ASYNC_CONNECT_POLL_COUNT=");
    Serial.println(connectPollCount);
    Serial.print("ASYNC_CONNECT_POLL_MAX_US=");
    Serial.println(connectPollMaxUs);
    Serial.print("ASYNC_CONNECT_TOTAL_MS=");
    Serial.println(connectTotalMs);
    Serial.print("ASYNC_CONNECT_PENDING_OBSERVED=");
    Serial.println(connectPendingObserved ? "YES" : "NO");

    Serial.print("ASYNC_FLUSH_BEGIN_US=");
    Serial.println(flushBeginUs);
    Serial.print("ASYNC_FLUSH_POLL_COUNT=");
    Serial.println(flushPollCount);
    Serial.print("ASYNC_FLUSH_POLL_MAX_US=");
    Serial.println(flushPollMaxUs);
    Serial.print("ASYNC_FLUSH_TOTAL_MS=");
    Serial.println(flushTotalMs);
    Serial.print("ASYNC_FLUSH_PENDING_OBSERVED=");
    Serial.println(flushPendingObserved ? "YES" : "NO");

    Serial.print("ASYNC_STOP_BEGIN_US=");
    Serial.println(stopBeginUs);
    Serial.print("ASYNC_STOP_POLL_COUNT=");
    Serial.println(stopPollCount);
    Serial.print("ASYNC_STOP_POLL_MAX_US=");
    Serial.println(stopPollMaxUs);
    Serial.print("ASYNC_STOP_TOTAL_MS=");
    Serial.println(stopTotalMs);
    Serial.print("ASYNC_STOP_PENDING_OBSERVED=");
    Serial.println(stopPendingObserved ? "YES" : "NO");

    Serial.print("ASYNC_CANCEL_CALL_US=");
    Serial.println(cancelCallUs);
    Serial.print("ASYNC_CANCEL_PENDING_OBSERVED=");
    Serial.println(cancelPendingObserved ? "YES" : "NO");

    Serial.print("ASYNC_SPI_LOCK_ERRORS=");
    Serial.println(spiLockErrors);

    Serial.print("ASYNC_LOOP_TICKS=");
    Serial.println(
        (uint32_t)(
            loopTicks -
            loopTicksAtProbeStart));

    Serial.print("ASYNC_API_CALL_LIMIT_US=");
    Serial.println(API_CALL_LIMIT_US);

    Serial.print("ASYNC_CASE_RESULT=");
    Serial.println(pass ? "PASS" : "FAIL");

    Serial.println("H4A04A1_CASE=END");
}

static void failProbe(
    const char *caseName,
    const char *reason)
{
    Serial.print("ASYNC_FAILURE_REASON=");
    Serial.println(reason);

    if (acquireEthernet())
    {
        probeClient.cancelStopAsync();
        probeClient.cancelConnectAsync();
        releaseEthernet();
    }

    printCommonResult(
        caseName,
        false);

    probeMode = PROBE_IDLE;
}

static bool apiDurationAllowed(
    uint32_t durationUs)
{
    return durationUs <= API_CALL_LIMIT_US;
}

static void startRun()
{
    resetMetrics();
    phaseStartedMs = millis();
    probeMode = PROBE_RUN_CONNECT_BEGIN;
}

static void startCancelConnect()
{
    resetMetrics();
    phaseStartedMs = millis();
    probeMode = PROBE_CANCEL_CONNECT_BEGIN;
}

static void startCancelStop()
{
    resetMetrics();
    phaseStartedMs = millis();
    probeMode = PROBE_CANCEL_STOP_CONNECT_BEGIN;
}

static void serviceRunProbe()
{
    if (probeMode == PROBE_RUN_CONNECT_BEGIN)
    {
        if (!acquireEthernet())
        {
            return;
        }

        const uint32_t startedUs = micros();
        const int state =
            probeClient.beginConnectAsync(
                targetIP,
                targetPort);
        connectBeginUs =
            (uint32_t)(
                micros() -
                startedUs);

        const bool inProgress =
            probeClient.connectAsyncInProgress();

        releaseEthernet();

        connectPendingObserved =
            state == 0 &&
            inProgress;

        if (!apiDurationAllowed(connectBeginUs))
        {
            failProbe(
                "RUN",
                "CONNECT_BEGIN_TOO_SLOW");
            return;
        }

        if (state < 0)
        {
            failProbe(
                "RUN",
                "CONNECT_BEGIN_FAILED");
            return;
        }

        if (state > 0)
        {
            connectTotalMs =
                (uint32_t)(
                    millis() -
                    phaseStartedMs);
            phaseStartedMs = millis();
            probeMode =
                PROBE_RUN_FLUSH_BEGIN;
            return;
        }

        probeMode =
            PROBE_RUN_CONNECT_POLL;
        return;
    }

    if (probeMode == PROBE_RUN_CONNECT_POLL)
    {
        if (
            (uint32_t)(
                millis() -
                phaseStartedMs) >
            OP_TIMEOUT_MS)
        {
            failProbe(
                "RUN",
                "CONNECT_TIMEOUT");
            return;
        }

        if (!acquireEthernet())
        {
            return;
        }

        const uint32_t startedUs = micros();
        const int state =
            probeClient.pollConnectAsync();
        const uint32_t durationUs =
            (uint32_t)(
                micros() -
                startedUs);

        ++connectPollCount;

        if (durationUs > connectPollMaxUs)
        {
            connectPollMaxUs = durationUs;
        }

        const bool inProgress =
            probeClient.connectAsyncInProgress();

        releaseEthernet();

        if (
            state == 0 &&
            inProgress)
        {
            connectPendingObserved = true;
        }

        if (!apiDurationAllowed(durationUs))
        {
            failProbe(
                "RUN",
                "CONNECT_POLL_TOO_SLOW");
            return;
        }

        if (state < 0)
        {
            failProbe(
                "RUN",
                "CONNECT_POLL_FAILED");
            return;
        }

        if (state > 0)
        {
            connectTotalMs =
                (uint32_t)(
                    millis() -
                    phaseStartedMs);
            phaseStartedMs = millis();
            probeMode =
                PROBE_RUN_FLUSH_BEGIN;
        }

        return;
    }

    if (probeMode == PROBE_RUN_FLUSH_BEGIN)
    {
        if (!acquireEthernet())
        {
            return;
        }

        const uint32_t startedUs = micros();
        const int state =
            probeClient.beginFlushAsync();
        flushBeginUs =
            (uint32_t)(
                micros() -
                startedUs);
        flushPendingObserved =
            probeClient.flushAsyncInProgress();

        releaseEthernet();

        if (!apiDurationAllowed(flushBeginUs))
        {
            failProbe(
                "RUN",
                "FLUSH_BEGIN_TOO_SLOW");
            return;
        }

        if (state < 0)
        {
            failProbe(
                "RUN",
                "FLUSH_BEGIN_FAILED");
            return;
        }

        if (state > 0)
        {
            flushTotalMs =
                (uint32_t)(
                    millis() -
                    phaseStartedMs);

            // Exercise the cancellation API in an already-complete state.
            probeClient.cancelFlushAsync();

            phaseStartedMs = millis();
            probeMode =
                PROBE_RUN_STOP_BEGIN;
            return;
        }

        probeMode =
            PROBE_RUN_FLUSH_POLL;
        return;
    }

    if (probeMode == PROBE_RUN_FLUSH_POLL)
    {
        if (
            (uint32_t)(
                millis() -
                phaseStartedMs) >
            OP_TIMEOUT_MS)
        {
            failProbe(
                "RUN",
                "FLUSH_TIMEOUT");
            return;
        }

        if (!acquireEthernet())
        {
            return;
        }

        const uint32_t startedUs = micros();
        const int state =
            probeClient.pollFlushAsync();
        const uint32_t durationUs =
            (uint32_t)(
                micros() -
                startedUs);

        ++flushPollCount;

        if (durationUs > flushPollMaxUs)
        {
            flushPollMaxUs = durationUs;
        }

        if (probeClient.flushAsyncInProgress())
        {
            flushPendingObserved = true;
        }

        releaseEthernet();

        if (!apiDurationAllowed(durationUs))
        {
            failProbe(
                "RUN",
                "FLUSH_POLL_TOO_SLOW");
            return;
        }

        if (state < 0)
        {
            failProbe(
                "RUN",
                "FLUSH_POLL_FAILED");
            return;
        }

        if (state > 0)
        {
            flushTotalMs =
                (uint32_t)(
                    millis() -
                    phaseStartedMs);

            probeClient.cancelFlushAsync();

            phaseStartedMs = millis();
            probeMode =
                PROBE_RUN_STOP_BEGIN;
        }

        return;
    }

    if (probeMode == PROBE_RUN_STOP_BEGIN)
    {
        if (!acquireEthernet())
        {
            return;
        }

        const uint32_t startedUs = micros();
        const int state =
            probeClient.beginStopAsync();
        stopBeginUs =
            (uint32_t)(
                micros() -
                startedUs);
        stopPendingObserved =
            probeClient.stopAsyncInProgress();

        releaseEthernet();

        if (!apiDurationAllowed(stopBeginUs))
        {
            failProbe(
                "RUN",
                "STOP_BEGIN_TOO_SLOW");
            return;
        }

        if (state < 0)
        {
            failProbe(
                "RUN",
                "STOP_BEGIN_FAILED");
            return;
        }

        if (state > 0)
        {
            stopTotalMs =
                (uint32_t)(
                    millis() -
                    phaseStartedMs);

            const bool pass =
                spiLockErrors == 0;

            printCommonResult(
                "RUN",
                pass);

            probeMode = PROBE_IDLE;
            return;
        }

        probeMode =
            PROBE_RUN_STOP_POLL;
        return;
    }

    if (probeMode == PROBE_RUN_STOP_POLL)
    {
        if (
            (uint32_t)(
                millis() -
                phaseStartedMs) >
            OP_TIMEOUT_MS)
        {
            failProbe(
                "RUN",
                "STOP_TIMEOUT");
            return;
        }

        if (!acquireEthernet())
        {
            return;
        }

        const uint32_t startedUs = micros();
        const int state =
            probeClient.pollStopAsync();
        const uint32_t durationUs =
            (uint32_t)(
                micros() -
                startedUs);

        ++stopPollCount;

        if (durationUs > stopPollMaxUs)
        {
            stopPollMaxUs = durationUs;
        }

        if (probeClient.stopAsyncInProgress())
        {
            stopPendingObserved = true;
        }

        releaseEthernet();

        if (!apiDurationAllowed(durationUs))
        {
            failProbe(
                "RUN",
                "STOP_POLL_TOO_SLOW");
            return;
        }

        if (state < 0)
        {
            failProbe(
                "RUN",
                "STOP_POLL_FORCED_CLOSE");
            return;
        }

        if (state > 0)
        {
            stopTotalMs =
                (uint32_t)(
                    millis() -
                    phaseStartedMs);

            const bool pass =
                !probeClient.stopAsyncInProgress() &&
                spiLockErrors == 0;

            printCommonResult(
                "RUN",
                pass);

            probeMode = PROBE_IDLE;
        }

        return;
    }
}

static void serviceCancelConnectProbe()
{
    if (probeMode != PROBE_CANCEL_CONNECT_BEGIN)
    {
        return;
    }

    if (!acquireEthernet())
    {
        return;
    }

    const uint32_t startedUs = micros();
    const int state =
        probeClient.beginConnectAsync(
            targetIP,
            targetPort);
    connectBeginUs =
        (uint32_t)(
            micros() -
            startedUs);

    const bool inProgress =
        probeClient.connectAsyncInProgress();

    connectPendingObserved =
        state == 0 &&
        inProgress;

    const uint32_t cancelStartedUs = micros();
    probeClient.cancelConnectAsync();
    cancelCallUs =
        (uint32_t)(
            micros() -
            cancelStartedUs);

    const bool stillInProgress =
        probeClient.connectAsyncInProgress();

    releaseEthernet();

    cancelPendingObserved =
        connectPendingObserved;

    const bool pass =
        state == 0 &&
        connectPendingObserved &&
        !stillInProgress &&
        apiDurationAllowed(connectBeginUs) &&
        apiDurationAllowed(cancelCallUs) &&
        spiLockErrors == 0;

    printCommonResult(
        "CANCEL_CONNECT",
        pass);

    probeMode = PROBE_IDLE;
}

static void serviceCancelStopProbe()
{
    if (probeMode == PROBE_CANCEL_STOP_CONNECT_BEGIN)
    {
        if (!acquireEthernet())
        {
            return;
        }

        const uint32_t startedUs = micros();
        const int state =
            probeClient.beginConnectAsync(
                targetIP,
                targetPort);
        connectBeginUs =
            (uint32_t)(
                micros() -
                startedUs);

        connectPendingObserved =
            state == 0 &&
            probeClient.connectAsyncInProgress();

        releaseEthernet();

        if (!apiDurationAllowed(connectBeginUs))
        {
            failProbe(
                "CANCEL_STOP",
                "CONNECT_BEGIN_TOO_SLOW");
            return;
        }

        if (state < 0)
        {
            failProbe(
                "CANCEL_STOP",
                "CONNECT_BEGIN_FAILED");
            return;
        }

        if (state > 0)
        {
            phaseStartedMs = millis();
            probeMode =
                PROBE_CANCEL_STOP_BEGIN;
            return;
        }

        probeMode =
            PROBE_CANCEL_STOP_CONNECT_POLL;
        return;
    }

    if (probeMode == PROBE_CANCEL_STOP_CONNECT_POLL)
    {
        if (
            (uint32_t)(
                millis() -
                phaseStartedMs) >
            OP_TIMEOUT_MS)
        {
            failProbe(
                "CANCEL_STOP",
                "CONNECT_TIMEOUT");
            return;
        }

        if (!acquireEthernet())
        {
            return;
        }

        const uint32_t startedUs = micros();
        const int state =
            probeClient.pollConnectAsync();
        const uint32_t durationUs =
            (uint32_t)(
                micros() -
                startedUs);

        ++connectPollCount;

        if (durationUs > connectPollMaxUs)
        {
            connectPollMaxUs = durationUs;
        }

        if (
            state == 0 &&
            probeClient.connectAsyncInProgress())
        {
            connectPendingObserved = true;
        }

        releaseEthernet();

        if (!apiDurationAllowed(durationUs))
        {
            failProbe(
                "CANCEL_STOP",
                "CONNECT_POLL_TOO_SLOW");
            return;
        }

        if (state < 0)
        {
            failProbe(
                "CANCEL_STOP",
                "CONNECT_POLL_FAILED");
            return;
        }

        if (state > 0)
        {
            phaseStartedMs = millis();
            probeMode =
                PROBE_CANCEL_STOP_BEGIN;
        }

        return;
    }

    if (probeMode == PROBE_CANCEL_STOP_BEGIN)
    {
        if (!acquireEthernet())
        {
            return;
        }

        const uint32_t startedUs = micros();
        const int stopState =
            probeClient.beginStopAsync();
        stopBeginUs =
            (uint32_t)(
                micros() -
                startedUs);

        stopPendingObserved =
            probeClient.stopAsyncInProgress();

        const uint32_t cancelStartedUs = micros();
        probeClient.cancelStopAsync();
        cancelCallUs =
            (uint32_t)(
                micros() -
                cancelStartedUs);

        const bool stillPending =
            probeClient.stopAsyncInProgress();

        releaseEthernet();

        cancelPendingObserved =
            stopState == 0 &&
            stopPendingObserved;

        const bool pass =
            stopState == 0 &&
            stopPendingObserved &&
            !stillPending &&
            apiDurationAllowed(stopBeginUs) &&
            apiDurationAllowed(cancelCallUs) &&
            spiLockErrors == 0;

        printCommonResult(
            "CANCEL_STOP",
            pass);

        probeMode = PROBE_IDLE;
    }
}

static void processCommand(
    char *line)
{
    if (strcmp(line, "S") == 0)
    {
        printReady();
        return;
    }

    if (probeMode != PROBE_IDLE)
    {
        Serial.println("ASYNC_COMMAND=BUSY");
        return;
    }

    char command[24] = {0};
    char ipText[24] = {0};
    unsigned int port = 0;

    if (
        sscanf(
            line,
            "%23s %23s %u",
            command,
            ipText,
            &port) != 3)
    {
        Serial.println("ASYNC_COMMAND=INVALID");
        return;
    }

    IPAddress parsedIP;

    if (
        !parseIPv4(
            ipText,
            parsedIP) ||
        port == 0 ||
        port > 65535)
    {
        Serial.println("ASYNC_COMMAND=INVALID_TARGET");
        return;
    }

    targetIP = parsedIP;
    targetPort = (uint16_t)port;

    if (strcmp(command, "RUN") == 0)
    {
        Serial.println("ASYNC_COMMAND_RUN=ACK");
        startRun();
        return;
    }

    if (strcmp(command, "CANCEL_CONNECT") == 0)
    {
        Serial.println("ASYNC_COMMAND_CANCEL_CONNECT=ACK");
        startCancelConnect();
        return;
    }

    if (strcmp(command, "CANCEL_STOP") == 0)
    {
        Serial.println("ASYNC_COMMAND_CANCEL_STOP=ACK");
        startCancelStop();
        return;
    }

    Serial.println("ASYNC_COMMAND=UNKNOWN");
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c =
            (char)Serial.read();

        if (c == '\r')
        {
            continue;
        }

        if (c == '\n')
        {
            serialLine[serialLineLen] = '\0';

            if (serialLineLen > 0)
            {
                processCommand(serialLine);
            }

            serialLineLen = 0;
            continue;
        }

        if (
            serialLineLen + 1 <
            sizeof(serialLine))
        {
            serialLine[serialLineLen++] = c;
        }
        else
        {
            serialLineLen = 0;
            Serial.println(
                "ASYNC_COMMAND=LINE_TOO_LONG");
        }
    }
}

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    ++loopTicks;

    serviceSerial();

    serviceRunProbe();
    serviceCancelConnectProbe();
    serviceCancelStopProbe();

    yield();
}
