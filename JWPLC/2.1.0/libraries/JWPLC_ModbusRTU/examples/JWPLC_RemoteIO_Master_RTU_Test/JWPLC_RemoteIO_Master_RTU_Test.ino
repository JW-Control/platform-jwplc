/*
  JWPLC_RemoteIO_Master_RTU_Test
  Alpha7 A7.1 - Arduino Master -> Arduino Remote I/O Slave

  Objetivo:
  - Validar la API oficial JWPLC_ModbusRTU.
  - Master cooperativo/no bloqueante: request...() + task().
  - FC02 Read Discrete Inputs.
  - FC01 Read Coils.
  - FC05 Write Single Coil.
  - FC15 Write Multiple Coils.
  - Mantener imagen DO cada 40 ms durante holds para convivir con
    el fail-safe de 100 ms del Slave.

  Banco:
  - Master local ID 247.
  - Slave ID 2.
  - 115200 8N1.
  - RS-485 A/P -> A/P, B/N -> B/N y GND comun.
  - Sin cargas reales conectadas a los reles.

  Serial 115200:
  - RUN    : validacion automatica FC02/01/05/15.
  - WALK   : walking outputs Q0_0..Q0_7 con refresh DO.
  - STOP   : detener y ordenar Q=0.
  - STATUS : imprimir estado y estadisticas Modbus.
*/

#include <Arduino.h>
#include <JWPLC_ModbusRTU.h>

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t MODBUS_BAUD = 115200UL;
static constexpr uint32_t MODBUS_CONFIG = SERIAL_8N1;
static constexpr uint8_t MASTER_LOCAL_ID = 247;
static constexpr uint8_t REMOTE_SLAVE_ID = 2;
static constexpr uint32_t MODBUS_TIMEOUT_MS = 250UL;
static constexpr uint16_t MODBUS_FRAME_GAP_MS = 2;
static constexpr uint32_t DO_REFRESH_MS = 40UL;
static constexpr uint32_t VALIDATION_HOLD_MS = 800UL;
static constexpr uint32_t WALK_HOLD_MS = 1500UL;

enum RunMode : uint8_t
{
    MODE_IDLE = 0,
    MODE_VALIDATION,
    MODE_WALK
};

enum Step : uint8_t
{
    STEP_IDLE = 0,
    STEP_CLEAR_START,
    STEP_CLEAR_WAIT,
    STEP_INPUTS_START,
    STEP_INPUTS_WAIT,
    STEP_FC5_ON_START,
    STEP_FC5_ON_WAIT,
    STEP_READ_Q0_ON_START,
    STEP_READ_Q0_ON_WAIT,
    STEP_HOLD_Q0,
    STEP_READ_Q0_HELD_START,
    STEP_READ_Q0_HELD_WAIT,
    STEP_FC5_OFF_START,
    STEP_FC5_OFF_WAIT,
    STEP_READ_Q0_OFF_START,
    STEP_READ_Q0_OFF_WAIT,
    STEP_PATTERN_START,
    STEP_PATTERN_WAIT,
    STEP_READ_PATTERN_START,
    STEP_READ_PATTERN_WAIT,
    STEP_HOLD_PATTERN,
    STEP_READ_PATTERN_HELD_START,
    STEP_READ_PATTERN_HELD_WAIT,
    STEP_FINAL_CLEAR_START,
    STEP_FINAL_CLEAR_WAIT,
    STEP_FINAL_READ_START,
    STEP_FINAL_READ_WAIT,
    STEP_DONE,
    STEP_WALK_ON_START,
    STEP_WALK_ON_WAIT,
    STEP_WALK_HOLD,
    STEP_WALK_VERIFY_START,
    STEP_WALK_VERIFY_WAIT,
    STEP_WALK_OFF_START,
    STEP_WALK_OFF_WAIT,
    STEP_WALK_NEXT
};

static RunMode runMode = MODE_IDLE;
static Step step = STEP_IDLE;
static uint8_t rxBits = 0;
static uint8_t txBits = 0;
static uint32_t holdUntilMs = 0;
static uint32_t lastRefreshMs = 0;
static uint8_t walkChannel = 0;
static uint32_t passCount = 0;
static uint32_t failCount = 0;
static String serialLine;

