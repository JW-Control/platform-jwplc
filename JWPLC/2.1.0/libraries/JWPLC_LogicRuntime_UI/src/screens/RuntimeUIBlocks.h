#ifndef JWPLC_LOGIC_RUNTIME_UI_BLOCKS_H
#define JWPLC_LOGIC_RUNTIME_UI_BLOCKS_H

#include <Arduino.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>

#include "../RuntimeUIView.h"

class RuntimeUIBlocks
{
public:
  RuntimeUIBlocks();

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
    List = 0,
    Detail
  };

  static constexpr uint8_t VISIBLE_ROWS = 5;
  static constexpr uint32_t VALUE_REFRESH_MS = 100;

  struct ListCache
  {
    bool valid;
    bool hasProgram;
    uint16_t blockCount;
    uint16_t topIndex;
    uint16_t selectedIndex;
    bool values[VISIBLE_ROWS];
    bool valueValid[VISIBLE_ROWS];
    char programName[JWPLC_LOGIC_PROGRAM_NAME_BYTES + 1];
  };

  void invalidateCache();
  void drawCurrentMode();

  void drawListStatic();
  void drawListRows(bool force);
  void drawListRow(uint8_t visibleRow,
                   uint16_t blockIndex,
                   bool selected);
  void drawListCommands();
  void redrawListCommand(uint8_t previousCommand,
                         uint8_t currentCommand);
  void redrawListSelection(uint16_t previousIndex,
                           uint16_t currentIndex,
                           uint16_t previousTopIndex);
  void updateListValues(bool force);
  void handleListInput();

  void drawDetailStatic();
  void updateDetailFields(bool force);
  void handleDetailInput();

  void normalizeSelection();
  void copyProgramName(char *destination,
                       size_t destinationCapacity) const;
  const char *blockTypeName(LogicBlockType type) const;
  void formatSource(char *destination,
                    size_t destinationCapacity,
                    uint16_t source) const;
  void formatResource(char *destination,
                      size_t destinationCapacity,
                      const LogicBlockDefinition &block) const;
  void formatParameter(char *destination,
                       size_t destinationCapacity,
                       const LogicBlockDefinition &block) const;
  uint16_t runtimeStateColor() const;
  const char *runtimeStateText() const;

  JWPLC_LogicRuntime *_runtime;
  ListCache _cache;
  Mode _mode;
  bool _fullRedraw;
  uint16_t _selectedIndex;
  uint16_t _topIndex;
  uint8_t _selectedCommand;
  bool _detailValueValid;
  bool _detailValue;
  uint32_t _lastValueRefreshMs;
  RuntimeUIView _requestedView;
};

#endif