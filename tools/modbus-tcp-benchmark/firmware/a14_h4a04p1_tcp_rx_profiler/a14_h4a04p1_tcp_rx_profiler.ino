/*
  A14 H4A0.4-P1 - Package-first TCP RX bottleneck profiler.

  Same source is compiled twice:
  - BASE: JWPLC_ETHERNET_ENABLE_PROFILE_HOOKS=0
  - PROFILE: JWPLC_ETHERNET_ENABLE_PROFILE_HOOKS=1

  Product source is always JWPLC/2.1.0. No temporary product patches.

  TCP port: 5001
  TCP command:
    'R' -> enter TCP_RX and reset counters

  Serial commands:
    R -> reset measurement/profile counters
    F -> freeze TCP RX accounting without closing the client
    S -> print snapshot
*/

#include <JWPLC_Ethernet.h>
#include <utility/w5100.h>

#ifndef JWPLC_H4A04P3_VERIFY_PAYLOAD
#define JWPLC_H4A04P3_VERIFY_PAYLOAD 0
#endif

#ifndef JWPLC_H4A04P6_DIRECT_READ
#define JWPLC_H4A04P6_DIRECT_READ 0
#endif

#ifndef JWPLC_H4A04P7_DEFER_TCP_COMMIT
#define JWPLC_H4A04P7_DEFER_TCP_COMMIT 0
#endif

#ifndef JWPLC_H4A04P8_REUSE_CONNECTED_RESULT
#define JWPLC_H4A04P8_REUSE_CONNECTED_RESULT 0
#endif

#ifndef JWPLC_H4A04P9_RX_MAX_CHUNKS
#define JWPLC_H4A04P9_RX_MAX_CHUNKS 8
#endif

#ifndef JWPLC_G2_TCP_INT_GUIDED
#define JWPLC_G2_TCP_INT_GUIDED 0
#endif

static constexpr uint16_t TCP_PORT = 5001;
static constexpr uint8_t ETH_INT_PIN = 15;
static constexpr uint8_t TCP_INT_MASK =
    SnIR::RECV | SnIR::DISCON | SnIR::TIMEOUT;
static constexpr size_t TCP_BUFFER_BYTES = 1024;
static constexpr uint8_t TCP_RX_MAX_CHUNKS_PER_LOCK =
    JWPLC_H4A04P9_RX_MAX_CHUNKS;

enum BenchMode : uint8_t
{
    MODE_IDLE = 0,
    MODE_TCP_RX
};

static EthernetServer tcpServer(TCP_PORT);
static EthernetClient tcpClient;

static BenchMode mode = MODE_IDLE;
static bool tcpRxFrozen = false;
static bool transportStarted = false;
static bool readyAnnounced = false;

static uint8_t tcpBuffer[TCP_BUFFER_BYTES];

static uint64_t rxBytes = 0;
static uint32_t rxOperations = 0;
static uint32_t transportErrors = 0;
static uint32_t tcpSpiLockErrors = 0;
static uint32_t tcpSpiHoldCount = 0;
static uint64_t tcpSpiHoldTotalUs = 0;
static uint32_t tcpSpiHoldMaxUs = 0;
static uint32_t tcpServicePasses = 0;
static uint32_t tcpServiceActivePasses = 0;
static uint32_t tcpServiceEmptyPasses = 0;

static volatile bool ethIntPending = false;
static volatile uint32_t ethIntIsrCount = 0;
static bool ethIntConfigured = false;
static uint8_t ethIntSocket = MAX_SOCK_NUM;
static uint32_t tcpIntSkipCount = 0;
static uint32_t tcpIntWakeCount = 0;
static uint32_t tcpIntLowFallbackCount = 0;
static uint32_t tcpIntRsrRearmCount = 0;
static uint32_t tcpIntPinRearmCount = 0;

#if JWPLC_H4A04P3_VERIFY_PAYLOAD
static uint32_t rxFnv1a32 = 2166136261UL;
#endif

static const char *modeName()
{
    return mode == MODE_TCP_RX
        ? "TCP_RX"
        : "IDLE";
}

