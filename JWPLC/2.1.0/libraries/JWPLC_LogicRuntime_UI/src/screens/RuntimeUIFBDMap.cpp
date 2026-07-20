#include "RuntimeUIFBDMap.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
uint16_t absoluteDistance(int16_t a, int16_t b)
{
  return static_cast<uint16_t>(a > b ? a - b : b - a);
}
}

RuntimeUIFBDMap::RuntimeUIFBDMap()
    : _model(nullptr),
      _editSession(),
      _view(View::Map),
      _returnView(View::Map),
      _applyContext(ApplyContext::None),
      _feedback(Feedback::None),
      _fullRedraw(true),
      _layoutValid(false),
      _valueCacheValid(false),
      _inputReleaseGate(false),
      _restoreIdleReturnPending(false),
      _applyRequested(false),
      _applyCompleted(false),
      _applySuccess(false),
      _awaitingApply(false),
      _horizontalMode(HorizontalWindowMode::LeftEdge),
      _selectedIndex(0),
      _detailInputIndex(0),
      _detailFocus(DetailFocus::Inputs),
      _detailParameterIndex(0),
      _layoutBlockCount(0),
      _layoutLinkCount(0),
      _centralStartLevel(1),
      _maxLevel(0),
      _viewportY(0),
      _lastMapRefreshMs(0),
      _lastDetailRefreshMs(0),
      _lastEditRefreshMs(0),
      _appliedRefreshPeriodMs(0),
      _editInputFocus(EditInputFocus::Source),
      _editSourceCandidate(0),
      _editInverted(false),
      _tonMajor(0),
      _tonMinor(0),
      _tonBase(TonBase::Seconds),
      _tonField(TonField::Major),
      _tonOriginalMs(0),
      _tonOriginalResource(0),
      _tonFieldsCacheValid(false),
      _tonMajorCache(0),
      _tonMinorCache(0),
      _tonBaseCache(TonBase::Seconds),
      _tonFieldCache(TonField::Major),
      _tonElapsedCacheValid(false),
      _tonElapsedColorOn(false),
      _tonElapsedCache{},
      _tonFooterDefaultVisible(false),
      _addNodeSelected(false),
      _addSourceBlock(0),
      _addType(AddType::DigitalInput),
      _addTypeIndex(0),
      _addConfigRow(0),
      _addSources{JWPLC_LOGIC_V2_SOURCE_CONST_TRUE,
                  JWPLC_LOGIC_V2_SOURCE_CONST_TRUE},
      _addResource(0),
      _addSourceCandidate(0),
      _addEditingSourceIndex(0),
      _addResourceBackup(0),
      _addTonMajor(1),
      _addTonMinor(0),
      _addTonBase(TonBase::Seconds),
      _addTonField(TonField::Major),
      _newBlockIndex(0),
      _headerCacheValid(false),
      _headerTitleCache{},
      _headerLine1Cache{},
      _headerLine2Cache{},
      _headerStateCache(LogicV2EngineState::Empty),
      _detailCacheValid(false),
      _detailCacheBlock(0),
      _detailCacheInput(0),
      _detailCacheFocus(DetailFocus::Inputs),
      _detailBlockValueCache(false),
      _detailInputValueCache(false),
      _detailTonSelectedCache(false),
      _detailTonConfiguredCache{},
      _detailTonElapsedCache{},
      _levels{},
      _lanes{},
      _nodeX{},
      _nodeY{},
      _valueCache{}
{
}

void RuntimeUIFBDMap::resetState()
{
  _view = View::Map;
  _returnView = View::Map;
  _applyContext = ApplyContext::None;
  _feedback = Feedback::None;
  _fullRedraw = true;
  _valueCacheValid = false;
  _inputReleaseGate = false;
  _restoreIdleReturnPending = false;
  _applyRequested = false;
  _applyCompleted = false;
  _applySuccess = false;
  _awaitingApply = false;
  _horizontalMode = HorizontalWindowMode::LeftEdge;
  _selectedIndex = 0;
  _detailInputIndex = 0;
  _detailFocus = DetailFocus::Inputs;
  _detailParameterIndex = 0;
  _centralStartLevel = 1;
  _maxLevel = 0;
  _viewportY = 0;
  _lastMapRefreshMs = millis();
  _lastDetailRefreshMs = _lastMapRefreshMs;
  _lastEditRefreshMs = _lastMapRefreshMs;
  _appliedRefreshPeriodMs = 0;

  _editInputFocus = EditInputFocus::Source;
  _editSourceCandidate = 0;
  _editInverted = false;

  _tonMajor = 0;
  _tonMinor = 0;
  _tonBase = TonBase::Seconds;
  _tonField = TonField::Major;
  _tonOriginalMs = 0;
  _tonOriginalResource = 0;
  _tonFieldsCacheValid = false;
  _tonElapsedCacheValid = false;
  _tonElapsedCache[0] = '\0';
  _tonFooterDefaultVisible = false;

  _addNodeSelected = false;
  _addSourceBlock = 0;
  _addType = AddType::DigitalInput;
  _addTypeIndex = 0;
  _addConfigRow = 0;
  _addSources[0] = JWPLC_LOGIC_V2_SOURCE_CONST_TRUE;
  _addSources[1] = JWPLC_LOGIC_V2_SOURCE_CONST_TRUE;
  _addResource = 0;
  _addSourceCandidate = 0;
  _addEditingSourceIndex = 0;
  _addResourceBackup = 0;
  _addTonMajor = 1;
  _addTonMinor = 0;
  _addTonBase = TonBase::Seconds;
  _addTonField = TonField::Major;
  _newBlockIndex = 0;

  invalidateHeaderCache();
  _detailCacheValid = false;
  invalidateLayout();
}

