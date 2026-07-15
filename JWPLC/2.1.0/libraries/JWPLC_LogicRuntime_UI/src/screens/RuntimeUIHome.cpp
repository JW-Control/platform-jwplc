#include "RuntimeUIHome.h"

#include <cstdio>
#include <cstring>

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
      _cache{},
      _fullRedraw(true),
      _selectedMenu(0),
      _lastScanRefreshMs(0),
      _messageUntilMs(0),
      _requestedView(RuntimeUIView::None)
{
  invalidateCache();
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
  _requestedView = RuntimeUIView::None;

  invalidateCache();
  drawStaticLayout();
  updateImmediateFields(true);
  drawMenu();

  _cache.valid = true;
  _fullRedraw = false;
  _lastScanRefreshMs = millis();
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
    invalidateCache();
    drawStaticLayout();
    updateImmediateFields(true);
    drawMenu();

    _cache.valid = true;
    _fullRedraw = false;
    _lastScanRefreshMs = millis();
  }

  handleInput();
  updateImmediateFields(false);

  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - _lastScanRefreshMs) >= SCAN_REFRESH_MS)
  {
    _lastScanRefreshMs = now;
    updateScanField(false);
  }

  if (_messageUntilMs != 0 &&
      static_cast<int32_t>(now - _messageUntilMs) >= 0)
  {
    _messageUntilMs = 0;
    drawFooter(JWPLC_Display.tft(),
               "Flechas: mover   OK: abrir   ESC: IDLE");
  }
}

void RuntimeUIHome::exit()
{
  _messageUntilMs = 0;
  _requestedView = RuntimeUIView::None;
}

void RuntimeUIHome::forceRedraw()
{
  _fullRedraw = true;
  _lastScanRefreshMs = 0;
  invalidateCache();
}

RuntimeUIView RuntimeUIHome::takeRequestedView()
{
  const RuntimeUIView requested = _requestedView;
  _requestedView = RuntimeUIView::None;
  return requested;
}

void RuntimeUIHome::invalidateCache()
{
  std::memset(&_cache, 0, sizeof(_cache));
  _cache.valid = false;
}

void RuntimeUIHome::drawStaticLayout()
{
  if (_runtime == nullptr)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "JWPLC LOGIC");
  drawPanel(tft, 4, 28, 312, 57, "ESTADO DEL PROGRAMA");

  drawFieldLabel(tft, 12, 43, "Programa:");
  drawFieldLabel(tft, 12, 57, "ID / Gen:");
  drawFieldLabel(tft, 12, 71, "Bloques:");

  drawFooter(tft, "Flechas: mover   OK: abrir   ESC: IDLE");
}

void RuntimeUIHome::updateImmediateFields(bool force)
{
  updateRuntimeBadge(force);
  const bool blockCountChanged = updateProgramFields(force);
  updateStorageField(force);

  if (force || blockCountChanged)
  {
    updateScanField(true);
  }
}

void RuntimeUIHome::updateRuntimeBadge(bool force)
{
  if (_runtime == nullptr)
  {
    return;
  }

  const JWPLCLogicRuntimeState currentState = _runtime->state();
  if (!force && _cache.valid && currentState == _cache.runtimeState)
  {
    return;
  }

  updateHeaderState(JWPLC_Display.tft(),
                    runtimeStateText(),
                    runtimeStateColor());
  _cache.runtimeState = currentState;
}

bool RuntimeUIHome::updateProgramFields(bool force)
{
  if (_runtime == nullptr)
  {
    return false;
  }

  char currentProgramName[JWPLC_LOGIC_PROGRAM_NAME_BYTES + 1] = {};
  copyProgramName(currentProgramName, sizeof(currentProgramName));

  uint32_t currentProgramId = 0;
  uint32_t currentGeneration = 0;
  uint16_t currentBlockCount = 0;
  const bool currentHasIdentity =
      _runtime->storage().loadedProgramIdentity(currentProgramId,
                                                currentGeneration,
                                                currentBlockCount);

  const bool programNameChanged =
      force || !_cache.valid ||
      std::strcmp(currentProgramName, _cache.programName) != 0;

  if (programNameChanged)
  {
    updateTextField(JWPLC_Display.tft(),
                    68,
                    43,
                    40,
                    currentProgramName);
    std::strncpy(_cache.programName,
                 currentProgramName,
                 sizeof(_cache.programName) - 1);
    _cache.programName[sizeof(_cache.programName) - 1] = '\0';
  }

  const bool identityChanged =
      force || !_cache.valid ||
      currentHasIdentity != _cache.hasIdentity ||
      currentProgramId != _cache.programId ||
      currentGeneration != _cache.generation;

  if (identityChanged)
  {
    char identity[40];
    if (currentHasIdentity)
    {
      std::snprintf(identity,
                    sizeof(identity),
                    "%lu / %lu",
                    static_cast<unsigned long>(currentProgramId),
                    static_cast<unsigned long>(currentGeneration));
    }
    else
    {
      std::snprintf(identity, sizeof(identity), "SIN IDENTIDAD");
    }

    updateTextField(JWPLC_Display.tft(),
                    68,
                    57,
                    40,
                    identity);
  }

  const bool blockCountChanged =
      force || !_cache.valid || currentBlockCount != _cache.blockCount;

  _cache.hasIdentity = currentHasIdentity;
  _cache.programId = currentProgramId;
  _cache.generation = currentGeneration;
  _cache.blockCount = currentBlockCount;

  return blockCountChanged;
}

