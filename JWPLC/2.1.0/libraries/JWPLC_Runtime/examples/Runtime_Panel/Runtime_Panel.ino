/*
  JWPLC Basic - Runtime_Panel

  Activa el modo Runtime: el equipo se programa desde el panel, como un
  LOGO! 8, sin escribir lógica en el sketch.

    - Pantalla RUN: la de reposo de siempre (PWR/RUN/ERR, entradas, salidas).
    - Cualquier tecla abre el MENU: Programa, Parametros, Tarjeta SD,
      Configuracion, Red, Diagnostico e INICIAR/DETENER.
    - Programa > Editar abre el editor FBD: ◄ ► ▲ ▼ recorren el mapa, OK
      edita el bloque o la salida, ESC guarda en FRAM o descarta.

  Con loadDemoIfEmpty(true), si la FRAM no tiene programa se carga uno de
  ejemplo (arranque a dos manos con retardo, marcha/parada, contador de
  piezas, baliza, alumbrado semanal y un aviso).

  En modo Runtime el sketch no define jwplcUIEnter/jwplcUIUpdate: la
  pantalla USER y las salidas Q son del Runtime.
*/

#include <JWPLC_Runtime.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - Runtime_Panel");

    JWPLC_Runtime.loadDemoIfEmpty(true);
    JWPLC_Runtime.begin();
}

void loop()
{
    // Opcional: el programa en marcha se puede leer desde el sketch.
    static bool q1 = false;
    if (JWPLC_Runtime.output(1) != q1)
    {
        q1 = JWPLC_Runtime.output(1);
        Serial.println(q1 ? "Q1=1" : "Q1=0");
    }
}
