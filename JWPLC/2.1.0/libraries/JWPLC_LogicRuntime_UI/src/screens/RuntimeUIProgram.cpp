#include "RuntimeUIProgram.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
  static const char *const MENU_LABELS[4] = {
      "PREPARAR",
      "RUN",
      "STOP",
      "VOLVER"};

  static constexpr int16_t MENU_X[4] = {2, 81, 160, 239};
  static constexpr int16_t MENU_Y = 135;
  static constexpr int16_t MENU_W = 77;
  static constexpr int16_t MENU_H = 20;

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

RuntimeUIProgram::RuntimeUIProgram()
    : _runtime(nullptr),
      _cache{},
      _fullRedraw(true),
      _selectedMenu(0),
      _messageUntilMs(0),
      _requestedView(RuntimeUIView::None),
      _requestedAction(RuntimeUIProgramAction::None),
      _feedbackPending(RuntimeUIProgramFeedback::None)
{
  invalidateDynamicCache();
}

void RuntimeUIProgram::attach(JWPLC_LogicRuntime &runtime)
{
  _runtime = &runtime;
  forceRedraw();
}

void RuntimeUIProgram::enter()
{
  _selectedMenu = 0;
  _messageUntilMs = 0;
  _requestedView = RuntimeUIView::None;
  _requestedAction = RuntimeUIProgramAction::None;
  _feedbackPending = RuntimeUIProgramFeedback::None;

  invalidateDynamicCache();
  drawStaticLayout();
  updateDynamicFields(true);
  drawMenu();

  _cache.valid = true;
  _fullRedraw = false;
}

void RuntimeUIProgram::refresh(const JWPLC_IOState *io,
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
    invalidateDynamicCache();
    drawStaticLayout();
    updateDynamicFields(true);
    drawMenu();
    _cache.valid = true;
    _fullRedraw = false;
  }

  handleInput();
  consumeFeedback();
  updateDynamicFields(false);
  restoreFooterIfDue();
}

void RuntimeUIProgram::exit()
{
  _messageUntilMs = 0;
  _requestedView = RuntimeUIView::None;
  _requestedAction = RuntimeUIProgramAction::None;
  _feedbackPending = RuntimeUIProgramFeedback::None;
}

void RuntimeUIProgram::forceRedraw()
{
  _fullRedraw = true;
  invalidateDynamicCache();
}

void RuntimeUIProgram::invalidateDynamicCache()
{
  std::memset(&_cache, 0, sizeof(_cache));
  _cache.valid = false;
}

RuntimeUIView RuntimeUIProgram::takeRequestedView()
{
  const RuntimeUIView requested = _requestedView;
  _requestedView = RuntimeUIView::None;
  return requested;
}

RuntimeUIProgramAction RuntimeUIProgram::takeRequestedAction()
{
  const RuntimeUIProgramAction requested = _requestedAction;
  _requestedAction = RuntimeUIProgramAction::None;
  return requested;
}

void RuntimeUIProgram::setFeedback(RuntimeUIProgramFeedback feedback)
{
  _feedbackPending = feedback;
}

void RuntimeUIProgram::drawStaticLayout()
{
  if (_runtime == nullptr)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "PROGRAMA");

  drawPanel(tft, 4, 28, 312, 57, "IDENTIDAD");
  drawFieldLabel(tft, 12, 43, "Nombre:");
  drawFieldLabel(tft, 12, 57, "ID / Gen:");
  drawFieldLabel(tft, 12, 71, "Bloques:");

  drawPanel(tft, 4, 89, 312, 42, "ESTADO");
  drawFieldLabel(tft, 12, 103, "Persist:");
  drawFieldLabel(tft, 12, 117, "Error:");

  drawFooter(tft, "OK: accion   Flechas: mover   ESC: IDLE");
}

void RuntimeUIProgram::updateDynamicFields(bool force)
{
  updateRuntimeBadge(force);
  updateProgramFields(force);
  updateStatusFields(force);
}

