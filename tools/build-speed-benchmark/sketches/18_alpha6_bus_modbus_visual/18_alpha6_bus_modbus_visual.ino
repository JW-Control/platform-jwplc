#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_ModbusRTU.h>

// ============================================================
// ALPHA6 - VALIDACION VISUAL BUS / MODBUS RTU
//
// El MISMO firmware se usa en ambos JWPLC.
//
// Comandos:
//   S = configurar como Slave ID 1
//   M = configurar como Master
//   X = detener Modbus
//   ? = mostrar ayuda
//
// Configuracion:
//   19200 baud
//   SERIAL_8E1
// ============================================================

enum TestRole : uint8_t
{
    ROLE_IDLE = 0,
    ROLE_SLAVE,
    ROLE_MASTER
};

static TestRole g_role = ROLE_IDLE;

static constexpr uint8_t SLAVE_ID = 1;
static constexpr uint8_t MASTER_LOCAL_ID = 247;

static constexpr uint32_t MODBUS_BAUD = 19200UL;
static constexpr uint32_t MODBUS_CONFIG = SERIAL_8E1;

// Periodo menor que BUS_LED_ACTIVE_HOLD_MS (800 ms).
// Cuando hay comunicacion correcta, BUS deberia mantenerse verde.
static constexpr uint32_t MASTER_PERIOD_MS = 700UL;

// Suficiente para una respuesta pequena a 19200 baud,
// pero corto para que el TMO visual aparezca rapidamente.
static constexpr uint32_t MASTER_TIMEOUT_MS = 250UL;

static uint16_t g_holdingRegisters[4] =
{
    1000,
    2000,
    3000,
    4000
};

static uint16_t g_masterValues[2] = {0, 0};

static uint32_t g_lastMasterPollMs = 0;
static uint32_t g_slaveCounterMs = 0;

static void printHelp()
{
    Serial.println();
    Serial.println(F("=============================================="));
    Serial.println(F(" ALPHA6 - BUS / MODBUS RTU VISUAL TEST"));
    Serial.println(F("=============================================="));
    Serial.println(F("Usar el MISMO firmware en ambos JWPLC."));
    Serial.println();
    Serial.println(F(" S = Slave ID 1"));
    Serial.println(F(" M = Master -> consulta Slave ID 1"));
    Serial.println(F(" X = detener Modbus"));
    Serial.println(F(" ? = ayuda"));
    Serial.println();
    Serial.println(F("Prueba recomendada:"));
    Serial.println(F(" 1. En JWPLC A enviar M sin Slave activo."));
    Serial.println(F("    Esperado: BUS = TMO rojo."));
    Serial.println(F(" 2. En JWPLC B enviar S."));
    Serial.println(F("    Esperado: comunicacion OK."));
    Serial.println(F(" 3. Master debe pasar TMO -> --- verde sin reset."));
    Serial.println(F(" 4. Slave debe mostrar --- verde con actividad."));
    Serial.println();
}

static void stopModbus()
{
    JWPLC_ModbusRTU.end();

    g_role = ROLE_IDLE;
    g_lastMasterPollMs = 0;

    JWPLC_Display.goIdle();

    Serial.println();
    Serial.println(F("BUS_TEST_ROLE=IDLE"));
    Serial.println(F("Modbus detenido."));
}

static void configureSlave()
{
    JWPLC_ModbusRTU.end();

    g_holdingRegisters[0] = 1000;
    g_holdingRegisters[1] = 2000;
    g_holdingRegisters[2] = 3000;
    g_holdingRegisters[3] = 4000;

    JWPLC_ModbusRTU.setHoldingRegisters(
        g_holdingRegisters,
        4);

    const bool ok = JWPLC_ModbusRTU.begin(
        SLAVE_ID,
        MODBUS_BAUD,
        MODBUS_CONFIG);

    g_role = ok ? ROLE_SLAVE : ROLE_IDLE;
    g_slaveCounterMs = millis();

    JWPLC_Display.goIdle();

    Serial.println();
    Serial.println(F("=============================================="));
    Serial.println(F(" ROLE = SLAVE"));
    Serial.println(F("=============================================="));
    Serial.print(F("BEGIN="));
    Serial.println(ok ? F("PASS") : F("FAIL"));
    Serial.println(F("Slave ID : 1"));
    Serial.println(F("Baud     : 19200"));
    Serial.println(F("Formato  : SERIAL_8E1"));

    if (ok)
    {
        Serial.println(F("BUS_TEST_SLAVE_READY=PASS"));
        Serial.println(F("Esperado IDLE: BUS --- negro."));
        Serial.println(F("Esperado con trafico: BUS --- verde."));
    }
    else
    {
        Serial.print(F("ERROR="));
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        Serial.println(F("BUS_TEST_SLAVE_READY=FAIL"));
    }
}

