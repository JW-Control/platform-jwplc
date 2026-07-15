#include "RuntimeUIHome.h"

#include <cstdio>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
  static const char *const MENU_LABELS[4] = {
      "PROGRAMA",
      "BLOQUES",
      "MEMORIA",
      "DIAGNOSTICO"};

  static constexpr int16_t MENU_X[4] = {4, 163, 4, 163};
  static constexpr int16_t MENU_Y[4] = {91, 91, 122, 122};
  static constexpr int16_t MENU_W = 153;
  static constexpr int16_t MENU_H = 26;

  const char *shortBootState(JWPLCLogicStorageBootState state)
  {
    switch (state)
    {
    case JWPLCLogicStorageBootState::NotEvaluated:
      return "NO EVAL";
    case JWPLCLogicStorageBootState::NotReady:
      return "NO LISTO";
    case JWPLCLogicStorageBootState::Unformatted:
      return "SIN FORMATO";
    case JWPLCLogicStorageBootState::Empty:
      return "VACIO";
    case JWPLCLogicStorageBootState::ActiveProgramLoaded:
      return "ACTIVO";
    case JWPLCLogicStorageBootState::FallbackProgramLoaded:
      return "FALLBACK";
    case JWPLCLogicStorageBootState::NoValidProgram:
      return "SIN PROG";
    case JWPLCLogicStorageBootState::InvalidProgram:
      return "PROG INVALIDO";
    case JWPLCLogicStorageBootState::CorruptMetadata:
      return "META CORRUPTA";
    default:
      return "DESCONOCIDO";
    }
  }

  const char *shortRetentiveState(JWPLCLogicRetentiveState state)
  {
    switch (state)
    {
    case JWPLCLogicRetentiveState::NotEvaluated:
      return "RET -";
    case JWPLCLogicRetentiveState::NotReady:
      return "RET NO LISTO";
    case JWPLCLogicRetentiveState::NoStoredProgram:
      return "RET SIN PROG";
    case JWPLCLogicRetentiveState::NoRetentiveBlocks:
      return "SIN RET";
    case JWPLCLogicRetentiveState::NoSnapshot:
      return "SIN SNAP";
    case JWPLCLogicRetentiveState::Restored:
      return "RET OK";
    case JWPLCLogicRetentiveState::Saved:
      return "RET GUARD";
    case JWPLCLogicRetentiveState::NoMatchingSnapshot:
      return "SNAP DIST";
    case JWPLCLogicRetentiveState::StoreError:
      return "RET ERROR";
    default:
      return "RET ?";
    }
  }
}

RuntimeUIHome::RuntimeUIHome()
    : _runtime(nullptr),
      _fullRedraw(true),
      _selectedMenu(0),
      _lastDynamicRefreshMs(0),
      _messageUntilMs(0)
{
}

void RuntimeUIHome::attach(JWPLC_LogicRuntime &runtime)
{
  _runtime = &runtime;
  forceRedraw();
}

void RuntimeUIHome::enter()
{
  _selectedMenu = 0;
  _messageUntilMs = 0;
  drawStaticLayout();
  drawDynamicState();
  drawMenu();
  _fullRedraw = false;
  _lastDynamicRefreshMs = millis();
}

void RuntimeUIHome::refresh(const JWPLC_IOState *io,
                            const JWPLC_RTCState *rtc)
{
  (void)io;
  (void)rtc;

  if (_runtime == nullptr)
  {
    return;
  }

  if (_fullRedraw)
  {
    drawStaticLayout();
    drawDynamicState();
    drawMenu();
    _fullRedraw = false;
    _lastDynamicRefreshMs = millis();
  }

  handleInput();

  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - _lastDynamicRefreshMs) >= 200U)
  {
    _lastDynamicRefreshMs = now;
    drawDynamicState();
  }

  if (_messageUntilMs != 0 &&
      static_cast<int32_t>(now - _messageUntilMs) >= 0)
  {
    _messageUntilMs = 0;
    drawFooter(JWPLC_Display.tft(), "Flechas: mover   OK: abrir   ESC: IDLE");
  }
}

void RuntimeUIHome::exit()
{
  _messageUntilMs = 0;
}

void RuntimeUIHome::forceRedraw()
{
  _fullRedraw = true;
  _lastDynamicRefreshMs = 0;
}

void RuntimeUIHome::drawStaticLayout()
{
  if (_runtime == nullptr)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeader(tft,
             "JWPLC LOGIC",
             runtimeStateText(),
             runtimeStateColor());
  drawPanel(tft, 4, 28, 312, 57, "ESTADO DEL PROGRAMA");
  drawFooter(tft, "Flechas: mover   OK: abrir   ESC: IDLE");
}