void RuntimeUIFBDMap::attach(RuntimeUIV2ReadModel &model)
{
  _model = &model;
  LogicV2EnginePrototype *engine = model.mutableEngine();
  if (engine != nullptr)
  {
    _editSession.attach(*engine);
  }
  resetState();
}

void RuntimeUIFBDMap::detach()
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }
  _editSession.detach();
  _model = nullptr;
  resetState();
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
}

void RuntimeUIFBDMap::enter()
{
  if (_model == nullptr || !_model->isAttached())
  {
    return;
  }

  _view = View::Map;
  _addNodeSelected = false;
  _feedback = Feedback::None;
  normalizeSelection();
  buildLayout();
  ensureSelectionVisible();
  invalidateHeaderCache();
  _detailCacheValid = false;
  _valueCacheValid = false;
  _tonFieldsCacheValid = false;
  _tonElapsedCacheValid = false;
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
  JWPLC_Display.setUserRefreshPeriodMs(MAP_REFRESH_MS);
  _appliedRefreshPeriodMs = MAP_REFRESH_MS;

  clearScreen(JWPLC_Display.tft());
  renderCurrentFull(true);
  gateInputUntilRelease(false);
}

void RuntimeUIFBDMap::exit()
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }
  _applyRequested = false;
  _applyCompleted = false;
  _awaitingApply = false;
  _inputReleaseGate = false;
  _restoreIdleReturnPending = false;
  _valueCacheValid = false;
  _detailCacheValid = false;
  _tonFieldsCacheValid = false;
  _tonElapsedCacheValid = false;
  invalidateHeaderCache();
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
}

void RuntimeUIFBDMap::forceRedraw()
{
  _fullRedraw = true;
  _valueCacheValid = false;
  _detailCacheValid = false;
  _tonFieldsCacheValid = false;
  _tonElapsedCacheValid = false;
  invalidateHeaderCache();
  _appliedRefreshPeriodMs = 0;
}

void RuntimeUIFBDMap::processPendingEdit()
{
  if (!_applyRequested)
  {
    return;
  }

  _applyRequested = false;
  _applySuccess = _editSession.apply(true);
  _applyCompleted = true;
}

uint32_t RuntimeUIFBDMap::desiredRefreshPeriod() const
{
  switch (_view)
  {
  case View::Map:
  case View::Detail:
    return MAP_REFRESH_MS;
  case View::EditInput:
  case View::EditTon:
  case View::AddType:
  case View::AddConfig:
  case View::AddSource:
  case View::AddResource:
  case View::AddTon:
  default:
    return EDIT_REFRESH_MS;
  }
}

void RuntimeUIFBDMap::syncRefreshPeriod()
{
  const uint32_t desired = desiredRefreshPeriod();
  if (_appliedRefreshPeriodMs == desired)
  {
    return;
  }
  JWPLC_Display.setUserRefreshPeriodMs(desired);
  _appliedRefreshPeriodMs = desired;
}

void RuntimeUIFBDMap::setView(View view, bool redraw)
{
  if (_view != view)
  {
    // La cabecera conserva sus caches entre vistas. drawUnifiedHeader() compara
    // titulo, contexto y estado por separado, por lo que Bxx/TIPO y RUN no se
    // repintan cuando siguen siendo iguales durante una transicion interna.
    _view = view;
    _detailCacheValid = false;
    _tonFieldsCacheValid = false;
    _tonElapsedCacheValid = false;
  }

  JWPLC_Display.setIdleReturnMode(
      _view == View::Map ? IDLE_RETURN_ESC_ONLY : IDLE_RETURN_DISABLED);
  syncRefreshPeriod();

  if (redraw)
  {
    renderCurrentFull(false);
  }
}

void RuntimeUIFBDMap::renderCurrentFull(bool initialEntry)
{
  switch (_view)
  {
  case View::Map:
    drawMapFull(true);
    break;
  case View::Detail:
    drawDetailFull(true);
    break;
  case View::EditInput:
    drawEditInputFull(true);
    break;
  case View::EditTon:
    drawEditTonFull(true);
    break;
  case View::AddType:
    drawAddTypeFull(true);
    break;
  case View::AddConfig:
    drawAddConfigFull(true);
    break;
  case View::AddSource:
    drawAddSourceFull(true);
    break;
  case View::AddResource:
    drawAddResourceFull(true);
    break;
  case View::AddTon:
    drawAddTonFull(true);
    break;
  }
  // En la entrada inicial se fuerza toda la cabecera. En transiciones internas
  // solo se actualizan las regiones cuyo contenido realmente cambio.
  drawUnifiedHeader(initialEntry);
  _fullRedraw = false;
}

