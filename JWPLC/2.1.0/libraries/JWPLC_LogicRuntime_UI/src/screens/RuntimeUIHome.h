#ifndef JWPLC_LOGIC_RUNTIME_UI_HOME_H
#define JWPLC_LOGIC_RUNTIME_UI_HOME_H

#include <Arduino.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>

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

private:
  static constexpr uint8_t MENU_COUNT = 4;

  void drawStaticLayout();
  void drawDynamicState();
  void drawMenu();
  void handleInput();
  void showSelectionMessage();

  const char *programName() const;
  uint16_t runtimeStateColor() const;
  const char *runtimeStateText() const;

  JWPLC_LogicRuntime *_runtime;
  bool _fullRedraw;
  uint8_t _selectedMenu;
  uint32_t _lastDynamicRefreshMs;
  uint32_t _messageUntilMs;
};

#endif