static void printHex8(uint8_t value)
{
    if (value < 0x10)
        Serial.print('0');
    Serial.print(value, HEX);
}

static void printBitmap(const char *label, uint8_t value)
{
    Serial.print(label);
    Serial.print(F("=0x"));
    printHex8(value);
    Serial.print(F(" bits="));

    for (uint8_t i = 0; i < 8; ++i)
    {
        Serial.print((value & (1U << i)) ? '1' : '0');
        if (i != 7)
            Serial.print(' ');
    }

    Serial.println();
}

static void abortRun(const char *label)
{
    Serial.print(F("[FAIL] "));
    Serial.print(label);
    Serial.print(F(" result="));
    Serial.print((int)JWPLC_ModbusRTU.masterResult());
    Serial.print(F(" error="));
    Serial.println(JWPLC_ModbusRTU.lastErrorString());
    ++failCount;
    runMode = MODE_IDLE;
    step = STEP_IDLE;
}

static bool consumeResult(const char *label)
{
    if (!JWPLC_ModbusRTU.masterDone())
        return false;

    const bool ok = JWPLC_ModbusRTU.masterSucceeded();

    if (ok)
    {
        ++passCount;
        Serial.print(F("[PASS] "));
        Serial.println(label);
    }
    else
    {
        abortRun(label);
    }

    JWPLC_ModbusRTU.clearMasterResult();
    return ok;
}

static bool startReadCoils()
{
    rxBits = 0;
    if (JWPLC_ModbusRTU.requestReadCoils(
            REMOTE_SLAVE_ID, 0, 8, &rxBits, MODBUS_TIMEOUT_MS))
        return true;

    Serial.print(F("[FAIL] no inicia FC01: "));
    Serial.println(JWPLC_ModbusRTU.lastErrorString());
    ++failCount;
    runMode = MODE_IDLE;
    step = STEP_IDLE;
    return false;
}

static bool startReadInputs()
{
    rxBits = 0;
    if (JWPLC_ModbusRTU.requestReadDiscreteInputs(
            REMOTE_SLAVE_ID, 0, 8, &rxBits, MODBUS_TIMEOUT_MS))
        return true;

    Serial.print(F("[FAIL] no inicia FC02: "));
    Serial.println(JWPLC_ModbusRTU.lastErrorString());
    ++failCount;
    runMode = MODE_IDLE;
    step = STEP_IDLE;
    return false;
}

static bool startWriteSingleCoil(uint8_t address, bool value)
{
    if (JWPLC_ModbusRTU.requestWriteSingleCoil(
            REMOTE_SLAVE_ID, address, value, MODBUS_TIMEOUT_MS))
        return true;

    Serial.print(F("[FAIL] no inicia FC05: "));
    Serial.println(JWPLC_ModbusRTU.lastErrorString());
    ++failCount;
    runMode = MODE_IDLE;
    step = STEP_IDLE;
    return false;
}

static bool startWriteMultipleCoils(uint8_t pattern)
{
    txBits = pattern;
    if (JWPLC_ModbusRTU.requestWriteMultipleCoils(
            REMOTE_SLAVE_ID, 0, 8, &txBits, MODBUS_TIMEOUT_MS))
        return true;

    Serial.print(F("[FAIL] no inicia FC15: "));
    Serial.println(JWPLC_ModbusRTU.lastErrorString());
    ++failCount;
    runMode = MODE_IDLE;
    step = STEP_IDLE;
    return false;
}

static bool expectBits(const char *label, uint8_t expected)
{
    printBitmap(label, rxBits);

    if (rxBits == expected)
    {
        ++passCount;
        Serial.print(F("[PASS] "));
        Serial.print(label);
        Serial.print(F(" expected=0x"));
        printHex8(expected);
        Serial.println();
        return true;
    }

    ++failCount;
    Serial.print(F("[FAIL] "));
    Serial.print(label);
    Serial.print(F(" expected=0x"));
    printHex8(expected);
    Serial.print(F(" actual=0x"));
    printHex8(rxBits);
    Serial.println();
    runMode = MODE_IDLE;
    step = STEP_IDLE;
    return false;
}