void RuntimeUIHome::drawDynamicState()
{
  if (_runtime == nullptr)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  drawHeader(tft,
             "JWPLC LOGIC",
             runtimeStateText(),
             runtimeStateColor());

  char identity[48] = "-";
  char blocksAndScan[64] = "-";
  char storageState[48] = "-";

  uint32_t programId = 0;
  uint32_t generation = 0;
  uint16_t blockCount = 0;
  const bool hasIdentity =
      _runtime->storage().loadedProgramIdentity(programId,
                                                generation,
                                                blockCount);

  if (hasIdentity)
  {
    snprintf(identity,
             sizeof(identity),
             "%lu / %lu",
             static_cast<unsigned long>(programId),
             static_cast<unsigned long>(generation));
  }
  else
  {
    snprintf(identity, sizeof(identity), "SIN IDENTIDAD");
  }

  snprintf(blocksAndScan,
           sizeof(blocksAndScan),
           "%u  Scan %lu/%lu us",
           static_cast<unsigned int>(blockCount),
           static_cast<unsigned long>(_runtime->averageScanMicros()),
           static_cast<unsigned long>(_runtime->maxScanMicros()));

  snprintf(storageState,
           sizeof(storageState),
           "%s | %s",
           shortBootState(_runtime->storage().bootState()),
           shortRetentiveState(_runtime->retentiveState()));

  drawLabelValue(tft, 12, 43, "Programa:", programName(), 296);
  drawLabelValue(tft, 12, 57, "ID / Gen:", identity, 296);
  drawLabelValue(tft, 12, 71, "Bloques:", blocksAndScan, 296);

  tft.fillRect(128, 30, 181, 10, COLOR_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_MUTED, COLOR_PANEL);
  tft.setCursor(128, 32);
  tft.print(storageState);
}

void RuntimeUIHome::drawMenu()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();

  for (uint8_t index = 0; index < MENU_COUNT; ++index)
  {
    drawMenuButton(tft,
                   MENU_X[index],
                   MENU_Y[index],
                   MENU_W,
                   MENU_H,
                   MENU_LABELS[index],
                   index == _selectedMenu);
  }
}

void RuntimeUIHome::handleInput()
{
  uint8_t newSelection = _selectedMenu;

  if (JWPLC_Buttons.pressed(BTN_UP) ||
      JWPLC_Buttons.pressed(BTN_LEFT))
  {
    newSelection = _selectedMenu == 0
                       ? MENU_COUNT - 1
                       : static_cast<uint8_t>(_selectedMenu - 1);
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN) ||
           JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    newSelection = static_cast<uint8_t>((_selectedMenu + 1) % MENU_COUNT);
  }

  if (newSelection != _selectedMenu)
  {
    _selectedMenu = newSelection;
    JWPLC_Display.notifyActivity();
    drawMenu();
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    JWPLC_Display.notifyActivity();
    showSelectionMessage();
  }
}

void RuntimeUIHome::showSelectionMessage()
{
  char message[64];
  snprintf(message,
           sizeof(message),
           "%s: siguiente modulo de la interfaz",
           MENU_LABELS[_selectedMenu]);

  drawFooter(JWPLC_Display.tft(), message);
  _messageUntilMs = millis() + 1500U;
}

const char *RuntimeUIHome::programName() const
{
  if (_runtime == nullptr)
  {
    return "SIN RUNTIME";
  }

  if (_runtime->storage().hasLoadedProgram())
  {
    const LogicProgram program = _runtime->storage().activeProgram();
    return program.name ? program.name : "PROGRAMA PERSISTENTE";
  }

  return _runtime->hasProgram() ? "PROGRAMA EN RAM" : "SIN PROGRAMA";
}

uint16_t RuntimeUIHome::runtimeStateColor() const
{
  if (_runtime == nullptr)
  {
    return COLOR_MUTED;
  }

  switch (_runtime->state())
  {
  case JWPLCLogicRuntimeState::Running:
    return COLOR_OK;
  case JWPLCLogicRuntimeState::Fault:
    return COLOR_ERROR;
  case JWPLCLogicRuntimeState::Stopped:
    return COLOR_WARNING;
  case JWPLCLogicRuntimeState::Ready:
  default:
    return COLOR_ACCENT;
  }
}

const char *RuntimeUIHome::runtimeStateText() const
{
  return _runtime
             ? JWPLC_LogicRuntime::stateName(_runtime->state())
             : "SIN RUNTIME";
}
