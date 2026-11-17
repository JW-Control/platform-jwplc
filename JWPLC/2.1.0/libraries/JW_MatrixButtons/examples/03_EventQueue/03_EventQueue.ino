/*
  03_EventQueue

  Cola de eventos PRESS / RELEASE / REPEAT de la botonera frontal del
  JWPLC Basic / Basic Core (objeto global JWPLC_Buttons, ya escaneado por el
  package; el sketch no llama a update()).

  Cada evento trae:
    e.id       tecla (BTN_LEFT, BTN_UP, BTN_RIGHT, BTN_ESC, BTN_OK, BTN_DOWN)
    e.type     BTN_EV_PRESS, BTN_EV_RELEASE o BTN_EV_REPEAT
    e.mult     multiplicador de la repetición (1, 10, 100...)
    e.held_ms  tiempo que lleva pulsada

  eventCount()/getEvent() muestran los eventos del último scan: con el scan en
  segundo plano (cada 5 ms) pueden perderse eventos entre dos vueltas de
  loop(). Para no perder pulsaciones usar pressed()/released(), que quedan
  retenidos.

  Qué mirar: el Monitor serie (115200) imprime una línea por evento.
*/

#include <Arduino.h>

// Nombres para imprimir, en el orden de los tipos (1 = PRESS, 2 = RELEASE, 3 = REPEAT).
const char NOMBRE_EVENTO[][8] = {"?", "PRESS", "RELEASE", "REPEAT"};

void setup()
{
  Serial.begin(115200);

  JWPLC_Buttons.clearPendingInput();
  Serial.println("Event queue demo listo.");
}

void loop()
{
  uint8_t cantidad = JWPLC_Buttons.eventCount();
  if (cantidad == 0)
    return;

  for (uint8_t i = 0; i < cantidad; i++)
  {
    JW_ButtonEvent e;
    if (JWPLC_Buttons.getEvent(i, e))
    {
      Serial.print("id=");
      Serial.print(e.id);
      Serial.print(" ");
      Serial.print(e.type <= BTN_EV_REPEAT ? NOMBRE_EVENTO[e.type] : NOMBRE_EVENTO[0]);
      Serial.print(" mult=");
      Serial.print(e.mult);
      Serial.print(" held=");
      Serial.println(e.held_ms);
    }
  }
  // Ya leídos: no volver a imprimirlos si el próximo scan aún no llegó.
  JWPLC_Buttons.clearEventQueue();
}