void RuntimeUIProgram::updateRuntimeBadge(bool force)
{
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

void RuntimeUIProgram::updateProgramFields(bool force)
{
  char currentProgramName[JWPLC_LOGIC_PROGRAM_NAME_BYTES + 1] = {};
  copyProgramName(currentProgramName, sizeof(currentProgramName));

  uint32_t currentProgramId = 0;
  uint32_t currentGeneration = 0;
  uint16_t currentBlockCount = 0;
  const bool currentHasIdentity =
      _runtime->storage().loadedProgramIdentity(currentProgramId,
                                                currentGeneration,
                                                currentBlockCount);

  if (!currentHasIdentity)
  {
    currentBlockCount = _runtime->blockCount();
  }

  const uint16_t currentRetentiveBlocks = _runtime->retentiveBlockCount();

  if (force || !_cache.valid ||
      std::strcmp(currentProgramName, _cache.programName) != 0)
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

  if (force || !_cache.valid ||
      currentHasIdentity != _cache.hasIdentity ||
      currentProgramId != _cache.programId ||
      currentGeneration != _cache.generation)
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

    updateTextField(JWPLC_Display.tft(), 68, 57, 40, identity);
  }

  if (force || !_cache.valid ||
      currentBlockCount != _cache.blockCount ||
      currentRetentiveBlocks != _cache.retentiveBlocks)
  {
    char blocks[40];
    std::snprintf(blocks,
                  sizeof(blocks),
                  "%u | Retentivos %u",
                  static_cast<unsigned int>(currentBlockCount),
                  static_cast<unsigned int>(currentRetentiveBlocks));
    updateTextField(JWPLC_Display.tft(), 68, 71, 40, blocks);
  }

  _cache.hasIdentity = currentHasIdentity;
  _cache.programId = currentProgramId;
  _cache.generation = currentGeneration;
  _cache.blockCount = currentBlockCount;
  _cache.retentiveBlocks = currentRetentiveBlocks;
}

void RuntimeUIProgram::updateStatusFields(bool force)
{
  const JWPLCLogicStorageBootState currentBootState =
      _runtime->storage().bootState();
  const JWPLCLogicRetentiveState currentRetentiveState =
      _runtime->retentiveState();
  const JWPLCLogicRuntimeError currentRuntimeError = _runtime->lastError();

  if (force || !_cache.valid ||
      currentBootState != _cache.bootState ||
      currentRetentiveState != _cache.retentiveState)
  {
    char persistentState[40];
    std::snprintf(persistentState,
                  sizeof(persistentState),
                  "%s | %s",
                  shortBootState(currentBootState),
                  shortRetentiveState(currentRetentiveState));
    updateTextField(JWPLC_Display.tft(),
                    68,
                    103,
                    40,
                    persistentState);
  }

  if (force || !_cache.valid || currentRuntimeError != _cache.runtimeError)
  {
    updateTextField(JWPLC_Display.tft(),
                    68,
                    117,
                    40,
                    JWPLC_LogicRuntime::errorName(currentRuntimeError));
  }

  _cache.bootState = currentBootState;
  _cache.retentiveState = currentRetentiveState;
  _cache.runtimeError = currentRuntimeError;
}

void RuntimeUIProgram::drawMenu()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  for (uint8_t index = 0; index < MENU_COUNT; ++index)
  {
    drawMenuButton(tft,
                   MENU_X[index],
                   MENU_Y,
                   MENU_W,
                   MENU_H,
                   MENU_LABELS[index],
                   index == _selectedMenu);
  }
}

void RuntimeUIProgram::redrawMenuSelection(uint8_t previousSelection,
                                           uint8_t currentSelection)
{
  if (previousSelection >= MENU_COUNT || currentSelection >= MENU_COUNT)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  drawMenuButton(tft,
                 MENU_X[previousSelection],
                 MENU_Y,
                 MENU_W,
                 MENU_H,
                 MENU_LABELS[previousSelection],
                 false);
  drawMenuButton(tft,
                 MENU_X[currentSelection],
                 MENU_Y,
                 MENU_W,
                 MENU_H,
                 MENU_LABELS[currentSelection],
                 true);
}

