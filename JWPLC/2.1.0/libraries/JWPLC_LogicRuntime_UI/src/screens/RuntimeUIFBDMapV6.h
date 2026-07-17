#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V6_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V6_H

#include "RuntimeUIFBDMapV5.h"

/**
 * @brief Revisión v0.5.3 del editor TON.
 *
 * Aprovecha el repeat y applyAxis() de JW_MatrixButtons para que UP/DOWN
 * mantenidos aceleren el campo VALOR. También actualiza Ta cada 100 ms dentro
 * de EDITAR T sin reconstruir la pantalla completa.
 */
class RuntimeUIFBDMapV6 : public RuntimeUIFBDMapV5
{
public:
  RuntimeUIFBDMapV6()
      : RuntimeUIFBDMapV5(),
        _lastParameterElapsedRefreshMs(0)
  {
  }

  void attach(RuntimeUIV2ReadModel &model)
  {
    _lastParameterElapsedRefreshMs = 0;
    RuntimeUIFBDMapV5::attach(model);
  }

  void detach()
  {
    _lastParameterElapsedRefreshMs = 0;
    RuntimeUIFBDMapV5::detach();
  }

  void enter()
  {
    _lastParameterElapsedRefreshMs = millis();
    RuntimeUIFBDMapV5::enter();
  }

  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc)
  {
    // applyAxis consume tanto PRESS como EV_REPEAT. El perfil global del JWPLC
    // ya habilita repeat para UP/DOWN y acelera su frecuencia progresivamente.
    if (parameterEditorActiveForExtension() &&
        parameterValueFocusedForExtension())
    {
      applyParameterValueAxisForExtension();
    }

    RuntimeUIFBDMapV5::refresh(io, rtc);

    if (!parameterEditorActiveForExtension())
    {
      _lastParameterElapsedRefreshMs = millis();
      return;
    }

    const uint32_t nowMs = millis();
    if (static_cast<uint32_t>(
            nowMs - _lastParameterElapsedRefreshMs) < 100UL)
    {
      return;
    }

    _lastParameterElapsedRefreshMs = nowMs;
    refreshParameterElapsedForExtension();
  }

  void exit()
  {
    _lastParameterElapsedRefreshMs = 0;
    RuntimeUIFBDMapV5::exit();
  }

  void forceRedraw()
  {
    _lastParameterElapsedRefreshMs = 0;
    RuntimeUIFBDMapV5::forceRedraw();
  }

private:
  uint32_t _lastParameterElapsedRefreshMs;
};

#endif
