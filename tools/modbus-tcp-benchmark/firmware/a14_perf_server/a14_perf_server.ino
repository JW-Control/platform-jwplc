/*
  A14.3 PERF-S1 - JWPLC Modbus TCP Server benchmark

  Objetivo:
  - FC01 hasta 2000 bits
  - FC03 hasta 125 registers
  - FC15 hasta 1968 bits
  - FC16 hasta 123 registers
  - conexión TCP persistente
  - sin prints periódicos durante la ventana de benchmark

  Comandos Serial:
    R -> reset de estadísticas antes de cada caso
    S -> snapshot de estadísticas al terminar cada caso

  Importante:
  - Ethernet sigue siendo gestionado por el runtime JWPLC.
  - No llamar Ethernet.begin() ni JWPLC_Ethernet.begin().
*/

#include <JWPLC_ModbusTCP.h>

static constexpr uint8_t UNIT_ID = 1;
static constexpr uint16_t SERVER_PORT = 502;

static constexpr uint16_t COIL_COUNT = 2000;
static constexpr uint16_t HOLDING_COUNT = 125;

static uint8_t coils[(COIL_COUNT + 7U) / 8U];
static uint16_t holdingRegisters[HOLDING_COUNT];

static bool readyAnnounced = false;

static uint32_t lastLoopUs = 0;
static uint64_t loopGapSumUs = 0;
static uint32_t loopGapSamples = 0;
static uint32_t loopGapMaxUs = 0;

static void resetPerfCounters()
{
    JWPLC_ModbusTCP.resetStats();

    loopGapSumUs = 0;
    loopGapSamples = 0;
    loopGapMaxUs = 0;
    lastLoopUs = micros();
}

static void printSnapshot()
{
    const JWPLCModbusTCPStats &s =
        JWPLC_ModbusTCP.stats();

    uint32_t loopGapAvgUs = 0;

    if (loopGapSamples > 0)
    {
        loopGapAvgUs =
            (uint32_t)(loopGapSumUs / loopGapSamples);
    }

    Serial.println();
    Serial.println(
        "========================================");
    Serial.println(
        " A14.3 PERF-S1 SERVER SNAPSHOT");
    Serial.println(
        "========================================");

    Serial.print("CLIENT_CONNECTIONS=");
    Serial.println(s.clientConnections);

    Serial.print("RX_FRAMES=");
    Serial.println(s.rxFrames);

    Serial.print("TX_FRAMES=");
    Serial.println(s.txFrames);

    Serial.print("REQUESTS_OK=");
    Serial.println(s.requestsOk);

    Serial.print("EXCEPTIONS_SENT=");
    Serial.println(s.exceptionsSent);

    Serial.print("PROTOCOL_ERRORS=");
    Serial.println(s.protocolErrors);

    Serial.print("FRAME_TIMEOUTS=");
    Serial.println(s.frameTimeouts);

    Serial.print("BUS_LOCK_TIMEOUTS=");
    Serial.println(s.busLockTimeouts);

    Serial.print("LOOP_GAP_AVG_US=");
    Serial.println(loopGapAvgUs);

    Serial.print("LOOP_GAP_MAX_US=");
    Serial.println(loopGapMaxUs);

    Serial.print("SERVER_READY=");
    Serial.println(
        JWPLC_ModbusTCP.serverReady()
            ? "YES"
            : "NO");

    Serial.print("CLIENT_CONNECTED=");
    Serial.println(
        JWPLC_ModbusTCP.clientConnected()
            ? "YES"
            : "NO");

    Serial.println("A14_PERF_SNAPSHOT=END");
}

static void serviceSerialCommands()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();

        if (c == 'R' || c == 'r')
        {
            resetPerfCounters();
            Serial.println("A14_PERF_RESET=PASS");
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

    for (uint16_t i = 0; i < sizeof(coils); ++i)
    {
        coils[i] = (uint8_t)(0xA5U ^ (uint8_t)i);
    }

    for (uint16_t i = 0; i < HOLDING_COUNT; ++i)
    {
        holdingRegisters[i] =
            (uint16_t)(0x1000U + i);
    }

    JWPLC_ModbusTCP.setCoils(
        coils,
        COIL_COUNT);

    JWPLC_ModbusTCP.setHoldingRegisters(
        holdingRegisters,
        HOLDING_COUNT);

    if (!JWPLC_ModbusTCP.beginServer(
            UNIT_ID,
            SERVER_PORT))
    {
        Serial.println(
            "A14_PERF_SERVER_CONFIG=FAIL");
        return;
    }

    Serial.println(
        "A14_PERF_SERVER_CONFIG=PASS");

    resetPerfCounters();
}

void loop()
{
    const uint32_t nowUs = micros();

    if (lastLoopUs != 0)
    {
        const uint32_t gap =
            (uint32_t)(nowUs - lastLoopUs);

        loopGapSumUs += gap;
        ++loopGapSamples;

        if (gap > loopGapMaxUs)
        {
            loopGapMaxUs = gap;
        }
    }

    lastLoopUs = nowUs;

    JWPLC_ModbusTCP.task();

    serviceSerialCommands();

    if (
        !readyAnnounced &&
        JWPLC_ModbusTCP.serverReady()
    )
    {
        readyAnnounced = true;

        Serial.print(
            "A14_PERF_SERVER_READY=PASS IP=");
        Serial.print(
            JWPLC_Ethernet.localIP());

        Serial.print(" PORT=");
        Serial.print(SERVER_PORT);

        Serial.print(" UNIT_ID=");
        Serial.println(UNIT_ID);
    }
}