static void resetCounters()
{
    rxBytes = 0;
    rxOperations = 0;
    transportErrors = 0;
    tcpSpiLockErrors = 0;
    tcpSpiHoldCount = 0;
    tcpSpiHoldTotalUs = 0;
    tcpSpiHoldMaxUs = 0;
    tcpServicePasses = 0;
    tcpServiceActivePasses = 0;
    tcpServiceEmptyPasses = 0;
    ethIntIsrCount = 0;
    tcpIntSkipCount = 0;
    tcpIntWakeCount = 0;
    tcpIntLowFallbackCount = 0;
    tcpIntRsrRearmCount = 0;
    tcpIntPinRearmCount = 0;
    tcpRxFrozen = false;

#if JWPLC_H4A04P3_VERIFY_PAYLOAD
    rxFnv1a32 = 2166136261UL;
#endif

#if JWPLC_SPI_PROFILE_FIFO_REUSE_CHUNKS
    SPI.jwplcResetReadBytesReuseFifoProfile();
#endif

#if JWPLC_ETHERNET_ENABLE_PROFILE_HOOKS
    Ethernet.jwplcProfileResetTcpRx();
#endif
}

static void printProfile()
{
#if JWPLC_ETHERNET_ENABLE_PROFILE_HOOKS
    const JWPLCEthernetTcpRxProfile p =
        Ethernet.jwplcProfileGetTcpRx();

    Serial.println("TCP_PROFILE_ENABLED=YES");

    Serial.print("TCP_PROF_SOCKET_STATUS_CALLS=");
    Serial.println(p.socketStatusCalls);
    Serial.print("TCP_PROF_SOCKET_STATUS_TOTAL_US=");
    Serial.println((unsigned long long)p.socketStatusTotalUs);

    Serial.print("TCP_PROF_AVAILABLE_CALLS=");
    Serial.println(p.recvAvailableCalls);
    Serial.print("TCP_PROF_AVAILABLE_TOTAL_US=");
    Serial.println((unsigned long long)p.recvAvailableTotalUs);
    Serial.print("TCP_PROF_AVAILABLE_ZERO_CALLS=");
    Serial.println(p.recvAvailableZeroCalls);
    Serial.print("TCP_PROF_AVAILABLE_NONZERO_CALLS=");
    Serial.println(p.recvAvailableNonzeroCalls);
    Serial.print("TCP_PROF_AVAILABLE_RSR_REFRESH_CALLS=");
    Serial.println(p.recvAvailableRsrRefreshCalls);
    Serial.print("TCP_PROF_AVAILABLE_RSR_REFRESH_TOTAL_US=");
    Serial.println((unsigned long long)p.recvAvailableRsrRefreshTotalUs);

    Serial.print("TCP_PROF_RECV_CALLS=");
    Serial.println(p.recvCalls);
    Serial.print("TCP_PROF_RECV_TOTAL_US=");
    Serial.println((unsigned long long)p.recvTotalUs);

    Serial.print("TCP_PROF_RECV_RSR_REFRESH_CALLS=");
    Serial.println(p.recvRsrRefreshCalls);
    Serial.print("TCP_PROF_RECV_RSR_REFRESH_TOTAL_US=");
    Serial.println((unsigned long long)p.recvRsrRefreshTotalUs);

    Serial.print("TCP_PROF_PAYLOAD_READ_CALLS=");
    Serial.println(p.recvPayloadReadCalls);
    Serial.print("TCP_PROF_PAYLOAD_READ_TOTAL_US=");
    Serial.println((unsigned long long)p.recvPayloadReadTotalUs);
    Serial.print("TCP_PROF_PAYLOAD_BYTES=");
    Serial.println((unsigned long long)p.recvPayloadBytes);

    Serial.print("TCP_PROF_COMMIT_CALLS=");
    Serial.println(p.recvCommitCalls);
    Serial.print("TCP_PROF_COMMIT_TOTAL_US=");
    Serial.println((unsigned long long)p.recvCommitTotalUs);
#else
    Serial.println("TCP_PROFILE_ENABLED=NO");
#endif
}

