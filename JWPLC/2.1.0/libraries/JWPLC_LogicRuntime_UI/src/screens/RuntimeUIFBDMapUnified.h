#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_UNIFIED_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_UNIFIED_H

#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime_V2.h>

#include "../edit/RuntimeUIV2EditSession.h"
#include "../model/RuntimeUIV2ReadModel.h"

/**
 * @brief Implementación consolidada e independiente del editor FBD v2.
 *
 * Esta clase NO hereda de RuntimeUIFBDMapV4...V14. Su objetivo es reemplazar
 * completamente la cadena histórica una vez que alcance paridad funcional y
 * pase la validación física.
 *
 * Reglas de diseño:
 * - una sola máquina de estados;
 * - una sola cabecera compartida;
 * - una sola función de transición entre vistas;
 * - limpieza regional, nunca clearScreen() entre vistas FBD;
 * - entrada y render procesados por la misma implementación;
 * - motor v2 y sesión transaccional permanecen externos y sin cambios.
 */
class RuntimeUIFBDMapUnified final
{
public:
  RuntimeUIFBDMapUnified();

  void attach(RuntimeUIV2ReadModel &model);
  void detach();
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();
  void processPendingEdit();

  bool needsTftRefresh() const;
  uint32_t requestedRefreshPeriodMs() const;

private:
  enum class View : uint8_t
  {
    Map = 0,
    Detail,
    EditInput,
    EditTon,
    AddType,
    Configure,
    SourceList,
    SourceEdit,
    ParameterList,
    ParameterEdit,
    DeleteConfirm
  };

  enum class DetailFocus : uint8_t
  {
    Inputs = 0,
    Parameters,
    Actions
  };

  enum class TonBase : uint8_t
  {
    Seconds = 0,
    Minutes,
    Hours
  };

  enum class TonField : uint8_t
  {
    Major = 0,
    Minor,
    Base
  };

  struct HeaderModel
  {
    char title[16];
    char line1[24];
    char line2[24];
    char state[12];
    uint16_t stateColor;
  };

  struct TonDraft
  {
    uint32_t major;
    uint32_t minor;
    TonBase base;
    TonField focus;
    uint32_t originalMs;
    uint16_t originalResource;
  };

  static constexpr uint32_t STATIC_REFRESH_MS = 100UL;
  static constexpr uint32_t EDIT_REFRESH_MS = 40UL;

  static constexpr int16_t SCREEN_W = 320;
  static constexpr int16_t SCREEN_H = 170;
  static constexpr int16_t HEADER_H = 24;
  static constexpr int16_t PANEL_X = 2;
  static constexpr int16_t PANEL_Y = 27;
  static constexpr int16_t PANEL_W = 316;
  static constexpr int16_t PANEL_H = 142;
  static constexpr int16_t CONTENT_X = PANEL_X + 1;
  static constexpr int16_t CONTENT_Y = PANEL_Y + 1;
  static constexpr int16_t CONTENT_W = PANEL_W - 2;
  static constexpr int16_t CONTENT_H = PANEL_H - 2;

  static constexpr int16_t HEADER_TITLE_X = 0;
  static constexpr int16_t HEADER_TITLE_W = 108;
  static constexpr int16_t HEADER_CONTEXT_X = 108;
  static constexpr int16_t HEADER_CONTEXT_W = 125;
  static constexpr int16_t HEADER_STATE_X = 239;
  static constexpr int16_t HEADER_STATE_W = 75;
  static constexpr int16_t HEADER_LINE1_Y = 3;
  static constexpr int16_t HEADER_LINE2_Y = 12;

  void resetState();
  void invalidateAllCaches();

  void transitionTo(View nextView);
  void leaveView(View previousView, View nextView);
  void enterView(View previousView, View nextView);
  void clearTransitionRegions(View previousView, View nextView);

  void handleInput();
  void handleMapInput();
  void handleDetailInput();
  void handleEditInputInput();
  void handleEditTonInput();
  void handleWizardInput();
  void handleDeleteInput();

  void render(bool force);
  void renderHeader(bool force);
  void renderMap(bool force);
  void renderDetail(bool force);
  void renderEditInput(bool force);
  void renderEditTon(bool force);
  void renderWizard(bool force);
  void renderDeleteConfirm(bool force);

  HeaderModel buildHeaderModel() const;
  void renderHeaderTitle(const HeaderModel &model, bool force);
  void renderHeaderContext(const HeaderModel &model, bool force);
  void renderHeaderState(const HeaderModel &model, bool force);

  void clearContentArea();
  void clearHeaderTitleArea();
  void clearHeaderContextArea();
  void clearHeaderStateArea();

  void beginInputEdit();
  void cancelInputEdit();
  void applyInputEdit();

  void beginTonEdit();
  void cancelTonEdit();
  void applyTonEdit();
  void loadTonDraft();
  uint32_t tonDraftMilliseconds() const;
  static uint16_t tonResourceFromBase(TonBase base);
  static TonBase tonBaseFromResource(uint16_t resource);

  bool selectedBlockHasConsumers() const;
  void beginDeleteConfirm();
  void cancelDeleteConfirm();
  void applyDelete();

  RuntimeUIV2ReadModel *_model;
  RuntimeUIV2EditSession _editSession;

  View _view;
  View _previousView;
  bool _attached;
  bool _visible;
  bool _forceRedraw;
  bool _contentDirty;
  bool _headerDirty;
  bool _inputReleaseGate;

  uint16_t _selectedIndex;
  uint8_t _detailInputIndex;
  uint8_t _detailParameterIndex;
  DetailFocus _detailFocus;

  TonDraft _tonDraft;

  volatile bool _applyRequested;
  volatile bool _applyCompleted;
  volatile bool _applySuccess;
  bool _awaitingApply;

  HeaderModel _headerCache;
  bool _headerCacheValid;
  uint32_t _lastRefreshMs;
};

#endif