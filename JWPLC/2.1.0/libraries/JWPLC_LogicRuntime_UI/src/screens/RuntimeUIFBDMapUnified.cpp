#include "RuntimeUIFBDMapUnified.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

void jwplcNormalizeUnifiedTonFrame(bool selected);

RuntimeUIFBDMapUnified::RuntimeUIFBDMapUnified()
    : _model(nullptr),
      _editSession(),
      _view(View::Map),
      _previousView(View::Map),
      _horizontalMode(HorizontalWindowMode::LeftEdge),
      _attached(false),
      _visible(false),
      _forceRedraw(true),
      _contentDirty(true),
      _headerDirty(true),
      _inputReleaseGate(false),
      _layoutValid(false),
      _selectedIndex(0),
      _detailInputIndex(0),
      _detailParameterIndex(0),
      _detailFocus(DetailFocus::Inputs),
      _layoutBlockCount(0),
      _layoutLinkCount(0),
      _centralStartLevel(1),
      _maxLevel(0),
      _viewportY(0),
      _levels{},
      _lanes{},
      _nodeX{},
      _nodeY{},
      _mapValueCache{},
      _mapValueCacheValid(false),
      _mapSelectionCache(0),
      _mapSelectionCacheValid(false),
      _detailCacheValid(false),
      _detailCacheBlock(0),
      _detailCacheInput(0),
      _detailCacheFocus(DetailFocus::Inputs),
      _detailCacheBlockValue(false),
      _detailInputValueCache{},
      _detailTonConfiguredCache{},
      _detailTonElapsedCache{},
      _detailTonColorCache(false),
      _detailTonSelectedCache(false),
      _lastDetailLiveMs(0),
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
  _horizontalMode = HorizontalWindowMode::LeftEdge;
  _visible = false;
  _forceRedraw = true;
  _contentDirty = true;
  _headerDirty = true;
  _inputReleaseGate = false;
  _selectedIndex = 0;
  _detailInputIndex = 0;
  _detailParameterIndex = 0;
  _detailFocus = DetailFocus::Inputs;
  _centralStartLevel = 1;
  _maxLevel = 0;
  _viewportY = 0;
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
  _lastDetailLiveMs = 0;
  invalidateLayout();
  invalidateAllCaches();
}

void RuntimeUIFBDMapUnified::invalidateAllCaches()
{
  std::memset(&_headerCache, 0, sizeof(_headerCache));
  _headerCacheValid = false;
  invalidateMapCache();
  invalidateDetailCache();
}

void RuntimeUIFBDMapUnified::invalidateLayout()
{
  _layoutValid = false;
  _layoutBlockCount = 0;
  _layoutLinkCount = 0;
  _maxLevel = 0;
  std::memset(_levels, 0, sizeof(_levels));
  std::memset(_lanes, 0, sizeof(_lanes));
  std::memset(_nodeX, 0, sizeof(_nodeX));
  std::memset(_nodeY, 0, sizeof(_nodeY));
  invalidateMapCache();
}

void RuntimeUIFBDMapUnified::invalidateMapCache()
{
  std::memset(_mapValueCache, 0, sizeof(_mapValueCache));
  _mapValueCacheValid = false;
  _mapSelectionCache = 0;
  _mapSelectionCacheValid = false;
}

void RuntimeUIFBDMapUnified::invalidateDetailCache()
{
  _detailCacheValid = false;
  _detailCacheBlock = 0;
  _detailCacheInput = 0;
  _detailCacheFocus = DetailFocus::Inputs;
  _detailCacheBlockValue = false;
  std::memset(_detailInputValueCache, 0, sizeof(_detailInputValueCache));
  _detailTonConfiguredCache[0] = '\0';
  _detailTonElapsedCache[0] = '\0';
  _detailTonColorCache = false;
  _detailTonSelectedCache = false;
}

void RuntimeUIFBDMapUnified::attach(RuntimeUIV2ReadModel &model)
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }
  resetState();
  _model = &model;
  _attached = model.isAttached();
  LogicV2EnginePrototype *engine = model.mutableEngine();
  if (engine != nullptr)
  {
    _editSession.attach(*engine);
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
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
  buildLayout();
  normalizeSelection();
  ensureSelectionVisible();
  _forceRedraw = true;
  _contentDirty = true;
  _headerDirty = true;
  invalidateAllCaches();
  gateInputUntilRelease();
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
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
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
  return true;
}