static void beginHold(uint32_t durationMs)
{
    const uint32_t now = millis();
    holdUntilMs = now + durationMs;
    lastRefreshMs = now - DO_REFRESH_MS;
}

static bool serviceHold(uint8_t pattern)
{
    if (JWPLC_ModbusRTU.masterBusy())
        return false;

    if (JWPLC_ModbusRTU.masterDone())
    {
        if (!consumeResult("FC15 refresh DO"))
            return false;
    }

    const uint32_t now = millis();
    if ((int32_t)(now - holdUntilMs) >= 0)
        return true;

    if ((uint32_t)(now - lastRefreshMs) >= DO_REFRESH_MS)
    {
        lastRefreshMs = now;
        (void)startWriteMultipleCoils(pattern);
    }

    return false;
}

static void serviceValidation()
{
    switch (step)
    {
    case STEP_CLEAR_START:
        if (startWriteMultipleCoils(0x00)) step = STEP_CLEAR_WAIT;
        break;

    case STEP_CLEAR_WAIT:
        if (JWPLC_ModbusRTU.masterDone() && consumeResult("FC15 clear inicial"))
            step = STEP_INPUTS_START;
        break;

    case STEP_INPUTS_START:
        if (startReadInputs()) step = STEP_INPUTS_WAIT;
        break;

    case STEP_INPUTS_WAIT:
        if (JWPLC_ModbusRTU.masterDone() && consumeResult("FC02 read DI"))
        {
            printBitmap("DI", rxBits);
            step = STEP_FC5_ON_START;
        }
        break;

    case STEP_FC5_ON_START:
        if (startWriteSingleCoil(0, true)) step = STEP_FC5_ON_WAIT;
        break;

    case STEP_FC5_ON_WAIT:
        if (JWPLC_ModbusRTU.masterDone() && consumeResult("FC05 Q0_0 ON"))
            step = STEP_READ_Q0_ON_START;
        break;

    case STEP_READ_Q0_ON_START:
        if (startReadCoils()) step = STEP_READ_Q0_ON_WAIT;
        break;

    case STEP_READ_Q0_ON_WAIT:
        if (JWPLC_ModbusRTU.masterDone() &&
            consumeResult("FC01 feedback Q0_0 ON") &&
            expectBits("COILS", 0x01))
        {
            beginHold(VALIDATION_HOLD_MS);
            step = STEP_HOLD_Q0;
        }
        break;

    case STEP_HOLD_Q0:
        if (serviceHold(0x01)) step = STEP_READ_Q0_HELD_START;
        break;

    case STEP_READ_Q0_HELD_START:
        if (startReadCoils()) step = STEP_READ_Q0_HELD_WAIT;
        break;

    case STEP_READ_Q0_HELD_WAIT:
        if (JWPLC_ModbusRTU.masterDone() &&
            consumeResult("FC01 feedback Q0_0 after refresh") &&
            expectBits("COILS_HELD", 0x01))
            step = STEP_FC5_OFF_START;
        break;

    case STEP_FC5_OFF_START:
        if (startWriteSingleCoil(0, false)) step = STEP_FC5_OFF_WAIT;
        break;

    case STEP_FC5_OFF_WAIT:
        if (JWPLC_ModbusRTU.masterDone() && consumeResult("FC05 Q0_0 OFF"))
            step = STEP_READ_Q0_OFF_START;
        break;

    case STEP_READ_Q0_OFF_START:
        if (startReadCoils()) step = STEP_READ_Q0_OFF_WAIT;
        break;

    case STEP_READ_Q0_OFF_WAIT:
        if (JWPLC_ModbusRTU.masterDone() &&
            consumeResult("FC01 feedback Q0_0 OFF") &&
            expectBits("COILS", 0x00))
            step = STEP_PATTERN_START;
        break;

    case STEP_PATTERN_START:
        if (startWriteMultipleCoils(0x55)) step = STEP_PATTERN_WAIT;
        break;

    case STEP_PATTERN_WAIT:
        if (JWPLC_ModbusRTU.masterDone() && consumeResult("FC15 pattern 0x55"))
            step = STEP_READ_PATTERN_START;
        break;

    case STEP_READ_PATTERN_START:
        if (startReadCoils()) step = STEP_READ_PATTERN_WAIT;
        break;

    case STEP_READ_PATTERN_WAIT:
        if (JWPLC_ModbusRTU.masterDone() &&
            consumeResult("FC01 feedback 0x55") &&
            expectBits("COILS", 0x55))
        {
            beginHold(VALIDATION_HOLD_MS);
            step = STEP_HOLD_PATTERN;
        }
        break;

    case STEP_HOLD_PATTERN:
        if (serviceHold(0x55)) step = STEP_READ_PATTERN_HELD_START;
        break;

    case STEP_READ_PATTERN_HELD_START:
        if (startReadCoils()) step = STEP_READ_PATTERN_HELD_WAIT;
        break;

    case STEP_READ_PATTERN_HELD_WAIT:
        if (JWPLC_ModbusRTU.masterDone() &&
            consumeResult("FC01 feedback 0x55 after refresh") &&
            expectBits("COILS_HELD", 0x55))
            step = STEP_FINAL_CLEAR_START;
        break;

    case STEP_FINAL_CLEAR_START:
        if (startWriteMultipleCoils(0x00)) step = STEP_FINAL_CLEAR_WAIT;
        break;

    case STEP_FINAL_CLEAR_WAIT:
        if (JWPLC_ModbusRTU.masterDone() && consumeResult("FC15 clear final"))
            step = STEP_FINAL_READ_START;
        break;

    case STEP_FINAL_READ_START:
        if (startReadCoils()) step = STEP_FINAL_READ_WAIT;
        break;

    case STEP_FINAL_READ_WAIT:
        if (JWPLC_ModbusRTU.masterDone() &&
            consumeResult("FC01 feedback final") &&
            expectBits("COILS_FINAL", 0x00))
            step = STEP_DONE;
        break;

    case STEP_DONE:
        Serial.println();
        Serial.println(F("=============================================="));
        Serial.println(F("ALPHA7_A7_1_ARDUINO_ARDUINO=PASS"));
        Serial.print(F("PASS_COUNT=")); Serial.println(passCount);
        Serial.print(F("FAIL_COUNT=")); Serial.println(failCount);
        Serial.println(F("FC01=PASS"));
        Serial.println(F("FC02=PASS"));
        Serial.println(F("FC05=PASS"));
        Serial.println(F("FC15=PASS"));
        Serial.println(F("DO_REFRESH_40MS=PASS"));
        Serial.println(F("=============================================="));
        runMode = MODE_IDLE;
        step = STEP_IDLE;
        break;

    default:
        break;
    }
}

