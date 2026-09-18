/*
  ETH14-G1A - JWPLC Ethernet raw transport benchmark

  Objetivo:
  - medir transporte Ethernet sin parsing Modbus;
  - TCP RX: PC -> JWPLC;
  - TCP TX: JWPLC -> PC;
  - UDP RX: PC -> JWPLC;
  - UDP TX: JWPLC -> PC.

  Importante:
  - Ethernet continúa gestionado por el runtime JWPLC;
  - NO llamar Ethernet.begin();
  - NO llamar JWPLC_Ethernet.begin();
  - no deshabilitar periféricos del autoload para esta prueba.

  Puertos:
  - TCP: 5001
  - UDP: 5002

  Protocolo TCP:
  - primer byte 'R': modo TCP_RX;
  - primer byte 'T': modo TCP_TX;
  - cerrar conexión termina el caso.

  Protocolo UDP:
  - datagrama ASCII "URX": arma UDP_RX y resetea contadores;
  - datagrama ASCII "UTX": arma UDP_TX, toma IP/puerto remoto
    del datagrama y resetea contadores;
  - datagrama ASCII "STOP": detiene UDP_TX;
  - durante UDP_RX, los demás datagramas se contabilizan como
    payload recibido.

  Comandos Serial:
  - R: reset de contadores;
  - S: snapshot;
*/

#include <JWPLC_Ethernet.h>

static constexpr uint16_t TCP_PORT = 5001;
static constexpr uint16_t UDP_PORT = 5002;

static constexpr size_t TCP_BUFFER_BYTES = 1024;
static constexpr size_t UDP_PAYLOAD_BYTES = 1472;

enum RawBenchMode : uint8_t
{
    MODE_IDLE = 0,
    MODE_TCP_RX,
    MODE_TCP_TX,
    MODE_UDP_RX,
    MODE_UDP_TX
};

static EthernetServer tcpServer(TCP_PORT);
static EthernetClient tcpClient;
static EthernetUDP udpSocket;

static RawBenchMode mode = MODE_IDLE;

static bool transportStarted = false;
static bool readyAnnounced = false;

static uint8_t tcpBuffer[TCP_BUFFER_BYTES];
static uint8_t udpBuffer[UDP_PAYLOAD_BYTES];

static IPAddress udpTxRemoteIP;
static uint16_t udpTxRemotePort = 0;

static uint64_t rxBytes = 0;
static uint64_t txBytes = 0;
static uint32_t rxOperations = 0;
static uint32_t txOperations = 0;
static uint32_t transportErrors = 0;

// ETH14 G1A diagnostic counters: raw benchmark only.
static uint32_t udpTxAttempts = 0;
static uint32_t udpBeginPacketErrors = 0;
static uint32_t udpWriteErrors = 0;
static uint32_t udpEndPacketErrors = 0;
static uint32_t udpSpiLockErrors = 0;
static uint32_t tcpSpiLockErrors = 0;
static uint32_t tcpSpiHoldCount = 0;
static uint64_t tcpSpiHoldTotalUs = 0;
static uint32_t tcpSpiHoldMaxUs = 0;
static size_t udpLastShortWriteBytes = 0;

static uint32_t lastLoopUs = 0;
static uint64_t loopGapSumUs = 0;
static uint32_t loopGapSamples = 0;
static uint32_t loopGapMaxUs = 0;

static const char *modeName()
{
    switch (mode)
    {
        case MODE_TCP_RX:
            return "TCP_RX";

        case MODE_TCP_TX:
            return "TCP_TX";

        case MODE_UDP_RX:
            return "UDP_RX";

        case MODE_UDP_TX:
            return "UDP_TX";

        default:
            return "IDLE";
    }
}

static void resetCounters()
{
    rxBytes = 0;
    txBytes = 0;
    rxOperations = 0;
    txOperations = 0;
    transportErrors = 0;

    udpTxAttempts = 0;
    udpBeginPacketErrors = 0;
    udpWriteErrors = 0;
    udpEndPacketErrors = 0;
    udpSpiLockErrors = 0;
    tcpSpiLockErrors = 0;
    tcpSpiHoldCount = 0;
    tcpSpiHoldTotalUs = 0;
    tcpSpiHoldMaxUs = 0;
    udpLastShortWriteBytes = 0;

    loopGapSumUs = 0;
    loopGapSamples = 0;
    loopGapMaxUs = 0;
    lastLoopUs = micros();
}