uint32_t RuntimeUIFBDMapUnified::requestedRefreshPeriodMs() const
{
  return (_view == View::Map || _view == View::Detail)
             ? STATIC_REFRESH_MS
             : EDIT_REFRESH_MS;
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
  JWPLC_Display.setIdleReturnMode(
      nextView == View::Map ? IDLE_RETURN_ESC_ONLY : IDLE_RETURN_DISABLED);
  enterView(previous, nextView);
  gateInputUntilRelease();
}

void RuntimeUIFBDMapUnified::leaveView(View previousView, View nextView)
{
  (void)previousView;
  (void)nextView;
}

void RuntimeUIFBDMapUnified::enterView(View previousView, View nextView)
{
  (void)previousView;
  if (nextView == View::Detail)
  {
    const LogicV2BlockRecord *definition =
        _model != nullptr ? _model->block(_selectedIndex) : nullptr;
    _detailInputIndex = 0;
    _detailParameterIndex = 0;
    _detailFocus =
        definition != nullptr && definition->inputCount == 0 &&
                selectedBlockHasParameter()
            ? DetailFocus::Parameters
            : DetailFocus::Inputs;
    normalizeDetailFocus();
    _lastDetailLiveMs = 0;
    invalidateDetailCache();
  }
  else if (nextView == View::Map)
  {
    ensureSelectionVisible();
    invalidateMapCache();
  }
}

void RuntimeUIFBDMapUnified::clearTransitionRegions(View previousView,
                                                    View nextView)
{
  (void)previousView;
  (void)nextView;
  // La limpieza ocurre una sola vez en el renderer final de la vista destino.
}

void RuntimeUIFBDMapUnified::clearContentArea()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(CONTENT_X, CONTENT_Y, CONTENT_W, CONTENT_H, COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}

void RuntimeUIFBDMapUnified::clearHeaderTitleArea()
{
  JWPLC_Display.tft().fillRect(
      HEADER_TITLE_X, 0, HEADER_TITLE_W, HEADER_H, COLOR_PANEL);
}

void RuntimeUIFBDMapUnified::clearHeaderContextArea()
{
  JWPLC_Display.tft().fillRect(
      HEADER_CONTEXT_X, 0, HEADER_CONTEXT_W, HEADER_H, COLOR_PANEL);
}

void RuntimeUIFBDMapUnified::clearHeaderStateArea()
{
  JWPLC_Display.tft().fillRect(
      HEADER_STATE_X, 0, HEADER_STATE_W, HEADER_H, COLOR_PANEL);
}

bool RuntimeUIFBDMapUnified::anyButtonHeld() const
{
  for (uint8_t button = 0; button < BTN_COUNT; ++button)
  {
    if (JWPLC_Buttons.isDown(button))
    {
      return true;
    }
  }
  return false;
}

void RuntimeUIFBDMapUnified::gateInputUntilRelease()
{
  _inputReleaseGate = true;
  JWPLC_Buttons.clearPendingInput();
}

bool RuntimeUIFBDMapUnified::consumeInputReleaseGate()
{
  if (!_inputReleaseGate)
  {
    return false;
  }
  JWPLC_Buttons.clearPendingInput();
  if (anyButtonHeld())
  {
    return true;
  }
  _inputReleaseGate = false;
  JWPLC_Buttons.clearPendingInput();
  return true;
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
  if (_layoutValid &&
      (_layoutBlockCount != _model->blockCount() ||
       _layoutLinkCount != _model->linkCount()))
  {
    invalidateLayout();
  }
  if (!_layoutValid)
  {
    buildLayout();
    normalizeSelection();
    ensureSelectionVisible();
    _contentDirty = true;
    _headerDirty = true;
  }
  if (!consumeInputReleaseGate())
  {
    handleInput();
  }
  render(_forceRedraw);
  _forceRedraw = false;
  _lastRefreshMs = millis();
}

