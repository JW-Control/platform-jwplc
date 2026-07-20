#include "RuntimeUIFBDMapUnified.h"

#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

RuntimeUIFBDMapUnified::RuntimeUIFBDMapUnified()
    : _model(nullptr),
      _editSession(),
      _view(View::Map),
      _previousView(View::Map),
      _attached(false),
      _visible(false),
      _forceRedraw(true),
      _contentDirty(true),
      _headerDirty(true),
      _inputReleaseGate(false),
      _selectedIndex(0),
      _detailInputIndex(0),
      _detailParameterIndex(0),
      _detailFocus(DetailFocus::Inputs),
      _tonDraft{},
      _applyRequested(false),
      _applyCompleted(false),
      _applySuccess(false),
      _awaitingApply(false),
      _headerCache{},
      _headerCacheValid(false),
      _lastRefreshMs(0)
{
  resetState();
}

void RuntimeUIFBDMapUnified::resetState()
{
  _view = View::Map;
  _previousView = View::Map;
  _visible = false;
  _forceRedraw = true;
  _contentDirty = true;
  _headerDirty = true;
  _inputReleaseGate = false;

  _selectedIndex = 0;
  _detailInputIndex = 0;
  _detailParameterIndex = 0;
  _detailFocus = DetailFocus::Inputs;

  _tonDraft.major = 1;
  _tonDraft.minor = 0;
  _tonDraft.base = TonBase::Seconds;
  _tonDraft.focus = TonField::Major;
  _tonDraft.originalMs = 1000;
  _tonDraft.originalResource = 0;

  _applyRequested = false;
  _applyCompleted = false;
  _applySuccess = false;
  _awaitingApply = false;
  _lastRefreshMs = 0;

  invalidateAllCaches();
}

void RuntimeUIFBDMapUnified::invalidateAllCaches()
{
  std::memset(&_headerCache, 0, sizeof(_headerCache));
  _headerCacheValid = false;
}

void RuntimeUIFBDMapUnified::attach(RuntimeUIV2ReadModel &model)
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }
  _editSession.detach();

  resetState();
  _model = &model;

  LogicV2EnginePrototype *engine = model.mutableEngine();
  if (model.isAttached() && engine != nullptr)
  {
    _editSession.attach(*engine);
    _attached = _editSession.isAttached();
  }
  else
  {
    _attached = false;
  }
}

void RuntimeUIFBDMapUnified::detach()
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }
  _editSession.detach();

  resetState();
  _model = nullptr;
  _attached = false;
}

void RuntimeUIFBDMapUnified::enter()
{
  if (!_attached || _model == nullptr || !_model->isAttached())
  {
    return;
  }

  _visible = true;
  _view = View::Map;
  _previousView = View::Map;
  _forceRedraw = true;
  _contentDirty = true;
  _headerDirty = true;
  invalidateAllCaches();
}

void RuntimeUIFBDMapUnified::exit()
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }

  _visible = false;
  _awaitingApply = false;
  _applyRequested = false;
  _applyCompleted = false;
  _inputReleaseGate = false;
}

void RuntimeUIFBDMapUnified::forceRedraw()
{
  _forceRedraw = true;
  _contentDirty = true;
  _headerDirty = true;
  invalidateAllCaches();
}

bool RuntimeUIFBDMapUnified::needsTftRefresh() const
{
  // Entrada y render todavía comparten refresh(). No se bloquea el callback por
  // estado visual; las regiones sucias evitan escrituras TFT redundantes.
  return true;
}

uint32_t RuntimeUIFBDMapUnified::requestedRefreshPeriodMs() const
{
  switch (_view)
  {
  case View::Map:
  case View::Detail:
    return STATIC_REFRESH_MS;

  default:
    return EDIT_REFRESH_MS;
  }
}

void RuntimeUIFBDMapUnified::transitionTo(View nextView)
{
  if (nextView == _view)
  {
    return;
  }

  const View previous = _view;
  leaveView(previous, nextView);
  clearTransitionRegions(previous, nextView);

  _previousView = previous;
  _view = nextView;
  _headerDirty = true;
  _contentDirty = true;
  _forceRedraw = false;
  invalidateAllCaches();

  enterView(previous, nextView);
}

void RuntimeUIFBDMapUnified::leaveView(View previousView, View nextView)
{
  (void)previousView;
  (void)nextView;
}

void RuntimeUIFBDMapUnified::enterView(View previousView, View nextView)
{
  (void)previousView;
  (void)nextView;
}

void RuntimeUIFBDMapUnified::clearTransitionRegions(View previousView,
                                                    View nextView)
{
  (void)previousView;
  (void)nextView;

  // Regla general: dentro del editor FBD nunca se usa clearScreen(). El título,
  // contexto y estado tienen regiones independientes; el contenido comparte un
  // único rectángulo interior. Las optimizaciones por pares de vistas se añaden
  // aquí, en un solo lugar, durante la migración de cada pantalla.
  clearContentArea();
}

void RuntimeUIFBDMapUnified::clearContentArea()
{
  JWPLC_Display.tft().fillRect(CONTENT_X,
                               CONTENT_Y,
                               CONTENT_W,
                               CONTENT_H,
                               COLOR_PANEL);
}

void RuntimeUIFBDMapUnified::clearHeaderTitleArea()
{
  JWPLC_Display.tft().fillRect(HEADER_TITLE_X,
                               0,
                               HEADER_TITLE_W,
                               HEADER_H,
                               COLOR_PANEL);
}

