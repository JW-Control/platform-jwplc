// Gate A15 I3: tildes, ñ y ° en la TFT (fuente GLCD cp437).
// JWPLC_HMI_Generated.h sale del HMI Designer (tests/golden/i3Accents.h):
// las etiquetas ya vienen convertidas a cp437 por el codegen.
//
// El TEXT «Señal» se cambia desde el sketch con texto UTF-8 normal: el
// firmware lo convierte en setText().
#include "JWPLC_HMI_Generated.h"

static const char *const MENSAJES[] = {
    "Listo ñ",
    "Presión OK",
    "25°C ±0,5",
    "¿Listo? ¡Sí!",
    "ÁÉÍÓÚ áéíóú",
};

void setup()
{
    jwplcHMISetup();
}

void loop()
{
    static uint32_t last = 0;
    static uint8_t i = 0;
    if (millis() - last < 2000)
        return;
    last = millis();

    strncpy(estado, MENSAJES[i], sizeof(estado) - 1);
    estado[sizeof(estado) - 1] = '\0';
    i = (uint8_t)((i + 1) % (sizeof(MENSAJES) / sizeof(MENSAJES[0])));

    presion += 0.7f;
    valvula = !valvula;
}
