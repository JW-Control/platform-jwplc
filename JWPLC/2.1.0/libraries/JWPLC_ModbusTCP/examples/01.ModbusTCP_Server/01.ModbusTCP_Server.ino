/*
  01.ModbusTCP_Server

  Primer ejemplo Server de JWPLC_ModbusTCP para Alpha14.

  Mapa:
  - Coils            00001..00008 -> 8 bits modificables
  - Discrete Inputs  10001..10008 -> 8 bits de solo lectura
  - Holding Regs     40001..40016 -> 16 registros modificables
  - Input Regs       30001..30016 -> 16 registros de solo lectura

  Parámetros:
  - Unit ID: 1
  - Puerto: 502

  Importante:
  - Ethernet se inicializa y mantiene desde el runtime JWPLC.
  - No llamar Ethernet.begin() ni JWPLC_Ethernet.begin() aquí.
  - task() debe ejecutarse frecuentemente.
*/

#include <JWPLC_ModbusTCP.h>

static uint8_t coils[1] = {0};
static uint8_t discreteInputs[1] = {0};
static uint16_t holdingRegisters[16] = {0};
static uint16_t inputRegisters[16] = {0};

static bool announcedReady = false;
static uint32_t lastStatusMs = 0;

void setup()
{
    Serial.begin(115200);

    JWPLC_ModbusTCP.setCoils(coils, 8);
    JWPLC_ModbusTCP.setDiscreteInputs(discreteInputs, 8);
    JWPLC_ModbusTCP.setHoldingRegisters(holdingRegisters, 16);
    JWPLC_ModbusTCP.setInputRegisters(inputRegisters, 16);

    if (!JWPLC_ModbusTCP.beginServer(1, 502))
    {
        Serial.println("Modbus TCP: no se pudo configurar el Server");
    }
    else
    {
        Serial.println("Modbus TCP: esperando Ethernet / puerto 502...");
    }
}

void loop()
{
    JWPLC_ModbusTCP.task();

    // Datos de ejemplo de solo lectura para verificar FC02/FC04.
    discreteInputs[0] = (uint8_t)((millis() / 1000UL) & 0xFFU);
    inputRegisters[0] = (uint16_t)(millis() / 1000UL);
    inputRegisters[1] = JWPLC_ModbusTCP.stats().requestsOk & 0xFFFFU;

    if (!announcedReady && JWPLC_ModbusTCP.serverReady())
    {
        announcedReady = true;
        Serial.print("Modbus TCP READY | IP: ");
        Serial.print(JWPLC_Ethernet.localIP());
        Serial.println(" | port 502 | Unit ID 1");
    }

    const uint32_t now = millis();
    if ((uint32_t)(now - lastStatusMs) >= 5000UL)
    {
        lastStatusMs = now;
        JWPLC_ModbusTCP.printStatus(Serial);
        Serial.print("Holding[0]: ");
        Serial.println(holdingRegisters[0]);
        Serial.print("Coils raw: 0x");
        Serial.println(coils[0], HEX);
    }
}