static void configureMaster()
{
    JWPLC_ModbusRTU.end();

    const bool ok = JWPLC_ModbusRTU.begin(
        MASTER_LOCAL_ID,
        MODBUS_BAUD,
        MODBUS_CONFIG);

    g_role = ok ? ROLE_MASTER : ROLE_IDLE;

    // Permitir la primera consulta inmediatamente.
    g_lastMasterPollMs = millis() - MASTER_PERIOD_MS;

    JWPLC_Display.goIdle();

    Serial.println();
    Serial.println(F("=============================================="));
    Serial.println(F(" ROLE = MASTER"));
    Serial.println(F("=============================================="));
    Serial.print(F("BEGIN="));
    Serial.println(ok ? F("PASS") : F("FAIL"));
    Serial.println(F("Local ID : 247"));
    Serial.println(F("Target   : Slave ID 1"));
    Serial.println(F("Baud     : 19200"));
    Serial.println(F("Formato  : SERIAL_8E1"));
    Serial.println(F("Periodo  : 700 ms"));
    Serial.println(F("Timeout  : 250 ms"));

    if (ok)
    {
        Serial.println(F("BUS_TEST_MASTER_READY=PASS"));
        Serial.println(F("Sin Slave: esperar TMO rojo."));
        Serial.println(F("Con Slave: esperar --- verde."));
    }
    else
    {
        Serial.print(F("ERROR="));
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        Serial.println(F("BUS_TEST_MASTER_READY=FAIL"));
    }
}

static void processSerialCommand()
{
    while (Serial.available() > 0)
    {
        char c = (char)Serial.read();

        if (c >= 'a' && c <= 'z')
        {
            c = (char)(c - 'a' + 'A');
        }

        switch (c)
        {
        case 'S':
            configureSlave();
            break;

        case 'M':
            configureMaster();
            break;

        case 'X':
            stopModbus();
            break;

        case '?':
            printHelp();
            break;

        case '\r':
        case '\n':
        case ' ':
        case '\t':
            break;

        default:
            Serial.print(F("Comando desconocido: "));
            Serial.println(c);
            printHelp();
            break;
        }
    }
}

static void runSlave()
{
    // Mantener un registro cambiante para demostrar que no recibimos
    // simplemente un valor fijo almacenado una vez.
    const uint32_t now = millis();

    if ((uint32_t)(now - g_slaveCounterMs) >= 1000UL)
    {
        g_slaveCounterMs = now;
        g_holdingRegisters[0]++;
    }

    // Obligatorio para responder solicitudes como Slave.
    JWPLC_ModbusRTU.task();
}

static void runMaster()
{
    const uint32_t now = millis();

    if ((uint32_t)(now - g_lastMasterPollMs) < MASTER_PERIOD_MS)
    {
        return;
    }

    g_lastMasterPollMs = now;

    const uint32_t startMs = millis();

    const bool ok = JWPLC_ModbusRTU.readHoldingRegisters(
        SLAVE_ID,
        0,
        2,
        g_masterValues,
        MASTER_TIMEOUT_MS);

    const uint32_t elapsedMs = millis() - startMs;

    if (ok)
    {
        Serial.print(F("MASTER_READ=OK"));
        Serial.print(F(" HR0="));
        Serial.print(g_masterValues[0]);
        Serial.print(F(" HR1="));
        Serial.print(g_masterValues[1]);
        Serial.print(F(" time_ms="));
        Serial.print(elapsedMs);
        Serial.print(F(" error="));
        Serial.println(JWPLC_ModbusRTU.lastErrorString());

        Serial.println(F("BUS_EXPECT=--- GREEN"));
    }
    else
    {
        Serial.print(F("MASTER_READ=FAIL"));
        Serial.print(F(" time_ms="));
        Serial.print(elapsedMs);
        Serial.print(F(" error="));
        Serial.println(JWPLC_ModbusRTU.lastErrorString());

        if (JWPLC_ModbusRTU.lastError() == JWPLC_MODBUS_TIMEOUT)
        {
            Serial.println(F("BUS_EXPECT=TMO RED"));
        }
        else
        {
            Serial.println(F("BUS_EXPECT=OTHER_ERROR"));
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    JWPLC_Display.goIdle();

    printHelp();

    Serial.println(F("BUS_TEST_BOOT=PASS"));
    Serial.println(F("Esperando comando M o S..."));
}

void loop()
{
    processSerialCommand();

    switch (g_role)
    {
    case ROLE_SLAVE:
        runSlave();
        break;

    case ROLE_MASTER:
        runMaster();
        break;

    case ROLE_IDLE:
    default:
        break;
    }

    delay(1);
}