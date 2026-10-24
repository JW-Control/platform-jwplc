// Gate A15 I1: VALUE con separador de miles y duración, BOOL «Piloto».
// JWPLC_HMI_Generated.h sale del HMI Designer (tests/golden/i1Formats.h).
//
// En la placa, cada segundo:
//   Piezas   suma 1234          -> 1,234 · 2,468 ... (coma de miles)
//   kWh      suma 12.5          -> 1.234,5 (punto de miles, coma decimal)
//   Resta    cuenta atrás 75 s  -> 01:15 ... 00:00 ... -00:05 (con signo)
//   Marcha   millis()           -> 000:00:01 ... (HH:MM:SS, variable en ms)
//   Ciclo    suma 7 s           -> 7s ... 1m03s ... 1h00m (compacto)
//   Bomba    alterna            -> piloto gris / verde
//   Alarma   con Resta negativa -> piloto amarillo sobre fondo rojo
#include "JWPLC_HMI_Generated.h"

void setup()
{
    jwplcHMISetup();
    t_restante = 75;
}

void loop()
{
    static uint32_t last = 0;
    if (millis() - last < 1000)
        return;
    last = millis();

    piezas += 1234;
    energia += 12.5f;
    t_restante -= 1;
    marcha_ms = millis();
    ciclo += 7;
    bomba = !bomba;
    alarma = t_restante < 0;
}