static void printSpiChunkProfile()
{
#if JWPLC_SPI_PROFILE_FIFO_REUSE_CHUNKS
    const JWPLCSpiFifoReuseChunkProfile p =
        SPI.jwplcGetReadBytesReuseFifoProfile();

    Serial.println("SPI_CHUNK_PROFILE_ENABLED=YES");
    Serial.print("SPI_CHUNK_COUNT=");
    Serial.println((unsigned long long)p.chunkCount);
    Serial.print("SPI_CHUNK_BYTES=");
    Serial.println((unsigned long long)p.bytes);
    Serial.print("SPI_CHUNK_SETUP_TOTAL_US=");
    Serial.println((unsigned long long)p.setupTotalUs);
    Serial.print("SPI_CHUNK_WIRE_WAIT_TOTAL_US=");
    Serial.println((unsigned long long)p.wireWaitTotalUs);
    Serial.print("SPI_CHUNK_COPY_OUT_TOTAL_US=");
    Serial.println((unsigned long long)p.copyOutTotalUs);
    Serial.print("SPI_CHUNK_OTHER_TOTAL_US=");
    Serial.println((unsigned long long)p.otherTotalUs);
#else
    Serial.println("SPI_CHUNK_PROFILE_ENABLED=NO");
#endif
}

static void printSnapshot()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println(" A14 H4A0.4-P1 TCP RX SNAPSHOT");
    Serial.println("========================================");

    Serial.print("RAW_SERVER_READY=");
    Serial.println(transportStarted ? "YES" : "NO");

    Serial.print("ETH_READY=");
    Serial.println(JWPLC_Ethernet.isReady() ? "YES" : "NO");

    Serial.print("ETH_LINK=");
    Serial.println(JWPLC_Ethernet.linkUp() ? "UP" : "DOWN");

    Serial.print("IP=");
    Serial.println(JWPLC_Ethernet.localIP());

    Serial.print("MODE=");
    Serial.println(modeName());

    Serial.print("TCP_RX_MAX_CHUNKS_PER_LOCK=");
    Serial.println(TCP_RX_MAX_CHUNKS_PER_LOCK);

    Serial.print("TCP_RX_FROZEN=");
    Serial.println(tcpRxFrozen ? "YES" : "NO");

    Serial.print("RX_BYTES=");
    Serial.println((unsigned long long)rxBytes);

    Serial.print("RX_OPERATIONS=");
    Serial.println(rxOperations);

    Serial.print("TRANSPORT_ERRORS=");
    Serial.println(transportErrors);

    Serial.print("TCP_SPI_LOCK_ERRORS=");
    Serial.println(tcpSpiLockErrors);

    Serial.print("TCP_SPI_HOLD_COUNT=");
    Serial.println(tcpSpiHoldCount);

    Serial.print("TCP_SPI_HOLD_TOTAL_US=");
    Serial.println((unsigned long long)tcpSpiHoldTotalUs);

    Serial.print("TCP_SPI_HOLD_AVG_US=");
    Serial.println(
        tcpSpiHoldCount > 0
            ? (uint32_t)(tcpSpiHoldTotalUs / tcpSpiHoldCount)
            : 0);

    Serial.print("TCP_SPI_HOLD_MAX_US=");
    Serial.println(tcpSpiHoldMaxUs);

    Serial.print("TCP_SERVICE_PASSES=");
    Serial.println(tcpServicePasses);

    Serial.print("TCP_SERVICE_ACTIVE_PASSES=");
    Serial.println(tcpServiceActivePasses);

    Serial.print("TCP_SERVICE_EMPTY_PASSES=");
    Serial.println(tcpServiceEmptyPasses);

    Serial.print("TCP_G2_INT_GUIDED=");
    Serial.println(JWPLC_G2_TCP_INT_GUIDED ? "YES" : "NO");

    Serial.print("ETH_INT_CONFIGURED=");
    Serial.println(ethIntConfigured ? "YES" : "NO");

    Serial.print("ETH_INT_PIN=");
    Serial.println(ETH_INT_PIN);

    Serial.print("ETH_INT_SOCKET=");
    Serial.println(ethIntSocket);

    Serial.print("ETH_INT_ISR_COUNT=");
    Serial.println((uint32_t)ethIntIsrCount);

    Serial.print("TCP_INT_SKIP_COUNT=");
    Serial.println(tcpIntSkipCount);

    Serial.print("TCP_INT_WAKE_COUNT=");
    Serial.println(tcpIntWakeCount);

    Serial.print("TCP_INT_LOW_FALLBACK_COUNT=");
    Serial.println(tcpIntLowFallbackCount);

    Serial.print("TCP_INT_RSR_REARM_COUNT=");
    Serial.println(tcpIntRsrRearmCount);

    Serial.print("TCP_INT_PIN_REARM_COUNT=");
    Serial.println(tcpIntPinRearmCount);