static void printSnapshot()
{
    bool tcpConnectedSnapshot = false;

    // Snapshot only: protect the single raw W5500 TCP status read.
    if (jwplcSPI_acquire(50))
    {
        jwplcSPI_deselectAll();
        tcpConnectedSnapshot = tcpClient.connected();
        jwplcSPI_release();
    }
    else
    {
        ++tcpSpiLockErrors;
    }

    uint32_t loopGapAvgUs = 0;

    if (loopGapSamples > 0)
    {
        loopGapAvgUs =
            (uint32_t)(
                loopGapSumUs /
                loopGapSamples);
    }

    Serial.println();
    Serial.println(
        "========================================");

    Serial.println(
        " ETH14 G1A RAW TRANSPORT SNAPSHOT");

    Serial.println(
        "========================================");

    Serial.print("RAW_SERVER_READY=");
    Serial.println(
        transportStarted
            ? "YES"
            : "NO");

    Serial.print("ETH_READY=");
    Serial.println(
        JWPLC_Ethernet.isReady()
            ? "YES"
            : "NO");

    Serial.print("ETH_LINK=");
    Serial.println(
        JWPLC_Ethernet.linkUp()
            ? "UP"
            : "DOWN");

    Serial.print("IP=");
    Serial.println(
        JWPLC_Ethernet.localIP());

    Serial.print("MODE=");
    Serial.println(modeName());

    Serial.print("TCP_PORT=");
    Serial.println(TCP_PORT);

    Serial.print("UDP_PORT=");
    Serial.println(UDP_PORT);

    Serial.print("RX_BYTES=");
    Serial.println(
        (unsigned long long)rxBytes);

    Serial.print("TX_BYTES=");
    Serial.println(
        (unsigned long long)txBytes);

    Serial.print("RX_OPERATIONS=");
    Serial.println(rxOperations);

    Serial.print("TX_OPERATIONS=");
    Serial.println(txOperations);

    Serial.print("TRANSPORT_ERRORS=");
    Serial.println(transportErrors);

    Serial.print("LOOP_GAP_AVG_US=");
    Serial.println(loopGapAvgUs);

    Serial.print("LOOP_GAP_MAX_US=");
    Serial.println(loopGapMaxUs);

    Serial.print("UDP_TX_ATTEMPTS=");
    Serial.println(udpTxAttempts);

    Serial.print("UDP_BEGIN_PACKET_ERRORS=");
    Serial.println(udpBeginPacketErrors);

    Serial.print("UDP_WRITE_ERRORS=");
    Serial.println(udpWriteErrors);

    Serial.print("UDP_END_PACKET_ERRORS=");
    Serial.println(udpEndPacketErrors);

    Serial.print("UDP_SPI_LOCK_ERRORS=");
    Serial.println(udpSpiLockErrors);
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

    Serial.print("UDP_LAST_SHORT_WRITE_BYTES=");
    Serial.println(udpLastShortWriteBytes);

    Serial.print("TCP_CONNECTED=");
    Serial.println(
        tcpClient &&
        tcpConnectedSnapshot
            ? "YES"
            : "NO");

    Serial.print("UDP_TX_REMOTE_PORT=");
    Serial.println(udpTxRemotePort);

    Serial.println(
        "ETH14_RAW_SNAPSHOT=END");
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c =
            (char)Serial.read();

        if (c == 'R' || c == 'r')
        {
            resetCounters();

            Serial.println(
                "ETH14_RAW_RESET=PASS");
        }
        else if (c == 'S' || c == 's')
        {
            printSnapshot();
        }
    }
}

static void updateLoopTiming()
{
    const uint32_t nowUs = micros();

    if (lastLoopUs != 0)
    {
        const uint32_t gap =
            (uint32_t)(
                nowUs -
                lastLoopUs);

        loopGapSumUs += gap;
        ++loopGapSamples;

        if (gap > loopGapMaxUs)
        {
            loopGapMaxUs = gap;
        }
    }

    lastLoopUs = nowUs;
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

    if (!udpSocket.begin(UDP_PORT))
    {
        return;
    }

    tcpServer.begin();

    if (!tcpServer)
    {
        udpSocket.stop();
        return;
    }

    transportStarted = true;
    resetCounters();
}

