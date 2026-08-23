#include <Arduino.h>
#include <JWPLC_GlobalPeripherals.h>

struct ButtonProbe
{
    uint8_t id;
    const char *name;
};

static const ButtonProbe BUTTONS[] = {
    {BTN_LEFT, "LEFT"},
    {BTN_UP, "UP"},
    {BTN_RIGHT, "RIGHT"},
    {BTN_ESC, "ESC"},
    {BTN_OK, "OK"},
    {BTN_DOWN, "DOWN"},
};

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("JWPLC Alpha5 - gate fisico botonera");
    Serial.print("Botonera ready: ");
    Serial.println(JWPLCButtons::isReady() ? "YES" : "NO");
    Serial.println("Presionar una vez cada boton: LEFT, UP, RIGHT, ESC, OK, DOWN.");

    JWPLCButtons::clearPendingInput();
}

void loop()
{
    static uint32_t lastNotReadyReportMs = 0;

    if (!JWPLCButtons::isReady())
    {
        const uint32_t now = millis();
        if ((uint32_t)(now - lastNotReadyReportMs) >= 1000)
        {
            lastNotReadyReportMs = now;
            Serial.println("[WAIT] Botonera aun no disponible");
        }

        delay(20);
        return;
    }

    for (const ButtonProbe &button : BUTTONS)
    {
        if (JWPLC_Buttons.pressed(button.id))
        {
            Serial.print("[PRESS] ");
            Serial.println(button.name);
        }
    }

    // El autoload JWPLC ya mantiene JWPLC_Buttons.update() en su task de escaneo.
    // Este sketch solo consume los latches de press; no duplica el scanner.
    delay(5);
}
