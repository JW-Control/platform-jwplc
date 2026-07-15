#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V2_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V2_H

#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>

#include "../model/RuntimeUIV2ReadModel.h"

/**
 * @brief Mapa FBD estable y de solo lectura para el motor RAM v2.
 *
 * Cada bloque recibe una posición lógica determinista. La selección se mueve
 * por conexiones o cercanía geográfica; el viewport solo se desplaza cuando el
 * bloque seleccionado cruza los márgenes visibles.
 *
 * Desde v0.4.3 los bloques próximos a un borde conservan una identificación
 * compacta y los cables se recortan por segmentos contra el viewport. Así no
 * desaparecen las conexiones cuando una fuente o consumidor está fuera de la
 * zona visible.
 */
class RuntimeUIFBDMapV2
{
public:
  RuntimeUIFBDMapV2();

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

  static constexpr uint16_t MAX_BLOCKS =
      JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS;
  static constexpr uint8_t MAX_LEVELS = 100;
  static constexpr uint32_t VALUE_REFRESH_MS = 100;

  static constexpr int16_t HEADER_INFO_X = 104;
  static constexpr int16_t HEADER_INFO_Y = 8;
  static constexpr uint8_t HEADER_INFO_COLUMNS = 18;

  static constexpr int16_t PANEL_X = 4;
  static constexpr int16_t PANEL_Y = 28;
  static constexpr int16_t PANEL_W = 312;
  static constexpr int16_t PANEL_H = 127;

  static constexpr int16_t MAP_X = PANEL_X + 1;
  static constexpr int16_t MAP_Y = PANEL_Y + 1;
  static constexpr int16_t MAP_W = PANEL_W - 2;
  static constexpr int16_t MAP_H = PANEL_H - 2;

  static constexpr int16_t NODE_W = 66;
  static constexpr int16_t NODE_H = 32;
  static constexpr int16_t NODE_GUTTER_W = 17;
  static constexpr int16_t NODE_TEXT_X = NODE_GUTTER_W + 2;
  static constexpr int16_t COLUMN_STEP = 88;
  static constexpr int16_t ROW_STEP = 40;
  static constexpr int16_t WORLD_MARGIN_X = 4;
  static constexpr int16_t WORLD_MARGIN_Y = 2;
  static constexpr int16_t KEEP_MARGIN_X = 12;
  static constexpr int16_t KEEP_MARGIN_Y = 6;

  static constexpr int16_t EDGE_HINT_W = 48;
  static constexpr int16_t EDGE_HINT_H = 10;

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
  void drawMap();
  void drawWires();
  void drawNodes();
  void drawWire(uint16_t consumerIndex,
                uint8_t inputIndex);
  void drawNode(uint16_t blockIndex);
  void drawFullNode(uint16_t blockIndex,
                    int16_t screenX,
                    int16_t screenY,
                    uint16_t border,
                    bool active,
                    bool selected);
  void drawPartialNode(uint16_t blockIndex,
                       int16_t screenX,
                       int16_t screenY,
                       uint16_t border,
                       bool active);
  void drawEdgeHint(uint16_t blockIndex,
                    int16_t screenX,
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

  bool valuesChanged();
  bool nodeFullyVisible(int16_t screenX, int16_t screenY) const;
  bool nodeIntersectsMap(int16_t screenX, int16_t screenY) const;
  bool nodeNearMap(int16_t screenX, int16_t screenY) const;
  int16_t screenX(uint16_t blockIndex) const;
  int16_t screenY(uint16_t blockIndex) const;
  int16_t inputPortY(const LogicV2BlockRecord &block,
                     uint8_t inputIndex) const;

  void formatBlockTitle(char *destination,
                        size_t capacity,
                        uint16_t blockIndex) const;
  void formatNodeData(char *destination,
                      size_t capacity,
                      uint16_t blockIndex) const;
  void formatPinLabel(char *destination,
                      size_t capacity,
                      const LogicV2BlockRecord &block,
                      const LogicV2InputLink &input,
                      uint8_t inputIndex) const;
  void formatResource(char *destination,
                      size_t capacity,
                      const LogicV2BlockRecord &block) const;
  void formatInputSource(char *destination,
                         size_t capacity,
                         uint16_t blockIndex,
                         uint8_t inputIndex) const;

  const char *stateText() const;
  uint16_t stateColor() const;

  RuntimeUIV2ReadModel *_model;
  Mode _mode;
  bool _fullRedraw;
  bool _layoutValid;
  bool _valueCacheValid;
  uint16_t _selectedIndex;
  uint16_t _layoutBlockCount;
  uint16_t _layoutLinkCount;
  int16_t _viewportX;
  int16_t _viewportY;
  uint32_t _lastValueRefreshMs;

  uint8_t _levels[MAX_BLOCKS];
  int16_t _nodeX[MAX_BLOCKS];
  int16_t _nodeY[MAX_BLOCKS];
  bool _valueCache[MAX_BLOCKS];
};

#endif