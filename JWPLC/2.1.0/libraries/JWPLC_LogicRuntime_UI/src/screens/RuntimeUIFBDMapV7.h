#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V7_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V7_H

#include "RuntimeUIFBDMapV6.h"

/**
 * @brief Revisión v0.5.4 con refresco regional y repeat rápido por unidad.
 *
 * Evita reconstruir el mapa o el detalle completo cuando cambia un único
 * estado. En EDITAR T conserva applyAxis() de JW_MatrixButtons, pero aplica un
 * perfil temporal más rápido y adaptado a ms/s/min/h.
 */
class RuntimeUIFBDMapV7 : public RuntimeUIFBDMapV6
{
  friend class RuntimeUIFBDMapV8;

public:
  RuntimeUIFBDMapV7();

  void attach(RuntimeUIV2ReadModel &model);
  void detach();
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();

private:
  static constexpr uint32_t EDITOR_TA_REFRESH_MS = 100UL;

  void resetOptimizedState();

  void configureFastRepeatProfile();
  void restoreDefaultRepeatProfile();

  void refreshMapOptimized();
  void handleMapInputOptimized();
  void drawMapLiveOptimized();
  void noteMapFullRendered();

  void refreshDetailOptimized();
  void handleDetailInputOptimized();
  void drawDetailLiveOptimized();
  void captureDetailCache();
  void drawDetailWireLive(uint8_t inputIndex,
                          uint8_t visibleRow);
  void drawDetailInputPinLive(uint8_t inputIndex,
                              uint8_t visibleRow);
  void drawDetailBlockStateLive();
  void drawTonDetailElapsedLive(bool force);

  void refreshParameterEditorOptimized();
  void refreshEditorElapsedLive(bool force);

  bool _fastRepeatActive;
  bool _fastRepeatUnitValid;
  TimeUnit _fastRepeatUnit;

  bool _mapSelectionCacheValid;
  uint16_t _mapSelectionCache;

  bool _detailCacheValid;
  uint16_t _detailCacheBlock;
  uint8_t _detailCachePage;
  bool _detailCacheBlockValue;
  bool _detailCacheInputs[DETAIL_INPUTS_PER_PAGE];
  bool _detailCacheTonTiming;
  bool _detailCacheTonActive;
  char _detailCacheTonElapsed[18];

  uint32_t _lastEditorElapsedRefreshMs;
  bool _editorElapsedCacheValid;
  bool _editorElapsedColorOn;
  char _editorElapsedCache[18];
};

#endif
