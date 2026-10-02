/*
  Alpha14 final RTU gate R-FC10 - Master
  500 kbaud FAST. Alterna FC10 + FC03 readback para
  1/2/4/8/16/32/64/123 registros.
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

static constexpr uint16_t QUANTITIES[] = {1, 2, 4, 8, 16, 32, 64, 123};
static constexpr uint8_t CASE_COUNT =
    sizeof(QUANTITIES) / sizeof(QUANTITIES[0]);

static uint32_t bootMarker = 0;
static bool ready = false;
static bool running = false;
static uint16_t txValues[123] = {};
static uint16_t rxValues[123] = {};

enum Phase : uint8_t
{
    PHASE_WRITE_START = 0,
    PHASE_WRITE_WAIT,
    PHASE_READ_START,
    PHASE_READ_WAIT
};

static Phase phase = PHASE_WRITE_START;
static uint8_t caseIndex = 0;
static uint32_t patternSequence = 0;
static uint32_t transactionStartUs = 0;

static uint32_t fc10Started = 0;
static uint32_t fc10Success = 0;
static uint32_t fc10Failed = 0;
static uint32_t fc03Started = 0;
static uint32_t fc03Success = 0;
static uint32_t fc03Failed = 0;
static uint32_t rejected = 0;
static uint32_t verifyFails = 0;
static uint32_t cycles = 0;
static uint32_t casePasses[CASE_COUNT] = {};
static uint32_t transactionMaxUs = 0;
static bool boundarySelfTestPass = false;

static void updateMax(uint32_t value, uint32_t &current)
{
    if (value > current)
        current = value;
}

static void preparePattern(uint16_t quantity)
{
    ++patternSequence;

    for (uint16_t i = 0; i < quantity; ++i)
    {
        txValues[i] =
            (uint16_t)(
                0x4000U ^
                (uint16_t)(patternSequence * 37U) ^
                (uint16_t)(i * 257U));

        rxValues[i] = 0U;
    }
}

static bool verifyPattern(uint16_t quantity)
{
    for (uint16_t i = 0; i < quantity; ++i)
    {
        if (rxValues[i] != txValues[i])
            return false;
    }

    return true;
}

static void resetCounters()
{
    JWPLC_ModbusRTU.resetStats();

    fc10Started = 0;
    fc10Success = 0;
    fc10Failed = 0;
    fc03Started = 0;
    fc03Success = 0;
    fc03Failed = 0;
    rejected = 0;
    verifyFails = 0;
    cycles = 0;
    transactionMaxUs = 0;

    for (uint8_t i = 0; i < CASE_COUNT; ++i)
        casePasses[i] = 0;

    phase = PHASE_WRITE_START;
    caseIndex = 0;
    transactionStartUs = 0;
}

static void completeTransaction(bool success, bool writePhase)
{
    const uint32_t durationUs =
        transactionStartUs == 0
            ? 0U
            : (uint32_t)(micros() - transactionStartUs);

    updateMax(durationUs, transactionMaxUs);
    transactionStartUs = 0;

    if (writePhase)
    {
        if (success) ++fc10Success;
        else ++fc10Failed;
    }
    else
    {
        if (success) ++fc03Success;
        else ++fc03Failed;
    }

    if (JWPLC_ModbusRTU.masterDone())
        JWPLC_ModbusRTU.clearMasterResult();
}

static void serviceTraffic()
{
    if (!running || !ready)
        return;

    JWPLC_ModbusRTU.task();

    const uint16_t quantity = QUANTITIES[caseIndex];

    switch (phase)
    {
    case PHASE_WRITE_START:
        if (JWPLC_ModbusRTU.masterBusy())
            return;

        preparePattern(quantity);

        if (JWPLC_ModbusRTU.requestWriteMultipleRegisters(
                SLAVE_ID,
                0,
                quantity,
                txValues,
                TIMEOUT_MS))
        {
            ++fc10Started;
            transactionStartUs = micros();
            phase = PHASE_WRITE_WAIT;
        }
        else
        {
            ++rejected;
        }
        break;

    case PHASE_WRITE_WAIT:
        if (!JWPLC_ModbusRTU.masterDone())
            return;

        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            completeTransaction(true, true);
            phase = PHASE_READ_START;
        }
        else
        {
            completeTransaction(false, true);
            phase = PHASE_WRITE_START;
        }
        break;

    case PHASE_READ_START:
        if (JWPLC_ModbusRTU.masterBusy())
            return;

        if (JWPLC_ModbusRTU.requestReadHoldingRegisters(
                SLAVE_ID,
                0,
                quantity,
                rxValues,
                TIMEOUT_MS))
        {
            ++fc03Started;
            transactionStartUs = micros();
            phase = PHASE_READ_WAIT;
        }
        else
        {
            ++rejected;
        }
        break;

    case PHASE_READ_WAIT:
        if (!JWPLC_ModbusRTU.masterDone())
            return;

        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            completeTransaction(true, false);

            if (verifyPattern(quantity))
                ++casePasses[caseIndex];
            else
                ++verifyFails;

            ++caseIndex;
            if (caseIndex >= CASE_COUNT)
            {
                caseIndex = 0;
                ++cycles;
            }
        }
        else
        {
            completeTransaction(false, false);
        }

        phase = PHASE_WRITE_START;
        break;
    }
}

static void printSnapshot()
{
    const JWPLCModbusRTUStats &s = JWPLC_ModbusRTU.stats();

    Serial.println("A14_RTU_FC10_MASTER_SNAPSHOT=BEGIN");
    Serial.print("BOOT_MARKER=");
    Serial.println(bootMarker);
    Serial.print("UPTIME_MS=");
    Serial.println(millis());
    Serial.print("MASTER_READY=");
    Serial.println(ready ? "YES" : "NO");
    Serial.print("RUNNING=");
    Serial.println(running ? "YES" : "NO");
    Serial.print("FC10_BOUNDARY_SELFTEST=");
    Serial.println(boundarySelfTestPass ? "PASS" : "FAIL");
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

    Serial.print("FC10_STARTED=");
    Serial.println(fc10Started);
    Serial.print("FC10_SUCCESS=");
    Serial.println(fc10Success);
    Serial.print("FC10_FAILED=");
    Serial.println(fc10Failed);
    Serial.print("FC03_STARTED=");
    Serial.println(fc03Started);
    Serial.print("FC03_SUCCESS=");
    Serial.println(fc03Success);
    Serial.print("FC03_FAILED=");
    Serial.println(fc03Failed);
    Serial.print("REQUEST_REJECTED=");
    Serial.println(rejected);
    Serial.print("VERIFY_FAILS=");
    Serial.println(verifyFails);
    Serial.print("CYCLES=");
    Serial.println(cycles);
    Serial.print("TRANSACTION_MAX_US=");
    Serial.println(transactionMaxUs);

    for (uint8_t i = 0; i < CASE_COUNT; ++i)
    {
        Serial.print("Q");
        Serial.print(QUANTITIES[i]);
        Serial.print("_PASSES=");
        Serial.println(casePasses[i]);
    }

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
    Serial.println("A14_RTU_FC10_MASTER_SNAPSHOT=END");
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
            Serial.println("A14_RTU_FC10_MASTER_RESET=PASS");
        }
        else if (c == 'G' || c == 'g')
        {
            resetCounters();
            running = ready;
            Serial.println(
                running
                    ? "A14_RTU_FC10_MASTER_START=PASS"
                    : "A14_RTU_FC10_MASTER_START=FAIL");
        }
        else if (c == 'X' || c == 'x')
        {
            running = false;
            Serial.println("A14_RTU_FC10_MASTER_STOP=PASS");
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

    ready =
        JWPLC_ModbusRTU.begin(
            MASTER_ID,
            BAUD,
            CONFIG);

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

    uint16_t dummy = 0x1234U;

    const bool rejectZero =
        !JWPLC_ModbusRTU.requestWriteMultipleRegisters(
            SLAVE_ID,
            0,
            0,
            &dummy,
            TIMEOUT_MS);

    const bool rejectTooMany =
        !JWPLC_ModbusRTU.requestWriteMultipleRegisters(
            SLAVE_ID,
            0,
            124,
            &dummy,
            TIMEOUT_MS);

    boundarySelfTestPass = rejectZero && rejectTooMany;

    resetCounters();

    Serial.println("A14_RTU_FC10_MASTER_BOOT=PASS");
}

void loop()
{
    serviceSerial();
    serviceTraffic();
}