void RuntimeUIFBDMapUnified::clearHeaderContextArea()
{
  JWPLC_Display.tft().fillRect(HEADER_CONTEXT_X,
                               0,
                               HEADER_CONTEXT_W,
                               HEADER_H,
                               COLOR_PANEL);
}

void RuntimeUIFBDMapUnified::clearHeaderStateArea()
{
  JWPLC_Display.tft().fillRect(HEADER_STATE_X,
                               0,
                               HEADER_STATE_W,
                               HEADER_H,
                               COLOR_PANEL);
}

void RuntimeUIFBDMapUnified::refresh(const JWPLC_IOState *io,
                                     const JWPLC_RTCState *rtc)
{
  (void)io;
  (void)rtc;

  if (!_visible || !_attached || _model == nullptr || !_model->isAttached())
  {
    return;
  }

  handleInput();
  render(_forceRedraw);
  _forceRedraw = false;
  _lastRefreshMs = millis();
}

void RuntimeUIFBDMapUnified::processPendingEdit()
{
  // La conexión con RuntimeUIV2EditSession se migrará junto con EDITAR IN,
  // EDITAR T, creación y eliminación. Mientras esta clase no sea la fachada
  // activa, no debe solicitar ni aplicar cambios al motor.
}

void RuntimeUIFBDMapUnified::handleInput()
{
  switch (_view)
  {
  case View::Map:
    handleMapInput();
    break;
  case View::Detail:
    handleDetailInput();
    break;
  case View::EditInput:
    handleEditInputInput();
    break;
  case View::EditTon:
    handleEditTonInput();
    break;
  case View::DeleteConfirm:
    handleDeleteInput();
    break;
  default:
    handleWizardInput();
    break;
  }
}

void RuntimeUIFBDMapUnified::render(bool force)
{
  renderHeader(force || _headerDirty);

  switch (_view)
  {
  case View::Map:
    renderMap(force || _contentDirty);
    break;
  case View::Detail:
    renderDetail(force || _contentDirty);
    break;
  case View::EditInput:
    renderEditInput(force || _contentDirty);
    break;
  case View::EditTon:
    renderEditTon(force || _contentDirty);
    break;
  case View::DeleteConfirm:
    renderDeleteConfirm(force || _contentDirty);
    break;
  default:
    renderWizard(force || _contentDirty);
    break;
  }

  _headerDirty = false;
  _contentDirty = false;
}

// Los siguientes métodos se implementarán por pantalla, copiando únicamente el
// comportamiento final validado. No llamarán a ninguna clase RuntimeUIFBDMapVx.
void RuntimeUIFBDMapUnified::handleMapInput() {}
void RuntimeUIFBDMapUnified::handleDetailInput() {}
void RuntimeUIFBDMapUnified::handleEditInputInput() {}
void RuntimeUIFBDMapUnified::handleEditTonInput() {}
void RuntimeUIFBDMapUnified::handleWizardInput() {}
void RuntimeUIFBDMapUnified::handleDeleteInput() {}

void RuntimeUIFBDMapUnified::renderHeader(bool force) { (void)force; }
void RuntimeUIFBDMapUnified::renderMap(bool force) { (void)force; }
void RuntimeUIFBDMapUnified::renderDetail(bool force) { (void)force; }
void RuntimeUIFBDMapUnified::renderEditInput(bool force) { (void)force; }
void RuntimeUIFBDMapUnified::renderEditTon(bool force) { (void)force; }
void RuntimeUIFBDMapUnified::renderWizard(bool force) { (void)force; }
void RuntimeUIFBDMapUnified::renderDeleteConfirm(bool force) { (void)force; }

RuntimeUIFBDMapUnified::HeaderModel
RuntimeUIFBDMapUnified::buildHeaderModel() const
{
  HeaderModel model{};
  return model;
}

void RuntimeUIFBDMapUnified::renderHeaderTitle(const HeaderModel &model,
                                                bool force)
{
  (void)model;
  (void)force;
}

void RuntimeUIFBDMapUnified::renderHeaderContext(const HeaderModel &model,
                                                  bool force)
{
  (void)model;
  (void)force;
}

void RuntimeUIFBDMapUnified::renderHeaderState(const HeaderModel &model,
                                                bool force)
{
  (void)model;
  (void)force;
}

void RuntimeUIFBDMapUnified::beginInputEdit() {}
void RuntimeUIFBDMapUnified::cancelInputEdit() {}
void RuntimeUIFBDMapUnified::applyInputEdit() {}
void RuntimeUIFBDMapUnified::beginTonEdit() {}
void RuntimeUIFBDMapUnified::cancelTonEdit() {}
void RuntimeUIFBDMapUnified::applyTonEdit() {}
void RuntimeUIFBDMapUnified::loadTonDraft() {}

uint32_t RuntimeUIFBDMapUnified::tonDraftMilliseconds() const
{
  return 0;
}

uint16_t RuntimeUIFBDMapUnified::tonResourceFromBase(TonBase base)
{
  return static_cast<uint16_t>(base);
}

RuntimeUIFBDMapUnified::TonBase
RuntimeUIFBDMapUnified::tonBaseFromResource(uint16_t resource)
{
  switch (resource & 0x0003U)
  {
  case 1:
    return TonBase::Minutes;
  case 2:
    return TonBase::Hours;
  case 0:
  default:
    return TonBase::Seconds;
  }
}

bool RuntimeUIFBDMapUnified::selectedBlockHasConsumers() const
{
  return false;
}

void RuntimeUIFBDMapUnified::beginDeleteConfirm() {}
void RuntimeUIFBDMapUnified::cancelDeleteConfirm() {}
void RuntimeUIFBDMapUnified::applyDelete() {}
