/*
  02.Buttons_Basic

  Botonera frontal del JWPLC Basic.

  El package escanea la matriz automáticamente en segundo plano.
  El sketch NO debe llamar JWPLC_Buttons.update().

  Funciones mostradas:
  - pressed(id):  true una vez por cada pulsación y consume ese PRESS pendiente.
  - released(id): true una vez por cada liberación y consume ese RELEASE pendiente.
  - isDown(id):   estado físico estable actual, no consume eventos.
  - clearPendingInput(): descarta eventos anteriores al inicio de la aplicación.
*/

#include <JWPLC_GlobalPeripherals.h>

static const uint8_t BUTTON_IDS[] = {
    BTN_LEFT, BTN_UP, BTN_RIGHT, BTN_ESC, BTN_OK, BTN_DOWN};

static const char *BUTTON_NAMES[] = {
    "LEFT", "UP", "RIGHT", "ESC", "OK", "DOWN"};

void setup()
{
    Serial.begin(115200);
    delay(300);

    // Evita interpretar como nuevas pulsaciones eventos que hayan quedado
    // pendientes durante el arranque.
    JWPLC_Buttons.clearPendingInput();

    Serial.println("JWPLC Basic - Buttons");
}

void loop()
{
    for (uint8_t i = 0; i < 6; ++i)
    {
        if (JWPLC_Buttons.pressed(BUTTON_IDS[i]))
        {
            Serial.print("PRESS   ");
            Serial.println(BUTTON_NAMES[i]);
        }

        if (JWPLC_Buttons.released(BUTTON_IDS[i]))
        {
            Serial.print("RELEASE ");
            Serial.println(BUTTON_NAMES[i]);
        }
    }

    // Ejemplo de isDown(): mientras UP siga físicamente presionado,
    // se informa cada 250 ms sin consumir PRESS/RELEASE.
    static uint32_t lastHoldPrintMs = 0;
    if (JWPLC_Buttons.isDown(BTN_UP) &&
        millis() - lastHoldPrintMs >= 250)
    {
        lastHoldPrintMs = millis();
        Serial.println("UP sigue presionado");
    }

    delay(5);
}