static void startTransportIfReady()
{
    // Raw TCP/W5500 start access must respect shared SPI ownership.
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
    if (
        readyAnnounced ||
        !transportStarted
    )
    {
        return;
    }

    readyAnnounced = true;

    Serial.print(
        "ETH14_RAW_SERVER_READY=PASS IP=");

    Serial.print(
        JWPLC_Ethernet.localIP());

    Serial.print(" TCP_PORT=");
    Serial.print(TCP_PORT);

    Serial.print(" UDP_PORT=");
    Serial.println(UDP_PORT);
}

static void acceptTcpClient()
{
    if (
        tcpClient &&
        tcpClient.connected()
    )
    {
        return;
    }

    if (tcpClient)
    {
        tcpClient.stop();
    }

    tcpClient = tcpServer.accept();

    if (tcpClient)
    {
        mode = MODE_IDLE;
        resetCounters();
    }
}

static void serviceTcpUnlocked()
{
    acceptTcpClient();

    if (
        !tcpClient ||
        !tcpClient.connected()
    )
    {
        if (
            mode == MODE_TCP_RX ||
            mode == MODE_TCP_TX
        )
        {
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

        const int command =
            tcpClient.read();

        if (command == 'R')
        {
            mode = MODE_TCP_RX;
            resetCounters();
        }
        else if (command == 'T')
        {
            mode = MODE_TCP_TX;
            resetCounters();
        }
        else
        {
            ++transportErrors;
            tcpClient.stop();
        }

        return;
    }

    if (mode == MODE_TCP_RX)
    {
        // Keep SPI ownership bounded while amortizing mutex/socket
        // overhead: consume at most four W5500 RX chunks per pass.
        static constexpr uint8_t TCP_RX_MAX_CHUNKS_PER_LOCK = 4;

        for (
            uint8_t rxChunkIndex = 0;
            rxChunkIndex < TCP_RX_MAX_CHUNKS_PER_LOCK;
            ++rxChunkIndex)
        {
            const int availableBytes =
                tcpClient.available();

            if (availableBytes <= 0)
            {
                return;
            }

            size_t chunk =
                (size_t)availableBytes;

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

            rxBytes +=
                (uint32_t)got;

            ++rxOperations;
        }

        return;
    }

    if (mode == MODE_TCP_TX)
    {
        const int availableForWrite =
            tcpClient.availableForWrite();

        if (availableForWrite <= 0)
        {
            return;
        }

        size_t chunk =
            (size_t)availableForWrite;

        if (chunk > sizeof(tcpBuffer))
        {
            chunk = sizeof(tcpBuffer);
        }

        const size_t written =
            tcpClient.write(
                tcpBuffer,
                chunk);

        if (written > 0)
        {
            txBytes += written;
            ++txOperations;
        }
        else
        {
            ++transportErrors;
        }
    }
}

static void serviceTcp()
{
    // Raw EthernetClient/EthernetServer access must respect shared SPI ownership.
    if (!jwplcSPI_acquire(50))
    {
        ++tcpSpiLockErrors;
        return;
    }

    const uint32_t tcpSpiHoldStartUs = micros();
    jwplcSPI_deselectAll();
    serviceTcpUnlocked();
    const uint32_t tcpSpiHoldUs = (uint32_t)(micros() - tcpSpiHoldStartUs);
    ++tcpSpiHoldCount;
    tcpSpiHoldTotalUs += tcpSpiHoldUs;

    if (tcpSpiHoldUs > tcpSpiHoldMaxUs)
    {
        tcpSpiHoldMaxUs = tcpSpiHoldUs;
    }

    jwplcSPI_release();
}

static bool packetEquals(
    const uint8_t *buffer,
    int length,
    const char *text)
{
    const int textLength =
        (int)strlen(text);

    if (length != textLength)
    {
        return false;
    }

    return memcmp(
        buffer,
        text,
        textLength) == 0;
}

static void serviceUdpUnlocked()
{
    const int packetSize =
        udpSocket.parsePacket();

    if (packetSize > 0)
    {
        int totalRead = 0;

        while (
            udpSocket.available() > 0 &&
            totalRead < (int)sizeof(udpBuffer)
        )
        {
            const int got =
                udpSocket.read(
                    udpBuffer + totalRead,
                    sizeof(udpBuffer) -
                        totalRead);

            if (got <= 0)
            {
                break;
            }

            totalRead += got;
        }

        if (
            packetEquals(
                udpBuffer,
                totalRead,
                "URX")
        )
        {
            mode = MODE_UDP_RX;
            resetCounters();
            return;
        }

        if (
            packetEquals(
                udpBuffer,
                totalRead,
                "UTX")
        )
        {
            udpTxRemoteIP =
                udpSocket.remoteIP();

            udpTxRemotePort =
                udpSocket.remotePort();

            mode = MODE_UDP_TX;
            resetCounters();
            return;
        }

        if (
            packetEquals(
                udpBuffer,
                totalRead,
                "STOP")
        )
        {
            mode = MODE_IDLE;
            return;
        }

        if (mode == MODE_UDP_RX)
        {
            rxBytes += totalRead;
            ++rxOperations;
        }
    }

    if (
        mode != MODE_UDP_TX ||
        udpTxRemotePort == 0
    )
    {
        return;
    }

    ++udpTxAttempts;

    // ETH14 G1A sequence diagnostic.
    // Keep the UDP payload at exactly sizeof(udpBuffer)
    // bytes while tagging every TX attempt in bytes [0..3].
    // Big-endian makes decoding independent of host CPU.
    const uint32_t udpTxSequence = udpTxAttempts;

    udpBuffer[0] =
        (uint8_t)((udpTxSequence >> 24) & 0xFFU);
    udpBuffer[1] =
        (uint8_t)((udpTxSequence >> 16) & 0xFFU);
    udpBuffer[2] =
        (uint8_t)((udpTxSequence >> 8) & 0xFFU);
    udpBuffer[3] =
        (uint8_t)(udpTxSequence & 0xFFU);

    if (
        !udpSocket.beginPacket(
            udpTxRemoteIP,
            udpTxRemotePort)
    )
    {
        ++udpBeginPacketErrors;
        ++transportErrors;
        return;
    }

    const size_t buffered =
        udpSocket.write(
            udpBuffer,
            sizeof(udpBuffer));

    if (buffered != sizeof(udpBuffer))
    {
        ++udpWriteErrors;
        udpLastShortWriteBytes = buffered;
        ++transportErrors;
        return;
    }

    if (!udpSocket.endPacket())
    {
        ++udpEndPacketErrors;
        ++transportErrors;
        return;
    }

    txBytes += buffered;
    ++txOperations;
}

// RAW benchmark wrapper:
// EthernetUDP does not acquire the JWPLC shared-SPI mutex
// by itself. Keep the complete UDP service step under one
// ownership window so parse/read and begin/write/endPacket
// cannot overlap another JWPLC SPI peripheral.
static void serviceUdp()
{
    // ETH14 G1A protected A/B:
    // raw EthernetUDP access must respect the JWPLC
    // shared-SPI ownership policy.
    if (!jwplcSPI_acquire(50))
    {
        ++udpSpiLockErrors;
        return;
    }

    jwplcSPI_deselectAll();
    serviceUdpUnlocked();
    jwplcSPI_release();
}

void setup()
{
    Serial.begin(115200);

    for (
        size_t i = 0;
        i < sizeof(tcpBuffer);
        ++i
    )
    {
        tcpBuffer[i] =
            (uint8_t)(i & 0xFFU);
    }

    for (
        size_t i = 0;
        i < sizeof(udpBuffer);
        ++i
    )
    {
        udpBuffer[i] =
            (uint8_t)(
                (i * 17U) &
                0xFFU);
    }

    resetCounters();

    Serial.println(
        "ETH14_RAW_TRANSPORT_CONFIG=PASS");
}

void loop()
{
    updateLoopTiming();

    serviceSerial();

    startTransportIfReady();

    if (!transportStarted)
    {
        return;
    }

    announceReady();

    serviceTcp();
    serviceUdp();
}
