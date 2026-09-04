/*
  04.FRAM_Basic

  FRAM SPI integrada del JWPLC Basic.

  Este ejemplo guarda una pequeña estructura persistente y una cadena.
  La FRAM es no volátil y no necesita ciclos de borrado como una Flash.

  Funciones mostradas:
  - size(): tamaño detectado/configurado.
  - readBlock()/writeBlock(): estructura con magic, versión y checksum.
  - writeCString()/readCString(): texto C sin String dinámico obligatorio.
*/

#include <JWPLC_GlobalPeripherals.h>

struct TallerState
{
    uint32_t bootCount;
    float setpoint;
    uint8_t enabled;
};

static constexpr uint32_t STATE_ADDR = 0;
static constexpr uint32_t TEXT_ADDR = 64;

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println("JWPLC Basic - FRAM");
    Serial.print("FRAM size = ");
    Serial.print(JWPLC_FRAM.size());
    Serial.println(" bytes");

    if (JWPLC_FRAM.size() == 0)
    {
        Serial.println("FRAM no disponible.");
        return;
    }

    TallerState state = {};

    // Si no existe todavía un bloque válido, crea valores iniciales.
    if (!JWPLC_FRAM.readBlock(STATE_ADDR, state, 1))
    {
        state.bootCount = 0;
        state.setpoint = 25.0f;
        state.enabled = 1;
        Serial.println("Primer uso: creando bloque FRAM.");
    }

    state.bootCount++;

    if (JWPLC_FRAM.writeBlock(STATE_ADDR, state, 1))
    {
        Serial.print("bootCount persistente = ");
        Serial.println(state.bootCount);
        Serial.print("setpoint = ");
        Serial.println(state.setpoint, 1);
    }
    else
    {
        Serial.println("Error escribiendo bloque FRAM.");
    }

    JWPLC_FRAM.writeCString(TEXT_ADDR, "JWPLC Taller Alpha8");

    char text[32] = {};
    if (JWPLC_FRAM.readCString(TEXT_ADDR, text, sizeof(text)))
    {
        Serial.print("Texto FRAM = ");
        Serial.println(text);
    }
}

void loop()
{
    // No hace falta refrescar la FRAM continuamente para este ejemplo.
    delay(1000);
}
