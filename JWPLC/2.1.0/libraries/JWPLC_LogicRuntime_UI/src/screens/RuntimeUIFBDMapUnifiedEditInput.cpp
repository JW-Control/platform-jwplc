#include "RuntimeUIFBDMapUnified.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
static constexpr int16_t SOURCE_X = 10;
static constexpr int16_t SOURCE_W = 145;
static constexpr int16_t LOGIC_X = 165;
static constexpr int16_t LOGIC_W = 145;

void drawInputValueRegion(int16_t x,
                          int16_t w,
                          int16_t y,
                          int16_t h,
                          const char *value,
                          bool selected)
{
  const uint16_t background = selected ? COLOR_SELECTED : COLOR_PANEL;
  const uint16_t foreground = selected ? COLOR_WARNING : COLOR_TEXT;
  Adafruit_ST7789 &tft = JWPLC_Display.tft();

  const int16_t valueY = static_cast<int16_t>(y + 22);
  tft.fillRect(x + 5, valueY, w - 10, 10, background);

  const size_t length = std::strlen(value ? value : "");
  const int16_t textWidth = static_cast<int16_t>(length * 6U);
  const int16_t textX = static_cast<int16_t>(x + (w - textWidth) / 2);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(foreground, background);
  tft.setCursor(textX, valueY);
  tft.print(value ? value : "");
}
}

void RuntimeUIFBDMapUnified::beginInputEdit()
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr ||
      definition->inputCount == 0 ||
      _detailInputIndex >= definition->inputCount ||
      !_editSession.begin())
  {
    return;
  }

  const LogicV2InputLink *link =
      _editSession.inputLink(_selectedIndex, _detailInputIndex);
  if (link == nullptr)
  {
    _editSession.cancel();
    return;
  }

  _inputDraftSource = link->source();
  _inputDraftInverted = link->inverted();
  _inputSourceCursor = inputSourceCursorFromValue(_inputDraftSource);
  _inputEditFocus = InputEditField::Source;
  _editFeedback = EditFeedback::None;
  _awaitingApply = false;
  _applyRequested = false;
  _applyCompleted = false;
  invalidateEditorCaches();
  transitionTo(View::EditInput);
}

void RuntimeUIFBDMapUnified::cancelInputEdit()
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }
  _awaitingApply = false;
  _applyRequested = false;
  _applyCompleted = false;
  _editFeedback = EditFeedback::None;
  _detailFocus = DetailFocus::Inputs;
  transitionTo(View::Detail);
}

void RuntimeUIFBDMapUnified::applyInputEdit()
{
  if (_awaitingApply || !_editSession.active())
  {
    return;
  }

  const bool prepared = _editSession.setInputSource(
      _selectedIndex,
      _detailInputIndex,
      _inputDraftSource,
      _inputDraftInverted);
  const bool valid =
      prepared &&
      _editSession.validate() == LogicV2PrototypeError::None;

  if (!valid)
  {
    _editFeedback = EditFeedback::Failed;
    drawEditorFooter();
    return;
  }

  _editFeedback = EditFeedback::Applying;
  _awaitingApply = true;
  _applyRequested = true;
  drawEditorFooter();
  gateInputUntilRelease();
}

void RuntimeUIFBDMapUnified::finishInputApply()
{
  if (!_applyCompleted)
  {
    return;
  }

  const bool success = _applySuccess;
  _applyCompleted = false;
  _applySuccess = false;
  _awaitingApply = false;

  if (!success)
  {
    _editFeedback = EditFeedback::Failed;
    drawEditorFooter();
    return;
  }

  _editSession.cancel();
  _editFeedback = EditFeedback::None;
  _detailFocus = DetailFocus::Inputs;

  // Cambiar una fuente puede modificar niveles y rutas del mapa.
  invalidateLayout();
  invalidateDetailCache();
  transitionTo(View::Detail);
}

uint16_t RuntimeUIFBDMapUnified::inputSourceChoiceCount() const
{
  // X, HI, LO y todos los bloques topológicamente anteriores.
  return static_cast<uint16_t>(3U + _selectedIndex);
}