static void serviceWalk()
{
    const uint8_t pattern = (uint8_t)(1U << walkChannel);

    switch (step)
    {
    case STEP_WALK_ON_START:
        Serial.print(F("[WALK] Q0_"));
        Serial.print(walkChannel);
        Serial.println(F(" ON"));
        if (startWriteMultipleCoils(pattern)) step = STEP_WALK_ON_WAIT;
        break;

    case STEP_WALK_ON_WAIT:
        if (JWPLC_ModbusRTU.masterDone() && consumeResult("FC15 WALK ON"))
        {
            beginHold(WALK_HOLD_MS);
            step = STEP_WALK_HOLD;
        }
        break;

    case STEP_WALK_HOLD:
        if (serviceHold(pattern)) step = STEP_WALK_VERIFY_START;
        break;

    case STEP_WALK_VERIFY_START:
        if (startReadCoils()) step = STEP_WALK_VERIFY_WAIT;
        break;

    case STEP_WALK_VERIFY_WAIT:
        if (JWPLC_ModbusRTU.masterDone() &&
            consumeResult("FC01 WALK feedback") &&
            expectBits("WALK_COILS", pattern))
            step = STEP_WALK_OFF_START;
        break;

    case STEP_WALK_OFF_START:
        if (startWriteMultipleCoils(0x00)) step = STEP_WALK_OFF_WAIT;
        break;

    case STEP_WALK_OFF_WAIT:
        if (JWPLC_ModbusRTU.masterDone() && consumeResult("FC15 WALK OFF"))
            step = STEP_WALK_NEXT;
        break;

    case STEP_WALK_NEXT:
        ++walkChannel;
        if (walkChannel >= 8)
        {
            Serial.println(F("ALPHA7_A7_1_WALKING_OUTPUTS=PASS"));
            runMode = MODE_IDLE;
            step = STEP_IDLE;
        }
        else
        {
            step = STEP_WALK_ON_START;
        }
        break;

    default:
        break;
    }
}

