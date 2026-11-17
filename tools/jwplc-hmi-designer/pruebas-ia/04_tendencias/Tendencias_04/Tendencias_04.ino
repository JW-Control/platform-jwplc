#include "JWPLC_HMI_Generated.h"
#include <math.h>

// Simulación no bloqueante de planta térmica para demostrar las gráficas TREND:
// - Página 0 (Proceso): HORNO en vivo (3 canales: Temp, Consigna, Potencia + 2 umbrales + SD)
// - Página 1 (Compacta): PRESION en escala MANUAL 0..10 bar con VALUE, PILOT y BAR
// - Página 2 (Ajustes): MENU para ajustar Consigna y Marcha en tiempo real

static float simTemp = 28.0f;
static uint32_t lastStepMs = 0;

void setup()
{
    Serial.begin(115200);
    jwplcHMISetup();

    consignaHorno = consignaSetpoint;
    tempHorno = simTemp;
    potenciaSalida = 0.0f;
    presionLinea = 4.2f;
    presionActual = presionLinea;
    caudalLinea = 115.0f;
    valvAlivio = false;
    nivelTanque = 64.0f;
}

void loop()
{
    if (JWPLC_Display.menuChanged(MENU_CONTROL))
    {
        switch (JWPLC_Display.menuNode())
        {
        case MENU_CONTROL_CONSIGNA:
            consignaHorno = consignaSetpoint;
            break;
        case MENU_CONTROL_MARCHA:
            break;
        default:
            break;
        }
    }

    const uint32_t now = millis();
    if (now - lastStepMs >= 100U)
    {
        lastStepMs = now;
        const float dt = 0.1f;
        const float t = (float)now * 0.001f;

        consignaHorno = consignaSetpoint;
        const float objetivo = marchaHorno ? (consignaSetpoint + 18.0f * sinf(t * 0.8f)) : 22.0f;
        const float error = objetivo - simTemp;
        float out = marchaHorno ? (50.0f + error * 3.5f) : 0.0f;
        if (out < 0.0f) out = 0.0f;
        if (out > 100.0f) out = 100.0f;

        simTemp += ((22.0f + out * 0.78f) - simTemp) * (dt * 0.6f);
        tempHorno = simTemp;
        potenciaSalida = out;

        presionLinea = 4.5f + 3.2f * sinf(t * 1.1f) + 0.6f * cosf(t * 2.7f);
        if (presionLinea < 0.0f) presionLinea = 0.0f;
        if (presionLinea > 9.8f) presionLinea = 9.8f;
        presionActual = presionLinea;
        valvAlivio = (presionLinea >= 8.0f);
        caudalLinea = valvAlivio ? 185.0f : (110.0f + 25.0f * cosf(t * 0.9f));
        nivelTanque = 58.0f + 24.0f * sinf(t * 0.35f);
    }
}