void RuntimeUIFBDMapUnified::processPendingEdit()
{
  if (!_applyRequested)
  {
    return;
  }
  _applyRequested = false;
  _applySuccess = _editSession.apply(true);
  _applyCompleted = true;
}

void RuntimeUIFBDMapUnified::handleInput()
{
  switch (_view)
  {
  case View::Map:
    handleMapInputStable();
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
  bool normalizeTonFrame = false;
  if (_view == View::Detail && selectedBlockHasParameter())
  {
    const bool blockStateChanged =
        !_detailCacheValid ||
        _detailCacheBlockValue != _model->blockValue(_selectedIndex);
    normalizeTonFrame =
        force || _contentDirty || _headerDirty || blockStateChanged;
  }

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

  if (normalizeTonFrame)
  {
    jwplcNormalizeUnifiedTonFrame(
        _detailFocus == DetailFocus::Parameters);
  }

  renderHeader(force);
  _headerDirty = false;
  _contentDirty = false;
}

RuntimeUIFBDMapUnified::HeaderModel
RuntimeUIFBDMapUnified::buildHeaderModel() const
{
  HeaderModel model{};
  std::snprintf(model.title,
                sizeof(model.title),
                _view == View::Map ? "MAPA FBD" : "DETALLE");
  if (_view == View::EditInput)
  {
    std::snprintf(model.title, sizeof(model.title), "EDITAR IN");
  }
  else if (_view == View::EditTon)
  {
    std::snprintf(model.title, sizeof(model.title), "EDITAR T");
  }
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr)
  {
    std::snprintf(model.line1, sizeof(model.line1), "SIN PROGRAMA");
    model.line2[0] = '\0';
  }
  else
  {
    std::snprintf(model.line1,
                  sizeof(model.line1),
                  "B%02u %s",
                  static_cast<unsigned>(_selectedIndex),
                  _model->typeShort(definition->type));
    if (_view == View::Map)
    {
      std::snprintf(model.line2,
                    sizeof(model.line2),
                    "%s",
                    _model->blockValue(_selectedIndex) ? "ON" : "OFF");
    }
    else if (_detailFocus == DetailFocus::Parameters &&
             selectedBlockHasParameter())
    {
      std::snprintf(model.line2, sizeof(model.line2), "PARAM T");
    }
    else if (definition->inputCount > 0)
    {
      const char *role =
          _model->inputRole(definition->type, _detailInputIndex);
      if (role != nullptr && role[0] != '\0')
      {
        std::snprintf(model.line2, sizeof(model.line2), "%s", role);
      }
      else
      {
        std::snprintf(model.line2,
                      sizeof(model.line2),
                      "IN%u/%u",
                      static_cast<unsigned>(_detailInputIndex + 1U),
                      static_cast<unsigned>(definition->inputCount));
      }
    }
    else
    {
      model.line2[0] = '\0';
    }
  }
  std::snprintf(model.state, sizeof(model.state), "%s", stateText());
  model.stateColor = stateColor();
  return model;
}

void RuntimeUIFBDMapUnified::renderHeader(bool force)
{
  const HeaderModel model = buildHeaderModel();
  const bool titleChanged =
      force || !_headerCacheValid ||
      std::strcmp(_headerCache.title, model.title) != 0;
  const bool line1Changed =
      force || !_headerCacheValid ||
      std::strcmp(_headerCache.line1, model.line1) != 0;
  const bool line2Changed =
      force || !_headerCacheValid ||
      std::strcmp(_headerCache.line2, model.line2) != 0;
  const bool stateChanged =
      force || !_headerCacheValid ||
      std::strcmp(_headerCache.state, model.state) != 0 ||
      _headerCache.stateColor != model.stateColor;

  renderHeaderTitle(model, titleChanged);
  renderHeaderContext(model, line1Changed, line2Changed);
  renderHeaderState(model, stateChanged);

  if (titleChanged || line1Changed || line2Changed || stateChanged)
  {
    _headerCache = model;
    _headerCacheValid = true;
  }
}

void RuntimeUIFBDMapUnified::renderHeaderTitle(const HeaderModel &model,
                                                bool force)
{
  if (!force)
  {
    return;
  }
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearHeaderTitleArea();
  tft.drawFastHLine(0, HEADER_H - 1, SCREEN_W, COLOR_BORDER);
  tft.setTextWrap(false);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
  tft.setCursor(6, 5);
  tft.print(model.title);
}

