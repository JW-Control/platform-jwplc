#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_H

#include "RuntimeUIFBDMapV14.h"

/**
 * @brief Fachada activa única del editor FBD v2.
 *
 * El resto de la librería no debe instanciar revisiones V4...V14 directamente.
 * Esta clase final fija la implementación validada y concentra políticas
 * transversales como la frecuencia adaptativa del callback TFT.
 */
class RuntimeUIFBDMap final : public RuntimeUIFBDMapV14
{
public:
  RuntimeUIFBDMap();

  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void forceRedraw();

private:
  void syncRefreshPeriod();
  uint32_t _appliedRefreshPeriodMs;
};

#endif