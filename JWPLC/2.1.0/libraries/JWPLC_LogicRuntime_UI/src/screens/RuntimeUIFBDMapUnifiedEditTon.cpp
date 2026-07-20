#include "RuntimeUIFBDMapUnified.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
static constexpr int16_t FIELD_X[3] = {10, 112, 214};
static constexpr int16_t FIELD_W[3] = {95, 95, 96};
static constexpr int16_t FIELD_Y = 104;
static constexpr int16_t FIELD_H = 39;
static constexpr uint32_t TON_MINIMUM_MS = 20UL;
}

void RuntimeUIFBDMapUnified::beginTonEdit()
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr ||
      definition->type != LogicV2BlockType::Ton ||
      !_editSession.begin())
  {
    return;
  }

  loadTonDraft();
  _editFeedback = EditFeedback::None;
  _awaitingApply = false;
  _applyRequested = false;
  _applyCompleted = false;
  invalidateEditorCaches();
  transitionTo(View::EditTon);
}

void RuntimeUIFBDMapUnified::cancelTonEdit()
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }
  _awaitingApply = false;
  _applyRequested = false;
  _applyCompleted = false;
  _editFeedback = EditFeedback::None;
  _detailFocus = DetailFocus::Parameters;
  transitionTo(View::Detail);
}

void RuntimeUIFBDMapUnified::applyTonEdit()
{
  if (_awaitingApply || !_editSession.active())
  {
    return;
  }

  uint32_t parameterMs = tonDraftMilliseconds();
  if (parameterMs < TON_MINIMUM_MS)
  {
    parameterMs = TON_MINIMUM_MS;
  }

  const bool prepared =
      _editSession.setBlockParameter(_selectedIndex, parameterMs) &&
      _editSession.setBlockResource(
          _selectedIndex,
          tonResourceFromBase(_tonDraft.base));
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

void RuntimeUIFBDMapUnified::finishTonApply()
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
  _detailFocus = DetailFocus::Parameters;
  invalidateDetailCache();
  transitionTo(View::Detail);
}

void RuntimeUIFBDMapUnified::loadTonDraft()
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr)
  {
    return;
  }

  _tonDraft.originalMs = definition->parameter;
  _tonDraft.originalResource = definition->resource;
  _tonDraft.base = tonBaseFromResource(definition->resource);
  _tonDraft.focus = TonField::Major;

  const uint32_t milliseconds =
      definition->parameter < TON_MINIMUM_MS
          ? TON_MINIMUM_MS
          : definition->parameter;

  switch (_tonDraft.base)
  {
  case TonBase::Minutes:
  {
    const uint32_t totalSeconds = milliseconds / 1000UL;
    _tonDraft.major = totalSeconds / 60UL;
    _tonDraft.minor = totalSeconds % 60UL;
    break;
  }
  case TonBase::Hours:
  {
    const uint32_t totalMinutes = milliseconds / 60000UL;
    _tonDraft.major = totalMinutes / 60UL;
    _tonDraft.minor = totalMinutes % 60UL;
    break;
  }
  case TonBase::Seconds:
  default:
  {
    const uint32_t totalCentiseconds = milliseconds / 10UL;
    _tonDraft.major = totalCentiseconds / 100UL;
    _tonDraft.minor = totalCentiseconds % 100UL;
    break;
  }
  }

  if (_tonDraft.major > 99UL)
  {
    _tonDraft.major = 99UL;
  }
}

uint32_t RuntimeUIFBDMapUnified::tonDraftMilliseconds() const
{
  switch (_tonDraft.base)
  {
  case TonBase::Minutes:
    return (_tonDraft.major * 60UL + _tonDraft.minor) * 1000UL;
  case TonBase::Hours:
    return (_tonDraft.major * 60UL + _tonDraft.minor) * 60000UL;
  case TonBase::Seconds:
  default:
    return (_tonDraft.major * 100UL + _tonDraft.minor) * 10UL;
  }
}

uint16_t RuntimeUIFBDMapUnified::tonResourceFromBase(TonBase base)
{
  return static_cast<uint16_t>(base);
}

uint32_t RuntimeUIFBDMapUnified::tonBaseResolutionMs(TonBase base)
{
  switch (base)
  {
  case TonBase::Minutes:
    return 1000UL;
  case TonBase::Hours:
    return 60000UL;
  case TonBase::Seconds:
  default:
    return 10UL;
  }
}

uint32_t RuntimeUIFBDMapUnified::tonMinorMaximum(TonBase base)
{
  return base == TonBase::Seconds ? 99UL : 59UL;
}

