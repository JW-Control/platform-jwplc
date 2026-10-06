/*
  Alpha14 final RTU R3 - Expansion workload Slave

  Un único Slave físico expone mapas suficientes para ocho slots lógicos.
  Esto modela el mix/tamaño de ADU y presupuesto del bus; no sustituye una
  prueba multidrop física con ocho dispositivos independientes.
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

static constexpr uint8_t COILS_INITIAL[3] = {
    0x5AU, 0x69U, 0x96U
};

static constexpr uint8_t DISCRETE_INITIAL[3] = {
    0xA5U, 0x3CU, 0xC3U
};

static constexpr uint16_t HOLDING_INITIAL[4] = {
    0x1111U, 0x2222U, 0x3333U, 0x4444U
};

static constexpr uint16_t INPUT_INITIAL[8] = {
    0x4100U, 0x4101U, 0x4102U, 0x4103U,
    0x4200U, 0x4201U, 0x4202U, 0x4203U
};

static uint32_t bootMarker = 0;
static bool ready = false;

static uint8_t coils[3] = {};
static uint8_t discreteInputs[3] = {};
static uint16_t holding[4] = {};
static uint16_t inputRegisters[8] = {};

static void resetMapsAndStats()
{
    for (uint8_t i = 0; i < 3U; ++i)
    {
        coils[i] = COILS_INITIAL[i];
        discreteInputs[i] = DISCRETE_INITIAL[i];
    }

    for (uint8_t i = 0; i < 4U; ++i)
        holding[i] = HOLDING_INITIAL[i];

    for (uint8_t i = 0; i < 8U; ++i)
        inputRegisters[i] = INPUT_INITIAL[i];

    JWPLC_ModbusRTU.resetStats();
}

static void printSnapshot()
{
    const JWPLCModbusRTUStats &s =
        JWPLC_ModbusRTU.stats();

    Serial.println("A14_RTU_EXPMIX_SLAVE_SNAPSHOT=BEGIN");
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

    for (uint8_t i = 0; i < 3U; ++i)
    {
        Serial.print("COILS");
        Serial.print(i);
        Serial.print("=");
        Serial.println(coils[i]);

        Serial.print("DISCRETE");
        Serial.print(i);
        Serial.print("=");
        Serial.println(discreteInputs[i]);
    }

    for (uint8_t i = 0; i < 4U; ++i)
    {
        Serial.print("HOLDING");
        Serial.print(i);
        Serial.print("=");
        Serial.println(holding[i]);
    }

    for (uint8_t i = 0; i < 8U; ++i)
    {
        Serial.print("INPUT");
        Serial.print(i);
        Serial.print("=");
        Serial.println(inputRegisters[i]);
    }

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
    Serial.println("A14_RTU_EXPMIX_SLAVE_SNAPSHOT=END");
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();

        if (c == 'R' || c == 'r')
        {
            resetMapsAndStats();
            Serial.println("A14_RTU_EXPMIX_SLAVE_RESET=PASS");
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

    JWPLC_ModbusRTU.setCoils(coils, 24);
    JWPLC_ModbusRTU.setDiscreteInputs(discreteInputs, 24);
    JWPLC_ModbusRTU.setHoldingRegisters(holding, 4);
    JWPLC_ModbusRTU.setInputRegisters(inputRegisters, 8);

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
    Serial.println("A14_RTU_EXPMIX_SLAVE_BOOT=PASS");
}

void loop()
{
    serviceSerial();

    if (ready)
        JWPLC_ModbusRTU.task();
}
