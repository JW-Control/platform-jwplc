#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_H

#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime_V2.h>

#include "../edit/RuntimeUIV2EditSession.h"
#include "../model/RuntimeUIV2ReadModel.h"

/**
 * @brief Editor FBD v2 consolidado.
 *
 * Una sola clase y una sola máquina de estados controlan MAPA, DETALLE,
 * EDITAR IN, EDITAR T y NUEVO BLOQUE. No hereda ni invoca revisiones Vx.
 * Las clases RuntimeUIFBDMapV2...V14 quedan congeladas como histórico interno.
 */
class RuntimeUIFBDMap final
{
public:
  RuntimeUIFBDMap();

  void attach(RuntimeUIV2ReadModel &model);
  void detach();
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();
  void processPendingEdit();

  /**
   * @brief La entrada aún se procesa dentro de refresh(); no se omite el callback.
   */
  bool needsTftRefresh() const { return true; }

private:
  enum class View : uint8_t
  {
    Map = 0,
    Detail,
    EditInput,
    EditTon,
    AddType,
    AddConfig,
    AddSource,
    AddResource,
    AddTon
  };

  enum class DetailFocus : uint8_t
  {
    Inputs = 0,
    Parameters
  };

  enum class EditInputFocus : uint8_t
  {
    Source = 0,
    Logic
  };

  enum class TonBase : uint8_t
  {
    Seconds = 0,
    Minutes = 1,
    Hours = 2
  };

  enum class TonField : uint8_t
  {
    Major = 0,
    Minor,
    Base
  };

  enum class AddType : uint8_t
  {
    DigitalInput = 0,
    Not,
    And2,
    Ton,
    DigitalOutput,
    Count
  };

  enum class AddConfigRow : uint8_t
  {
    First = 0,
    Second,
    Third,
    Create
  };

  enum class ApplyContext : uint8_t
  {
    None = 0,
    EditInput,
    EditTon,
    AddBlock
  };

  enum class Feedback : uint8_t
  {
    None = 0,
    Invalid,
    Applying,
    ApplyFailed
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

  static constexpr uint16_t MAX_BLOCKS =
      JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS;
  static constexpr uint8_t MAX_LEVELS = 100;
  static constexpr uint32_t MAP_REFRESH_MS = 100;
  static constexpr uint32_t DETAIL_REFRESH_MS = 100;
  static constexpr uint32_t EDIT_REFRESH_MS = 40;

  static constexpr int16_t SCREEN_W = 320;
  static constexpr int16_t SCREEN_H = 170;
  static constexpr int16_t HEADER_H = 24;
  static constexpr int16_t HEADER_TITLE_X = 0;
  static constexpr int16_t HEADER_TITLE_W = 108;
  static constexpr int16_t HEADER_CONTEXT_X = 108;
  static constexpr int16_t HEADER_CONTEXT_W = 125;
  static constexpr int16_t HEADER_CONTEXT_LINE1_Y = 3;
  static constexpr int16_t HEADER_CONTEXT_LINE2_Y = 12;
  static constexpr uint8_t HEADER_CONTEXT_COLUMNS = 20;

  static constexpr int16_t PANEL_X = 2;
  static constexpr int16_t PANEL_Y = 27;
  static constexpr int16_t PANEL_W = 316;
  static constexpr int16_t PANEL_H = 142;
  static constexpr int16_t MAP_X = PANEL_X + 1;
  static constexpr int16_t MAP_Y = PANEL_Y + 1;
  static constexpr int16_t MAP_W = PANEL_W - 2;
  static constexpr int16_t MAP_H = PANEL_H - 2;

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

  static constexpr int16_t FIELD_Y = 103;
  static constexpr int16_t FIELD_H = 38;
  static constexpr uint32_t TON_MINIMUM_MS = 20UL;

  // Lifecycle and state machine.
  void resetState();
  void setView(View view, bool redraw = true);
  void syncRefreshPeriod();
  uint32_t desiredRefreshPeriod() const;
  void renderCurrentFull(bool initialEntry = false);
  void handleApplyCompleted();
  void handleInput();
  void refreshLive();

  // Input release gate.
  bool anyButtonHeld() const;
  void gateInputUntilRelease(bool restoreIdleReturn = false);
  bool consumeInputReleaseGate();

  // Layout and navigation.
  void invalidateLayout();
  void buildLayout();
  void normalizeSelection();
  bool updateHorizontalWindow();
  bool ensureSelectionVisible();
  bool selectSource();
  bool selectConsumer();
  bool selectVertical(bool down);
  uint16_t nearestByY(const uint16_t *indices, uint8_t count) const;