#if JWPLC_H4A04P3_VERIFY_PAYLOAD
    Serial.println("PAYLOAD_VERIFY_ENABLED=YES");
    Serial.print("RX_FNV1A32=");
    Serial.println(rxFnv1a32);
#else
    Serial.println("PAYLOAD_VERIFY_ENABLED=NO");
#endif

    printProfile();
    printSpiChunkProfile();

    Serial.println("H4A04P1_SNAPSHOT=END");
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();

        if (c == 'R' || c == 'r')
        {
            resetCounters();
            Serial.println("H4A04P1_RESET=PASS");
        }
        else if (c == 'F' || c == 'f')
        {
            tcpRxFrozen = true;
            Serial.println("H4A04P1_FREEZE=PASS");
        }
        else if (c == 'S' || c == 's')
        {
            printSnapshot();
        }
    }
}

static void startTransportIfReadyUnlocked()
{
    if (transportStarted)
    {
        return;
    }

    if (!JWPLC_Ethernet.isReady())
    {
        return;
    }

    tcpServer.begin();

    if (!tcpServer)
    {
        return;
    }

    transportStarted = true;
    resetCounters();
}

static void startTransportIfReady()
{
    if (!jwplcSPI_acquire(50))
    {
        ++tcpSpiLockErrors;
        return;
    }

    jwplcSPI_deselectAll();
    startTransportIfReadyUnlocked();
    jwplcSPI_release();
}

static void announceReady()
{
    if (readyAnnounced || !transportStarted)
    {
        return;
    }

    readyAnnounced = true;

    Serial.print("H4A04P1_SERVER_READY=PASS IP=");
    Serial.print(JWPLC_Ethernet.localIP());
    Serial.print(" TCP_PORT=");
    Serial.println(TCP_PORT);
}

#if JWPLC_G2_TCP_INT_GUIDED
static void IRAM_ATTR onEthernetInterrupt()
{
    ethIntPending = true;
    ++ethIntIsrCount;
}

static void disableTcpIntUnlocked()
{
    if (!ethIntConfigured)
    {
        return;
    }

    detachInterrupt(digitalPinToInterrupt(ETH_INT_PIN));

    if (ethIntSocket < MAX_SOCK_NUM && W5100.getChip() == 55)
    {
        SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

        W5100.writeSnIMR(ethIntSocket, 0);

        const uint8_t simr =
            W5100.readSIMR_W5500();

        W5100.writeSIMR_W5500(
            (uint8_t)(simr & ~(1U << ethIntSocket)));

        W5100.writeSnIR(
            ethIntSocket,
            TCP_INT_MASK);

        SPI.endTransaction();
    }

    ethIntConfigured = false;
    ethIntSocket = MAX_SOCK_NUM;
    ethIntPending = false;
}

