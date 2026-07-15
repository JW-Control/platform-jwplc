#ifndef JWPLC_LOGIC_RUNTIME_UI_DIAGRAM_H
#define JWPLC_LOGIC_RUNTIME_UI_DIAGRAM_H

#include <Arduino.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>

#include "../RuntimeUIView.h"

/**
 * @brief Vista grafica principal del programa lógico.
 *
 * Presenta el bloque seleccionado en el centro, sus fuentes inmediatas a la
 * izquierda y hasta dos consumidores inmediatos a la derecha. La vista es de
 * solo lectura en esta primera etapa; el editor se añadirá sobre esta base.
 */
class RuntimeUIDiagram
{
public:
  RuntimeUIDiagram();

  void attach(JWPLC_LogicRuntime &runtime);
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();

  RuntimeUIView takeRequestedView();

private:
  enum class Mode : uint8_t
  {
    Graph = 0,
    Detail
  };

  static constexpr uint8_t COMMAND_COUNT = 3;
  static constexpr uint32_t VALUE_REFRESH_MS = 100;

  struct NodeCache
  {
    bool valid;
    uint16_t index;
    bool value;
  };

  struct DiagramCache
  {
    bool valid;
    JWPLCLogicRuntimeState runtimeState;
    uint16_t blockCount;
    uint16_t selectedIndex;
    bool selectedValue;
    NodeCache sourceA;
    NodeCache sourceB;
    NodeCache targets[2];
    uint8_t targetCount;
  };

  void invalidateCache();
  void normalizeSelection();
  void drawCurrentMode();

  void drawGraphStatic();
  void drawGraph(bool clearViewport);
  void drawNoProgram();
  void drawGraphCommands();
  void redrawGraphCommand(uint8_t previousCommand,
                          uint8_t currentCommand);
  void handleGraphInput();
  void updateGraphValues(bool force);

  void drawDetailStatic();
  void updateDetailFields(bool force);
  void handleDetailInput();

  void drawMainBlock(const LogicBlockDefinition &block,
                     bool value);
  void drawMiniBlock(int16_t x,
                     int16_t y,
                     uint16_t blockIndex,
                     bool value);
  void drawEndpoint(int16_t x,
                    int16_t y,
                    const char *title,
                    const char *subtitle,
                    bool value);
  void drawLeftConnection(int16_t sourceY,
                          int16_t destinationY,
                          bool active,
                          const char *portLabel);
  void drawRightConnection(int16_t sourceY,
                           int16_t destinationY,
                           bool active);

  uint8_t collectTargets(uint16_t sourceIndex,
                         uint16_t destination[2]) const;
  bool blockUsesSource(const LogicBlockDefinition &block,
                       uint16_t sourceIndex) const;
  bool expectsSourceA(LogicBlockType type) const;
  bool expectsSourceB(LogicBlockType type) const;

  void copyProgramName(char *destination,
                       size_t destinationCapacity) const;
  const char *blockTypeName(LogicBlockType type) const;
  const char *blockTypeShort(LogicBlockType type) const;
  void formatSource(char *destination,
                    size_t destinationCapacity,
                    uint16_t source) const;
  void formatResource(char *destination,
                      size_t destinationCapacity,
                      const LogicBlockDefinition &block) const;
  void formatParameter(char *destination,
                       size_t destinationCapacity,
                       const LogicBlockDefinition &block) const;
  void formatMainData(char *destination,
                      size_t destinationCapacity,
                      const LogicBlockDefinition &block) const;

  uint16_t runtimeStateColor() const;
  const char *runtimeStateText() const;

  JWPLC_LogicRuntime *_runtime;
  DiagramCache _cache;
  Mode _mode;
  bool _fullRedraw;
  uint16_t _selectedIndex;
  uint8_t _selectedCommand;
  bool _detailValueValid;
  bool _detailValue;
  uint32_t _lastValueRefreshMs;
  RuntimeUIView _requestedView;
};

#endif