  // Unified header.
  const char *titleForView(View view) const;
  void buildHeaderContext(char *line1,
                          size_t line1Capacity,
                          char *line2,
                          size_t line2Capacity) const;
  void drawUnifiedHeader(bool force = false);
  const char *stateText() const;
  uint16_t stateColor() const;
  void invalidateHeaderCache();

  // Map rendering and input.
  void handleMapInput();
  void drawMapFull(bool clearInterior = true);
  void drawMapLive();
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
  void drawNodePorts(uint16_t blockIndex, int16_t x, int16_t y);
  void drawEdgeHints();
  void drawEdgeHint(uint16_t blockIndex,
                    bool leftSide,
                    int16_t nodeScreenY,
                    uint16_t border,
                    bool active);
  void drawAddNode();
  void clearMapArea();
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

  // Detail rendering and input.
  void handleDetailInput();
  void normalizeDetailFocus();
  void drawDetailFull(bool clearInterior = true);
  void drawDetailLive();
  void drawDetailSource(uint8_t inputIndex,
                        uint8_t visibleRow,
                        bool selected);
  void drawDetailWires();
  void drawDetailBlock();
  void drawTonDetailPanel(bool selected, bool force = false);
  uint8_t detailPageStart() const;

  // Input editor.
  bool beginInputEdit();
  void handleEditInput();
  void drawEditInputFull(bool clearInterior = true);
  void cancelInputEdit();
  bool selectedInputAllowsOpen() const;
  uint16_t sourceCandidateCount(bool forAdd = false) const;
  uint16_t sourceCandidateAt(uint16_t candidateIndex,
                             bool forAdd = false) const;
  uint16_t sourceCandidateIndex(uint16_t source,
                                bool forAdd = false) const;
  void moveSourceCandidate(bool forward, bool forAdd = false);
  void formatSource(uint16_t source,
                    char *id,
                    size_t idCapacity,
                    char *type,
                    size_t typeCapacity) const;

  // TON editor.
  bool beginTonEdit();
  void handleEditTon();
  void drawEditTonFull(bool clearInterior = true);
  void drawTonFields(bool force = false);
  void drawTonFieldFull(TonField field);
  void drawTonFieldValueOnly(TonField field);
  void drawTonElapsed(bool force = false);
  void drawTonFooter(const char *text, uint16_t color);
  void cancelTonEdit();

  static const char *tonBaseText(TonBase base);
  static const char *tonMajorLabel(TonBase base);
  static const char *tonMinorLabel(TonBase base);
  static uint32_t tonMinorMaximum(TonBase base);
  static uint16_t tonResource(TonBase base);
  static TonBase tonBaseFromResource(uint16_t resource);
  static TonBase effectiveTonBase(uint32_t milliseconds, uint16_t resource);
  static uint32_t encodeTonMilliseconds(uint32_t major,
                                        uint32_t minor,
                                        TonBase base);
  static void decodeTonMilliseconds(uint32_t milliseconds,
                                    TonBase base,
                                    uint32_t &major,
                                    uint32_t &minor);
  static void formatTon(uint32_t major,
                        uint32_t minor,
                        TonBase base,
                        char *destination,
                        size_t capacity);
  static void formatTonMilliseconds(uint32_t milliseconds,
                                    uint16_t resource,
                                    char *destination,
                                    size_t capacity);

  // New block wizard.
  void enterAddNode();
  void leaveAddNode();
  void handleAddType();
  void handleAddConfig();
  void handleAddSource();
  void handleAddResource();
  void handleAddTon();
  void drawAddTypeFull(bool clearInterior = true);
  void drawAddConfigFull(bool clearInterior = true);
  void drawAddSourceFull(bool clearInterior = true);
  void drawAddResourceFull(bool clearInterior = true);
  void drawAddTonFull(bool clearInterior = true);
  void requestAddBlock();
  LogicV2BlockType selectedAddLogicType() const;
  uint8_t addInputCount() const;
  const char *addTypeText(AddType type) const;
  uint8_t addConfigRowCount() const;
  const char *addConfigRowLabel(uint8_t row) const;
  void formatAddConfigRowValue(uint8_t row,
                               char *destination,
                               size_t capacity) const;

