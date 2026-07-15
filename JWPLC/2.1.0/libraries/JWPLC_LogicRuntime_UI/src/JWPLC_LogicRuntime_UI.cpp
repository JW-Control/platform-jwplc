#include "JWPLC_LogicRuntime_UI.h"

JWPLC_LogicRuntime_UIClass JWPLC_LogicRuntime_UI;

JWPLC_LogicRuntime_UIClass::JWPLC_LogicRuntime_UIClass()
    : _runtime(nullptr),
      _attached(false),
      _userVisible(false),
      _lastRunLed(false),
      _lastErrLed(false),
      _home()
{
}

bool JWPLC_LogicRuntime_UIClass::begin(JWPLC_LogicRuntime &runtime)
{
  _runtime = &runtime;
  _attached = true;
  _userVisible = false;
  _home.attach(runtime);

  // La UI del runtime reserva ESC como retorno seguro hacia IDLE.
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
  JWPLC_Display.setUserRefreshPeriodMs(50);
  JWPLC_Display.clearPendingInput();

  const bool runLed = runtime.state() == JWPLCLogicRuntimeState::Running;
  const bool errLed = criticalErrorActive();
  JWPLC_Display.setRunLed(runLed);
  JWPLC_Display.setErrLed(errLed);
  _lastRunLed = runLed;
  _lastErrLed = errLed;
  return true;
}

void JWPLC_LogicRuntime_UIClass::update()
{
  if (!_attached || _runtime == nullptr)
  {
    return;
  }

  syncIdleIndicators();
}

void JWPLC_LogicRuntime_UIClass::end()
{
  _home.exit();
  _runtime = nullptr;
  _attached = false;
  _userVisible = false;
}

bool JWPLC_LogicRuntime_UIClass::isAttached() const
{
  return _attached && _runtime != nullptr;
}

JWPLC_LogicRuntime *JWPLC_LogicRuntime_UIClass::runtime()
{
  return _runtime;
}

const JWPLC_LogicRuntime *JWPLC_LogicRuntime_UIClass::runtime() const
{
  return _runtime;
}

void JWPLC_LogicRuntime_UIClass::forceRedraw()
{
  _home.forceRedraw();

  if (JWPLC_Display.isReady())
  {
    JWPLC_Display.forceRedraw();
  }
}

void JWPLC_LogicRuntime_UIClass::onDisplayEnter()
{
  if (!isAttached())
  {
    return;
  }

  _userVisible = true;
  syncIdleIndicators();
  _home.enter();
}

void JWPLC_LogicRuntime_UIClass::onDisplayRefresh(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
  if (!isAttached())
  {
    return;
  }

  syncIdleIndicators();
  _home.refresh(io, rtc);
}

void JWPLC_LogicRuntime_UIClass::onDisplayExit()
{
  if (!isAttached())
  {
    return;
  }

  _home.exit();
  _userVisible = false;
}

bool JWPLC_LogicRuntime_UIClass::criticalErrorActive() const
{
  if (_runtime == nullptr)
  {
    return false;
  }

  if (_runtime->state() == JWPLCLogicRuntimeState::Fault)
  {
    return true;
  }

  switch (_runtime->lastError())
  {
  case JWPLCLogicRuntimeError::UnsupportedStorage:
  case JWPLCLogicRuntimeError::IOInitializationFailed:
  case JWPLCLogicRuntimeError::InvalidProgram:
  case JWPLCLogicRuntimeError::ProgramExecutionFailed:
    return true;
  default:
    break;
  }

  if (_runtime->storage().metadataHealth() ==
      LogicSuperblockHealth::CorruptMetadata)
  {
    return true;
  }

  switch (_runtime->storage().bootState())
  {
  case JWPLCLogicStorageBootState::NoValidProgram:
  case JWPLCLogicStorageBootState::InvalidProgram:
  case JWPLCLogicStorageBootState::CorruptMetadata:
    return true;
  default:
    return false;
  }
}

void JWPLC_LogicRuntime_UIClass::syncIdleIndicators()
{
  const bool runLed =
      _runtime->state() == JWPLCLogicRuntimeState::Running;
  const bool errLed = criticalErrorActive();

  if (runLed != _lastRunLed)
  {
    JWPLC_Display.setRunLed(runLed);
    _lastRunLed = runLed;
  }

  if (errLed != _lastErrLed)
  {
    JWPLC_Display.setErrLed(errLed);
    _lastErrLed = errLed;
  }
}

// JWPLC_Display conserva callbacks weak para sketches normales. Al incluir esta
// librería, estas definiciones fuertes enrutan USER hacia la UI modular.
extern "C" void jwplcUserDisplayEnterCallback(void)
{
  JWPLC_LogicRuntime_UI.onDisplayEnter();
}

extern "C" void jwplcUserDisplayRefreshCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
  JWPLC_LogicRuntime_UI.onDisplayRefresh(io, rtc);
}

extern "C" void jwplcUserDisplayExitCallback(void)
{
  JWPLC_LogicRuntime_UI.onDisplayExit();
}