void RuntimeUIHome::updateStorageField(bool force)
{
  if (_runtime == nullptr)
  {
    return;
  }

  const JWPLCLogicStorageBootState currentBootState =
      _runtime->storage().bootState();
  const JWPLCLogicRetentiveState currentRetentiveState =
      _runtime->retentiveState();

  const bool changed =
      force || !_cache.valid ||
      currentBootState != _cache.bootState ||
      currentRetentiveState != _cache.retentiveState;

  if (!changed)
  {
    return;
  }

  char storageState[40];
  std::snprintf(storageState,
                sizeof(storageState),
                "%s | %s",
                shortBootState(currentBootState),
                shortRetentiveState(currentRetentiveState));

  updateTextField(JWPLC_Display.tft(),
                  128,
                  32,
                  30,
                  storageState,
                  COLOR_MUTED,
                  COLOR_PANEL);

  _cache.bootState = currentBootState;
  _cache.retentiveState = currentRetentiveState;
}

void RuntimeUIHome::updateScanField(bool force)
{
  if (_runtime == nullptr)
  {
    return;
  }

  const uint32_t currentAverageScanUs = _runtime->averageScanMicros();
  const uint32_t currentMaxScanUs = _runtime->maxScanMicros();

  const bool changed =
      force || !_cache.valid ||
      currentAverageScanUs != _cache.averageScanUs ||
      currentMaxScanUs != _cache.maxScanUs;

  if (!changed)
  {
    return;
  }

  char blocksAndScan[48];
  std::snprintf(blocksAndScan,
                sizeof(blocksAndScan),
                "%u  Scan %lu/%lu us",
                static_cast<unsigned int>(_cache.blockCount),
                static_cast<unsigned long>(currentAverageScanUs),
                static_cast<unsigned long>(currentMaxScanUs));

  updateTextField(JWPLC_Display.tft(),
                  68,
                  71,
                  40,
                  blocksAndScan);

  _cache.averageScanUs = currentAverageScanUs;
  _cache.maxScanUs = currentMaxScanUs;
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

void RuntimeUIHome::redrawMenuSelection(uint8_t previousSelection,
                                        uint8_t currentSelection)
{
  if (previousSelection >= MENU_COUNT || currentSelection >= MENU_COUNT)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  drawMenuButton(tft,
                 MENU_X[previousSelection],
                 MENU_Y[previousSelection],
                 MENU_W,
                 MENU_H,
                 MENU_LABELS[previousSelection],
                 false);
  drawMenuButton(tft,
                 MENU_X[currentSelection],
                 MENU_Y[currentSelection],
                 MENU_W,
                 MENU_H,
                 MENU_LABELS[currentSelection],
                 true);
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
    const uint8_t previousSelection = _selectedMenu;
    _selectedMenu = newSelection;
    JWPLC_Display.notifyActivity();
    redrawMenuSelection(previousSelection, _selectedMenu);
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    JWPLC_Display.notifyActivity();
    requestSelectedView();
  }
}

void RuntimeUIHome::requestSelectedView()
{
  switch (_selectedMenu)
  {
  case 0:
    _requestedView = RuntimeUIView::Program;
    return;
  case 1:
  case 2:
  case 3:
  default:
    showPendingMessage();
    return;
  }
}

void RuntimeUIHome::showPendingMessage()
{
  char message[64];
  std::snprintf(message,
                sizeof(message),
                "%s: siguiente modulo de la interfaz",
                MENU_LABELS[_selectedMenu]);

  drawFooter(JWPLC_Display.tft(), message);
  _messageUntilMs = millis() + 1500U;
}

void RuntimeUIHome::copyProgramName(char *destination,
                                    size_t destinationCapacity) const
{
  if (destination == nullptr || destinationCapacity == 0)
  {
    return;
  }

  const char *source = "SIN RUNTIME";

  if (_runtime != nullptr)
  {
    if (_runtime->storage().hasLoadedProgram())
    {
      const LogicProgram program = _runtime->storage().activeProgram();
      source = program.name ? program.name : "PROGRAMA PERSISTENTE";
    }
    else
    {
      source = _runtime->hasProgram() ? "PROGRAMA EN RAM" : "SIN PROGRAMA";
    }
  }

  std::strncpy(destination, source, destinationCapacity - 1);
  destination[destinationCapacity - 1] = '\0';
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