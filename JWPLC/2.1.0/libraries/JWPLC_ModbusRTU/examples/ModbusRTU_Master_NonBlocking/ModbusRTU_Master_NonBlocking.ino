/*
  ModbusRTU_Master_NonBlocking

  Gate inicial del Master cooperativo/no bloqueante de JWPLC_ModbusRTU.

  Requiere un slave Modbus RTU con:
  - Slave ID: 1
  - Baud: 19200
  - Config: SERIAL_8E1
  - Holding registers disponibles desde address 0

  Puedes usar otro JWPLC Basic con el ejemplo:
  ModbusRTU_Slave_HoldingRegisters

  Qué demostrar:
  - La lectura FC03 se inicia una vez y retorna de inmediato.
  - JWPLC_ModbusRTU.task() avanza la recepción en cada vuelta de loop().
  - appTicks continúa incrementándose incluso si el Slave está desconectado
    y la transacción termina por timeout.
*/

#include <JWPLC_ModbusRTU.h>

static uint16_t values[4];
static uint32_t appTicks = 0;
static uint32_t requestStartedAtMs = 0;
static uint32_t appTicksAtRequest = 0;
static uint32_t nextRequestMs = 1500;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC Modbus RTU master non-blocking");

    // En modo master usamos slaveId local 247 solo como ID interno no usado.
    if (!JWPLC_ModbusRTU.begin(247, 19200, SERIAL_8E1))
    {
        Serial.print("Modbus begin failed: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        return;
    }

    JWPLC_ModbusRTU.printStatus(Serial);
}

void loop()
{
    const uint32_t now = millis();

    // Trabajo de aplicación independiente del RTU.
    static uint32_t lastAppTickMs = 0;
    if ((uint32_t)(now - lastAppTickMs) >= 10)
    {
        lastAppTickMs = now;
        appTicks++;
    }

    // Motor Modbus cooperativo. Esta llamada debe ejecutarse con frecuencia.
    JWPLC_ModbusRTU.task();

    if (JWPLC_ModbusRTU.masterDone())
    {
        const uint32_t elapsedMs = now - requestStartedAtMs;
        const uint32_t appTicksDuringRequest = appTicks - appTicksAtRequest;

        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            Serial.print("FC03 async OK | elapsed_ms=");
            Serial.print(elapsedMs);
            Serial.print(" app_ticks=");
            Serial.print(appTicksDuringRequest);
            Serial.print(" | HR0=");
            Serial.print(values[0]);
            Serial.print(" HR1=");
            Serial.print(values[1]);
            Serial.print(" HR2=");
            Serial.print(values[2]);
            Serial.print(" HR3=");
            Serial.println(values[3]);
        }
        else
        {
            Serial.print("FC03 async ERROR | elapsed_ms=");
            Serial.print(elapsedMs);
            Serial.print(" app_ticks=");
            Serial.print(appTicksDuringRequest);
            Serial.print(" | ");
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
        }

        JWPLC_ModbusRTU.clearMasterResult();
        nextRequestMs = now + 1500;
    }

    if (!JWPLC_ModbusRTU.masterBusy() &&
        !JWPLC_ModbusRTU.masterDone() &&
        (int32_t)(now - nextRequestMs) >= 0)
    {
        appTicksAtRequest = appTicks;
        requestStartedAtMs = now;

        bool accepted = JWPLC_ModbusRTU.beginReadHoldingRegistersAsync(
            1,
            0,
            4,
            values,
            1000);

        if (accepted)
        {
            Serial.print("FC03 async START | app_ticks=");
            Serial.println(appTicksAtRequest);
        }
        else
        {
            Serial.print("FC03 async START ERROR: ");
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
            nextRequestMs = now + 1500;
        }
    }
}