uint16_t RuntimeUIFBDMapUnified::inputSourceCursorFromValue(
    uint16_t source) const
{
  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    return 0;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    return 1;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    return 2;
  }
  if (source < _selectedIndex)
  {
    return static_cast<uint16_t>(source + 3U);
  }
  return 0;
}

uint16_t RuntimeUIFBDMapUnified::inputSourceValueFromCursor(
    uint16_t cursor) const
{
  switch (cursor)
  {
  case 0:
    return JWPLC_LOGIC_V2_SOURCE_OPEN;
  case 1:
    return JWPLC_LOGIC_V2_SOURCE_CONST_TRUE;
  case 2:
    return JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
  default:
    return static_cast<uint16_t>(cursor - 3U);
  }
}

void RuntimeUIFBDMapUnified::formatInputSource(
    uint16_t source,
    char *destination,
    size_t capacity) const
{
  if (destination == nullptr || capacity == 0)
  {
    return;
  }

  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    std::snprintf(destination, capacity, "X  ABIERTO");
    return;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    std::snprintf(destination, capacity, "HI CONST 1");
    return;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    std::snprintf(destination, capacity, "LO CONST 0");
    return;
  }

  const LogicV2BlockRecord *sourceBlock =
      _model != nullptr ? _model->block(source) : nullptr;
  if (sourceBlock == nullptr)
  {
    std::snprintf(destination, capacity, "FUENTE INVALIDA");
    return;
  }

  std::snprintf(destination,
                capacity,
                "B%02u %s",
                static_cast<unsigned>(source),
                _model->typeShort(sourceBlock->type));
}

void RuntimeUIFBDMapUnified::drawInputEditorField(InputEditField field)
{
  const bool sourceField = field == InputEditField::Source;
  const bool selected = field == _inputEditFocus;
  const int16_t x = sourceField ? SOURCE_X : LOGIC_X;
  const int16_t w = sourceField ? SOURCE_W : LOGIC_W;
  const uint16_t fill = selected ? COLOR_SELECTED : COLOR_PANEL;
  const uint16_t border = selected ? COLOR_WARNING : COLOR_BORDER;

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRoundRect(x, EDIT_FIELD_Y, w, EDIT_FIELD_H, 4, fill);
  tft.drawRoundRect(x, EDIT_FIELD_Y, w, EDIT_FIELD_H, 4, border);
  if (selected)
  {
    tft.drawRoundRect(x + 1,
                      EDIT_FIELD_Y + 1,
                      w - 2,
                      EDIT_FIELD_H - 2,
                      3,
                      border);
  }

  char value[24];
  if (sourceField)
  {
    formatInputSource(_inputDraftSource, value, sizeof(value));
  }
  else
  {
    std::snprintf(value,
                  sizeof(value),
                  "%s",
                  _inputDraftInverted ? "NEGADA" : "DIRECTA");
  }

  const char *label = sourceField ? "FUENTE" : "LOGICA";
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(selected ? COLOR_WARNING : COLOR_MUTED, fill);
  tft.setCursor(x + 7, EDIT_FIELD_Y + 6);
  tft.print(label);

  drawInputValueRegion(x,
                       w,
                       EDIT_FIELD_Y,
                       EDIT_FIELD_H,
                       value,
                       selected);

  if (sourceField)
  {
    std::snprintf(_inputSourceCache,
                  sizeof(_inputSourceCache),
                  "%s",
                  value);
  }
  else
  {
    _inputInvertedCache = _inputDraftInverted;
  }
  _inputFocusCache = _inputEditFocus;
  _inputEditorCacheValid = true;
}

