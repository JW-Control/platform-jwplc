#ifndef JWPLC_LOGIC_RUNTIME_UI_HOME_H
#define JWPLC_LOGIC_RUNTIME_UI_HOME_H

#include <Arduino.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>

#include "../RuntimeUIView.h"

class RuntimeUIHome
{
public:
  RuntimeUIHome();

  void attach(JWPLC_LogicRuntime &runtime);
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();

  /** Devuelve y consume la vista solicitada mediante OK. */
  RuntimeUIView takeRequestedView();

private:
  static constexpr uint8_t MENU_COUNT = 4;
  static constexpr uint32_t SCAN_REFRESH_MS = 1000;

  struct DynamicCache
  {
    bool valid;
    JWPLCLogicRuntimeState runtimeState;
    JWPLCLogicStorageBootState bootState;
    JWPLCLogicRetentiveState retentiveState;
    bool hasIdentity;
    uint32_t programId;
    uint32_t generation;
    uint16_t blockCount;
    uint32_t averageScanUs;
    uint32_t maxScanUs;
    char programName[JWPLC_LOGIC_PROGRAM_NAME_BYTES + 1];
  };

  void invalidateCache();
  void drawStaticLayout();
  void updateImmediateFields(bool force);
  void updateRuntimeBadge(bool force);
  bool updateProgramFields(bool force);
  void updateStorageField(bool force);
  void updateScanField(bool force);

  void drawMenu();
  void redrawMenuSelection(uint8_t previousSelection,
                           uint8_t currentSelection);
  void handleInput();
  void requestSelectedView();
  void showPendingMessage();

  void copyProgramName(char *destination,
                       size_t destinationCapacity) const;
  uint16_t runtimeStateColor() const;
  const char *runtimeStateText() const;

  JWPLC_LogicRuntime *_runtime;
  DynamicCache _cache;
  bool _fullRedraw;
  uint8_t _selectedMenu;
  uint32_t _lastScanRefreshMs;
  uint32_t _messageUntilMs;
  RuntimeUIView _requestedView;
};

#endif