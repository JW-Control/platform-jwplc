#ifndef JWPLC_LOGIC_RUNTIME_UI_PROGRAM_H
#define JWPLC_LOGIC_RUNTIME_UI_PROGRAM_H

#include <Arduino.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>

#include "../RuntimeUIView.h"

enum class RuntimeUIProgramAction : uint8_t
{
  None = 0,
  Prepare,
  Start,
  Stop
};

enum class RuntimeUIProgramFeedback : uint8_t
{
  None = 0,
  PrepareActive,
  PrepareFallback,
  PrepareUnformatted,
  PrepareEmpty,
  PrepareNoValid,
  PrepareFailed,
  StartOk,
  StartFailed,
  StopOk
};

class RuntimeUIProgram
{
public:
  RuntimeUIProgram();

  void attach(JWPLC_LogicRuntime &runtime);
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();
  void invalidateDynamicCache();

  RuntimeUIView takeRequestedView();
  RuntimeUIProgramAction takeRequestedAction();
  void setFeedback(RuntimeUIProgramFeedback feedback);

private:
  static constexpr uint8_t MENU_COUNT = 4;

  struct DynamicCache
  {
    bool valid;
    JWPLCLogicRuntimeState runtimeState;
    JWPLCLogicRuntimeError runtimeError;
    JWPLCLogicStorageBootState bootState;
    JWPLCLogicRetentiveState retentiveState;
    bool hasIdentity;
    uint32_t programId;
    uint32_t generation;
    uint16_t blockCount;
    uint16_t retentiveBlocks;
    char programName[JWPLC_LOGIC_PROGRAM_NAME_BYTES + 1];
  };

  void drawStaticLayout();
  void updateDynamicFields(bool force);
  void updateRuntimeBadge(bool force);
  void updateProgramFields(bool force);
  void updateStatusFields(bool force);

  void drawMenu();
  void redrawMenuSelection(uint8_t previousSelection,
                           uint8_t currentSelection);
  void handleInput();
  void requestSelectedAction();
  void consumeFeedback();
  void restoreFooterIfDue();

  void copyProgramName(char *destination,
                       size_t destinationCapacity) const;
  uint16_t runtimeStateColor() const;
  const char *runtimeStateText() const;

  JWPLC_LogicRuntime *_runtime;
  DynamicCache _cache;
  bool _fullRedraw;
  uint8_t _selectedMenu;
  uint32_t _messageUntilMs;
  RuntimeUIView _requestedView;
  RuntimeUIProgramAction _requestedAction;
  volatile RuntimeUIProgramFeedback _feedbackPending;
};

#endif