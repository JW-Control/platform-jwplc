#include "JWPLC_LogicRuntime_UI.h"

JWPLC_LogicRuntime_UIClass JWPLC_LogicRuntime_UI;

JWPLC_LogicRuntime_UIClass::JWPLC_LogicRuntime_UIClass()
    : _backend(Backend::None),
      _runtime(nullptr),
      _v2Engine(nullptr),
      _attached(false),
      _userVisible(false),
      _lastRunLed(false),
      _lastErrLed(false),
      _currentView(RuntimeUIView::Home),
      _pendingProgramAction(RuntimeUIProgramAction::None),
      _home(),
      _program(),
      _diagram(),
      _blocks(),
      _v2Model(),
      _fbdMapV2()
{
}

bool JWPLC_LogicRuntime_UIClass::begin(JWPLC_LogicRuntime &runtime)
{
  _backend = Backend::RuntimeV1;
  _runtime = &runtime;
  _v2Engine = nullptr;
  _v2Model.detach();
  _fbdMapV2.detach();
  _attached = true;
  _userVisible = false;
  _currentView = RuntimeUIView::Home;
  _pendingProgramAction = RuntimeUIProgramAction::None;

  _home.attach(runtime);
  _program.attach(runtime);
  _diagram.attach(runtime);
  _blocks.attach(runtime);

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

bool JWPLC_LogicRuntime_UIClass::begin(LogicV2EnginePrototype &engine)
{
  _backend = Backend::EngineV2;
  _runtime = nullptr;
  _v2Engine = &engine;
  _v2Model.attach(engine);
  _fbdMapV2.attach(_v2Model);
  _attached = true;
  _userVisible = false;
  _currentView = RuntimeUIView::Diagram;
  _pendingProgramAction = RuntimeUIProgramAction::None;

  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
  JWPLC_Display.setUserRefreshPeriodMs(50);
  JWPLC_Display.clearPendingInput();

  const bool runLed = engine.state() == LogicV2EngineState::Running;
  const bool errLed = criticalErrorActive();
  JWPLC_Display.setRunLed(runLed);
  JWPLC_Display.setErrLed(errLed);
  _lastRunLed = runLed;
  _lastErrLed = errLed;
  return true;
}

void JWPLC_LogicRuntime_UIClass::update()
{
  if (!isAttached())
  {
    return;
  }

  if (_backend == Backend::RuntimeV1)
  {
    // Las operaciones de FRAM y control del runtime se ejecutan aquí, fuera del
    // callback gráfico que mantiene adquirido el bus SPI de la TFT.
    processPendingProgramAction();
  }

  syncIdleIndicators();
}

void JWPLC_LogicRuntime_UIClass::end()
{
  if (_backend == Backend::EngineV2)
  {
    _fbdMapV2.exit();
    _fbdMapV2.detach();
    _v2Model.detach();
  }
  else if (_backend == Backend::RuntimeV1)
  {
    exitCurrentView();
  }

  _backend = Backend::None;
  _runtime = nullptr;
  _v2Engine = nullptr;
  _attached = false;
  _userVisible = false;
  _currentView = RuntimeUIView::Home;
  _pendingProgramAction = RuntimeUIProgramAction::None;
}

bool JWPLC_LogicRuntime_UIClass::isAttached() const
{
  if (!_attached)
  {
    return false;
  }

  if (_backend == Backend::RuntimeV1)
  {
    return _runtime != nullptr;
  }
  if (_backend == Backend::EngineV2)
  {
    return _v2Engine != nullptr && _v2Model.isAttached();
  }
  return false;
}

JWPLC_LogicRuntime *JWPLC_LogicRuntime_UIClass::runtime()
{
  return _runtime;
}

const JWPLC_LogicRuntime *JWPLC_LogicRuntime_UIClass::runtime() const
{
  return _runtime;
}

LogicV2EnginePrototype *JWPLC_LogicRuntime_UIClass::v2Engine()
{
  return _v2Engine;
}

const LogicV2EnginePrototype *JWPLC_LogicRuntime_UIClass::v2Engine() const
{
  return _v2Engine;
}

void JWPLC_LogicRuntime_UIClass::forceRedraw()
{
  if (_backend == Backend::EngineV2)
  {
    _fbdMapV2.forceRedraw();
  }
  else
  {
    switch (_currentView)
    {
    case RuntimeUIView::Program:
      _program.forceRedraw();
      break;
    case RuntimeUIView::Diagram:
      _diagram.forceRedraw();
      break;
    case RuntimeUIView::Blocks:
      _blocks.forceRedraw();
      break;
    case RuntimeUIView::Home:
    default:
      _home.forceRedraw();
      break;
    }
  }

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
  _pendingProgramAction = RuntimeUIProgramAction::None;
  syncIdleIndicators();

  if (_backend == Backend::EngineV2)
  {
    _currentView = RuntimeUIView::Diagram;
    _fbdMapV2.enter();
    return;
  }

  _currentView = RuntimeUIView::Home;
  enterCurrentView();
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
  if (_backend == Backend::EngineV2)
  {
    _fbdMapV2.refresh(io, rtc);
    return;
  }

  refreshCurrentView(io, rtc);
  collectViewRequests();
}

void JWPLC_LogicRuntime_UIClass::onDisplayExit()
{
  if (!isAttached())
  {
    return;
  }

  if (_backend == Backend::EngineV2)
  {
    _fbdMapV2.exit();
    _currentView = RuntimeUIView::Diagram;
  }
  else
  {
    exitCurrentView();
    _currentView = RuntimeUIView::Home;
  }

  _pendingProgramAction = RuntimeUIProgramAction::None;
  _userVisible = false;
}

bool JWPLC_LogicRuntime_UIClass::displayRefreshNeeded(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc) const
{
  (void)io;
  (void)rtc;

  if (!isAttached() || !_userVisible)
  {
    return false;
  }

  if (_backend == Backend::EngineV2)
  {
    return _fbdMapV2.needsTftRefresh();
  }

  // Las vistas v1 todavía combinan entrada y render en refresh(). Se conserva
  // su comportamiento histórico hasta migrarlas a un contrato dirty-region.
  return true;
}

void JWPLC_LogicRuntime_UIClass::switchView(RuntimeUIView nextView)
{
  if (_backend != Backend::RuntimeV1 ||
      nextView == RuntimeUIView::None ||
      nextView == _currentView)
  {
    return;
  }

  exitCurrentView();
  _currentView = nextView;
  JWPLC_Display.clearPendingInput();
  enterCurrentView();
}

void JWPLC_LogicRuntime_UIClass::enterCurrentView()
{
  switch (_currentView)
  {
  case RuntimeUIView::Program:
    _program.enter();
    break;
  case RuntimeUIView::Diagram:
    _diagram.enter();
    break;
  case RuntimeUIView::Blocks:
    _blocks.enter();
    break;
  case RuntimeUIView::Home:
  default:
    _currentView = RuntimeUIView::Home;
    _home.enter();
    break;
  }
}

void JWPLC_LogicRuntime_UIClass::refreshCurrentView(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
  switch (_currentView)
  {
  case RuntimeUIView::Program:
    _program.refresh(io, rtc);
    break;
  case RuntimeUIView::Diagram:
    _diagram.refresh(io, rtc);
    break;
  case RuntimeUIView::Blocks:
    _blocks.refresh(io, rtc);
    break;
  case RuntimeUIView::Home:
  default:
    _home.refresh(io, rtc);
    break;
  }
}

void JWPLC_LogicRuntime_UIClass::exitCurrentView()
{
  switch (_currentView)
  {
  case RuntimeUIView::Program:
    _program.exit();
    break;
  case RuntimeUIView::Diagram:
    _diagram.exit();
    break;
  case RuntimeUIView::Blocks:
    _blocks.exit();
    break;
  case RuntimeUIView::Home:
  default:
    _home.exit();
    break;
  }
}

void JWPLC_LogicRuntime_UIClass::collectViewRequests()
{
  switch (_currentView)
  {
  case RuntimeUIView::Home:
  {
    const RuntimeUIView requested = _home.takeRequestedView();
    if (requested == RuntimeUIView::Program)
    {
      switchView(RuntimeUIView::Program);
    }
    else if (requested == RuntimeUIView::Blocks)
    {
      switchView(RuntimeUIView::Diagram);
    }
    break;
  }

  case RuntimeUIView::Program:
  {
    const RuntimeUIProgramAction action = _program.takeRequestedAction();
    if (action != RuntimeUIProgramAction::None &&
        _pendingProgramAction == RuntimeUIProgramAction::None)
    {
      _pendingProgramAction = action;
    }

    const RuntimeUIView requested = _program.takeRequestedView();
    if (requested == RuntimeUIView::Home)
    {
      switchView(RuntimeUIView::Home);
    }
    break;
  }

  case RuntimeUIView::Diagram:
  {
    const RuntimeUIView requested = _diagram.takeRequestedView();
    if (requested == RuntimeUIView::Blocks)
    {
      switchView(RuntimeUIView::Blocks);
    }
    else if (requested == RuntimeUIView::Home)
    {
      switchView(RuntimeUIView::Home);
    }
    break;
  }

  case RuntimeUIView::Blocks:
  {
    const RuntimeUIView requested = _blocks.takeRequestedView();
    if (requested == RuntimeUIView::Home)
    {
      switchView(RuntimeUIView::Diagram);
    }
    break;
  }

  default:
    break;
  }
}

void JWPLC_LogicRuntime_UIClass::processPendingProgramAction()
{
  if (_backend != Backend::RuntimeV1 || _runtime == nullptr)
  {
    return;
  }

  const RuntimeUIProgramAction action = _pendingProgramAction;
  if (action == RuntimeUIProgramAction::None)
  {
    return;
  }

  _pendingProgramAction = RuntimeUIProgramAction::None;

  if (!_userVisible || _currentView != RuntimeUIView::Program)
  {
    return;
  }

  RuntimeUIProgramFeedback feedback = RuntimeUIProgramFeedback::PrepareFailed;

  switch (action)
  {
  case RuntimeUIProgramAction::Prepare:
  {
    const JWPLCLogicStorageBootState bootState =
        _runtime->prepareStoredProgram();

    switch (bootState)
    {
    case JWPLCLogicStorageBootState::ActiveProgramLoaded:
      _runtime->restoreStoredRetentiveState();
      feedback = RuntimeUIProgramFeedback::PrepareActive;
      break;
    case JWPLCLogicStorageBootState::FallbackProgramLoaded:
      _runtime->restoreStoredRetentiveState();
      feedback = RuntimeUIProgramFeedback::PrepareFallback;
      break;
    case JWPLCLogicStorageBootState::Unformatted:
      feedback = RuntimeUIProgramFeedback::PrepareUnformatted;
      break;
    case JWPLCLogicStorageBootState::Empty:
      feedback = RuntimeUIProgramFeedback::PrepareEmpty;
      break;
    case JWPLCLogicStorageBootState::NoValidProgram:
    case JWPLCLogicStorageBootState::InvalidProgram:
    case JWPLCLogicStorageBootState::CorruptMetadata:
      feedback = RuntimeUIProgramFeedback::PrepareNoValid;
      break;
    case JWPLCLogicStorageBootState::NotEvaluated:
    case JWPLCLogicStorageBootState::NotReady:
    default:
      feedback = RuntimeUIProgramFeedback::PrepareFailed;
      break;
    }
    break;
  }

  case RuntimeUIProgramAction::Start:
    feedback = _runtime->start()
                   ? RuntimeUIProgramFeedback::StartOk
                   : RuntimeUIProgramFeedback::StartFailed;
    break;

  case RuntimeUIProgramAction::Stop:
    _runtime->stop();
    feedback = RuntimeUIProgramFeedback::StopOk;
    break;

  case RuntimeUIProgramAction::None:
  default:
    return;
  }

  _program.setFeedback(feedback);
}

bool JWPLC_LogicRuntime_UIClass::criticalErrorActive() const
{
  if (_backend == Backend::EngineV2)
  {
    return _v2Engine != nullptr &&
           _v2Engine->state() == LogicV2EngineState::Fault;
  }

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
  bool runLed = false;
  if (_backend == Backend::EngineV2)
  {
    runLed = _v2Engine != nullptr &&
             _v2Engine->state() == LogicV2EngineState::Running;
  }
  else if (_backend == Backend::RuntimeV1)
  {
    runLed = _runtime != nullptr &&
             _runtime->state() == JWPLCLogicRuntimeState::Running;
  }

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
// librería, estas definiciones fuertes enrutan USER hacia la UI modular. Se
// mantienen en el mismo objeto que JWPLC_LogicRuntime_UI para garantizar que el
// linker Arduino extraiga también la compuerta previa al SPI.
extern "C" void jwplcUserDisplayEnterCallback(void)
{
  JWPLC_LogicRuntime_UI.onDisplayEnter();
}

extern "C" bool jwplcUserDisplayRefreshNeededCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
  return JWPLC_LogicRuntime_UI.displayRefreshNeeded(io, rtc);
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
