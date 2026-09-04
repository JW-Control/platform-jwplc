/*
  Gate 7NB.3A-B - I/O sincronizado direccionado (sin broadcast)

  Proposito:
    - M2 gobierna patrones de Q0.x en M2/S1/S2.
    - No usa broadcast Modbus.
    - Cada Slave se arma por direccion y ejecuta en un instante futuro.
    - La conmutacion local usa JWPLC_writeOutputs() para actualizar el banco.

  Uso:
    S1: ROLE S1
    S2: ROLE S2
    M2: ROLE M2

    En M2:
      START  -> secuencia de patrones / "juego de luces"
      CLACK  -> alterna 0x00/0xFF en los tres nodos
      STOP   -> apaga todo
      STATUS -> diagnostico del gate

  IMPORTANTE:
    Ejecutar sin cargas/actuadores reales. Solo relays/loopback de banco.
*/

#include <Arduino.h>

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t MODBUS_BAUD = 115200UL;
static constexpr uint32_t MODBUS_CONFIG = SERIAL_8N1;
static constexpr uint32_t MODBUS_TIMEOUT_MS = 120UL;
static constexpr uint8_t MASTER_MODBUS_LOCAL_ID = 247;

// El target se fija suficientemente adelante para armar ambos Slaves
// mediante transacciones direccionadas independientes.
static constexpr uint32_t SYNC_LEAD_US = 220000UL;
static constexpr uint32_t ARM_ONE_WAY_COMP_US = 1000UL;
static constexpr uint16_t DEFAULT_STEP_MS = 650U;

// Mapa minimo del gate.
// HR_MASK se prepara primero.
// HR_ARM es el trigger: high byte = sequence, low byte = delay relativo [ms].
static constexpr uint16_t HR_MASK = 0;
static constexpr uint16_t HR_ARM = 1;
static constexpr uint16_t HR_COUNT = 2;

enum LocalRole : uint8_t
{
    ROLE_NONE = 0,
    ROLE_MASTER,
    ROLE_S1,
    ROLE_S2
};

enum RunMode : uint8_t
{
    MODE_STOPPED = 0,
    MODE_SHOW,
    MODE_CLACK
};

enum MasterStage : uint8_t
{
    STAGE_IDLE = 0,
    STAGE_MASK_S1_WAIT,
    STAGE_MASK_S2_WAIT,
    STAGE_ARM_S1_WAIT,
    STAGE_ARM_S2_WAIT,
    STAGE_WAIT_TARGET
};

struct PatternStep
{
    uint8_t m2;
    uint8_t s1;
    uint8_t s2;
    uint16_t holdMs;
    const char *name;
};

static const PatternStep showPattern[] = {
    {0x00, 0x00, 0x00, 450, "all-off"},
    {0xFF, 0xFF, 0xFF, 650, "all-on"},
    {0x00, 0x00, 0x00, 450, "all-off"},

    // Chase: mismo canal en las tres placas.
    {0x01, 0x01, 0x01, 350, "chase-q0"},
    {0x02, 0x02, 0x02, 350, "chase-q1"},
    {0x04, 0x04, 0x04, 350, "chase-q2"},
    {0x08, 0x08, 0x08, 350, "chase-q3"},
    {0x10, 0x10, 0x10, 350, "chase-q4"},
    {0x20, 0x20, 0x20, 350, "chase-q5"},
    {0x40, 0x40, 0x40, 350, "chase-q6"},
    {0x80, 0x80, 0x80, 450, "chase-q7"},

    // Alternancia cruzada entre nodos.
    {0xAA, 0x55, 0xAA, 650, "alternate-a"},
    {0x55, 0xAA, 0x55, 650, "alternate-b"},
    {0xAA, 0x55, 0xAA, 650, "alternate-a"},
    {0x55, 0xAA, 0x55, 650, "alternate-b"},

    // Ola por placas: 8 OFF y 8 ON deben ocurrir juntos en cada frontera.
    {0xFF, 0x00, 0x00, 500, "wave-m2"},
    {0x00, 0xFF, 0x00, 500, "wave-s1"},
    {0x00, 0x00, 0xFF, 500, "wave-s2"},
    {0xFF, 0x00, 0x00, 500, "wave-m2"},

    // Figuras simetricas.
    {0x81, 0x42, 0x24, 500, "mirror-1"},
    {0x42, 0x24, 0x18, 500, "mirror-2"},
    {0x24, 0x18, 0x24, 500, "mirror-3"},
    {0x18, 0x24, 0x42, 500, "mirror-4"},
    {0x24, 0x42, 0x81, 650, "mirror-5"},

    {0x00, 0x00, 0x00, 700, "all-off"}
};

