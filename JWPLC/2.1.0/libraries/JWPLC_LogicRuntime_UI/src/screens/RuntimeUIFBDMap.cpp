#include "RuntimeUIFBDMap.h"

RuntimeUIFBDMap::RuntimeUIFBDMap()
    : RuntimeUIFBDMapActiveRenderer(),
      _appliedRefreshPeriodMs(0)
{
}

void RuntimeUIFBDMap::syncRefreshPeriod()
{
  const uint32_t desired = recommendedRefreshPeriodMs();
  if (_appliedRefreshPeriodMs == desired)
  {
    return;
  }

  JWPLC_Display.setUserRefreshPeriodMs(desired);
  _appliedRefreshPeriodMs = desired;
}

bool RuntimeUIFBDMap::needsTftRefresh() const
{
  /**
   * @warning La UI FBD todavía combina procesamiento de botones y render dentro
   * de refresh(). Por tanto, omitir el callback basándose solo en regiones TFT
   * sucias puede bloquear eventos de navegación hasta que cambie una señal del
   * motor. Se mantiene el callback incondicional hasta separar formalmente:
   *
   *   1. updateInputAndState() sin bus SPI;
   *   2. needsRender() por regiones sucias;
   *   3. render() con el bus TFT adquirido.
   *
   * Las cachés regionales continúan evitando escrituras de píxeles redundantes,
   * y la frecuencia adaptativa conserva 100 ms en MAPA/DETALLE y 40 ms en los
   * asistentes. Solo se desactiva el salto previo del callback completo.
   */
  return true;
}

void RuntimeUIFBDMap::enter()
{
  RuntimeUIFBDMapActiveRenderer::enter();
  _appliedRefreshPeriodMs = 0;
  syncRefreshPeriod();
}

void RuntimeUIFBDMap::refresh(const JWPLC_IOState *io,
                              const JWPLC_RTCState *rtc)
{
  RuntimeUIFBDMapActiveRenderer::refresh(io, rtc);
  syncRefreshPeriod();
}

void RuntimeUIFBDMap::forceRedraw()
{
  RuntimeUIFBDMapActiveRenderer::forceRedraw();
  _appliedRefreshPeriodMs = 0;
  syncRefreshPeriod();
}
