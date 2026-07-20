#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_UNIFIED_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_UNIFIED_H

#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime_V2.h>

#include "../edit/RuntimeUIV2EditSession.h"
#include "../model/RuntimeUIV2ReadModel.h"

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

  enum class HorizontalWindowMode : uint8_t
  {
    LeftEdge = 0,
    Middle,
    RightEdge
  };

  struct GridRange
  {
    uint8_t minLevel;
    uint8_t maxLevel;
    uint8_t minLane;
    uint8_t maxLane;
    bool valid;
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
  static constexpr uint32_t DETAIL_LIVE_REFRESH_MS = 100UL;

  static constexpr uint16_t MAX_BLOCKS =
      JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS;
  static constexpr uint8_t MAX_LEVELS = 100;

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

  static constexpr int16_t MAP_X = CONTENT_X;
  static constexpr int16_t MAP_Y = CONTENT_Y;
  static constexpr int16_t MAP_W = CONTENT_W;
  static constexpr int16_t MAP_H = CONTENT_H;
  static constexpr int16_t NODE_W = 48;
  static constexpr int16_t NODE_H = 30;
  static constexpr int16_t NODE_GUTTER_W = 11;
  static constexpr int16_t ROW_STEP = 34;
  static constexpr int16_t WORLD_MARGIN_Y = 4;
  static constexpr int16_t KEEP_MARGIN_Y = 4;
  static constexpr uint8_t SLOT_COUNT = 5;
  static constexpr int16_t SLOT_X0 = MAP_X + 4;
  static constexpr int16_t SLOT_STEP = 63;
  static constexpr int16_t EDGE_HINT_W = 42;
  static constexpr int16_t EDGE_HINT_H = 24;
  static constexpr int16_t EDGE_HINT_Y_OFFSET = 3;

  static constexpr uint8_t DETAIL_INPUTS_PER_PAGE = 4;
  static constexpr int16_t DETAIL_SOURCE_X = 8;
  static constexpr int16_t DETAIL_SOURCE_W = 72;
  static constexpr int16_t DETAIL_SOURCE_H = 24;
  static constexpr int16_t DETAIL_SOURCE_STEP = 31;
  static constexpr int16_t DETAIL_BLOCK_X = 186;
  static constexpr int16_t DETAIL_BLOCK_Y = 44;
  static constexpr int16_t DETAIL_BLOCK_W = 112;
  static constexpr int16_t DETAIL_BLOCK_H = 92;
  static constexpr int16_t DETAIL_GUTTER_W = 24;
  static constexpr int16_t TON_PANEL_X = 214;
  static constexpr int16_t TON_PANEL_Y = 101;
  static constexpr int16_t TON_PANEL_W = 80;
  static constexpr int16_t TON_PANEL_H = 31;

  void resetState();
  void invalidateAllCaches();
  void invalidateLayout();
  void invalidateMapCache();
  void invalidateDetailCache();

  void transitionTo(View nextView);
  void leaveView(View previousView, View nextView);
  void enterView(View previousView, View nextView);
  void clearTransitionRegions(View previousView, View nextView);

  void handleInput();
  void handleMapInput();
  void handleMapInputStable();
  void handleDetailInput();
  void handleEditInputInput();
  void handleEditTonInput();
  void handleWizardInput();
  void handleDeleteInput();

  bool anyButtonHeld() const;
  void gateInputUntilRelease();
  bool consumeInputReleaseGate();

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
  void renderHeaderContext(const HeaderModel &model,
                           bool line1Changed,
                           bool line2Changed);
  void renderHeaderState(const HeaderModel &model, bool force);

  void clearContentArea();
  void clearHeaderTitleArea();
  void clearHeaderContextArea();
  void clearHeaderStateArea();

  void buildLayout();
  void normalizeSelection();
  bool ensureSelectionVisible();
  bool updateHorizontalWindow();
  bool selectSource();
  bool selectConsumer();
  bool selectVertical(bool down);
  uint16_t nearestByY(const uint16_t *indices, uint8_t count) const;

  bool isFullLevel(uint8_t level) const;
  bool isPreviewLevel(uint8_t level, bool &leftSide) const;
  int8_t slotForLevel(uint8_t level) const;
  int16_t screenX(uint16_t blockIndex) const;
  int16_t screenY(uint16_t blockIndex) const;
  bool nodeFullyVisible(int16_t x, int16_t y) const;
  int16_t renderedNodeWidth(uint16_t blockIndex) const;
  int16_t renderedNodeHeight(uint16_t blockIndex) const;
  int16_t renderedNodeYOffset(uint16_t blockIndex) const;
  int16_t inputPortY(const LogicV2BlockRecord &block,
                     uint8_t inputIndex) const;
  GridRange visibleGridRange() const;
  bool shouldDrawEdgeHint(uint16_t blockIndex,
                          const GridRange &range,
                          bool &leftSide) const;

  void drawMapFull();
  void drawMapLive();
  void drawMapSelectionFrame(uint16_t blockIndex);
  void drawWires();
  void drawWire(uint16_t consumerIndex, uint8_t inputIndex);
  void drawNodes();
  void drawNode(uint16_t blockIndex);
  void drawFullNode(uint16_t blockIndex,
                    int16_t x,
                    int16_t y,
                    uint16_t border,
                    bool active,
                    bool selected);
  void drawEdgeHints();
  void drawEdgeHint(uint16_t blockIndex,
                    bool leftSide,
                    int16_t nodeScreenY,
                    uint16_t border,
                    bool active);
  void drawNodePorts(uint16_t blockIndex, int16_t x, int16_t y);
  void drawClippedHorizontal(int16_t x0,
                             int16_t x1,
                             int16_t y,
                             uint16_t color);
  void drawClippedVertical(int16_t x,
                           int16_t y0,
                           int16_t y1,
                           uint16_t color);
  void drawOrthogonalWireClipped(int16_t x0,
                                 int16_t y0,
                                 int16_t x1,
                                 int16_t y1,
                                 int16_t routeX,
                                 uint16_t color);

  uint8_t detailPageStart() const;
  void normalizeDetailFocus();
  bool selectedBlockHasParameter() const;
  void drawDetailFull();
  void drawDetailLive();
  void drawDetailWires();
  void drawDetailSource(uint8_t inputIndex,
                        uint8_t visibleRow,
                        bool selected);
  void drawDetailBlock();
  void drawTonPanel(bool force);
  void drawTonPanelFrame(bool selected);
  void drawTonTextLine(int16_t y,
                       const char *prefix,
                       const char *value,
                       uint16_t color);
  void captureDetailCache();

  void formatBlockId(char *destination,
                     size_t capacity,
                     uint16_t blockIndex) const;
  void formatMapLine2(char *destination,
                      size_t capacity,
                      uint16_t blockIndex) const;
  const char *detailSymbol(LogicV2BlockType type) const;
  const char *stateText() const;
  uint16_t stateColor() const;
  static TonBase tonBaseFromResource(uint16_t resource);
  static void formatMillisecondsInBase(uint32_t milliseconds,
                                       TonBase base,
                                       char *destination,
                                       size_t capacity);

  void beginInputEdit();
  void cancelInputEdit();
  void applyInputEdit();

  void beginTonEdit();
  void cancelTonEdit();
  void applyTonEdit();
  void loadTonDraft();
  uint32_t tonDraftMilliseconds() const;
  static uint16_t tonResourceFromBase(TonBase base);

  bool selectedBlockHasConsumers() const;
  void beginDeleteConfirm();
  void cancelDeleteConfirm();
  void applyDelete();

  RuntimeUIV2ReadModel *_model;
  RuntimeUIV2EditSession _editSession;

  View _view;
  View _previousView;
  HorizontalWindowMode _horizontalMode;
  bool _attached;
  bool _visible;
  bool _forceRedraw;
  bool _contentDirty;
  bool _headerDirty;
  bool _inputReleaseGate;
  bool _layoutValid;

  uint16_t _selectedIndex;
  uint8_t _detailInputIndex;
  uint8_t _detailParameterIndex;
  DetailFocus _detailFocus;

  uint16_t _layoutBlockCount;
  uint16_t _layoutLinkCount;
  uint8_t _centralStartLevel;
  uint8_t _maxLevel;
  int16_t _viewportY;
  uint8_t _levels[MAX_BLOCKS];
  uint8_t _lanes[MAX_BLOCKS];
  int16_t _nodeX[MAX_BLOCKS];
  int16_t _nodeY[MAX_BLOCKS];

  bool _mapValueCache[MAX_BLOCKS];
  bool _mapValueCacheValid;
  uint16_t _mapSelectionCache;
  bool _mapSelectionCacheValid;

  bool _detailCacheValid;
  uint16_t _detailCacheBlock;
  uint8_t _detailCacheInput;
  DetailFocus _detailCacheFocus;
  bool _detailCacheBlockValue;
  bool _detailInputValueCache[JWPLC_LOGIC_V2_MAX_INPUTS_PER_BLOCK];
  char _detailTonConfiguredCache[16];
  char _detailTonElapsedCache[16];
  bool _detailTonColorCache;
  bool _detailTonSelectedCache;
  uint32_t _lastDetailLiveMs;

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