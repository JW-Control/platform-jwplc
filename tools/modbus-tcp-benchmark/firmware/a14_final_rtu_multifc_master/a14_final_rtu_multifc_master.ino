/*
  Alpha14 final RTU R2 - Multi-FC FAST Master

  Ciclo:
    FC01 read coils 8
    FC02 read discrete inputs 8
    FC03 read holding 2
    FC04 read input registers 4
    FC05 write single coil + FC01 verify
    FC06 write single register + FC03 verify
    FC0F write multiple coils 8 + FC01 verify
    FC10 write multiple registers 2 + FC03 verify

  500 kbaud / FIFO9 / BULK / QUEUED / GAP Master.
  Serial queda mudo durante la ventana.
*/

#include <Arduino.h>
#include <esp_system.h>
#include <JWPLC_ModbusRTU.h>
#include <JWPLC_RS485.h>

static constexpr uint8_t MASTER_ID = 247;
static constexpr uint8_t SLAVE_ID = 2;
static constexpr uint32_t BAUD = 500000UL;
static constexpr uint32_t CONFIG = SERIAL_8N1;
static constexpr uint32_t TIMEOUT_MS = 25UL;
static constexpr uint32_t FRAME_GAP_US = 100UL;
static constexpr uint8_t RX_FIFO = 9U;

enum Operation : uint8_t
{
    OP_FC01_BASE = 0,
    OP_FC02,
    OP_FC03_BASE,
    OP_FC04,
    OP_FC05,
    OP_FC01_AFTER_FC05,
    OP_FC06,
    OP_FC03_AFTER_FC06,
    OP_FC0F,
    OP_FC01_AFTER_FC0F,
    OP_FC10,
    OP_FC03_AFTER_FC10,
    OP_COUNT
};

static const uint8_t FUNCTION_CODES[OP_COUNT] = {
    0x01, 0x02, 0x03, 0x04,
    0x05, 0x01, 0x06, 0x03,
    0x0F, 0x01, 0x10, 0x03
};

static uint32_t bootMarker = 0;
static bool ready = false;
static bool running = false;
static bool stopRequested = false;
static bool transactionPending = false;
static uint8_t opIndex = 0;

static uint8_t coilsRead = 0;
static uint8_t discreteRead = 0;
static uint16_t holdingRead[2] = {};
static uint16_t inputRead[4] = {};

static uint8_t expectedCoils = 0x5AU;
static constexpr uint8_t EXPECTED_DISCRETE = 0xA5U;
static uint16_t expectedHolding[2] = {0x1111U, 0x2222U};
static constexpr uint16_t EXPECTED_INPUT[4] = {
    0x3101U, 0x3202U, 0x3303U, 0x3404U
};

static uint8_t multiCoilsWrite = 0;
static uint16_t multiRegistersWrite[2] = {};

static uint32_t started[OP_COUNT] = {};
static uint32_t success[OP_COUNT] = {};
static uint32_t failed[OP_COUNT] = {};
static uint32_t verifyFails = 0;
static uint32_t requestRejected = 0;
static uint32_t cycles = 0;
static uint32_t transactionStartUs = 0;
static uint32_t transactionMaxUs = 0;
static uint32_t sequence = 0;

static void updateMax(uint32_t v, uint32_t &dst)
{
    if (v > dst) dst = v;
}

static void resetCounters()
{
    JWPLC_ModbusRTU.resetStats();

    for (uint8_t i = 0; i < OP_COUNT; ++i)
    {
        started[i] = 0;
        success[i] = 0;
        failed[i] = 0;
    }

    verifyFails = 0;
    requestRejected = 0;
    cycles = 0;
    transactionStartUs = 0;
    transactionMaxUs = 0;
    sequence = 0;
    transactionPending = false;
    opIndex = 0;
    stopRequested = false;

    expectedCoils = 0x5AU;
    expectedHolding[0] = 0x1111U;
    expectedHolding[1] = 0x2222U;
}

static bool verifyCurrentOperation()
{
    switch ((Operation)opIndex)
    {
    case OP_FC01_BASE:
    case OP_FC01_AFTER_FC05:
    case OP_FC01_AFTER_FC0F:
        return coilsRead == expectedCoils;

    case OP_FC02:
        return discreteRead == EXPECTED_DISCRETE;

    case OP_FC03_BASE:
    case OP_FC03_AFTER_FC06:
    case OP_FC03_AFTER_FC10:
        return
            holdingRead[0] == expectedHolding[0] &&
            holdingRead[1] == expectedHolding[1];

    case OP_FC04:
        for (uint8_t i = 0; i < 4U; ++i)
        {
            if (inputRead[i] != EXPECTED_INPUT[i])
                return false;
        }
        return true;

    case OP_FC05:
    case OP_FC06:
    case OP_FC0F:
    case OP_FC10:
        return true;

    default:
        return false;
    }
}

