/*
  Gate 7NB.3A - sincronizacion de salidas master-driven

  Proposito:
    Validar que M2 gobierne la secuencia de Q0.x de S1/S2 y que los tres
    nodos conmuten practicamente al mismo tiempo usando broadcast Modbus RTU.

  IMPORTANTE:
    - Este sketch es un gate aislado, no reemplaza el soak 20.
    - No modifica JWPLC_ModbusRTU.
    - Usar sin cargas reales; solo loopback/relés de banco.

  Flujo por Serial:
    En cada placa: ROLE M2, ROLE S1 o ROLE S2.
    Luego, solo en M2: START.
*/

#include <Arduino.h>

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t MODBUS_BAUD = 115200UL;
static constexpr uint32_t PHASE_MS = 1500UL;
static constexpr uint16_t HR_COMMAND = 0;
static constexpr uint16_t HR_SEQUENCE = 1;
static constexpr uint16_t HR_COUNT = 2;

static constexpr uint16_t CMD_OFF_ALL = 0x0000;
static constexpr uint16_t CMD_ON_BASE = 0x0100;

enum LocalRole : uint8_t
{
    ROLE_NONE = 0,
    ROLE_MASTER,
    ROLE_S1,
    ROLE_S2
};

static LocalRole role = ROLE_NONE;
static uint8_t slaveId = 1;
static uint16_t holding[HR_COUNT] = {0, 0};
static uint16_t lastSequence = 0;
static uint16_t txSequence = 0;
static uint8_t channel = 0;
static bool onPhase = false;
static bool running = false;
static uint32_t lastPhaseMs = 0;
static String serialLine;

static uint8_t outputPins[8] = {
    Q0_0,
    Q0_1,
    Q0_2,
    Q0_3,
    Q0_4,
    Q0_5,
    Q0_6,
    Q0_7
};

static uint16_t crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 1U) ? (crc >> 1) ^ 0xA001U : (crc >> 1);
    }
    return crc;
}

static void allOutputsOff()
{
    for (uint8_t i = 0; i < 8; ++i)
        digitalWrite(outputPins[i], LOW);
}

static void applyCommand(uint16_t command)
{
    if (command == CMD_OFF_ALL)
    {
        allOutputsOff();
        return;
    }

    if ((command & 0xFF00U) != CMD_ON_BASE)
        return;

    const uint8_t target = (uint8_t)(command & 0x00FFU);
    if (target >= 8)
        return;

    allOutputsOff();
    digitalWrite(outputPins[target], HIGH);
}

static bool broadcastWriteSingleRegister(uint16_t address, uint16_t value)
{
    uint8_t frame[8];
    frame[0] = 0; // broadcast Modbus RTU
    frame[1] = 0x06;
    frame[2] = highByte(address);
    frame[3] = lowByte(address);
    frame[4] = highByte(value);
    frame[5] = lowByte(value);
    const uint16_t crc = crc16(frame, 6);
    frame[6] = lowByte(crc);
    frame[7] = highByte(crc);

    return JWPLC_RS485.write(frame, sizeof(frame)) == sizeof(frame);
}

static bool issueSynchronizedCommand(uint16_t command)
{
    ++txSequence;
    if (txSequence == 0)
        ++txSequence;

    // 1) Preparar el comando en todos los Slaves.
    if (!broadcastWriteSingleRegister(HR_COMMAND, command))
        return false;

    delayMicroseconds(250);

    // 2) El cambio de secuencia actua como trigger comun para S1/S2.
    if (!broadcastWriteSingleRegister(HR_SEQUENCE, txSequence))
        return false;

    // 3) M2 conmuta inmediatamente despues del trigger transmitido.
    applyCommand(command);
    return true;
}

static void serviceSlaveCommand()
{
    if (role == ROLE_MASTER || role == ROLE_NONE)
        return;

    const uint16_t seq = holding[HR_SEQUENCE];
    if (seq == 0 || seq == lastSequence)
        return;

    lastSequence = seq;
    applyCommand(holding[HR_COMMAND]);
}

static void serviceMasterSequence()
{
    if (role != ROLE_MASTER || !running)
        return;

    const uint32_t now = millis();
    if ((uint32_t)(now - lastPhaseMs) < PHASE_MS)
        return;

    lastPhaseMs = now;

    uint16_t command = CMD_OFF_ALL;
    if (!onPhase)
        command = (uint16_t)(CMD_ON_BASE | channel);

    if (!issueSynchronizedCommand(command))
    {
        Serial.println(F("SYNC_TX=FAIL"));
        return;
    }

    Serial.print(F("SYNC seq="));
    Serial.print(txSequence);
    Serial.print(F(" ch="));
    Serial.print(channel);
    Serial.print(F(" phase="));
    Serial.println(onPhase ? F("OFF") : F("ON"));

    onPhase = !onPhase;
    if (!onPhase)
        channel = (uint8_t)((channel + 1U) & 0x07U);
}

static void configureRole(LocalRole newRole)
{
    JWPLC_ModbusRTU.end();
    JWPLC_RS485.end();

    role = newRole;
    allOutputsOff();
    holding[HR_COMMAND] = 0;
    holding[HR_SEQUENCE] = 0;
    lastSequence = 0;

    if (role == ROLE_MASTER)
    {
        if (!JWPLC_RS485.begin(MODBUS_BAUD, SERIAL_8N1))
        {
            Serial.println(F("ROLE M2: RS485 FAIL"));
            role = ROLE_NONE;
            return;
        }
        Serial.println(F("ROLE=M2 READY"));
        return;
    }

    slaveId = (role == ROLE_S1) ? 1 : 2;
    JWPLC_ModbusRTU.setHoldingRegisters(holding, HR_COUNT);
    if (!JWPLC_ModbusRTU.begin(slaveId, MODBUS_BAUD, SERIAL_8N1))
    {
        Serial.println(F("SLAVE MODBUS FAIL"));
        role = ROLE_NONE;
        return;
    }

    JWPLC_ModbusRTU.setFrameGapMs(2);
    Serial.print(F("ROLE=S"));
    Serial.print(slaveId);
    Serial.println(F(" READY"));
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
        running = true;
        onPhase = false;
        channel = 0;
        lastPhaseMs = millis() - PHASE_MS;
        Serial.println(F("SYNC_GATE=RUNNING"));
    }
    else if (line == "STOP")
    {
        running = false;
        allOutputsOff();
        if (role == ROLE_MASTER)
            (void)issueSynchronizedCommand(CMD_OFF_ALL);
        Serial.println(F("SYNC_GATE=STOPPED"));
    }
    else
    {
        Serial.println(F("CMD: ROLE M2 | ROLE S1 | ROLE S2 | START | STOP"));
    }
}

void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(400);

    for (uint8_t i = 0; i < 8; ++i)
        pinMode(outputPins[i], OUTPUT);
    allOutputsOff();

    Serial.println();
    Serial.println(F("JWPLC Alpha7 Gate 7NB.3A - sync I/O master-driven"));
    Serial.println(F("Selecciona ROLE M2 / ROLE S1 / ROLE S2"));
}

void loop()
{
    if (role == ROLE_S1 || role == ROLE_S2)
    {
        JWPLC_ModbusRTU.task();
        serviceSlaveCommand();
    }

    serviceMasterSequence();

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
