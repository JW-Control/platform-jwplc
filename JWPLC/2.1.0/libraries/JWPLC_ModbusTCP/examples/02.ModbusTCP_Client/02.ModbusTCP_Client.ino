/*
  02.ModbusTCP_Client

  Ejemplo Client cooperativo de JWPLC_ModbusTCP.

  Este sketch lee periódicamente dos Holding Registers mediante FC03
  desde un Server Modbus TCP remoto.

  Parámetros de ejemplo:
  - Server IP: 192.168.0.100
  - Puerto: 502
  - Unit ID: 1
  - Holding Registers: dirección 0, cantidad 2
  - Polling: 1000 ms
  - Timeout por request: 1000 ms

  Antes de usar:
  - Cambiar SERVER_IP por la dirección del equipo Server real.

  Importante:
  - Ethernet se inicializa y mantiene desde el runtime JWPLC.
  - No llamar Ethernet.begin() ni JWPLC_Ethernet.begin().
  - JWPLC_ModbusTCPClient.task() debe ejecutarse frecuentemente.
  - La conexión TCP se abre de forma lazy y se reutiliza.
  - Ante timeout o pérdida de sesión, la siguiente request reconecta
    automáticamente; no es necesario volver a llamar begin().
*/

#include <JWPLC_ModbusTCP.h>

static IPAddress SERVER_IP(192, 168, 0, 100);

static constexpr uint16_t SERVER_PORT = 502;
static constexpr uint8_t UNIT_ID = 1;

static constexpr uint16_t START_ADDRESS = 0;
static constexpr uint16_t REGISTER_COUNT = 2;

static constexpr uint32_t POLL_PERIOD_MS = 1000;
static constexpr uint32_t REQUEST_TIMEOUT_MS = 1000;

static uint16_t holdingRegisters[REGISTER_COUNT] = {0};

static uint32_t nextPollMs = 0;

void setup()
{
    Serial.begin(115200);

    if (!JWPLC_ModbusTCPClient.begin(
            SERVER_IP,
            UNIT_ID,
            SERVER_PORT))
    {
        Serial.print("Modbus TCP Client: configuración inválida: ");
        Serial.println(JWPLC_ModbusTCPClient.resultString());
        return;
    }

    Serial.print("Modbus TCP Client configurado | Server: ");
    Serial.print(SERVER_IP);
    Serial.print(':');
    Serial.print(SERVER_PORT);
    Serial.print(" | Unit ID ");
    Serial.println(UNIT_ID);

    nextPollMs = millis();
}

void loop()
{
    // Motor cooperativo: mantener esta llamada frecuente.
    JWPLC_ModbusTCPClient.task();

    // Procesar el resultado de la última request.
    if (JWPLC_ModbusTCPClient.done())
    {
        if (JWPLC_ModbusTCPClient.succeeded())
        {
            Serial.print("FC03 OK | TID=");
            Serial.print(JWPLC_ModbusTCPClient.transactionId());

            Serial.print(" | HR[0]=");
            Serial.print(holdingRegisters[0]);

            Serial.print(" | HR[1]=");
            Serial.print(holdingRegisters[1]);

            Serial.print(" | TCP=");
            Serial.println(
                JWPLC_ModbusTCPClient.sessionConnected()
                    ? "CONNECTED"
                    : "DISCONNECTED");
        }
        else
        {
            Serial.print("FC03 ERROR | ");
            Serial.print(JWPLC_ModbusTCPClient.resultString());

            if (JWPLC_ModbusTCPClient.result() ==
                JWPLC_MODBUS_TCP_CLIENT_EXCEPTION)
            {
                Serial.print(" | exception=0x");
                Serial.print(
                    JWPLC_ModbusTCPClient.exceptionCode(),
                    HEX);
            }

            Serial.println();
        }

        JWPLC_ModbusTCPClient.clearResult();
        nextPollMs = millis() + POLL_PERIOD_MS;
    }

    // No iniciar otra request mientras la state machine esté ocupada.
    if (JWPLC_ModbusTCPClient.busy())
    {
        return;
    }

    const uint32_t now = millis();

    if ((int32_t)(now - nextPollMs) < 0)
    {
        return;
    }

    if (!JWPLC_ModbusTCPClient.requestReadHoldingRegisters(
            START_ADDRESS,
            REGISTER_COUNT,
            holdingRegisters,
            REQUEST_TIMEOUT_MS))
    {
        Serial.print("No se pudo iniciar FC03: ");
        Serial.println(JWPLC_ModbusTCPClient.resultString());

        nextPollMs = now + POLL_PERIOD_MS;
    }
}