bool RuntimeUIFBDMapUnified::changeTonBase(bool forward)
{
  const uint8_t current = static_cast<uint8_t>(_tonDraft.base);
  const uint8_t candidateValue = forward
                                     ? static_cast<uint8_t>((current + 1U) % 3U)
                                     : static_cast<uint8_t>(
                                           current == 0 ? 2U : current - 1U);
  const TonBase candidate = static_cast<TonBase>(candidateValue);
  const uint32_t milliseconds = tonDraftMilliseconds();
  const uint32_t resolution = tonBaseResolutionMs(candidate);

  if (resolution == 0 || milliseconds % resolution != 0)
  {
    _editFeedback = EditFeedback::InvalidBase;
    drawEditorFooter();
    return false;
  }

  uint32_t major = 0;
  uint32_t minor = 0;
  if (candidate == TonBase::Seconds)
  {
    const uint32_t totalCentiseconds = milliseconds / 10UL;
    major = totalCentiseconds / 100UL;
    minor = totalCentiseconds % 100UL;
  }
  else if (candidate == TonBase::Minutes)
  {
    const uint32_t totalSeconds = milliseconds / 1000UL;
    major = totalSeconds / 60UL;
    minor = totalSeconds % 60UL;
  }
  else
  {
    const uint32_t totalMinutes = milliseconds / 60000UL;
    major = totalMinutes / 60UL;
    minor = totalMinutes % 60UL;
  }

  if (major > 99UL)
  {
    _editFeedback = EditFeedback::InvalidBase;
    drawEditorFooter();
    return false;
  }

  _tonDraft.base = candidate;
  _tonDraft.major = major;
  _tonDraft.minor = minor;
  _editFeedback = EditFeedback::None;
  return true;
}

const char *RuntimeUIFBDMapUnified::tonMajorLabel() const
{
  switch (_tonDraft.base)
  {
  case TonBase::Minutes:
    return "MIN";
  case TonBase::Hours:
    return "HORA";
  case TonBase::Seconds:
  default:
    return "SEG";
  }
}

const char *RuntimeUIFBDMapUnified::tonMinorLabel() const
{
  switch (_tonDraft.base)
  {
  case TonBase::Minutes:
    return "SEG";
  case TonBase::Hours:
    return "MIN";
  case TonBase::Seconds:
  default:
    return "CENT";
  }
}