  // Formatting and geometry helpers.
  bool valuesChanged();
  void cacheValues();
  bool isFullLevel(uint8_t level) const;
  bool isPreviewLevel(uint8_t level, bool &leftSide) const;
  int8_t slotForLevel(uint8_t level) const;
  bool nodeFullyVisible(int16_t x, int16_t y) const;
  GridRange visibleGridRange() const;
  bool shouldDrawEdgeHint(uint16_t blockIndex,
                          const GridRange &range,
                          bool &leftSide) const;
  int16_t screenX(uint16_t blockIndex) const;
  int16_t screenY(uint16_t blockIndex) const;
  int16_t renderedNodeWidth(uint16_t blockIndex) const;
  int16_t renderedNodeHeight(uint16_t blockIndex) const;
  int16_t renderedNodeYOffset(uint16_t blockIndex) const;
  int16_t inputPortY(const LogicV2BlockRecord &block,
                     uint8_t inputIndex) const;
  void formatBlockId(char *destination,
                     size_t capacity,
                     uint16_t blockIndex) const;
  void formatMapLine2(char *destination,
                      size_t capacity,
                      uint16_t blockIndex) const;
  const char *detailSymbol(LogicV2BlockType type) const;

  RuntimeUIV2ReadModel *_model;
  RuntimeUIV2EditSession _editSession;
  View _view;
  View _returnView;
  ApplyContext _applyContext;
  Feedback _feedback;

  bool _fullRedraw;
  bool _layoutValid;
  bool _valueCacheValid;
  bool _inputReleaseGate;
  bool _restoreIdleReturnPending;
  volatile bool _applyRequested;
  volatile bool _applyCompleted;
  volatile bool _applySuccess;
  bool _awaitingApply;

  HorizontalWindowMode _horizontalMode;
  uint16_t _selectedIndex;
  uint8_t _detailInputIndex;
  DetailFocus _detailFocus;
  uint8_t _detailParameterIndex;
  uint16_t _layoutBlockCount;
  uint16_t _layoutLinkCount;
  uint8_t _centralStartLevel;
  uint8_t _maxLevel;
  int16_t _viewportY;
  uint32_t _lastMapRefreshMs;
  uint32_t _lastDetailRefreshMs;
  uint32_t _lastEditRefreshMs;
  uint32_t _appliedRefreshPeriodMs;

  // Input editor state.
  EditInputFocus _editInputFocus;
  uint16_t _editSourceCandidate;
  bool _editInverted;

  // TON editor state.
  uint32_t _tonMajor;
  uint32_t _tonMinor;
  TonBase _tonBase;
  TonField _tonField;
  uint32_t _tonOriginalMs;
  uint16_t _tonOriginalResource;
  bool _tonFieldsCacheValid;
  uint32_t _tonMajorCache;
  uint32_t _tonMinorCache;
  TonBase _tonBaseCache;
  TonField _tonFieldCache;
  bool _tonElapsedCacheValid;
  bool _tonElapsedColorOn;
  char _tonElapsedCache[20];
  bool _tonFooterDefaultVisible;

  // Add wizard state.
  bool _addNodeSelected;
  uint16_t _addSourceBlock;
  AddType _addType;
  uint8_t _addTypeIndex;
  uint8_t _addConfigRow;
  uint16_t _addSources[2];
  uint8_t _addResource;
  uint16_t _addSourceCandidate;
  uint8_t _addEditingSourceIndex;
  uint8_t _addResourceBackup;
  uint32_t _addTonMajor;
  uint32_t _addTonMinor;
  TonBase _addTonBase;
  TonField _addTonField;
  uint16_t _newBlockIndex;

  // Unified header cache.
  bool _headerCacheValid;
  char _headerTitleCache[16];
  char _headerLine1Cache[24];
  char _headerLine2Cache[24];
  LogicV2EngineState _headerStateCache;

  // Detail/TON caches.
  bool _detailCacheValid;
  uint16_t _detailCacheBlock;
  uint8_t _detailCacheInput;
  DetailFocus _detailCacheFocus;
  bool _detailBlockValueCache;
  bool _detailInputValueCache;
  bool _detailTonSelectedCache;
  char _detailTonConfiguredCache[20];
  char _detailTonElapsedCache[20];

  uint8_t _levels[MAX_BLOCKS];
  uint8_t _lanes[MAX_BLOCKS];
  int16_t _nodeX[MAX_BLOCKS];
  int16_t _nodeY[MAX_BLOCKS];
  bool _valueCache[MAX_BLOCKS];
};

#endif