static bool configureTcpIntUnlocked()
{
    const uint8_t socketNumber =
        tcpClient.getSocketNumber();

    if (
        W5100.getChip() != 55 ||
        socketNumber >= MAX_SOCK_NUM)
    {
        return false;
    }

    disableTcpIntUnlocked();

    pinMode(ETH_INT_PIN, INPUT);

    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

    W5100.writeSnIMR(socketNumber, 0);

    uint8_t simr =
        W5100.readSIMR_W5500();

    W5100.writeSIMR_W5500(
        (uint8_t)(simr & ~(1U << socketNumber)));

    W5100.writeSnIR(
        socketNumber,
        TCP_INT_MASK);

    W5100.writeSnIMR(
        socketNumber,
        TCP_INT_MASK);

    W5100.writeSIMR_W5500(
        (uint8_t)(simr | (1U << socketNumber)));

    uint16_t rsr = 0;
    (void)W5100.readSnRX_RSRStable(
        socketNumber,
        rsr);

    SPI.endTransaction();

    ethIntSocket = socketNumber;
    ethIntPending = false;
    ethIntIsrCount = 0;

    attachInterrupt(
        digitalPinToInterrupt(ETH_INT_PIN),
        onEthernetInterrupt,
        FALLING);

    ethIntConfigured = true;

    if (rsr > 0)
    {
        ethIntPending = true;
        ++tcpIntRsrRearmCount;
    }

    if (digitalRead(ETH_INT_PIN) == LOW)
    {
        ethIntPending = true;
        ++tcpIntPinRearmCount;
    }

    return true;
}

static void rearmTcpIntUnlocked()
{
    if (
        !ethIntConfigured ||
        ethIntSocket >= MAX_SOCK_NUM ||
        W5100.getChip() != 55)
    {
        return;
    }

    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

    const uint8_t ir =
        W5100.readSnIR(ethIntSocket);

    const uint8_t clearMask =
        (uint8_t)(ir & TCP_INT_MASK);

    if (clearMask != 0)
    {
        W5100.writeSnIR(
            ethIntSocket,
            clearMask);
    }

    uint16_t rsr = 0;
    (void)W5100.readSnRX_RSRStable(
        ethIntSocket,
        rsr);

    SPI.endTransaction();

    if (rsr > 0)
    {
        ethIntPending = true;
        ++tcpIntRsrRearmCount;
    }

    if (digitalRead(ETH_INT_PIN) == LOW)
    {
        ethIntPending = true;
        ++tcpIntPinRearmCount;
    }
}
#endif

static bool acceptTcpClient()
{
    // This deliberately exercises the existing cooperative server-side
    // stop lifecycle used by the current RAW benchmark.
    if (tcpClient.stopAsyncInProgress())
    {
        const int stopState =
            tcpClient.pollStopAsync();

        if (stopState == 0)
        {
            return false;
        }
    }

    if (tcpClient && tcpClient.connected())
    {
        return true;
    }

    if (tcpClient)
    {
        const int stopState =
            tcpClient.beginStopAsync();

        if (stopState == 0)
        {
            return false;
        }
    }

    tcpClient = tcpServer.accept();

    if (tcpClient)
    {
        mode = MODE_IDLE;
        resetCounters();
        return true;
    }

    return false;
}

