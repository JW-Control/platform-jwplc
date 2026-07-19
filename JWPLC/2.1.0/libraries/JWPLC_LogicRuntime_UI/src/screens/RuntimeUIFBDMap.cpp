#include "RuntimeUIFBDMap.h"

#include <JWPLC_GlobalPeripherals.h>

RuntimeUIFBDMap::RuntimeUIFBDMap()
    : RuntimeUIFBDMapV14(),
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
  // La consulta no consume eventos. Un pulso o repeat pendiente debe llegar al
  // callback gráfico para que la pantalla activa lo procese normalmente.
  if (JWPLCButtons::anyPressedOrRepeated())
  {
    return true;
  }

  // Los asistentes y editores todavía combinan entrada y render. Se mantienen
  // conservadores a 25 Hz hasta separar formalmente ambos pasos.
  if (parameterEditorActiveForExtension())
  {
    return true;
  }

  if (normalMapRootActiveV11())
  {
    return mapNeedsTftRefreshForExtensionV7();
  }

  if (detailModeActiveV11())
  {
    return detailNeedsTftRefreshForExtensionV7();
  }

  return true;
}

void RuntimeUIFBDMap::enter()
{
  RuntimeUIFBDMapV14::enter();
  _appliedRefreshPeriodMs = 0;
  syncRefreshPeriod();
}

void RuntimeUIFBDMap::refresh(const JWPLC_IOState *io,
                              const JWPLC_RTCState *rtc)
{
  RuntimeUIFBDMapV14::refresh(io, rtc);
  syncRefreshPeriod();
}

void RuntimeUIFBDMap::forceRedraw()
{
  RuntimeUIFBDMapV14::forceRedraw();
  _appliedRefreshPeriodMs = 0;
  syncRefreshPeriod();
}