void RuntimeUIFBDMapUnified::renderHeaderContext(
    const HeaderModel &model,
    bool line1Changed,
    bool line2Changed)
{
  if (!line1Changed && !line2Changed)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.drawFastHLine(HEADER_CONTEXT_X,
                    HEADER_H - 1,
                    HEADER_CONTEXT_W,
                    COLOR_BORDER);

  // Las filas tienen cachés independientes. Al pasar MAPA <-> DETALLE normalmente
  // solo cambia ON/OFF por Trg/PARAM T; Bxx TIPO permanece intacto y no parpadea.
  if (line1Changed)
  {
    updateTextField(tft,
                    HEADER_CONTEXT_X + 4,
                    HEADER_LINE1_Y,
                    20,
                    model.line1,
                    COLOR_MUTED,
                    COLOR_PANEL);
  }
  if (line2Changed)
  {
    updateTextField(tft,
                    HEADER_CONTEXT_X + 4,
                    HEADER_LINE2_Y,
                    20,
                    model.line2,
                    COLOR_MUTED,
                    COLOR_PANEL);
  }
}

void RuntimeUIFBDMapUnified::renderHeaderState(const HeaderModel &model,
                                                bool force)
{
  if (!force)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearHeaderStateArea();
  tft.drawFastHLine(HEADER_STATE_X,
                    HEADER_H - 1,
                    HEADER_STATE_W + 6,
                    COLOR_BORDER);

  // Unified usa fondo gris estable. El estado se expresa mediante borde y texto,
  // evitando la insignia genérica con fondo negro/verde observada físicamente.
  tft.fillRoundRect(HEADER_STATE_X,
                    4,
                    HEADER_STATE_W,
                    16,
                    3,
                    COLOR_PANEL);
  tft.drawRoundRect(HEADER_STATE_X,
                    4,
                    HEADER_STATE_W,
                    16,
                    3,
                    model.stateColor);

  const size_t length = std::strlen(model.state);
  const int16_t textWidth = static_cast<int16_t>(length * 6U);
  const int16_t textX = static_cast<int16_t>(
      HEADER_STATE_X + (HEADER_STATE_W - textWidth) / 2);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(model.stateColor, COLOR_PANEL);
  tft.setCursor(textX, 8);
  tft.print(model.state);
}

void RuntimeUIFBDMapUnified::handleEditInputInput() {}
void RuntimeUIFBDMapUnified::handleEditTonInput() {}
void RuntimeUIFBDMapUnified::handleWizardInput() {}
void RuntimeUIFBDMapUnified::handleDeleteInput() {}
void RuntimeUIFBDMapUnified::renderEditInput(bool force) { (void)force; }
void RuntimeUIFBDMapUnified::renderEditTon(bool force) { (void)force; }
void RuntimeUIFBDMapUnified::renderWizard(bool force) { (void)force; }
void RuntimeUIFBDMapUnified::renderDeleteConfirm(bool force) { (void)force; }
void RuntimeUIFBDMapUnified::beginInputEdit() {}
void RuntimeUIFBDMapUnified::cancelInputEdit() {}
void RuntimeUIFBDMapUnified::applyInputEdit() {}
void RuntimeUIFBDMapUnified::beginTonEdit() {}
void RuntimeUIFBDMapUnified::cancelTonEdit() {}
void RuntimeUIFBDMapUnified::applyTonEdit() {}
void RuntimeUIFBDMapUnified::loadTonDraft() {}
uint32_t RuntimeUIFBDMapUnified::tonDraftMilliseconds() const { return 0; }
uint16_t RuntimeUIFBDMapUnified::tonResourceFromBase(TonBase base)
{
  return static_cast<uint16_t>(base);
}
bool RuntimeUIFBDMapUnified::selectedBlockHasConsumers() const { return false; }
void RuntimeUIFBDMapUnified::beginDeleteConfirm() {}
void RuntimeUIFBDMapUnified::cancelDeleteConfirm() {}
void RuntimeUIFBDMapUnified::applyDelete() {}