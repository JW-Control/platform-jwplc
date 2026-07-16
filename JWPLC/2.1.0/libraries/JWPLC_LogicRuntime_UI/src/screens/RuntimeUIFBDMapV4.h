#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V4_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V4_H

#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>

#include "../model/RuntimeUIV2ReadModel.h"

/**
 * @brief Mapa FBD compacto v0.4.5 para el motor RAM v2.
 *
 * El mapa usa bloques estrechos de dos líneas (Bxx / tipo o recurso), elimina
 * el footer de ayuda y reserva los laterales para referencias simétricas a la
 * columna inmediatamente anterior o siguiente.
 *
 * OK abre una vista gráfica de detalle con las fuentes conectadas y el bloque
 * ampliado. La vista sigue siendo de solo lectura; no modifica el programa.
 */
class RuntimeUIFBDMapV4
{
public:
  RuntimeUIFBDMapV4();

  void attach(RuntimeUIV2ReadModel &model);
  void detach();
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();

private:
  enum class Mode : uint8_t
  {
    Map = 0,
    Detail
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
  static constexpr uint32_t VALUE_REFRESH_MS = 100;
  static constexpr uint32_t DETAIL_REFRESH_MS = 150;

  static constexpr int16_t HEADER_INFO_X = 112;
  static constexpr int16_t HEADER_INFO_Y = 8;
  static constexpr uint8_t HEADER_INFO_COLUMNS = 20;

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
  static constexpr int16_t COLUMN_STEP = 64;
  static constexpr int16_t ROW_STEP = 34;
  static constexpr int16_t WORLD_MARGIN_X = 4;
  static constexpr int16_t WORLD_MARGIN_Y = 4;
  static constexpr int16_t KEEP_MARGIN_X = 46;
  static constexpr int16_t KEEP_MARGIN_Y = 4;

  static constexpr int16_t EDGE_HINT_W = 42;
  static constexpr int16_t EDGE_HINT_H = 24;

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

  void invalidateLayout();
  void buildLayout();
  void normalizeSelection();
  bool ensureSelectionVisible();

  void handleMapInput();
  void handleDetailInput();
  bool selectSource();
  bool selectConsumer();
  bool selectVertical(bool down);
  uint16_t nearestByY(const uint16_t *indices,
                      uint8_t count) const;

  void drawMapStatic();
  void clearMapArea();
  void drawMapHeaderInfo();
  void drawMapFull();
  void drawMapLive();
  void drawWires();
  void drawNodes();
  void drawEdgeHints();
  void drawWire(uint16_t consumerIndex,
                uint8_t inputIndex);
  void drawNode(uint16_t blockIndex);
  void drawFullNode(uint16_t blockIndex,
                    int16_t screenX,
                    int16_t screenY,
                    uint16_t border,
                    bool active,
                    bool selected);
  void drawEdgeHint(uint16_t blockIndex,
                    bool leftSide,
                    int16_t screenY,
                    uint16_t border,
                    bool active);
  void drawNodePorts(uint16_t blockIndex,
                     int16_t screenX,
                     int16_t screenY);

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

  void drawDetailStatic();
  void drawDetail(bool force);
  void drawDetailSource(uint8_t inputIndex,
                        uint8_t visibleRow,
                        bool selected);
  void drawDetailBlock();
  void drawDetailWires();
  int16_t detailInputY(uint8_t visibleRow) const;
  uint8_t detailPageStart() const;

  bool valuesChanged();
  void cacheValues();
  void updateHeaderStateIfNeeded(bool force);
  bool nodeFullyVisible(int16_t screenX, int16_t screenY) const;
  GridRange visibleGridRange() const;
  bool shouldDrawEdgeHint(uint16_t blockIndex,
                          const GridRange &range,
                          bool &leftSide) const;
  int16_t screenX(uint16_t blockIndex) const;
  int16_t screenY(uint16_t blockIndex) const;
  int16_t inputPortY(const LogicV2BlockRecord &block,
                     uint8_t inputIndex) const;

  void formatBlockId(char *destination,
                     size_t capacity,
                     uint16_t blockIndex) const;
  void formatMapLine2(char *destination,
                      size_t capacity,
                      uint16_t blockIndex) const;
  void formatSourceCompact(char *destination,
                           size_t capacity,
                           uint16_t blockIndex,
                           uint8_t inputIndex) const;
  const char *detailSymbol(LogicV2BlockType type) const;

  const char *stateText() const;
  uint16_t stateColor() const;

  RuntimeUIV2ReadModel *_model;
  Mode _mode;
  bool _fullRedraw;
  bool _layoutValid;
  bool _valueCacheValid;
  bool _headerStateValid;
  LogicV2EngineState _lastHeaderState;
  uint16_t _selectedIndex;
  uint8_t _detailInputIndex;
  uint16_t _layoutBlockCount;
  uint16_t _layoutLinkCount;
  int16_t _viewportX;
  int16_t _viewportY;
  uint32_t _lastValueRefreshMs;
  uint32_t _lastDetailRefreshMs;

  uint8_t _levels[MAX_BLOCKS];
  uint8_t _lanes[MAX_BLOCKS];
  int16_t _nodeX[MAX_BLOCKS];
  int16_t _nodeY[MAX_BLOCKS];
  bool _valueCache[MAX_BLOCKS];
};

#endif