void RuntimeUIProgram::handleInput()
{
  uint8_t newSelection = _selectedMenu;

  if (JWPLC_Buttons.pressed(BTN_LEFT) ||
      JWPLC_Buttons.pressed(BTN_UP))
  {
    newSelection = _selectedMenu == 0
                       ? MENU_COUNT - 1
                       : static_cast<uint8_t>(_selectedMenu - 1);
  }
  else if (JWPLC_Buttons.pressed(BTN_RIGHT) ||
           JWPLC_Buttons.pressed(BTN_DOWN))
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
    requestSelectedAction();
  }
}

void RuntimeUIProgram::requestSelectedAction()
{
  switch (_selectedMenu)
  {
  case 0:
    _requestedAction = RuntimeUIProgramAction::Prepare;
    drawFooter(JWPLC_Display.tft(), "Preparando programa persistente...");
    _messageUntilMs = 0;
    break;
  case 1:
    _requestedAction = RuntimeUIProgramAction::Start;
    drawFooter(JWPLC_Display.tft(), "Solicitando RUN...");
    _messageUntilMs = 0;
    break;
  case 2:
    _requestedAction = RuntimeUIProgramAction::Stop;
    drawFooter(JWPLC_Display.tft(), "Solicitando STOP...");
    _messageUntilMs = 0;
    break;
  case 3:
  default:
    _requestedView = RuntimeUIView::Home;
    break;
  }
}

void RuntimeUIProgram::consumeFeedback()
{
  const RuntimeUIProgramFeedback feedback = _feedbackPending;
  if (feedback == RuntimeUIProgramFeedback::None)
  {
    return;
  }

  _feedbackPending = RuntimeUIProgramFeedback::None;

  const char *message = "Accion completada";
  switch (feedback)
  {
  case RuntimeUIProgramFeedback::PrepareActive:
    message = "Programa activo preparado; RUN sigue manual";
    break;
  case RuntimeUIProgramFeedback::PrepareFallback:
    message = "Fallback preparado; RUN sigue manual";
    break;
  case RuntimeUIProgramFeedback::PrepareUnformatted:
    message = "FRAM sin formato: no hay programa";
    break;
  case RuntimeUIProgramFeedback::PrepareEmpty:
    message = "Program store vacio";
    break;
  case RuntimeUIProgramFeedback::PrepareNoValid:
    message = "No existe un programa valido";
    break;
  case RuntimeUIProgramFeedback::PrepareFailed:
    message = "No se pudo preparar el programa";
    break;
  case RuntimeUIProgramFeedback::StartOk:
    message = "RUN iniciado";
    break;
  case RuntimeUIProgramFeedback::StartFailed:
    message = "RUN rechazado: revise programa/error";
    break;
  case RuntimeUIProgramFeedback::StopOk:
    message = "STOP aplicado; salidas apagadas";
    break;
  case RuntimeUIProgramFeedback::None:
  default:
    break;
  }

  drawFooter(JWPLC_Display.tft(), message);
  _messageUntilMs = millis() + 2500U;
}

void RuntimeUIProgram::restoreFooterIfDue()
{
  if (_messageUntilMs == 0)
  {
    return;
  }

  const uint32_t now = millis();
  if (static_cast<int32_t>(now - _messageUntilMs) < 0)
  {
    return;
  }

  _messageUntilMs = 0;
  drawFooter(JWPLC_Display.tft(),
             "OK: accion   Flechas: mover   ESC: IDLE");
}

void RuntimeUIProgram::copyProgramName(char *destination,
                                       size_t destinationCapacity) const
{
  if (destination == nullptr || destinationCapacity == 0)
  {
    return;
  }

  const char *source = "SIN RUNTIME";
  if (_runtime != nullptr)
  {
    const LogicProgram *loadedProgram = _runtime->program();
    source = loadedProgram && loadedProgram->name
                 ? loadedProgram->name
                 : "SIN PROGRAMA";
  }

  std::strncpy(destination, source, destinationCapacity - 1);
  destination[destinationCapacity - 1] = '\0';
}

uint16_t RuntimeUIProgram::runtimeStateColor() const
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

const char *RuntimeUIProgram::runtimeStateText() const
{
  return _runtime
             ? JWPLC_LogicRuntime::stateName(_runtime->state())
             : "SIN RUNTIME";
}