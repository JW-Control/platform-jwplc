/*
  03_EventQueue

  Cola de eventos PRESS / RELEASE / REPEAT de la botonera frontal del
  JWPLC Basic / Basic Core (objeto global JWPLC_Buttons, ya escaneado por el
  package; el sketch no llama a update()).

  eventCount()/getEvent() muestran los eventos del último scan: con el scan en
  segundo plano (cada 5 ms) pueden perderse eventos entre dos vueltas de
  loop(). Para no perder pulsaciones usar pressed()/released(), que quedan
  retenidos.
*/

#include <Arduino.h>

static const char *evName(JW_MatrixButtons::EvType t)
{
  switch (t)
  {
    case JW_MatrixButtons::EV_PRESS: return "PRESS";
    case JW_MatrixButtons::EV_RELEASE: return "RELEASE";
    case JW_MatrixButtons::EV_REPEAT: return "REPEAT";
    default: return "?";
  }
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  JWPLC_Buttons.clearPendingInput();
  Serial.println("Event queue demo listo.");
}

void loop()
{
  for (uint8_t i = 0; i < JWPLC_Buttons.eventCount(); i++)
  {
    JW_MatrixButtons::BtnEvent e;
    if (JWPLC_Buttons.getEvent(i, e))
    {
      Serial.print("id=");
      Serial.print(e.id);
      Serial.print(" ");
      Serial.print(evName(e.type));
      Serial.print(" mult=");
      Serial.print(e.mult);
      Serial.print(" held=");
      Serial.println(e.held_ms);
    }
  }
  // Ya leídos: no volver a imprimirlos si el próximo scan aún no llegó.
  JWPLC_Buttons.clearEventQueue();

  delay(5);
}
