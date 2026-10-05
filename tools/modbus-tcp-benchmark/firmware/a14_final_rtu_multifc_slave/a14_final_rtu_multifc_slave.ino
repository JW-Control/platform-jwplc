/*
  Alpha14 final RTU R2 - Multi-FC FAST Slave
  Soporta FC01/02/03/04/05/06/0F/10 sobre mapas representativos:
  8 coils, 8 discrete inputs, 2 holding registers y 4 input registers.
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

static constexpr uint8_t INITIAL_COILS = 0x5AU;
static constexpr uint8_t INITIAL_DISCRETE = 0xA5U;
static constexpr uint16_t INITIAL_HOLDING0 = 0x1111U;
static constexpr uint16_t INITIAL_HOLDING1 = 0x2222U;

static uint32_t bootMarker = 0;
static bool ready = false;

static uint8_t coils = INITIAL_COILS;
static uint8_t discreteInputs = INITIAL_DISCRETE;
static uint16_t holding[2] = {
    INITIAL_HOLDING0,
    INITIAL_HOLDING1
};
static uint16_t inputRegisters[4] = {
    0x3101U,
    0x3202U,
    0x3303U,
    0x3404U
};

static void resetMapsAndStats()
{
    coils = INITIAL_COILS;
    discreteInputs = INITIAL_DISCRETE;

    holding[0] = INITIAL_HOLDING0;
    holding[1] = INITIAL_HOLDING1;

    inputRegisters[0] = 0x3101U;
    inputRegisters[1] = 0x3202U;
    inputRegisters[2] = 0x3303U;
    inputRegisters[3] = 0x3404U;

    JWPLC_ModbusRTU.resetStats();
}

static void printSnapshot()
{
    const JWPLCModbusRTUStats &s =
        JWPLC_ModbusRTU.stats();

    Serial.println("A14_RTU_MULTIFC_SLAVE_SNAPSHOT=BEGIN");
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
    Serial.print("COILS_FINAL=");
    Serial.println(coils);
    Serial.print("DISCRETE_FINAL=");
    Serial.println(discreteInputs);
    Serial.print("HOLDING0_FINAL=");
    Serial.println(holding[0]);
    Serial.print("HOLDING1_FINAL=");
    Serial.println(holding[1]);
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
    Serial.println("A14_RTU_MULTIFC_SLAVE_SNAPSHOT=END");
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();

        if (c == 'R' || c == 'r')
        {
            resetMapsAndStats();
            Serial.println("A14_RTU_MULTIFC_SLAVE_RESET=PASS");
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

    JWPLC_ModbusRTU.setCoils(&coils, 8);
    JWPLC_ModbusRTU.setDiscreteInputs(&discreteInputs, 8);
    JWPLC_ModbusRTU.setHoldingRegisters(holding, 2);
    JWPLC_ModbusRTU.setInputRegisters(inputRegisters, 4);

    ready = JWPLC_ModbusRTU.begin(
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

    resetMapsAndStats();
    Serial.println("A14_RTU_MULTIFC_SLAVE_BOOT=PASS");
}

void loop()
{
    serviceSerial();

    if (ready)
        JWPLC_ModbusRTU.task();
}
