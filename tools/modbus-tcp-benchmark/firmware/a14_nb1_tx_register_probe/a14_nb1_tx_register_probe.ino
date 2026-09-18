#include <JWPLC_Ethernet.h>
#include <jwplc_spi_bus.h>
#include <SPI.h>
#include "utility/w5100.h"

static constexpr uint16_t PROBE_PORT = 5004;
static constexpr uint16_t PAYLOAD_BYTES = 1024;

EthernetServer probeServer(PROBE_PORT);
EthernetClient probeClient;
static uint8_t payload[PAYLOAD_BYTES];

struct TxRegs
{
    uint16_t wr;
    uint16_t rd;
    uint16_t fsr;
    bool stable;
};

static bool started = false;
static bool commandSeen = false;
static bool sendPending = false;
static bool done = false;
static bool failed = false;
static int resultCode = 0;
static uint32_t sendStartedUs = 0;
static uint32_t sendOkElapsedUs = 0;

static TxRegs beforeRegs = {};
static TxRegs afterBufferRegs = {};
static TxRegs afterSendRegs = {};
static TxRegs sendOkRegs = {};

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

static TxRegs readRegs(uint8_t s)
{
    TxRegs r;
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    r.wr = W5100.readSnTX_WR(s);
    r.rd = W5100.readSnTX_RD(s);
    r.fsr = readFsrStableBounded(s, r.stable);
    SPI.endTransaction();
    return r;
}

static uint16_t bufferWithoutSend(uint8_t s, const uint8_t *data, uint16_t length)
{
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

    bool stable = false;
    const uint16_t freeBytes = readFsrStableBounded(s, stable);
    if (!stable || freeBytes < length)
    {
        SPI.endTransaction();
        return 0;
    }

    uint16_t ptr = W5100.readSnTX_WR(s);
    const uint16_t offset = ptr & W5100.SMASK;
    const uint16_t destination = offset + W5100.SBASE(s);

    if (W5100.hasOffsetAddressMapping() ||
        (uint16_t)(offset + length) <= W5100.SSIZE)
    {
        W5100.write(destination, data, length);
    }
    else
    {
        const uint16_t firstPart = W5100.SSIZE - offset;
        W5100.write(destination, data, firstPart);
        W5100.write(W5100.SBASE(s), data + firstPart, length - firstPart);
    }

    W5100.writeSnTX_WR(s, (uint16_t)(ptr + length));
    SPI.endTransaction();
    return length;
}

static void printRegs(const char *prefix, const TxRegs &r)
{
    Serial.print(prefix); Serial.print("_TX_WR="); Serial.println(r.wr);
    Serial.print(prefix); Serial.print("_TX_RD="); Serial.println(r.rd);
    Serial.print(prefix); Serial.print("_TX_FSR="); Serial.println(r.fsr);
    Serial.print(prefix); Serial.print("_TX_FSR_STABLE="); Serial.println(r.stable ? "YES" : "NO");
}

static void finish(int code, bool isFailure)
{
    resultCode = code;
    failed = isFailure;
    done = true;
}

static void serviceLocked()
{
    if (!started)
    {
        if (!JWPLC_Ethernet.isReady()) return;
        probeServer.begin();
        if (!probeServer) return;
        started = true;
        return;
    }

    if (done) return;

    if (!probeClient)
    {
        probeClient = probeServer.accept();
        return;
    }

    if (!probeClient.connected())
    {
        finish(-10, true);
        return;
    }

    if (!commandSeen)
    {
        if (probeClient.available() <= 0) return;
        if (probeClient.read() != 'R')
        {
            finish(-11, true);
            return;
        }

        commandSeen = true;
        const uint8_t s = probeClient.getSocketNumber();
        if (s >= MAX_SOCK_NUM)
        {
            finish(-12, true);
            return;
        }

        beforeRegs = readRegs(s);
        const uint16_t buffered = bufferWithoutSend(s, payload, PAYLOAD_BYTES);
        if (buffered != PAYLOAD_BYTES)
        {
            finish(-13, true);
            return;
        }

        afterBufferRegs = readRegs(s);

        SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
        W5100.writeSnIR(s, (uint8_t)(SnIR::SEND_OK | SnIR::TIMEOUT));
        W5100.execCmdSn(s, Sock_SEND);
        SPI.endTransaction();

        sendStartedUs = micros();
        afterSendRegs = readRegs(s);
        sendPending = true;
        return;
    }

    if (sendPending)
    {
        const uint8_t s = probeClient.getSocketNumber();
        SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
        const uint8_t ir = W5100.readSnIR(s);
        const uint8_t sr = W5100.readSnSR(s);
        SPI.endTransaction();

        if ((ir & SnIR::TIMEOUT) != 0)
        {
            finish(-20, true);
            return;
        }

        if ((ir & SnIR::SEND_OK) != 0)
        {
            SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
            W5100.writeSnIR(s, SnIR::SEND_OK);
            SPI.endTransaction();

            sendOkElapsedUs = (uint32_t)(micros() - sendStartedUs);
            sendOkRegs = readRegs(s);
            sendPending = false;
            finish(1, false);
            return;
        }

        if (sr == SnSR::CLOSED)
        {
            finish(-21, true);
        }
    }
}

static void printResult()
{
    if (!done) return;
    static bool printed = false;
    if (printed) return;
    printed = true;

    Serial.println("NB1_D2R_RESULT=BEGIN");
    Serial.print("RESULT_CODE="); Serial.println(resultCode);
    Serial.print("PROBE_FAILED="); Serial.println(failed ? "YES" : "NO");
    printRegs("BEFORE", beforeRegs);
    printRegs("AFTER_BUFFER", afterBufferRegs);
    printRegs("AFTER_SEND", afterSendRegs);
    printRegs("SEND_OK", sendOkRegs);
    Serial.print("SEND_OK_ELAPSED_US="); Serial.println(sendOkElapsedUs);
    Serial.println("NB1_D2R_RESULT=END");
}

void setup()
{
    Serial.begin(115200);
    for (uint16_t i = 0; i < PAYLOAD_BYTES; ++i) payload[i] = (uint8_t)(i & 0xFFU);
}

void loop()
{
    if (jwplcSPI_acquire(50))
    {
        jwplcSPI_deselectAll();
        serviceLocked();
        jwplcSPI_release();
    }

    if (started)
    {
        static bool announced = false;
        if (!announced)
        {
            announced = true;
            Serial.print("NB1_D2R_READY=YES IP=");
            Serial.print(JWPLC_Ethernet.localIP());
            Serial.print(" PORT=");
            Serial.println(PROBE_PORT);
        }
    }

    printResult();
    delay(0);
}