void RuntimeUIFBDMapUnified::drawTonEditorField(TonField field)
{
  const uint8_t index = static_cast<uint8_t>(field);
  const int16_t x = FIELD_X[index];
  const int16_t w = FIELD_W[index];
  const bool selected = field == _tonDraft.focus;
  const uint16_t fill = selected ? COLOR_SELECTED : COLOR_PANEL;
  const uint16_t border = selected ? COLOR_WARNING : COLOR_BORDER;

  const char *label = "BASE";
  char value[12];
  if (field == TonField::Major)
  {
    label = tonMajorLabel();
    std::snprintf(value,
                  sizeof(value),
                  "<%02lu>",
                  static_cast<unsigned long>(_tonDraft.major));
  }
  else if (field == TonField::Minor)
  {
    label = tonMinorLabel();
    std::snprintf(value,
                  sizeof(value),
                  "<%02lu>",
                  static_cast<unsigned long>(_tonDraft.minor));
  }
  else
  {
    const char *baseText =
        _tonDraft.base == TonBase::Seconds
            ? "<s>"
            : (_tonDraft.base == TonBase::Minutes ? "<m>" : "<h>");
    std::snprintf(value, sizeof(value), "%s", baseText);
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRoundRect(x, FIELD_Y, w, FIELD_H, 4, fill);
  tft.drawRoundRect(x, FIELD_Y, w, FIELD_H, 4, border);
  if (selected)
  {
    tft.drawRoundRect(x + 1,
                      FIELD_Y + 1,
                      w - 2,
                      FIELD_H - 2,
                      3,
                      border);
  }

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(selected ? COLOR_WARNING : COLOR_MUTED, fill);
  tft.setCursor(x + 7, FIELD_Y + 5);
  tft.print(label);

  const int16_t valueWidth = static_cast<int16_t>(std::strlen(value) * 6U);
  const int16_t valueX = static_cast<int16_t>(x + (w - valueWidth) / 2);
  tft.setTextColor(selected ? COLOR_WARNING : COLOR_TEXT, fill);
  tft.setCursor(valueX, FIELD_Y + 22);
  tft.print(value);

  _tonMajorCache = _tonDraft.major;
  _tonMinorCache = _tonDraft.minor;
  _tonBaseCache = _tonDraft.base;
  _tonFocusCache = _tonDraft.focus;
  _tonEditorCacheValid = true;
}

void RuntimeUIFBDMapUnified::drawTonEditorElapsed(bool force)
{
  char elapsed[16];
  formatMillisecondsInBase(
      _model->tonElapsedMs(_selectedIndex, millis()),
      _tonDraft.base,
      elapsed,
      sizeof(elapsed));

  const bool colorOn =
      _model->tonTiming(_selectedIndex) ||
      _model->blockValue(_selectedIndex);

  if (!force &&
      _tonEditorCacheValid &&
      std::strcmp(_tonEditorElapsedCache, elapsed) == 0 &&
      _tonEditorElapsedColorCache == colorOn)
  {
    return;
  }

  updateTextField(JWPLC_Display.tft(),
                  CONTENT_X + 84,
                  CONTENT_Y + 58,
                  16,
                  elapsed,
                  colorOn ? COLOR_OK : COLOR_MUTED,
                  COLOR_PANEL);

  std::snprintf(_tonEditorElapsedCache,
                sizeof(_tonEditorElapsedCache),
                "%s",
                elapsed);
  _tonEditorElapsedColorCache = colorOn;
}

void RuntimeUIFBDMapUnified::drawTonEditorFull()
{
  clearContentArea();

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  updateTextField(tft,
                  CONTENT_X + 8,
                  CONTENT_Y + 9,
                  48,
                  "LEFT/RIGHT CAMPO   UP/DOWN CAMBIAR",
                  COLOR_MUTED,
                  COLOR_PANEL);

  char configured[16];
  formatMillisecondsInBase(
      tonDraftMilliseconds(),
      _tonDraft.base,
      configured,
      sizeof(configured));

  updateTextField(tft,
                  CONTENT_X + 8,
                  CONTENT_Y + 34,
                  18,
                  "T CONFIGURADO",
                  COLOR_TEXT,
                  COLOR_PANEL);
  updateTextField(tft,
                  CONTENT_X + 104,
                  CONTENT_Y + 34,
                  16,
                  configured,
                  COLOR_WARNING,
                  COLOR_PANEL);

  updateTextField(tft,
                  CONTENT_X + 8,
                  CONTENT_Y + 58,
                  18,
                  "Ta LECTURA",
                  COLOR_MUTED,
                  COLOR_PANEL);

  drawTonEditorElapsed(true);
  drawTonEditorField(TonField::Major);
  drawTonEditorField(TonField::Minor);
  drawTonEditorField(TonField::Base);
  drawEditorFooter();
}

void RuntimeUIFBDMapUnified::renderEditTon(bool force)
{
  if (force || !_tonEditorCacheValid)
  {
    drawTonEditorFull();
    return;
  }

  drawTonEditorElapsed(false);
}

void RuntimeUIFBDMapUnified::handleEditTonInput()
{
  finishTonApply();
  if (_view != View::EditTon || _awaitingApply)
  {
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    JWPLC_Display.notifyActivity();
    cancelTonEdit();
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    JWPLC_Display.notifyActivity();
    applyTonEdit();
    return;
  }

  // pressed() es latcheado y consumible: cada botón se consulta una sola vez.
  const bool goRight = JWPLC_Buttons.pressed(BTN_RIGHT);
  const bool goLeft = !goRight && JWPLC_Buttons.pressed(BTN_LEFT);
  if (goLeft || goRight)
  {
    const TonField previous = _tonDraft.focus;
    if (goRight)
    {
      _tonDraft.focus = static_cast<TonField>(
          (static_cast<uint8_t>(_tonDraft.focus) + 1U) % 3U);
    }
    else
    {
      const uint8_t current = static_cast<uint8_t>(_tonDraft.focus);
      _tonDraft.focus = static_cast<TonField>(current == 0 ? 2U : current - 1U);
    }

    _editFeedback = EditFeedback::None;
    JWPLC_Display.notifyActivity();
    drawTonEditorField(previous);
    drawTonEditorField(_tonDraft.focus);
    drawEditorFooter();
    return;
  }

  bool changed = false;
  if (_tonDraft.focus == TonField::Major)
  {
    changed = JWPLC_Buttons.applyAxis(
        _tonDraft.major,
        0,
        99,
        BTN_DOWN,
        BTN_UP,
        false,
        true);
  }
  else if (_tonDraft.focus == TonField::Minor)
  {
    changed = JWPLC_Buttons.applyAxis(
        _tonDraft.minor,
        0,
        tonMinorMaximum(_tonDraft.base),
        BTN_DOWN,
        BTN_UP,
        false,
        true);
  }
  else
  {
    const bool up = JWPLC_Buttons.pressed(BTN_UP);
    const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
    if (up || down)
    {
      changed = changeTonBase(up);
      if (changed)
      {
        JWPLC_Display.notifyActivity();
        drawTonEditorField(TonField::Major);
        drawTonEditorField(TonField::Minor);
        drawTonEditorField(TonField::Base);
        drawTonEditorElapsed(true);

        char configured[16];
        formatMillisecondsInBase(
            tonDraftMilliseconds(),
            _tonDraft.base,
            configured,
            sizeof(configured));
        updateTextField(JWPLC_Display.tft(),
                        CONTENT_X + 104,
                        CONTENT_Y + 34,
                        16,
                        configured,
                        COLOR_WARNING,
                        COLOR_PANEL);
        drawEditorFooter();
      }
      return;
    }
  }

  if (!changed)
  {
    return;
  }

  _editFeedback = EditFeedback::None;
  JWPLC_Display.notifyActivity();
  drawTonEditorField(_tonDraft.focus);

  char configured[16];
  formatMillisecondsInBase(
      tonDraftMilliseconds(),
      _tonDraft.base,
      configured,
      sizeof(configured));
  updateTextField(JWPLC_Display.tft(),
                  CONTENT_X + 104,
                  CONTENT_Y + 34,
                  16,
                  configured,
                  COLOR_WARNING,
                  COLOR_PANEL);
  drawEditorFooter();
}
