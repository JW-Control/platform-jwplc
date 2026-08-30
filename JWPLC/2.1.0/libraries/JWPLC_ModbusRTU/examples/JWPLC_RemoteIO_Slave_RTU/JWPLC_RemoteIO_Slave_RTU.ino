/*
  JWPLC_RemoteIO_Slave_RTU
  Alpha7 A7.1 - Arduino Remote I/O Slave usando JWPLC_ModbusRTU oficial

  Objetivo:
  - Exponer I0_0..I0_7 por FC02.
  - Exponer feedback Q0_0..Q0_7 por FC01.
  - Recibir Q por FC05 y FC15.
  - Aplicar fail-safe: 100 ms sin escritura DO valida -> todas las Q OFF.
  - No implementar parser RTU manual en el sketch.

  Banco:
  - Slave ID 2.
  - 115200 8N1.
  - RS-485 A/P -> A/P, B/N -> B/N y GND comun.
  - Ejecutar sin cargas reales conectadas a los reles.
*/

#include <Arduino.h>
#include <JWPLC_ModbusRTU.h>

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint8_t REMOTE_SLAVE_ID = 2;
static constexpr uint32_t MODBUS_BAUD = 115200UL;
static constexpr uint32_t MODBUS_CONFIG = SERIAL_8N1;
static constexpr uint16_t MODBUS_FRAME_GAP_MS = 2;
static constexpr uint32_t OUTPUT_FAILSAFE_MS = 100UL;

static uint8_t coilMap = 0x00;
static uint8_t discreteInputMap = 0x00;
static bool failsafeActive = false;
static uint8_t lastAppliedCoils = 0xFF;
static uint32_t lastStatusMs = 0;
static String serialLine;

static void applyOutputs(uint8_t bitmap)
{
    if (bitmap == lastAppliedCoils)
        return;

    JWPLC_writeOutputs(bitmap);
    lastAppliedCoils = bitmap;
}

static void refreshDiscreteInputs()
{
    discreteInputMap = JWPLC_readInputs();
}

static void serviceFailsafe()
{
    if (!JWPLC_ModbusRTU.hasCoilWrite())
    {
        applyOutputs(0x00);
        return;
    }

    const uint32_t elapsedMs =
        millis() - JWPLC_ModbusRTU.lastCoilWriteMs();

    if (elapsedMs > OUTPUT_FAILSAFE_MS)
    {
        if (!failsafeActive)
        {
            failsafeActive = true;
            Serial.print(F("[FAILSAFE] "));
            Serial.print(elapsedMs);
            Serial.println(F(" ms sin FC05/FC15 -> Q=0"));
        }

        coilMap = 0x00;
        applyOutputs(0x00);
        return;
    }

    if (failsafeActive)
    {
        failsafeActive = false;
        Serial.println(F("[FAILSAFE] liberado por escritura DO valida"));
    }

    applyOutputs(coilMap);
}

static void printStatus()
{
    const JWPLCModbusRTUStats &stats = JWPLC_ModbusRTU.stats();

    Serial.println();
    Serial.println(F("---- SLAVE STATUS ----"));
    Serial.print(F("ID=")); Serial.println(REMOTE_SLAVE_ID);
    Serial.print(F("READY=")); Serial.println(JWPLC_ModbusRTU.isReady() ? F("YES") : F("NO"));
    Serial.print(F("DI=0x"));
    if (discreteInputMap < 0x10) Serial.print('0');
    Serial.println(discreteInputMap, HEX);
    Serial.print(F("Q=0x"));
    if (coilMap < 0x10) Serial.print('0');
    Serial.println(coilMap, HEX);
    Serial.print(F("FAILSAFE=")); Serial.println(failsafeActive ? F("ACTIVE") : F("NO"));
    Serial.print(F("VALID_REQUEST=")); Serial.println(JWPLC_ModbusRTU.hasValidRequest() ? F("YES") : F("NO"));
    Serial.print(F("COIL_WRITE=")); Serial.println(JWPLC_ModbusRTU.hasCoilWrite() ? F("YES") : F("NO"));
    Serial.print(F("RX/TX/OK="));
    Serial.print(stats.rxFrames); Serial.print('/');
    Serial.print(stats.txFrames); Serial.print('/');
    Serial.println(stats.requestsOk);
    Serial.print(F("CRC/EXCEPTIONS="));
    Serial.print(stats.crcErrors); Serial.print('/');
    Serial.println(stats.exceptionsSent);
    Serial.print(F("LAST_ERROR=")); Serial.println(JWPLC_ModbusRTU.lastErrorString());
    Serial.println(F("----------------------"));
}

static void handleCommand(String line)
{
    line.trim();
    line.toUpperCase();

    if (line == "STATUS")
        printStatus();
    else if (line == "CLEAR")
    {
        coilMap = 0x00;
        applyOutputs(0x00);
        JWPLC_ModbusRTU.resetStats();
        Serial.println(F("SLAVE_CLEAR=PASS"));
    }
    else
        Serial.println(F("CMD: STATUS | CLEAR"));
}

void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(500);

    Serial.println();
    Serial.println(F("JWPLC Alpha7 A7.1 - Remote I/O Slave oficial"));

    coilMap = 0x00;
    discreteInputMap = JWPLC_readInputs();
    applyOutputs(0x00);

    JWPLC_ModbusRTU.setCoils(&coilMap, 8);
    JWPLC_ModbusRTU.setDiscreteInputs(&discreteInputMap, 8);

    if (!JWPLC_ModbusRTU.begin(REMOTE_SLAVE_ID, MODBUS_BAUD, MODBUS_CONFIG))
    {
        Serial.print(F("MODBUS_BEGIN=FAIL "));
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        return;
    }

    JWPLC_ModbusRTU.setFrameGapMs(MODBUS_FRAME_GAP_MS);
    JWPLC_ModbusRTU.resetStats();

    Serial.println(F("MODBUS_BEGIN=PASS"));
    Serial.println(F("SLAVE_ID=2"));
    Serial.println(F("FAILSAFE_MS=100"));
    Serial.println(F("CMD: STATUS | CLEAR"));
}

void loop()
{
    refreshDiscreteInputs();
    JWPLC_ModbusRTU.task();
    serviceFailsafe();

    const uint32_t now = millis();

    if ((uint32_t)(now - lastStatusMs) >= 5000UL)
    {
        lastStatusMs = now;
        const JWPLCModbusRTUStats &stats = JWPLC_ModbusRTU.stats();

        Serial.print(F("[LIVE] DI=0x"));
        if (discreteInputMap < 0x10) Serial.print('0');
        Serial.print(discreteInputMap, HEX);
        Serial.print(F(" Q=0x"));
        if (coilMap < 0x10) Serial.print('0');
        Serial.print(coilMap, HEX);
        Serial.print(F(" RX/TX/OK="));
        Serial.print(stats.rxFrames); Serial.print('/');
        Serial.print(stats.txFrames); Serial.print('/');
        Serial.print(stats.requestsOk);
        Serial.print(F(" CRC=")); Serial.print(stats.crcErrors);
        Serial.print(F(" FS=")); Serial.println(failsafeActive ? F("YES") : F("NO"));
    }

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