static void printStatus()
{
    const JWPLCModbusRTUStats &stats = JWPLC_ModbusRTU.stats();

    Serial.println();
    Serial.println(F("---- MASTER STATUS ----"));
    Serial.print(F("READY=")); Serial.println(JWPLC_ModbusRTU.isReady() ? F("YES") : F("NO"));
    Serial.print(F("BUSY=")); Serial.println(JWPLC_ModbusRTU.masterBusy() ? F("YES") : F("NO"));
    Serial.print(F("MODE=")); Serial.println((int)runMode);
    Serial.print(F("STEP=")); Serial.println((int)step);
    Serial.print(F("RX/TX/OK="));
    Serial.print(stats.rxFrames); Serial.print('/');
    Serial.print(stats.txFrames); Serial.print('/');
    Serial.println(stats.requestsOk);
    Serial.print(F("CRC/TIMEOUT="));
    Serial.print(stats.crcErrors); Serial.print('/');
    Serial.println(stats.masterTimeouts);
    Serial.print(F("LAST_ERROR=")); Serial.println(JWPLC_ModbusRTU.lastErrorString());
    Serial.println(F("-----------------------"));
}

static void startValidation()
{
    if (runMode != MODE_IDLE || JWPLC_ModbusRTU.masterBusy())
    {
        Serial.println(F("[CMD] Master ocupado"));
        return;
    }

    if (JWPLC_ModbusRTU.masterDone())
        JWPLC_ModbusRTU.clearMasterResult();

    passCount = 0;
    failCount = 0;
    runMode = MODE_VALIDATION;
    step = STEP_CLEAR_START;
    Serial.println(F("A7.1 validation START"));
}

static void startWalk()
{
    if (runMode != MODE_IDLE || JWPLC_ModbusRTU.masterBusy())
    {
        Serial.println(F("[CMD] Master ocupado"));
        return;
    }

    if (JWPLC_ModbusRTU.masterDone())
        JWPLC_ModbusRTU.clearMasterResult();

    passCount = 0;
    failCount = 0;
    walkChannel = 0;
    runMode = MODE_WALK;
    step = STEP_WALK_ON_START;
    Serial.println(F("A7.1 walking outputs START"));
}

static void handleCommand(String line)
{
    line.trim();
    line.toUpperCase();

    if (line == "RUN")
        startValidation();
    else if (line == "WALK")
        startWalk();
    else if (line == "STOP")
    {
        runMode = MODE_IDLE;
        step = STEP_IDLE;
        Serial.println(F("A7.1 STOP"));
    }
    else if (line == "STATUS")
        printStatus();
    else
        Serial.println(F("CMD: RUN | WALK | STOP | STATUS"));
}

void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(500);

    Serial.println();
    Serial.println(F("JWPLC Alpha7 A7.1 - Remote I/O Master oficial"));

    if (!JWPLC_ModbusRTU.begin(MASTER_LOCAL_ID, MODBUS_BAUD, MODBUS_CONFIG))
    {
        Serial.print(F("MODBUS_BEGIN=FAIL "));
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        return;
    }

    JWPLC_ModbusRTU.setFrameGapMs(MODBUS_FRAME_GAP_MS);
    JWPLC_ModbusRTU.resetStats();

    Serial.println(F("MODBUS_BEGIN=PASS"));
    Serial.println(F("TARGET_SLAVE=2"));
    Serial.println(F("CMD: RUN | WALK | STOP | STATUS"));
}

void loop()
{
    JWPLC_ModbusRTU.task();

    if (runMode == MODE_VALIDATION)
        serviceValidation();
    else if (runMode == MODE_WALK)
        serviceWalk();

    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();

        if (c == '\n' || c == '\r')
        {
            if (serialLine.length() > 0)
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
