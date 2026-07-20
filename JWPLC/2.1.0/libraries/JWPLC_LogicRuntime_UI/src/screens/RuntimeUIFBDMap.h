#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_H

#include "RuntimeUIFBDMapActiveRenderer.h"

/**
 * @brief Fachada activa única del editor FBD v2.
 *
 * El resto de la librería no debe instanciar revisiones V4...V14 directamente.
 * Esta clase fija el renderer activo que intercepta transiciones antes de los
 * layouts históricos y concentra la frecuencia adaptativa del callback TFT.
 */
class RuntimeUIFBDMap final : public RuntimeUIFBDMapActiveRenderer
{
public:
  RuntimeUIFBDMap();

  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void forceRedraw();

  /**
   * @brief Mantiene el callback periódico mientras entrada y render compartan
   * refresh(). Las cachés regionales siguen evitando escrituras redundantes.
   */
  bool needsTftRefresh() const;

private:
  void syncRefreshPeriod();
  uint32_t _appliedRefreshPeriodMs;
};

#endif