static void serviceTcpUnlocked()
{
#if JWPLC_H4A04P8_REUSE_CONNECTED_RESULT
    // acceptTcpClient() already resolved socket usability in this cooperative
    // pass. Reuse only that result; no TCP state survives into a later pass.
    if (!acceptTcpClient())
#else
    acceptTcpClient();

    if (!tcpClient || !tcpClient.connected())
#endif
    {
        if (mode == MODE_TCP_RX)
        {
#if JWPLC_G2_TCP_INT_GUIDED
            disableTcpIntUnlocked();
#endif
            mode = MODE_IDLE;
        }

        return;
    }

    if (mode == MODE_IDLE)
    {
        if (tcpClient.available() <= 0)
        {
            return;
        }

        const int command = tcpClient.read();

        if (command == 'R')
        {
            mode = MODE_TCP_RX;
            resetCounters();

#if JWPLC_G2_TCP_INT_GUIDED
            if (!configureTcpIntUnlocked())
            {
                ++transportErrors;
                mode = MODE_IDLE;
            }
#endif
        }
        else
        {
            ++transportErrors;
            (void)tcpClient.beginStopAsync();
        }

        return;
    }

    if (mode != MODE_TCP_RX || tcpRxFrozen)
    {
        return;
    }

    for (
        uint8_t index = 0;
        index < TCP_RX_MAX_CHUNKS_PER_LOCK;
        ++index)
    {
#if JWPLC_H4A04P7_DEFER_TCP_COMMIT
        const int got =
            tcpClient.jwplcReadTcpFastDeferred(
                tcpBuffer,
                sizeof(tcpBuffer));

        if (got <= 0)
        {
            break;
        }
#elif JWPLC_H4A04P6_DIRECT_READ
        // socketRecv() is already non-blocking. Calling read() directly lets
        // it refresh Sn_RX_RSR and consume data in one SPI transaction.
        const int got =
            tcpClient.read(
                tcpBuffer,
                sizeof(tcpBuffer));

        if (got <= 0)
        {
            return;
        }
#else
        const int availableBytes =
            tcpClient.available();

        if (availableBytes <= 0)
        {
            return;
        }

        size_t chunk = (size_t)availableBytes;

        if (chunk > sizeof(tcpBuffer))
        {
            chunk = sizeof(tcpBuffer);
        }

        const int got =
            tcpClient.read(
                tcpBuffer,
                chunk);

        if (got <= 0)
        {
            ++transportErrors;
            return;
        }
#endif

#if JWPLC_H4A04P3_VERIFY_PAYLOAD
        for (int i = 0; i < got; ++i)
        {
            rxFnv1a32 ^= tcpBuffer[i];
            rxFnv1a32 *= 16777619UL;
        }
#endif

        rxBytes += (uint32_t)got;
        ++rxOperations;
    }

#if JWPLC_H4A04P7_DEFER_TCP_COMMIT
    if (!tcpClient.jwplcCommitRxFast())
    {
        ++transportErrors;
    }
#endif
}

static void serviceTcp()
{
    // Freeze is the accounting barrier for both throughput and profiling.
    // Do not add status/available/read/profile work after the F ACK.
    if (tcpRxFrozen && mode == MODE_TCP_RX)
    {
        return;
    }

#if JWPLC_G2_TCP_INT_GUIDED
    if (
        mode == MODE_TCP_RX &&
        ethIntConfigured)
    {
        const bool pinLow =
            digitalRead(ETH_INT_PIN) == LOW;

        if (!ethIntPending && !pinLow)
        {
            ++tcpIntSkipCount;
            return;
        }

        if (!ethIntPending && pinLow)
        {
            ++tcpIntLowFallbackCount;
        }

        ethIntPending = false;
        ++tcpIntWakeCount;
    }
#endif

    if (!jwplcSPI_acquire(50))
    {
        ++tcpSpiLockErrors;
        return;
    }

    const uint32_t holdStartUs = micros();
    const bool countServicePass = (mode == MODE_TCP_RX);
    const uint64_t rxBytesBefore = rxBytes;

    jwplcSPI_deselectAll();
    serviceTcpUnlocked();

#if JWPLC_G2_TCP_INT_GUIDED
    if (
        countServicePass &&
        mode == MODE_TCP_RX &&
        ethIntConfigured)
    {
        rearmTcpIntUnlocked();
    }
#endif

    if (countServicePass)
    {
        ++tcpServicePasses;

        if (rxBytes > rxBytesBefore)
        {
            ++tcpServiceActivePasses;
        }
        else
        {
            ++tcpServiceEmptyPasses;
        }
    }

    const uint32_t holdUs =
        (uint32_t)(micros() - holdStartUs);

    ++tcpSpiHoldCount;
    tcpSpiHoldTotalUs += holdUs;

    if (holdUs > tcpSpiHoldMaxUs)
    {
        tcpSpiHoldMaxUs = holdUs;
    }

    jwplcSPI_release();
}

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    serviceSerial();

    if (!transportStarted)
    {
        startTransportIfReady();
    }

    if (transportStarted)
    {
        announceReady();
        serviceTcp();
    }

    yield();
}