static constexpr size_t SHOW_STEP_COUNT = sizeof(showPattern) / sizeof(showPattern[0]);

static LocalRole role = ROLE_NONE;
static RunMode runMode = MODE_STOPPED;
static MasterStage masterStage = STAGE_IDLE;
static String serialLine;

static uint16_t holding[HR_COUNT] = {0, 0};
static uint16_t lastArm = 0;
static bool slavePending = false;
static uint8_t slavePendingMask = 0;
static uint32_t slaveApplyAtUs = 0;
static uint32_t slaveApplyCount = 0;

static uint8_t txSequence = 0;
static uint8_t activeM2Mask = 0;
static uint8_t activeS1Mask = 0;
static uint8_t activeS2Mask = 0;
static uint16_t activeHoldMs = DEFAULT_STEP_MS;
static const char *activeName = "idle";
static uint32_t masterTargetUs = 0;
static uint32_t nextStepMs = 0;
static size_t showIndex = 0;
static bool clackOn = false;
static uint32_t masterApplyCount = 0;
static uint32_t syncFailCount = 0;
static uint32_t maxArmTxnUs = 0;
static uint32_t currentTxnStartedUs = 0;

static void applyMask(uint8_t mask)
{
    JWPLC_writeOutputs(mask);
}

static bool dueUs(uint32_t now, uint32_t target)
{
    return (int32_t)(now - target) >= 0;
}

static const char *roleName()
{
    switch (role)
    {
        case ROLE_MASTER: return "M2";
        case ROLE_S1: return "S1";
        case ROLE_S2: return "S2";
        default: return "NONE";
    }
}

static const char *modeName()
{
    switch (runMode)
    {
        case MODE_SHOW: return "SHOW";
        case MODE_CLACK: return "CLACK";
        default: return "STOPPED";
    }
}

static void serviceSlaveSchedule()
{
    if (role != ROLE_S1 && role != ROLE_S2)
        return;

    const uint16_t arm = holding[HR_ARM];
    if (arm != 0 && arm != lastArm)
    {
        lastArm = arm;
        const uint8_t delayMs = (uint8_t)(arm & 0x00FFU);
        slavePendingMask = (uint8_t)(holding[HR_MASK] & 0x00FFU);
        slaveApplyAtUs = micros() + ((uint32_t)delayMs * 1000UL);
        slavePending = true;

        Serial.print(F("ARM role="));
        Serial.print(roleName());
        Serial.print(F(" seq="));
        Serial.print((uint8_t)(arm >> 8));
        Serial.print(F(" mask=0x"));
        if (slavePendingMask < 16) Serial.print('0');
        Serial.print(slavePendingMask, HEX);
        Serial.print(F(" delayMs="));
        Serial.println(delayMs);
    }

    if (slavePending && dueUs(micros(), slaveApplyAtUs))
    {
        applyMask(slavePendingMask);
        slavePending = false;
        ++slaveApplyCount;
    }
}

static bool masterRequestRegister(uint8_t slaveId, uint16_t address, uint16_t value)
{
    if (JWPLC_ModbusRTU.masterDone())
        JWPLC_ModbusRTU.clearMasterResult();

    currentTxnStartedUs = micros();
    return JWPLC_ModbusRTU.requestWriteSingleRegister(
        slaveId,
        address,
        value,
        MODBUS_TIMEOUT_MS);
}

static bool masterFinishTransaction(const __FlashStringHelper *label)
{
    if (!JWPLC_ModbusRTU.masterDone())
        return false;

    const uint32_t elapsedUs = micros() - currentTxnStartedUs;
    if (elapsedUs > maxArmTxnUs)
        maxArmTxnUs = elapsedUs;

    const bool ok = JWPLC_ModbusRTU.masterSucceeded();
    if (!ok)
    {
        ++syncFailCount;
        Serial.print(F("SYNC_TX_FAIL stage="));
        Serial.print(label);
        Serial.print(F(" err="));
        Serial.println((int)JWPLC_ModbusRTU.masterResult());
    }

    JWPLC_ModbusRTU.clearMasterResult();
    return ok;
}

static uint8_t remainingDelayMs()
{
    const uint32_t now = micros();
    int32_t remaining = (int32_t)(masterTargetUs - now);
    remaining -= (int32_t)ARM_ONE_WAY_COMP_US;

    if (remaining <= 1000)
        return 1;

    uint32_t delayMs = (uint32_t)remaining / 1000UL;
    if (delayMs > 250UL)
        delayMs = 250UL;
    if (delayMs == 0)
        delayMs = 1;
    return (uint8_t)delayMs;
}

