#include "RuntimeUIFBDMapV14.h"

uint32_t RuntimeUIFBDMapV14::recommendedRefreshPeriodMs() const
{
  static constexpr uint32_t STATIC_PERIOD_MS = 100UL;
  static constexpr uint32_t INTERACTIVE_PERIOD_MS = 40UL;

  // El editor TON existente permanece dentro de Mode::Detail. Se comprueba
  // primero para no degradar el repeat de los campos LOGO! a 10 Hz.
  if (parameterEditorActiveForExtension())
  {
    return INTERACTIVE_PERIOD_MS;
  }

  // El mapa y el detalle normal ya tienen caches regionales. A 10 Hz mantienen
  // la lectura de Ta y los estados visuales sin adquirir el bus SPI 20 veces/s.
  if (normalMapRootActiveV11() || detailModeActiveV11())
  {
    return STATIC_PERIOD_MS;
  }

  // Nodo +, asistentes, selección de fuentes, edición de entradas y cualquier
  // subpantalla transaccional conservan la frecuencia interactiva.
  return INTERACTIVE_PERIOD_MS;
}
