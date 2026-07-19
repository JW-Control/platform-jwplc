#include "JWPLC_LogicRuntime_UI.h"

bool JWPLC_LogicRuntime_UIClass::displayRefreshNeeded(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc) const
{
  (void)io;
  (void)rtc;

  if (!isAttached() || !_userVisible)
  {
    return false;
  }

  if (_backend == Backend::EngineV2)
  {
    return _fbdMapV2.needsTftRefresh();
  }

  // Las vistas v1 todavía combinan entrada y render en refresh(). Se conserva
  // su comportamiento histórico hasta migrarlas a un contrato dirty-region.
  return true;
}

extern "C" bool jwplcUserDisplayRefreshNeededCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
  return JWPLC_LogicRuntime_UI.displayRefreshNeeded(io, rtc);
}