static uint16_t makeArmValue(uint8_t sequence, uint8_t delayMs)
{
    return ((uint16_t)sequence << 8) | delayMs;
}

static void loadNextPattern()
{
    if (runMode == MODE_SHOW)
    {
        const PatternStep &step = showPattern[showIndex];
        activeM2Mask = step.m2;
        activeS1Mask = step.s1;
        activeS2Mask = step.s2;
        activeHoldMs = step.holdMs;
        activeName = step.name;
        showIndex = (showIndex + 1U) % SHOW_STEP_COUNT;
        return;
    }

    // CLACK: banco completo ON/OFF en los tres nodos.
    clackOn = !clackOn;
    const uint8_t mask = clackOn ? 0xFF : 0x00;
    activeM2Mask = mask;
    activeS1Mask = mask;
    activeS2Mask = mask;
    activeHoldMs = 800;
    activeName = clackOn ? "clack-on" : "clack-off";
}

static void beginMasterStep()
{
    loadNextPattern();

    ++txSequence;
    if (txSequence == 0)
        ++txSequence;

    masterTargetUs = micros() + SYNC_LEAD_US;

    if (!masterRequestRegister(1, HR_MASK, activeS1Mask))
    {
        ++syncFailCount;
        masterStage = STAGE_IDLE;
        nextStepMs = millis() + 250;
        Serial.println(F("SYNC_START_FAIL mask-s1"));
        return;
    }

    masterStage = STAGE_MASK_S1_WAIT;
}

static void serviceMasterState()
{
    if (role != ROLE_MASTER || runMode == MODE_STOPPED)
        return;

    JWPLC_ModbusRTU.task();

    switch (masterStage)
    {
        case STAGE_IDLE:
            if ((int32_t)(millis() - nextStepMs) >= 0)
                beginMasterStep();
            break;

        case STAGE_MASK_S1_WAIT:
            if (!JWPLC_ModbusRTU.masterDone())
                break;
            if (!masterFinishTransaction(F("mask-s1")))
            {
                masterStage = STAGE_IDLE;
                nextStepMs = millis() + 250;
                break;
            }
            if (!masterRequestRegister(2, HR_MASK, activeS2Mask))
            {
                ++syncFailCount;
                masterStage = STAGE_IDLE;
                nextStepMs = millis() + 250;
                break;
            }
            masterStage = STAGE_MASK_S2_WAIT;
            break;

        case STAGE_MASK_S2_WAIT:
            if (!JWPLC_ModbusRTU.masterDone())
                break;
            if (!masterFinishTransaction(F("mask-s2")))
            {
                masterStage = STAGE_IDLE;
                nextStepMs = millis() + 250;
                break;
            }
            if (!masterRequestRegister(
                    1,
                    HR_ARM,
                    makeArmValue(txSequence, remainingDelayMs())))
            {
                ++syncFailCount;
                masterStage = STAGE_IDLE;
                nextStepMs = millis() + 250;
                break;
            }
            masterStage = STAGE_ARM_S1_WAIT;
            break;

        case STAGE_ARM_S1_WAIT:
            if (!JWPLC_ModbusRTU.masterDone())
                break;
            if (!masterFinishTransaction(F("arm-s1")))
            {
                masterStage = STAGE_IDLE;
                nextStepMs = millis() + 250;
                break;
            }
            if (!masterRequestRegister(
                    2,
                    HR_ARM,
                    makeArmValue(txSequence, remainingDelayMs())))
            {
                ++syncFailCount;
                masterStage = STAGE_IDLE;
                nextStepMs = millis() + 250;
                break;
            }
            masterStage = STAGE_ARM_S2_WAIT;
            break;

        case STAGE_ARM_S2_WAIT:
            if (!JWPLC_ModbusRTU.masterDone())
                break;
            if (!masterFinishTransaction(F("arm-s2")))
            {
                masterStage = STAGE_IDLE;
                nextStepMs = millis() + 250;
                break;
            }
            masterStage = STAGE_WAIT_TARGET;
            break;

        case STAGE_WAIT_TARGET:
            if (!dueUs(micros(), masterTargetUs))
                break;

            applyMask(activeM2Mask);
            ++masterApplyCount;

            Serial.print(F("PATTERN seq="));
            Serial.print(txSequence);
            Serial.print(F(" name="));
            Serial.print(activeName);
            Serial.print(F(" M2/S1/S2=0x"));
            if (activeM2Mask < 16) Serial.print('0');
            Serial.print(activeM2Mask, HEX);
            Serial.print(F("/0x"));
            if (activeS1Mask < 16) Serial.print('0');
            Serial.print(activeS1Mask, HEX);
            Serial.print(F("/0x"));
            if (activeS2Mask < 16) Serial.print('0');
            Serial.println(activeS2Mask, HEX);

            masterStage = STAGE_IDLE;
            nextStepMs = millis() + activeHoldMs;
            break;
    }
}