void RuntimeUIFBDMapUnified::drawInputEditorFull()
{
  clearContentArea();

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  updateTextField(tft,
                  CONTENT_X + 8,
                  CONTENT_Y + 10,
                  48,
                  "LEFT/RIGHT CAMPO   UP/DOWN CAMBIAR",
                  COLOR_MUTED,
                  COLOR_PANEL);

  drawInputEditorField(InputEditField::Source);
  drawInputEditorField(InputEditField::Logic);

  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  const char *role = definition != nullptr
                         ? _model->inputRole(definition->type,
                                             _detailInputIndex)
                         : nullptr;
  char inputText[24];
  if (role != nullptr && role[0] != '\0')
  {
    std::snprintf(inputText, sizeof(inputText), "ENTRADA: %s", role);
  }
  else
  {
    std::snprintf(inputText,
                  sizeof(inputText),
                  "ENTRADA: IN%u",
                  static_cast<unsigned>(_detailInputIndex + 1U));
  }

  updateTextField(tft,
                  CONTENT_X + 8,
                  CONTENT_Y + 91,
                  36,
                  inputText,
                  COLOR_TEXT,
                  COLOR_PANEL);
  updateTextField(tft,
                  CONTENT_X + 8,
                  CONTENT_Y + 108,
                  46,
                  _inputDraftInverted
                      ? "RESULTADO: FUENTE NEGADA"
                      : "RESULTADO: FUENTE DIRECTA",
                  COLOR_MUTED,
                  COLOR_PANEL);

  drawEditorFooter();
}

void RuntimeUIFBDMapUnified::renderEditInput(bool force)
{
  if (force || !_inputEditorCacheValid)
  {
    drawInputEditorFull();
  }
}

void RuntimeUIFBDMapUnified::handleEditInputInput()
{
  finishInputApply();
  if (_view != View::EditInput || _awaitingApply)
  {
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    JWPLC_Display.notifyActivity();
    cancelInputEdit();
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    JWPLC_Display.notifyActivity();
    applyInputEdit();
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_LEFT) ||
      JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    const InputEditField previous = _inputEditFocus;
    _inputEditFocus =
        _inputEditFocus == InputEditField::Source
            ? InputEditField::Logic
            : InputEditField::Source;
    _editFeedback = EditFeedback::None;
    JWPLC_Display.notifyActivity();
    drawInputEditorField(previous);
    drawInputEditorField(_inputEditFocus);
    drawEditorFooter();
    return;
  }

  const bool up = JWPLC_Buttons.pressed(BTN_UP);
  const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
  if (!up && !down)
  {
    return;
  }

  _editFeedback = EditFeedback::None;
  JWPLC_Display.notifyActivity();

  if (_inputEditFocus == InputEditField::Source)
  {
    const uint16_t count = inputSourceChoiceCount();
    if (count > 0)
    {
      if (up)
      {
        _inputSourceCursor = static_cast<uint16_t>(
            (_inputSourceCursor + 1U) % count);
      }
      else
      {
        _inputSourceCursor =
            _inputSourceCursor == 0
                ? static_cast<uint16_t>(count - 1U)
                : static_cast<uint16_t>(_inputSourceCursor - 1U);
      }
      _inputDraftSource = inputSourceValueFromCursor(_inputSourceCursor);

      char value[24];
      formatInputSource(_inputDraftSource, value, sizeof(value));
      drawInputValueRegion(SOURCE_X,
                           SOURCE_W,
                           EDIT_FIELD_Y,
                           EDIT_FIELD_H,
                           value,
                           true);
      std::snprintf(_inputSourceCache,
                    sizeof(_inputSourceCache),
                    "%s",
                    value);
    }
  }
  else
  {
    _inputDraftInverted = !_inputDraftInverted;
    const char *value = _inputDraftInverted ? "NEGADA" : "DIRECTA";
    drawInputValueRegion(LOGIC_X,
                         LOGIC_W,
                         EDIT_FIELD_Y,
                         EDIT_FIELD_H,
                         value,
                         true);
    _inputInvertedCache = _inputDraftInverted;

    updateTextField(JWPLC_Display.tft(),
                    CONTENT_X + 8,
                    CONTENT_Y + 108,
                    46,
                    _inputDraftInverted
                        ? "RESULTADO: FUENTE NEGADA"
                        : "RESULTADO: FUENTE DIRECTA",
                    COLOR_MUTED,
                    COLOR_PANEL);
  }

  drawEditorFooter();
}