static bool startCurrentOperation()
{
    const uint32_t next = ++sequence;

    coilsRead = 0;
    discreteRead = 0;

    switch ((Operation)opIndex)
    {
    case OP_FC01_BASE:
    case OP_FC01_AFTER_FC05:
    case OP_FC01_AFTER_FC0F:
        return JWPLC_ModbusRTU.requestReadCoils(
            SLAVE_ID, 0, 8, &coilsRead, TIMEOUT_MS);

    case OP_FC02:
        return JWPLC_ModbusRTU.requestReadDiscreteInputs(
            SLAVE_ID, 0, 8, &discreteRead, TIMEOUT_MS);

    case OP_FC03_BASE:
    case OP_FC03_AFTER_FC06:
    case OP_FC03_AFTER_FC10:
        holdingRead[0] = 0;
        holdingRead[1] = 0;
        return JWPLC_ModbusRTU.requestReadHoldingRegisters(
            SLAVE_ID, 0, 2, holdingRead, TIMEOUT_MS);

    case OP_FC04:
        for (uint8_t i = 0; i < 4U; ++i) inputRead[i] = 0;
        return JWPLC_ModbusRTU.requestReadInputRegisters(
            SLAVE_ID, 0, 4, inputRead, TIMEOUT_MS);

    case OP_FC05:
    {
        const bool value = (next & 1U) != 0U;
        const bool accepted = JWPLC_ModbusRTU.requestWriteSingleCoil(
            SLAVE_ID, 0, value, TIMEOUT_MS);
        if (accepted)
        {
            if (value) expectedCoils |= 0x01U;
            else expectedCoils &= (uint8_t)~0x01U;
        }
        return accepted;
    }

    case OP_FC06:
    {
        const uint16_t value =
            (uint16_t)(0x5000U ^ (uint16_t)(next * 13U));
        const bool accepted = JWPLC_ModbusRTU.requestWriteSingleRegister(
            SLAVE_ID, 0, value, TIMEOUT_MS);
        if (accepted) expectedHolding[0] = value;
        return accepted;
    }

    case OP_FC0F:
        multiCoilsWrite = (uint8_t)(0x3CU ^ (uint8_t)(next & 0xFFU));
        if (JWPLC_ModbusRTU.requestWriteMultipleCoils(
                SLAVE_ID, 0, 8, &multiCoilsWrite, TIMEOUT_MS))
        {
            expectedCoils = multiCoilsWrite;
            return true;
        }
        return false;

    case OP_FC10:
        multiRegistersWrite[0] =
            (uint16_t)(0x6000U ^ (uint16_t)(next * 17U));
        multiRegistersWrite[1] =
            (uint16_t)(0x7000U ^ (uint16_t)(next * 29U));

        if (JWPLC_ModbusRTU.requestWriteMultipleRegisters(
                SLAVE_ID, 0, 2, multiRegistersWrite, TIMEOUT_MS))
        {
            expectedHolding[0] = multiRegistersWrite[0];
            expectedHolding[1] = multiRegistersWrite[1];
            return true;
        }
        return false;

    default:
        return false;
    }
}

static void advanceOperation()
{
    ++opIndex;

    if (opIndex >= OP_COUNT)
    {
        opIndex = 0;
        ++cycles;
    }
}

static void serviceTraffic()
{
    if (!ready || !running)
        return;

    JWPLC_ModbusRTU.task();

    if (transactionPending)
    {
        if (!JWPLC_ModbusRTU.masterDone())
            return;

        const uint32_t dt =
            (uint32_t)(micros() - transactionStartUs);
        updateMax(dt, transactionMaxUs);

        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            ++success[opIndex];

            if (!verifyCurrentOperation())
                ++verifyFails;
        }
        else
        {
            ++failed[opIndex];
        }

        JWPLC_ModbusRTU.clearMasterResult();
        transactionPending = false;
        transactionStartUs = 0;
        advanceOperation();
    }

    if (stopRequested &&
        opIndex == 0 &&
        !transactionPending &&
        !JWPLC_ModbusRTU.masterBusy())
    {
        running = false;
        stopRequested = false;
        Serial.println("A14_RTU_MULTIFC_MASTER_STOP=PASS");
        return;
    }

    if (transactionPending || JWPLC_ModbusRTU.masterBusy())
        return;

    if (startCurrentOperation())
    {
        ++started[opIndex];
        transactionStartUs = micros();
        transactionPending = true;
    }
    else
    {
        ++requestRejected;
    }
}

