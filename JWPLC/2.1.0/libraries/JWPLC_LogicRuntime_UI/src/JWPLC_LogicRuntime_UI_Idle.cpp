#include "JWPLC_LogicRuntime_UI.h"

bool JWPLC_LogicRuntime_UIClass::canReturnToIdle() const
{
  if (!_attached || !_userVisible)
  {
    return true;
  }

  if (_backend == Backend::EngineV2)
  {
    return _fbdMapV2.canReturnToIdle();
  }

  // El runtime v1 conserva el comportamiento histórico de JWPLC_Display.
  return true;
}

// JWPLC_Display consulta este hook antes de sacar USER hacia IDLE. La definición
// fuerte permite que las subpantallas FBD consuman ESC en su propio refresh.
extern "C" bool jwplcCanReturnToIdle(void)
{
  return JWPLC_LogicRuntime_UI.canReturnToIdle();
}
