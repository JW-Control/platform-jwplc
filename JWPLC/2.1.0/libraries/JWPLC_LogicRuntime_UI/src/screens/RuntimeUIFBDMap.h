#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_H

#include "RuntimeUIFBDMapActiveRenderer.h"
#include "RuntimeUIFBDMapUnified.h"

/**
 * @brief Fachada única del editor FBD v2.
 *
 * El modo normal conserva temporalmente el renderer histórico. El preview
 * consolidado usa RuntimeUIFBDMapUnified, que no hereda de ninguna revisión Vx.
 * Ambos comparten la misma API para no duplicar callbacks USER.
 */
class RuntimeUIFBDMap final
{
public:
  RuntimeUIFBDMap();

  void setUnifiedPreview(bool enabled);
  bool unifiedPreviewEnabled() const;

  void attach(RuntimeUIV2ReadModel &model);
  void detach();
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();
  void processPendingEdit();
  bool needsTftRefresh() const;

private:
  uint32_t desiredRefreshPeriodMs() const;
  void syncRefreshPeriod();

  bool _unifiedPreview;
  uint32_t _appliedRefreshPeriodMs;
  RuntimeUIFBDMapActiveRenderer _legacy;
  RuntimeUIFBDMapUnified _unified;
};

#endif