static void printSnapshot()
{
    const JWPLCModbusRTUStats &s = JWPLC_ModbusRTU.stats();

    Serial.println("A14_RTU_MULTIFC_MASTER_SNAPSHOT=BEGIN");
    Serial.print("BOOT_MARKER=");
    Serial.println(bootMarker);
    Serial.print("UPTIME_MS=");
    Serial.println(millis());
    Serial.print("MASTER_READY=");
    Serial.println(ready ? "YES" : "NO");
    Serial.print("RUNNING=");
    Serial.println(running ? "YES" : "NO");
    Serial.print("STOP_REQUESTED=");
    Serial.println(stopRequested ? "YES" : "NO");
    Serial.print("RTU_BAUD_EFFECTIVE=");
    Serial.println(JWPLC_ModbusRTU.effectiveBaudRate());
    Serial.print("RTU_RX_FIFO_FULL=");
    Serial.println(RX_FIFO);
    Serial.print("RTU_RX_MODE=");
    Serial.println(JWPLC_ModbusRTU.bulkRxEnabled() ? "BULK" : "BYTE");
    Serial.print("RTU_TX_MODE=");
    Serial.println(JWPLC_ModbusRTU.queuedTxActive() ? "QUEUED" : "BLOCKING");
    Serial.print("RTU_SERVER_FRAMING=");
    Serial.println(
        JWPLC_ModbusRTU.earlyServerDispatchEnabled()
            ? "STRUCTURAL"
            : "GAP");
    Serial.print("RTU_FRAME_GAP_US=");
    Serial.println(JWPLC_ModbusRTU.frameGapUs());

    for (uint8_t i = 0; i < OP_COUNT; ++i)
    {
        Serial.print("OP");
        Serial.print(i);
        Serial.print("_FC=");
        Serial.println(FUNCTION_CODES[i]);

        Serial.print("OP");
        Serial.print(i);
        Serial.print("_STARTED=");
        Serial.println(started[i]);

        Serial.print("OP");
        Serial.print(i);
        Serial.print("_SUCCESS=");
        Serial.println(success[i]);

        Serial.print("OP");
        Serial.print(i);
        Serial.print("_FAILED=");
        Serial.println(failed[i]);
    }

    Serial.print("VERIFY_FAILS=");
    Serial.println(verifyFails);
    Serial.print("REQUEST_REJECTED=");
    Serial.println(requestRejected);
    Serial.print("CYCLES=");
    Serial.println(cycles);
    Serial.print("TRANSACTION_MAX_US=");
    Serial.println(transactionMaxUs);
    Serial.print("RTU_RX_FRAMES=");
    Serial.println(s.rxFrames);
    Serial.print("RTU_TX_FRAMES=");
    Serial.println(s.txFrames);
    Serial.print("RTU_REQUESTS_OK=");
    Serial.println(s.requestsOk);
    Serial.print("RTU_CRC_ERRORS=");
    Serial.println(s.crcErrors);
    Serial.print("RTU_MASTER_TIMEOUTS=");
    Serial.println(s.masterTimeouts);
    Serial.println("A14_RTU_MULTIFC_MASTER_SNAPSHOT=END");
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();

        if (c == 'R' || c == 'r')
        {
            running = false;
            resetCounters();
            Serial.println("A14_RTU_MULTIFC_MASTER_RESET=PASS");
        }
        else if (c == 'G' || c == 'g')
        {
            resetCounters();
            running = ready;
            Serial.println(
                running
                    ? "A14_RTU_MULTIFC_MASTER_START=PASS"
                    : "A14_RTU_MULTIFC_MASTER_START=FAIL");
        }
        else if (c == 'X' || c == 'x')
        {
            if (running)
                stopRequested = true;
            else
                Serial.println("A14_RTU_MULTIFC_MASTER_STOP=PASS");
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

    ready = JWPLC_ModbusRTU.begin(
        MASTER_ID, BAUD, CONFIG);

    if (ready)
    {
        ready = JWPLC_ModbusRTU.motor(ASYNC);
        JWPLC_ModbusRTU.setFrameGapUs(FRAME_GAP_US);
        JWPLC_ModbusRTU.setBulkRxEnabled(true);
        JWPLC_ModbusRTU.setQueuedTxEnabled(true);
        JWPLC_ModbusRTU.setEarlyServerDispatchEnabled(false);

        if (!JWPLC_RS485.serial().setRxFIFOFull(RX_FIFO))
            ready = false;
    }

    resetCounters();
    Serial.println("A14_RTU_MULTIFC_MASTER_BOOT=PASS");
}

void loop()
{
    serviceSerial();
    serviceTraffic();
}
