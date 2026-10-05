/*
  Alpha14 final RTU R3 - Expansion workload Master

  Pattern A: 3DI + 3DO + 1AI + 1AO
  Pattern B: 2DI + 2DO + 2AI + 2AO

  Ocho transacciones por scan. El objetivo es capacidad de workload, por lo
  que no se insertan readbacks extra dentro de la ventana. R2 ya validó la
  corrección funcional de FC02/FC04/FC0F/FC10; R3 verifica payloads de lectura,
  respuestas, cross-count y estado final escrito.

  Perfil FAST: 500 kbaud / FIFO9 / BULK / QUEUED / GAP Master.
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

static constexpr uint8_t DI_EXPECTED[3] = {
    0xA5U, 0x3CU, 0xC3U
};

static constexpr uint16_t AI_EXPECTED[8] = {
    0x4100U, 0x4101U, 0x4102U, 0x4103U,
    0x4200U, 0x4201U, 0x4202U, 0x4203U
};

static constexpr uint8_t COILS_INITIAL[3] = {
    0x5AU, 0x69U, 0x96U
};

static constexpr uint16_t HOLDING_INITIAL[4] = {
    0x1111U, 0x2222U, 0x3333U, 0x4444U
};

enum SlotType : uint8_t
{
    SLOT_DI = 0,
    SLOT_DO,
    SLOT_AI,
    SLOT_AO
};

struct Slot
{
    SlotType type;
    uint16_t startAddress;
    uint8_t logicalIndex;
};

static constexpr Slot PATTERN_3311[8] = {
    {SLOT_DI, 0, 0},
    {SLOT_DI, 8, 1},
    {SLOT_DI, 16, 2},
    {SLOT_DO, 0, 0},
    {SLOT_DO, 8, 1},
    {SLOT_DO, 16, 2},
    {SLOT_AI, 0, 0},
    {SLOT_AO, 0, 0}
};

static constexpr Slot PATTERN_2222[8] = {
    {SLOT_DI, 0, 0},
    {SLOT_DI, 8, 1},
    {SLOT_DO, 0, 0},
    {SLOT_DO, 8, 1},
    {SLOT_AI, 0, 0},
    {SLOT_AI, 4, 1},
    {SLOT_AO, 0, 0},
    {SLOT_AO, 2, 1}
};

enum PatternMode : uint8_t
{
    PATTERN_NONE = 0,
    PATTERN_3_3_1_1,
    PATTERN_2_2_2_2
};

static uint32_t bootMarker = 0;
static bool ready = false;
static bool running = false;
static bool stopRequested = false;
static bool transactionPending = false;

static PatternMode patternMode = PATTERN_NONE;
static uint8_t slotIndex = 0;
static uint32_t scanSequence = 0;

static uint8_t diRead = 0;
static uint16_t aiRead[4] = {};
static uint8_t doWrite = 0;
static uint16_t aoWrite[2] = {};

static uint8_t expectedCoils[3] = {
    COILS_INITIAL[0],
    COILS_INITIAL[1],
    COILS_INITIAL[2]
};

static uint16_t expectedHolding[4] = {
    HOLDING_INITIAL[0],
    HOLDING_INITIAL[1],
    HOLDING_INITIAL[2],
    HOLDING_INITIAL[3]
};

static uint32_t typeStarted[4] = {};
static uint32_t typeSuccess[4] = {};
static uint32_t typeFailed[4] = {};
static uint32_t verifyFails = 0;
static uint32_t requestRejected = 0;
static uint32_t scans = 0;
static uint32_t transactionStartUs = 0;
static uint32_t transactionMaxUs = 0;

static const Slot *activePattern()
{
    if (patternMode == PATTERN_3_3_1_1)
        return PATTERN_3311;

    if (patternMode == PATTERN_2_2_2_2)
        return PATTERN_2222;

    return nullptr;
}

static const char *patternName()
{
    if (patternMode == PATTERN_3_3_1_1)
        return "3DI_3DO_1AI_1AO";

    if (patternMode == PATTERN_2_2_2_2)
        return "2DI_2DO_2AI_2AO";

    return "NONE";
}

static void updateMax(uint32_t v, uint32_t &dst)
{
    if (v > dst) dst = v;
}

static void resetExpectedMaps()
{
    for (uint8_t i = 0; i < 3U; ++i)
        expectedCoils[i] = COILS_INITIAL[i];

    for (uint8_t i = 0; i < 4U; ++i)
        expectedHolding[i] = HOLDING_INITIAL[i];
}

static void resetCounters()
{
    JWPLC_ModbusRTU.resetStats();

    for (uint8_t i = 0; i < 4U; ++i)
    {
        typeStarted[i] = 0;
        typeSuccess[i] = 0;
        typeFailed[i] = 0;
    }

    verifyFails = 0;
    requestRejected = 0;
    scans = 0;
    slotIndex = 0;
    scanSequence = 0;
    transactionPending = false;
    transactionStartUs = 0;
    transactionMaxUs = 0;
    stopRequested = false;

    resetExpectedMaps();
}

static bool verifyReadSlot(const Slot &slot)
{
    if (slot.type == SLOT_DI)
        return diRead == DI_EXPECTED[slot.logicalIndex];

    if (slot.type == SLOT_AI)
    {
        const uint8_t base =
            (uint8_t)(slot.logicalIndex * 4U);

        for (uint8_t i = 0; i < 4U; ++i)
        {
            if (aiRead[i] != AI_EXPECTED[base + i])
                return false;
        }

        return true;
    }

    return true;
}

static bool startSlot(const Slot &slot)
{
    if (slot.type == SLOT_DI)
    {
        diRead = 0;

        return JWPLC_ModbusRTU.requestReadDiscreteInputs(
            SLAVE_ID,
            slot.startAddress,
            8,
            &diRead,
            TIMEOUT_MS);
    }

    if (slot.type == SLOT_DO)
    {
        doWrite =
            (uint8_t)(
                0x2DU ^
                (uint8_t)(scanSequence & 0xFFU) ^
                (uint8_t)(slot.logicalIndex * 0x33U));

        const bool accepted =
            JWPLC_ModbusRTU.requestWriteMultipleCoils(
                SLAVE_ID,
                slot.startAddress,
                8,
                &doWrite,
                TIMEOUT_MS);

        if (accepted)
            expectedCoils[slot.logicalIndex] = doWrite;

        return accepted;
    }

    if (slot.type == SLOT_AI)
    {
        for (uint8_t i = 0; i < 4U; ++i)
            aiRead[i] = 0;

        return JWPLC_ModbusRTU.requestReadInputRegisters(
            SLAVE_ID,
            slot.startAddress,
            4,
            aiRead,
            TIMEOUT_MS);
    }

    if (slot.type == SLOT_AO)
    {
        aoWrite[0] =
            (uint16_t)(
                0x6000U ^
                (uint16_t)(scanSequence * 17U) ^
                (uint16_t)(slot.logicalIndex * 0x0111U));

        aoWrite[1] =
            (uint16_t)(
                0x7000U ^
                (uint16_t)(scanSequence * 29U) ^
                (uint16_t)(slot.logicalIndex * 0x0222U));

        const bool accepted =
            JWPLC_ModbusRTU.requestWriteMultipleRegisters(
                SLAVE_ID,
                slot.startAddress,
                2,
                aoWrite,
                TIMEOUT_MS);

        if (accepted)
        {
            const uint8_t base =
                (uint8_t)(slot.logicalIndex * 2U);

            expectedHolding[base] = aoWrite[0];
            expectedHolding[base + 1U] = aoWrite[1];
        }

        return accepted;
    }

    return false;
}

static void advanceSlot()
{
    ++slotIndex;

    if (slotIndex >= 8U)
    {
        slotIndex = 0;
        ++scans;
        ++scanSequence;
    }
}

static void serviceTraffic()
{
    if (!ready || !running)
        return;

    JWPLC_ModbusRTU.task();

    const Slot *pattern = activePattern();
    if (pattern == nullptr)
        return;

    if (transactionPending)
    {
        if (!JWPLC_ModbusRTU.masterDone())
            return;

        const Slot &slot = pattern[slotIndex];
        const uint32_t dt =
            (uint32_t)(micros() - transactionStartUs);

        updateMax(dt, transactionMaxUs);

        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            ++typeSuccess[(uint8_t)slot.type];

            if (!verifyReadSlot(slot))
                ++verifyFails;
        }
        else
        {
            ++typeFailed[(uint8_t)slot.type];
        }

        JWPLC_ModbusRTU.clearMasterResult();
        transactionPending = false;
        transactionStartUs = 0;
        advanceSlot();
    }

    if (stopRequested &&
        slotIndex == 0 &&
        !transactionPending &&
        !JWPLC_ModbusRTU.masterBusy())
    {
        running = false;
        stopRequested = false;
        Serial.println("A14_RTU_EXPMIX_MASTER_STOP=PASS");
        return;
    }

    if (transactionPending || JWPLC_ModbusRTU.masterBusy())
        return;

    const Slot &slot = pattern[slotIndex];

    if (startSlot(slot))
    {
        ++typeStarted[(uint8_t)slot.type];
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
    const JWPLCModbusRTUStats &s =
        JWPLC_ModbusRTU.stats();

    Serial.println("A14_RTU_EXPMIX_MASTER_SNAPSHOT=BEGIN");
    Serial.print("BOOT_MARKER=");
    Serial.println(bootMarker);
    Serial.print("UPTIME_MS=");
    Serial.println(millis());
    Serial.print("MASTER_READY=");
    Serial.println(ready ? "YES" : "NO");
    Serial.print("RUNNING=");
    Serial.println(running ? "YES" : "NO");
    Serial.print("PATTERN=");
    Serial.println(patternName());
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

    Serial.print("DI_STARTED=");
    Serial.println(typeStarted[SLOT_DI]);
    Serial.print("DI_SUCCESS=");
    Serial.println(typeSuccess[SLOT_DI]);
    Serial.print("DI_FAILED=");
    Serial.println(typeFailed[SLOT_DI]);

    Serial.print("DO_STARTED=");
    Serial.println(typeStarted[SLOT_DO]);
    Serial.print("DO_SUCCESS=");
    Serial.println(typeSuccess[SLOT_DO]);
    Serial.print("DO_FAILED=");
    Serial.println(typeFailed[SLOT_DO]);

    Serial.print("AI_STARTED=");
    Serial.println(typeStarted[SLOT_AI]);
    Serial.print("AI_SUCCESS=");
    Serial.println(typeSuccess[SLOT_AI]);
    Serial.print("AI_FAILED=");
    Serial.println(typeFailed[SLOT_AI]);

    Serial.print("AO_STARTED=");
    Serial.println(typeStarted[SLOT_AO]);
    Serial.print("AO_SUCCESS=");
    Serial.println(typeSuccess[SLOT_AO]);
    Serial.print("AO_FAILED=");
    Serial.println(typeFailed[SLOT_AO]);

    Serial.print("VERIFY_FAILS=");
    Serial.println(verifyFails);
    Serial.print("REQUEST_REJECTED=");
    Serial.println(requestRejected);
    Serial.print("SCANS=");
    Serial.println(scans);
    Serial.print("TRANSACTION_MAX_US=");
    Serial.println(transactionMaxUs);

    for (uint8_t i = 0; i < 3U; ++i)
    {
        Serial.print("EXPECTED_COILS");
        Serial.print(i);
        Serial.print("=");
        Serial.println(expectedCoils[i]);
    }

    for (uint8_t i = 0; i < 4U; ++i)
    {
        Serial.print("EXPECTED_HOLDING");
        Serial.print(i);
        Serial.print("=");
        Serial.println(expectedHolding[i]);
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
    Serial.println("A14_RTU_EXPMIX_MASTER_SNAPSHOT=END");
}

static void startPattern(PatternMode mode)
{
    running = false;
    patternMode = mode;
    resetCounters();
    running = ready && patternMode != PATTERN_NONE;

    Serial.println(
        running
            ? "A14_RTU_EXPMIX_MASTER_START=PASS"
            : "A14_RTU_EXPMIX_MASTER_START=FAIL");
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();

        if (c == 'R' || c == 'r')
        {
            running = false;
            patternMode = PATTERN_NONE;
            resetCounters();
            Serial.println("A14_RTU_EXPMIX_MASTER_RESET=PASS");
        }
        else if (c == 'A' || c == 'a')
        {
            startPattern(PATTERN_3_3_1_1);
        }
        else if (c == 'B' || c == 'b')
        {
            startPattern(PATTERN_2_2_2_2);
        }
        else if (c == 'X' || c == 'x')
        {
            if (running)
                stopRequested = true;
            else
                Serial.println("A14_RTU_EXPMIX_MASTER_STOP=PASS");
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

    resetCounters();
    Serial.println("A14_RTU_EXPMIX_MASTER_BOOT=PASS");
}

void loop()
{
    serviceSerial();
    serviceTraffic();
}
