/*
  ModbusRTU_Master_Cooperative_Validation

  Gate Alpha7 para validar el Master cooperativo/no bloqueante.

  Peer recomendado:
    ModbusRTU_Slave_HoldingRegisters

  Configuracion:
  - Master local ID interno: 247
  - Slave objetivo: 1
  - 19200, SERIAL_8E1
  - FC06 escribe challenge en HR1
  - FC03 lee HR0..HR3 y verifica HR1

  Prueba de timeout:
  1. Deja ambos JWPLC conectados y verifica ciclos WRITE/READ PASS.
  2. Desconecta A/B del Slave sin resetear el Master.
  3. Debe aparecer TIMEOUT con APP_TICKS > 0 durante la espera.
  4. Reconecta A/B y deben volver los PASS sin reset del Master.

  Si APP_TICKS sigue aumentando durante un timeout, la aplicacion no quedo
  detenida esperando la respuesta Modbus.
*/

#include <JWPLC_ModbusRTU.h>

static constexpr uint8_t MASTER_LOCAL_ID = 247;
static constexpr uint8_t TARGET_SLAVE_ID = 1;
static constexpr uint32_t MODBUS_BAUD = 19200UL;
static constexpr uint32_t MODBUS_CONFIG = SERIAL_8E1;
static constexpr uint32_t MODBUS_TIMEOUT_MS = 1000UL;
static constexpr uint32_t CYCLE_PAUSE_MS = 500UL;

enum TestPhase : uint8_t
{
    TEST_START_WRITE = 0,
    TEST_WAIT_WRITE,
    TEST_START_READ,
    TEST_WAIT_READ,
    TEST_PAUSE
};

static TestPhase phase = TEST_START_WRITE;
static uint16_t challenge = 0;
static uint16_t values[4] = {};
static uint32_t appTicks = 0;
static uint32_t appTicksAtRequest = 0;
static uint32_t requestStartedAtMs = 0;
static uint32_t nextCycleMs = 0;

static void beginMeasurement()
{
    appTicksAtRequest = appTicks;
    requestStartedAtMs = millis();
}

static void printTransactionResult(const char *operation)
{
    const uint32_t elapsedMs = millis() - requestStartedAtMs;
    const uint32_t ticksDuringRequest = appTicks - appTicksAtRequest;

    Serial.print(operation);
    Serial.print(JWPLC_ModbusRTU.masterSucceeded() ? F(" PASS") : F(" FAIL"));
    Serial.print(F(" | elapsed_ms="));
    Serial.print(elapsedMs);
    Serial.print(F(" | APP_TICKS="));
    Serial.print(ticksDuringRequest);
    Serial.print(F(" | result="));
    Serial.println(JWPLC_ModbusRTU.lastErrorString());
}

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println(F("JWPLC MODBUS RTU MASTER COOPERATIVE VALIDATION - ALPHA7"));

    if (!JWPLC_ModbusRTU.begin(
            MASTER_LOCAL_ID,
            MODBUS_BAUD,
            MODBUS_CONFIG))
    {
        Serial.print(F("MODBUS_BEGIN=FAIL | "));
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        return;
    }

    JWPLC_ModbusRTU.setFrameGapMs(5);
    JWPLC_ModbusRTU.resetStats();

    Serial.println(F("MODBUS_BEGIN=PASS"));
    Serial.println(F("TASK_REQUIRED=YES"));
    Serial.println(F("Disconnect/reconnect Slave A/B to validate timeout recovery."));
}

void loop()
{
    // Trabajo de aplicacion independiente. Debe seguir avanzando incluso
    // mientras una transaccion Modbus espera respuesta o timeout.
    appTicks++;

    // Motor cooperativo: obligatorio y frecuente.
    JWPLC_ModbusRTU.task();

    const uint32_t now = millis();

    switch (phase)
    {
    case TEST_START_WRITE:
        challenge++;
        beginMeasurement();

        if (JWPLC_ModbusRTU.requestWriteSingleRegister(
                TARGET_SLAVE_ID,
                1,
                challenge,
                MODBUS_TIMEOUT_MS))
        {
            phase = TEST_WAIT_WRITE;
        }
        else
        {
            Serial.print(F("FC06 REQUEST REJECTED | "));
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
            nextCycleMs = now + CYCLE_PAUSE_MS;
            phase = TEST_PAUSE;
        }
        break;

    case TEST_WAIT_WRITE:
        if (!JWPLC_ModbusRTU.masterDone())
            break;

        printTransactionResult("FC06");

        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            JWPLC_ModbusRTU.clearMasterResult();
            phase = TEST_START_READ;
        }
        else
        {
            JWPLC_ModbusRTU.clearMasterResult();
            nextCycleMs = now + CYCLE_PAUSE_MS;
            phase = TEST_PAUSE;
        }
        break;

    case TEST_START_READ:
        beginMeasurement();

        if (JWPLC_ModbusRTU.requestReadHoldingRegisters(
                TARGET_SLAVE_ID,
                0,
                4,
                values,
                MODBUS_TIMEOUT_MS))
        {
            phase = TEST_WAIT_READ;
        }
        else
        {
            Serial.print(F("FC03 REQUEST REJECTED | "));
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
            nextCycleMs = now + CYCLE_PAUSE_MS;
            phase = TEST_PAUSE;
        }
        break;

    case TEST_WAIT_READ:
        if (!JWPLC_ModbusRTU.masterDone())
            break;

        printTransactionResult("FC03");

        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            Serial.print(F("VERIFY_HR1="));
            Serial.print(values[1] == challenge ? F("PASS") : F("FAIL"));
            Serial.print(F(" | expected="));
            Serial.print(challenge);
            Serial.print(F(" actual="));
            Serial.println(values[1]);
        }

        JWPLC_ModbusRTU.clearMasterResult();
        nextCycleMs = now + CYCLE_PAUSE_MS;
        phase = TEST_PAUSE;
        break;

    case TEST_PAUSE:
        if ((int32_t)(now - nextCycleMs) >= 0)
            phase = TEST_START_WRITE;
        break;
    }

    static uint32_t lastStatsMs = 0;

    if ((uint32_t)(now - lastStatsMs) >= 5000UL)
    {
        lastStatsMs = now;

        const JWPLCModbusRTUStats &s = JWPLC_ModbusRTU.stats();

        Serial.print(F("STATS TX="));
        Serial.print(s.txFrames);
        Serial.print(F(" RX="));
        Serial.print(s.rxFrames);
        Serial.print(F(" OK="));
        Serial.print(s.requestsOk);
        Serial.print(F(" CRC="));
        Serial.print(s.crcErrors);
        Serial.print(F(" TMO="));
        Serial.println(s.masterTimeouts);
    }
}