static void stopGate()
{
    runMode = MODE_STOPPED;
    masterStage = STAGE_IDLE;
    slavePending = false;
    applyMask(0x00);
    Serial.println(F("SYNC_GATE=STOPPED"));
}

static void configureRole(LocalRole newRole)
{
    JWPLC_ModbusRTU.end();
    JWPLC_RS485.end();

    role = newRole;
    runMode = MODE_STOPPED;
    masterStage = STAGE_IDLE;
    slavePending = false;
    holding[HR_MASK] = 0;
    holding[HR_ARM] = 0;
    lastArm = 0;
    applyMask(0x00);

    if (role == ROLE_MASTER)
    {
        if (!JWPLC_ModbusRTU.begin(
                MASTER_MODBUS_LOCAL_ID,
                MODBUS_BAUD,
                MODBUS_CONFIG))
        {
            Serial.println(F("ROLE M2: MODBUS FAIL"));
            role = ROLE_NONE;
            return;
        }

        JWPLC_ModbusRTU.setFrameGapMs(2);
        Serial.println(F("ROLE=M2 READY addressed-master"));
        return;
    }

    const uint8_t slaveId = (role == ROLE_S1) ? 1 : 2;
    JWPLC_ModbusRTU.setHoldingRegisters(holding, HR_COUNT);

    if (!JWPLC_ModbusRTU.begin(slaveId, MODBUS_BAUD, MODBUS_CONFIG))
    {
        Serial.println(F("ROLE SLAVE: MODBUS FAIL"));
        role = ROLE_NONE;
        return;
    }

    JWPLC_ModbusRTU.setFrameGapMs(2);
    Serial.print(F("ROLE=S"));
    Serial.print(slaveId);
    Serial.println(F(" READY addressed-scheduler"));
}

static void printStatus()
{
    Serial.print(F("STATUS role="));
    Serial.print(roleName());
    Serial.print(F(" mode="));
    Serial.print(modeName());
    Serial.print(F(" stage="));
    Serial.print((int)masterStage);
    Serial.print(F(" apply="));
    Serial.print(role == ROLE_MASTER ? masterApplyCount : slaveApplyCount);
    Serial.print(F(" syncFail="));
    Serial.print(syncFailCount);
    Serial.print(F(" maxTxnUs="));
    Serial.print(maxArmTxnUs);
    Serial.print(F(" crc="));
    Serial.println(JWPLC_ModbusRTU.stats().crcErrors);
}

static void handleCommand(String line)
{
    line.trim();
    line.toUpperCase();

    if (line == "ROLE M2")
        configureRole(ROLE_MASTER);
    else if (line == "ROLE S1")
        configureRole(ROLE_S1);
    else if (line == "ROLE S2")
        configureRole(ROLE_S2);
    else if (line == "START" && role == ROLE_MASTER)
    {
        runMode = MODE_SHOW;
        showIndex = 0;
        nextStepMs = millis();
        masterStage = STAGE_IDLE;
        Serial.println(F("SYNC_GATE=RUNNING mode=SHOW addressed"));
    }
    else if (line == "CLACK" && role == ROLE_MASTER)
    {
        runMode = MODE_CLACK;
        clackOn = false;
        nextStepMs = millis();
        masterStage = STAGE_IDLE;
        Serial.println(F("SYNC_GATE=RUNNING mode=CLACK addressed"));
    }
    else if (line == "STOP")
        stopGate();
    else if (line == "STATUS")
        printStatus();
    else
        Serial.println(F("CMD: ROLE M2 | ROLE S1 | ROLE S2 | START | CLACK | STOP | STATUS"));
}

void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(400);

    applyMask(0x00);

    Serial.println();
    Serial.println(F("JWPLC Alpha7 Gate 7NB.3A-B - addressed synchronized patterns"));
    Serial.println(F("Sin broadcast. M2 arma S1/S2 y todos ejecutan en target futuro."));
    Serial.println(F("Selecciona ROLE M2 / ROLE S1 / ROLE S2"));
}

void loop()
{
    if (role == ROLE_S1 || role == ROLE_S2)
    {
        JWPLC_ModbusRTU.task();
        serviceSlaveSchedule();
    }

    serviceMasterState();

    while (Serial.available())
    {
        const char c = (char)Serial.read();
        if (c == '\n' || c == '\r')
        {
            if (serialLine.length())
            {
                handleCommand(serialLine);
                serialLine = "";
            }
        }
        else
        {
            serialLine += c;
        }
    }
}
