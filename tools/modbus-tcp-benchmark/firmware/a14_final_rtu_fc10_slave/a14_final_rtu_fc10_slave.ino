/*
  Alpha14 final RTU gate R-FC10 - Slave
  FC10 server + FC03 readback, 500 kbaud FAST.
  Serial queda mudo durante la ventana.
*/

#include <Arduino.h>
#include <esp_system.h>
#include <JWPLC_ModbusRTU.h>
#include <JWPLC_RS485.h>

static constexpr uint8_t SLAVE_ID = 2;
static constexpr uint32_t BAUD = 500000UL;
static constexpr uint32_t CONFIG = SERIAL_8N1;
static constexpr uint32_t FRAME_GAP_US = 100UL;
static constexpr uint8_t RX_FIFO = 8U;

static uint32_t bootMarker = 0;
static bool ready = false;
static uint16_t holding[128] = {};

static void resetCounters()
{
    JWPLC_ModbusRTU.resetStats();
}

static void printSnapshot()
{
    const JWPLCModbusRTUStats &s =
        JWPLC_ModbusRTU.stats();

    Serial.println("A14_RTU_FC10_SLAVE_SNAPSHOT=BEGIN");
    Serial.print("BOOT_MARKER=");
    Serial.println(bootMarker);
    Serial.print("UPTIME_MS=");
    Serial.println(millis());
    Serial.print("SLAVE_READY=");
    Serial.println(ready ? "YES" : "NO");
    Serial.print("RTU_BAUD_EFFECTIVE=");
    Serial.println(JWPLC_ModbusRTU.effectiveBaudRate());
    Serial.print("RTU_RX_FIFO_FULL=");
    Serial.println(RX_FIFO);
    Serial.print("RTU_RX_MODE=");
    Serial.println(
        JWPLC_ModbusRTU.bulkRxEnabled()
            ? "BULK"
            : "BYTE");
    Serial.print("RTU_TX_MODE=");
    Serial.println(
        JWPLC_ModbusRTU.queuedTxActive()
            ? "QUEUED"
            : "BLOCKING");
    Serial.print("RTU_SERVER_FRAMING=");
    Serial.println(
        JWPLC_ModbusRTU.earlyServerDispatchEnabled()
            ? "STRUCTURAL"
            : "GAP");
    Serial.print("RTU_FRAME_GAP_US=");
    Serial.println(JWPLC_ModbusRTU.frameGapUs());
    Serial.print("RTU_RX_FRAMES=");
    Serial.println(s.rxFrames);
    Serial.print("RTU_TX_FRAMES=");
    Serial.println(s.txFrames);
    Serial.print("RTU_REQUESTS_OK=");
    Serial.println(s.requestsOk);
    Serial.print("RTU_CRC_ERRORS=");
    Serial.println(s.crcErrors);
    Serial.print("RTU_EXCEPTIONS_SENT=");
    Serial.println(s.exceptionsSent);
    Serial.println("A14_RTU_FC10_SLAVE_SNAPSHOT=END");
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();

        if (c == 'R' || c == 'r')
        {
            resetCounters();
            Serial.println("A14_RTU_FC10_SLAVE_RESET=PASS");
        }
        else if (c == 'S' || c == 's')
        {
            printSnapshot();
        }
    }
}

void setup()
{
    Serial.begin(115200);
    bootMarker = (uint32_t)esp_random();

    for (uint16_t i = 0; i < 128U; ++i)
        holding[i] = 0U;

    JWPLC_ModbusRTU.setHoldingRegisters(
        holding,
        128U);

    ready =
        JWPLC_ModbusRTU.begin(
            SLAVE_ID,
            BAUD,
            CONFIG);

    if (ready)
    {
        ready = JWPLC_ModbusRTU.motor(ASYNC);
        JWPLC_ModbusRTU.setFrameGapUs(FRAME_GAP_US);
        JWPLC_ModbusRTU.setBulkRxEnabled(true);
        JWPLC_ModbusRTU.setQueuedTxEnabled(true);
        JWPLC_ModbusRTU.setEarlyServerDispatchEnabled(true);

        if (!JWPLC_RS485.serial().setRxFIFOFull(RX_FIFO))
            ready = false;
    }

    resetCounters();

    Serial.println("A14_RTU_FC10_SLAVE_BOOT=PASS");
}

void loop()
{
    serviceSerial();

    if (ready)
        JWPLC_ModbusRTU.task();
}