void RuntimeUIFBDMap::handleApplyCompleted()
{
  if (!_applyCompleted)
  {
    return;
  }

  const bool success = _applySuccess;
  const ApplyContext context = _applyContext;
  _applyCompleted = false;
  _awaitingApply = false;
  _applyContext = ApplyContext::None;

  if (!success)
  {
    _feedback = Feedback::ApplyFailed;
    gateInputUntilRelease(false);
    _fullRedraw = true;
    return;
  }

  _editSession.cancel();
  _feedback = Feedback::None;
  invalidateLayout();
  buildLayout();

  if (context == ApplyContext::AddBlock)
  {
    _selectedIndex = _newBlockIndex;
    normalizeSelection();
    ensureSelectionVisible();
    _addNodeSelected = false;
    setView(View::Map, false);
  }
  else
  {
    normalizeSelection();
    ensureSelectionVisible();
    normalizeDetailFocus();
    setView(View::Detail, false);
    if (context == ApplyContext::EditTon)
    {
      _detailFocus = DetailFocus::Parameters;
    }
  }

  gateInputUntilRelease(false);
  renderCurrentFull(false);
}

void RuntimeUIFBDMap::refresh(const JWPLC_IOState *io,
                              const JWPLC_RTCState *rtc)
{
  (void)io;
  (void)rtc;

  if (_model == nullptr || !_model->isAttached())
  {
    return;
  }

  handleApplyCompleted();

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
    _fullRedraw = true;
  }

  if (_fullRedraw)
  {
    renderCurrentFull(false);
  }
  else
  {
    drawUnifiedHeader(false);
  }

  syncRefreshPeriod();

  if (consumeInputReleaseGate())
  {
    return;
  }

  handleInput();
  refreshLive();
}

void RuntimeUIFBDMap::handleInput()
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
    handleEditInput();
    break;
  case View::EditTon:
    handleEditTon();
    break;
  case View::AddType:
    handleAddType();
    break;
  case View::AddConfig:
    handleAddConfig();
    break;
  case View::AddSource:
    handleAddSource();
    break;
  case View::AddResource:
    handleAddResource();
    break;
  case View::AddTon:
    handleAddTon();
    break;
  }
}

void RuntimeUIFBDMap::refreshLive()
{
  const uint32_t nowMs = millis();
  if (_view == View::Map)
  {
    if (static_cast<uint32_t>(nowMs - _lastMapRefreshMs) < MAP_REFRESH_MS)
    {
      return;
    }
    _lastMapRefreshMs = nowMs;
    if (valuesChanged())
    {
      drawMapLive();
      drawUnifiedHeader(false);
    }
    return;
  }

  if (_view == View::Detail)
  {
    if (static_cast<uint32_t>(nowMs - _lastDetailRefreshMs) < DETAIL_REFRESH_MS)
    {
      return;
    }
    _lastDetailRefreshMs = nowMs;
    drawDetailLive();
    drawUnifiedHeader(false);
    return;
  }

  if (_view == View::EditTon)
  {
    if (static_cast<uint32_t>(nowMs - _lastEditRefreshMs) < EDIT_REFRESH_MS)
    {
      return;
    }
    _lastEditRefreshMs = nowMs;
    drawTonElapsed(false);
  }
}

bool RuntimeUIFBDMap::anyButtonHeld() const
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

void RuntimeUIFBDMap::gateInputUntilRelease(bool restoreIdleReturn)
{
  _inputReleaseGate = true;
  if (restoreIdleReturn)
  {
    _restoreIdleReturnPending = true;
  }
  JWPLC_Buttons.clearPendingInput();
}

bool RuntimeUIFBDMap::consumeInputReleaseGate()
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
  if (_restoreIdleReturnPending)
  {
    _restoreIdleReturnPending = false;
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
  }
  return true;
}

uint16_t RuntimeUIFBDMap::nearestByY(const uint16_t *indices,
                                     uint8_t count) const
{
  if (indices == nullptr || count == 0)
  {
    return JWPLC_LOGIC_NO_SOURCE;
  }

  uint16_t selected = indices[0];
  uint16_t bestDistance = absoluteDistance(_nodeY[selected],
                                           _nodeY[_selectedIndex]);
  for (uint8_t index = 1; index < count; ++index)
  {
    const uint16_t candidate = indices[index];
    const uint16_t distance = absoluteDistance(_nodeY[candidate],
                                               _nodeY[_selectedIndex]);
    if (distance < bestDistance)
    {
      selected = candidate;
      bestDistance = distance;
    }
  }
  return selected;
}
