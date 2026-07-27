#include "RuntimeUIFBDMapV13.h"

#include <cstdio>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
static constexpr int16_t PANEL_X_V13 = 2;
static constexpr int16_t PANEL_Y_V13 = 27;
static constexpr int16_t PANEL_W_V13 = 316;
static constexpr int16_t PANEL_H_V13 = 142;

static constexpr int16_t LOGO_FIELD_X0 = 10;
static constexpr int16_t LOGO_FIELD_X1 = 108;
static constexpr int16_t LOGO_FIELD_X2 = 206;
static constexpr int16_t LOGO_FIELD_W0 = 94;
static constexpr int16_t LOGO_FIELD_W1 = 94;
static constexpr int16_t LOGO_FIELD_W2 = 104;

static constexpr int16_t WIZARD_FIELD_Y = 42;
static constexpr int16_t WIZARD_FIELD_H = 24;
static constexpr int16_t EXISTING_FIELD_Y = 103;
static constexpr int16_t EXISTING_FIELD_H = 38;

static constexpr int16_t TON_PANEL_X_V13 = 214;
static constexpr int16_t TON_PANEL_Y_V13 = 101;
static constexpr int16_t TON_PANEL_W_V13 = 80;
static constexpr int16_t TON_PANEL_H_V13 = 31;

static constexpr uint32_t LOGO_MINIMUM_TIME_MS = 20UL;
}

RuntimeUIFBDMapV13::RuntimeUIFBDMapV13()
    : RuntimeUIFBDMapV12(),
      _wizardLogoInitialized(false),
      _wizardLogoEditing(false),
      _wizardMajor(1),
      _wizardMinor(0),
      _wizardBase(LogoTimeBase::Seconds),
      _wizardFocus(LogoField::Major),
      _wizardMajorBackup(1),
      _wizardMinorBackup(0),
      _wizardBaseBackup(LogoTimeBase::Seconds),
      _existingLogoInitialized(false),
      _existingMajor(0),
      _existingMinor(0),
      _existingBase(LogoTimeBase::Seconds),
      _existingFocus(LogoField::Major),
      _existingOriginalMs(0),
      _existingOriginalResource(TON_BASE_SECONDS),
      _lastExistingElapsedRefreshMs(0),
      _lastDetailOverlayRefreshMs(0)
{
}

RuntimeUIFBDMapV13::LogoTimeBase
RuntimeUIFBDMapV13::baseFromResource(uint16_t resource)
{
  switch (resource & 0x0003U)
  {
  case TON_BASE_MINUTES:
    return LogoTimeBase::Minutes;
  case TON_BASE_HOURS:
    return LogoTimeBase::Hours;
  case TON_BASE_SECONDS:
  default:
    return LogoTimeBase::Seconds;
  }
}

uint16_t RuntimeUIFBDMapV13::resourceFromBase(LogoTimeBase base)
{
  return static_cast<uint16_t>(base);
}

const char *RuntimeUIFBDMapV13::baseText(LogoTimeBase base)
{
  switch (base)
  {
  case LogoTimeBase::Minutes:
    return "m";
  case LogoTimeBase::Hours:
    return "h";
  case LogoTimeBase::Seconds:
  default:
    return "s";
  }
}

const char *RuntimeUIFBDMapV13::majorLabel(LogoTimeBase base)
{
  switch (base)
  {
  case LogoTimeBase::Minutes:
    return "MIN";
  case LogoTimeBase::Hours:
    return "HORA";
  case LogoTimeBase::Seconds:
  default:
    return "SEG";
  }
}

const char *RuntimeUIFBDMapV13::minorLabel(LogoTimeBase base)
{
  return base == LogoTimeBase::Seconds ? "CENT" :
         base == LogoTimeBase::Minutes ? "SEG" : "MIN";
}

uint32_t RuntimeUIFBDMapV13::minorMaximum(LogoTimeBase base)
{
  return base == LogoTimeBase::Seconds ? 99UL : 59UL;
}

uint32_t RuntimeUIFBDMapV13::maximumMilliseconds(LogoTimeBase base)
{
  switch (base)
  {
  case LogoTimeBase::Minutes:
    return 99UL * 60000UL + 59UL * 1000UL;
  case LogoTimeBase::Hours:
    return 99UL * 3600000UL + 59UL * 60000UL;
  case LogoTimeBase::Seconds:
  default:
    return 99UL * 1000UL + 99UL * 10UL;
  }
}

RuntimeUIFBDMapV13::LogoTimeBase RuntimeUIFBDMapV13::effectiveBase(
    uint32_t milliseconds,
    uint16_t resource)
{
  const LogoTimeBase requested = baseFromResource(resource);
  const auto exactFor = [milliseconds](LogoTimeBase base) -> bool
  {
    const uint32_t quantum = base == LogoTimeBase::Seconds ? 10UL :
                             base == LogoTimeBase::Minutes ? 1000UL : 60000UL;
    return milliseconds <= maximumMilliseconds(base) &&
           milliseconds % quantum == 0;
  };

  if (exactFor(requested))
  {
    return requested;
  }
  if (exactFor(LogoTimeBase::Seconds))
  {
    return LogoTimeBase::Seconds;
  }
  if (exactFor(LogoTimeBase::Minutes))
  {
    return LogoTimeBase::Minutes;
  }
  return LogoTimeBase::Hours;
}

uint32_t RuntimeUIFBDMapV13::encodeMilliseconds(uint32_t major,
                                                uint32_t minor,
                                                LogoTimeBase base)
{
  switch (base)
  {
  case LogoTimeBase::Minutes:
    return major * 60000UL + minor * 1000UL;
  case LogoTimeBase::Hours:
    return major * 3600000UL + minor * 60000UL;
  case LogoTimeBase::Seconds:
  default:
    return major * 1000UL + minor * 10UL;
  }
}

void RuntimeUIFBDMapV13::decodeMilliseconds(uint32_t milliseconds,
                                            LogoTimeBase base,
                                            uint32_t &major,
                                            uint32_t &minor)
{
  uint32_t total = 0;
  uint32_t radix = 60UL;
  switch (base)
  {
  case LogoTimeBase::Minutes:
    total = milliseconds / 1000UL;
    radix = 60UL;
    break;
  case LogoTimeBase::Hours:
    total = milliseconds / 60000UL;
    radix = 60UL;
    break;
  case LogoTimeBase::Seconds:
  default:
    total = milliseconds / 10UL;
    radix = 100UL;
    break;
  }

  const uint32_t maximumUnits = 99UL * radix + (radix - 1UL);
  if (total > maximumUnits)
  {
    total = maximumUnits;
  }
  major = total / radix;
  minor = total % radix;
}

void RuntimeUIFBDMapV13::formatLogoTime(uint32_t major,
                                        uint32_t minor,
                                        LogoTimeBase base,
                                        char *destination,
                                        size_t capacity)
{
  if (destination == nullptr || capacity == 0)
  {
    return;
  }
  std::snprintf(destination,
                capacity,
                "%02lu:%02lu%s",
                static_cast<unsigned long>(major),
                static_cast<unsigned long>(minor),
                baseText(base));
}

void RuntimeUIFBDMapV13::formatMilliseconds(uint32_t milliseconds,
                                            uint16_t resource,
                                            char *destination,
                                            size_t capacity)
{
  const LogoTimeBase base = effectiveBase(milliseconds, resource);
  uint32_t major = 0;
  uint32_t minor = 0;
  decodeMilliseconds(milliseconds, base, major, minor);
  formatLogoTime(major, minor, base, destination, capacity);
}

void RuntimeUIFBDMapV13::resetWizardLogoState()
{
  _wizardLogoInitialized = wizardIsTonV11();
  _wizardLogoEditing = false;
  _wizardMajor = 1;
  _wizardMinor = 0;
  _wizardBase = LogoTimeBase::Seconds;
  _wizardFocus = LogoField::Major;
  _wizardMajorBackup = _wizardMajor;
  _wizardMinorBackup = _wizardMinor;
  _wizardBaseBackup = _wizardBase;
}

void RuntimeUIFBDMapV13::resetV13State()
{
  _wizardLogoInitialized = false;
  _wizardLogoEditing = false;
  _wizardMajor = 1;
  _wizardMinor = 0;
  _wizardBase = LogoTimeBase::Seconds;
  _wizardFocus = LogoField::Major;
  _wizardMajorBackup = 1;
  _wizardMinorBackup = 0;
  _wizardBaseBackup = LogoTimeBase::Seconds;

  _existingLogoInitialized = false;
  _existingMajor = 0;
  _existingMinor = 0;
  _existingBase = LogoTimeBase::Seconds;
  _existingFocus = LogoField::Major;
  _existingOriginalMs = 0;
  _existingOriginalResource = TON_BASE_SECONDS;
  _lastExistingElapsedRefreshMs = 0;
  _lastDetailOverlayRefreshMs = 0;
}

void RuntimeUIFBDMapV13::attach(RuntimeUIV2ReadModel &model)
{
  resetV13State();
  RuntimeUIFBDMapV12::attach(model);
}

void RuntimeUIFBDMapV13::detach()
{
  RuntimeUIFBDMapV12::detach();
  resetV13State();
}

void RuntimeUIFBDMapV13::enter()
{
  resetV13State();
  RuntimeUIFBDMapV12::enter();
}

void RuntimeUIFBDMapV13::exit()
{
  RuntimeUIFBDMapV12::exit();
  resetV13State();
}

void RuntimeUIFBDMapV13::forceRedraw()
{
  _lastExistingElapsedRefreshMs = 0;
  _lastDetailOverlayRefreshMs = 0;
  RuntimeUIFBDMapV12::forceRedraw();
}

void RuntimeUIFBDMapV13::refresh(const JWPLC_IOState *io,
                                 const JWPLC_RTCState *rtc)
{
  if (parameterEditorActiveForExtension())
  {
    refreshExistingLogoEditor();
    return;
  }

  RuntimeUIFBDMapV12::refresh(io, rtc);

  // El editor V5 puede activarse durante el refresh anterior al pulsar OK en el
  // detalle. Se sustituye inmediatamente por la pantalla LOGO! de V13.
  if (parameterEditorActiveForExtension())
  {
    beginExistingLogoEditor();
    return;
  }

  drawDetailLogoOverlay(false);
}

void RuntimeUIFBDMapV13::enterConfigureMain()
{
  if (wizardIsTonV11())
  {
    resetWizardLogoState();
  }
  RuntimeUIFBDMapV11::enterConfigureMain();
}

void RuntimeUIFBDMapV13::formatParameterValue(uint8_t parameterIndex,
                                              char *destination,
                                              size_t capacity) const
{
  if (!wizardIsTonV11())
  {
    RuntimeUIFBDMapV11::formatParameterValue(parameterIndex,
                                             destination,
                                             capacity);
    return;
  }

  (void)parameterIndex;
  formatLogoTime(_wizardMajor,
                 _wizardMinor,
                 _wizardBase,
                 destination,
                 capacity);
}

void RuntimeUIFBDMapV13::beginWizardLogoEdit()
{
  if (!_wizardLogoInitialized)
  {
    resetWizardLogoState();
  }
  _wizardMajorBackup = _wizardMajor;
  _wizardMinorBackup = _wizardMinor;
  _wizardBaseBackup = _wizardBase;
  _wizardFocus = LogoField::Major;
  _wizardLogoEditing = true;
}

void RuntimeUIFBDMapV13::drawWizardLogoFields()
{
  char major[24];
  char minor[24];
  char base[24];
  std::snprintf(major,
                sizeof(major),
                "%s <%02lu>",
                majorLabel(_wizardBase),
                static_cast<unsigned long>(_wizardMajor));
  std::snprintf(minor,
                sizeof(minor),
                "%s <%02lu>",
                minorLabel(_wizardBase),
                static_cast<unsigned long>(_wizardMinor));
  std::snprintf(base,
                sizeof(base),
                "BASE <%s>",
                baseText(_wizardBase));

  drawMenuButton(JWPLC_Display.tft(),
                 LOGO_FIELD_X0,
                 WIZARD_FIELD_Y,
                 LOGO_FIELD_W0,
                 WIZARD_FIELD_H,
                 major,
                 _wizardFocus == LogoField::Major);
  drawMenuButton(JWPLC_Display.tft(),
                 LOGO_FIELD_X1,
                 WIZARD_FIELD_Y,
                 LOGO_FIELD_W1,
                 WIZARD_FIELD_H,
                 minor,
                 _wizardFocus == LogoField::Minor);
  drawMenuButton(JWPLC_Display.tft(),
                 LOGO_FIELD_X2,
                 WIZARD_FIELD_Y,
                 LOGO_FIELD_W2,
                 WIZARD_FIELD_H,
                 base,
                 _wizardFocus == LogoField::Base);
}

void RuntimeUIFBDMapV13::drawParameterEditScreen()
{
  if (!wizardIsTonV11())
  {
    RuntimeUIFBDMapV11::drawParameterEditScreen();
    return;
  }

  if (!_wizardLogoEditing)
  {
    beginWizardLogoEdit();
  }

  drawConfigHeader("PARAMETRO: T");
  drawWizardLogoFields();
  drawFixedContext(contextualSource());
  drawConfigFooter("LEFT/RIGHT CAMPO   OK ACEPTAR");
}

bool RuntimeUIFBDMapV13::handleLogoFields(uint32_t &major,
                                          uint32_t &minor,
                                          LogoTimeBase &base,
                                          LogoField &focus,
                                          bool wizardScreen)
{
  const bool left = JWPLC_Buttons.pressed(BTN_LEFT);
  const bool right = !left && JWPLC_Buttons.pressed(BTN_RIGHT);
  if (left || right)
  {
    uint8_t field = static_cast<uint8_t>(focus);
    field = right
                ? static_cast<uint8_t>((field + 1U) % 3U)
                : (field == 0 ? 2U : static_cast<uint8_t>(field - 1U));
    focus = static_cast<LogoField>(field);
    restoreWizardRepeatProfileV11();
    JWPLC_Display.notifyActivity();
    return true;
  }

  if (focus == LogoField::Base)
  {
    restoreWizardRepeatProfileV11();
    const bool up = JWPLC_Buttons.pressed(BTN_UP);
    const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
    if (!up && !down)
    {
      return false;
    }

    uint8_t value = static_cast<uint8_t>(base);
    value = down
                ? static_cast<uint8_t>((value + 1U) % 3U)
                : (value == 0 ? 2U : static_cast<uint8_t>(value - 1U));
    base = static_cast<LogoTimeBase>(value);
    const uint32_t maximumMinor = minorMaximum(base);
    if (minor > maximumMinor)
    {
      minor = maximumMinor;
    }
    JWPLC_Display.notifyActivity();
    return true;
  }

  uint32_t *target = focus == LogoField::Major ? &major : &minor;
  const uint32_t maximum =
      focus == LogoField::Major ? 99UL : minorMaximum(base);
  const bool changed = JWPLC_Buttons.applyAxis(
      target,
      0,
      maximum,
      BTN_DOWN,
      BTN_UP,
      false,
      true);
  if (changed)
  {
    JWPLC_Display.notifyActivity();
  }
  (void)wizardScreen;
  return changed;
}

bool RuntimeUIFBDMapV13::handleParameterEditInput()
{
  if (!wizardIsTonV11())
  {
    return RuntimeUIFBDMapV11::handleParameterEditInput();
  }

  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    returnFromParameterEdit(false);
    return true;
  }
  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    returnFromParameterEdit(true);
    return true;
  }

  if (!handleLogoFields(_wizardMajor,
                        _wizardMinor,
                        _wizardBase,
                        _wizardFocus,
                        true))
  {
    return false;
  }

  drawWizardLogoFields();
  drawConfigFooter("LEFT/RIGHT CAMPO   OK ACEPTAR");
  return true;
}

void RuntimeUIFBDMapV13::returnFromParameterEdit(bool accept)
{
  if (wizardIsTonV11())
  {
    restoreWizardRepeatProfileV11();
    if (!accept)
    {
      _wizardMajor = _wizardMajorBackup;
      _wizardMinor = _wizardMinorBackup;
      _wizardBase = _wizardBaseBackup;
    }
    _wizardLogoEditing = false;
  }
  RuntimeUIFBDMapV12::returnFromParameterEdit(accept);
}

void RuntimeUIFBDMapV13::requestWizardCreateV11()
{
  if (!wizardIsTonV11())
  {
    RuntimeUIFBDMapV11::requestWizardCreateV11();
    return;
  }

  const uint32_t parameter = encodeMilliseconds(_wizardMajor,
                                                _wizardMinor,
                                                _wizardBase);
  if (parameter < LOGO_MINIMUM_TIME_MS)
  {
    drawConfigFooter("T MINIMO 00:02s", COLOR_WARNING);
    return;
  }

  requestLogoTonCreateV11(parameter, resourceFromBase(_wizardBase));
}

void RuntimeUIFBDMapV13::beginExistingLogoEditor()
{
  const LogicV2BlockRecord *definition = selectedTonForExtension();
  if (definition == nullptr)
  {
    return;
  }

  _existingOriginalMs = definition->parameter;
  _existingOriginalResource = definition->resource;
  _existingBase = effectiveBase(definition->parameter,
                                definition->resource);
  decodeMilliseconds(definition->parameter,
                     _existingBase,
                     _existingMajor,
                     _existingMinor);
  _existingFocus = LogoField::Major;
  _existingLogoInitialized = true;
  _lastExistingElapsedRefreshMs = 0;
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
  drawExistingLogoScreen();
}

void RuntimeUIFBDMapV13::drawExistingLogoFields()
{
  char major[24];
  char minor[24];
  char base[24];
  std::snprintf(major,
                sizeof(major),
                "%s <%02lu>",
                majorLabel(_existingBase),
                static_cast<unsigned long>(_existingMajor));
  std::snprintf(minor,
                sizeof(minor),
                "%s <%02lu>",
                minorLabel(_existingBase),
                static_cast<unsigned long>(_existingMinor));
  std::snprintf(base,
                sizeof(base),
                "BASE <%s>",
                baseText(_existingBase));

  drawMenuButton(JWPLC_Display.tft(),
                 LOGO_FIELD_X0,
                 EXISTING_FIELD_Y,
                 LOGO_FIELD_W0,
                 EXISTING_FIELD_H,
                 major,
                 _existingFocus == LogoField::Major);
  drawMenuButton(JWPLC_Display.tft(),
                 LOGO_FIELD_X1,
                 EXISTING_FIELD_Y,
                 LOGO_FIELD_W1,
                 EXISTING_FIELD_H,
                 minor,
                 _existingFocus == LogoField::Minor);
  drawMenuButton(JWPLC_Display.tft(),
                 LOGO_FIELD_X2,
                 EXISTING_FIELD_Y,
                 LOGO_FIELD_W2,
                 EXISTING_FIELD_H,
                 base,
                 _existingFocus == LogoField::Base);
}

void RuntimeUIFBDMapV13::drawExistingActual()
{
  char actual[20];
  char line[36];
  formatMilliseconds(_existingOriginalMs,
                     _existingOriginalResource,
                     actual,
                     sizeof(actual));
  std::snprintf(line, sizeof(line), "T ACTUAL   %s", actual);
  drawFieldLabel(JWPLC_Display.tft(),
                 22,
                 50,
                 line,
                 COLOR_TEXT,
                 COLOR_PANEL);
}

void RuntimeUIFBDMapV13::drawExistingElapsed(bool force)
{
  const uint32_t nowMs = millis();
  if (!force &&
      static_cast<uint32_t>(nowMs - _lastExistingElapsedRefreshMs) <
          EXISTING_ELAPSED_REFRESH_MS)
  {
    return;
  }
  _lastExistingElapsedRefreshMs = nowMs;

  RuntimeUIV2ReadModel *model = readModelV11();
  if (model == nullptr)
  {
    return;
  }

  char elapsed[20];
  char line[36];
  formatMilliseconds(
      model->tonElapsedMs(selectedBlockIndexForExtension(), nowMs),
      resourceFromBase(_existingBase),
      elapsed,
      sizeof(elapsed));
  std::snprintf(line, sizeof(line), "Ta LECTURA %s", elapsed);

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(22, 70, 276, 12, COLOR_PANEL);
  drawFieldLabel(tft,
                 22,
                 72,
                 line,
                 (model->tonTiming(selectedBlockIndexForExtension()) ||
                  model->blockValue(selectedBlockIndexForExtension()))
                     ? COLOR_OK
                     : COLOR_MUTED,
                 COLOR_PANEL);
}

void RuntimeUIFBDMapV13::drawExistingFooter(const char *text,
                                            uint16_t color)
{
  updateTextField(JWPLC_Display.tft(),
                  78,
                  154,
                  28,
                  text,
                  color,
                  COLOR_PANEL);
}

void RuntimeUIFBDMapV13::drawExistingLogoScreen()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "EDITAR T LOGO");
  tft.fillRect(PANEL_X_V13,
               PANEL_Y_V13,
               PANEL_W_V13,
               PANEL_H_V13,
               COLOR_PANEL);
  tft.drawRect(PANEL_X_V13,
               PANEL_Y_V13,
               PANEL_W_V13,
               PANEL_H_V13,
               COLOR_BORDER);

  char headerInfo[24];
  std::snprintf(headerInfo,
                sizeof(headerInfo),
                "B%02u TON  __:__",
                static_cast<unsigned>(selectedBlockIndexForExtension()));
  updateTextField(tft,
                  112,
                  8,
                  20,
                  headerInfo,
                  COLOR_MUTED,
                  COLOR_PANEL);

  drawExistingActual();
  drawExistingElapsed(true);
  drawExistingLogoFields();
  drawExistingFooter("OK GUARDAR   ESC CANCELAR", COLOR_MUTED);
}

void RuntimeUIFBDMapV13::refreshExistingLogoEditor()
{
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
  if (!_existingLogoInitialized)
  {
    beginExistingLogoEditor();
    return;
  }

  if (tonParameterApplyCompletedForExtension())
  {
    const bool success = tonParameterApplySucceededForExtension();
    finishTonParameterApplyForExtension(success);
    if (success)
    {
      _existingLogoInitialized = false;
      _lastDetailOverlayRefreshMs = 0;
      return;
    }
    drawExistingFooter("ERROR AL APLICAR", COLOR_ERROR);
  }

  drawExistingElapsed(false);
  if (tonParameterApplyPendingForExtension())
  {
    return;
  }

  if (consumeInputReleaseGateForExtension())
  {
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    cancelTonParameterEditorForExtension();
    _existingLogoInitialized = false;
    _lastDetailOverlayRefreshMs = 0;
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    const uint32_t parameter = encodeMilliseconds(_existingMajor,
                                                  _existingMinor,
                                                  _existingBase);
    JWPLC_Display.notifyActivity();
    if (parameter < LOGO_MINIMUM_TIME_MS)
    {
      drawExistingFooter("T MINIMO 00:02s", COLOR_WARNING);
      return;
    }

    if (!requestTonParameterApplyForExtension(
            parameter,
            resourceFromBase(_existingBase)))
    {
      drawExistingFooter("PARAMETRO NO VALIDO", COLOR_ERROR);
      return;
    }
    drawExistingFooter("APLICANDO CAMBIOS...", COLOR_WARNING);
    return;
  }

  if (!handleLogoFields(_existingMajor,
                        _existingMinor,
                        _existingBase,
                        _existingFocus,
                        false))
  {
    return;
  }

  drawExistingLogoFields();
  drawExistingElapsed(true);
  drawExistingFooter("OK GUARDAR   ESC CANCELAR", COLOR_MUTED);
}

void RuntimeUIFBDMapV13::drawDetailLogoOverlay(bool force)
{
  if (!detailModeActiveV11())
  {
    return;
  }

  RuntimeUIV2ReadModel *model = readModelV11();
  const LogicV2BlockRecord *definition = selectedBlockV11();
  if (model == nullptr ||
      definition == nullptr ||
      definition->type != LogicV2BlockType::Ton)
  {
    return;
  }

  const uint32_t nowMs = millis();
  if (!force &&
      static_cast<uint32_t>(nowMs - _lastDetailOverlayRefreshMs) <
          DETAIL_OVERLAY_REFRESH_MS)
  {
    return;
  }
  _lastDetailOverlayRefreshMs = nowMs;

  char configured[16];
  char elapsed[16];
  formatMilliseconds(definition->parameter,
                     definition->resource,
                     configured,
                     sizeof(configured));
  formatMilliseconds(model->tonElapsedMs(selectedBlockIndexV11(), nowMs),
                     definition->resource,
                     elapsed,
                     sizeof(elapsed));

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(TON_PANEL_X_V13 + 3,
               TON_PANEL_Y_V13 + 3,
               TON_PANEL_W_V13 - 6,
               TON_PANEL_H_V13 - 5,
               COLOR_BACKGROUND);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(TON_PANEL_X_V13 + 4, TON_PANEL_Y_V13 + 4);
  tft.print("T ");
  tft.print(configured);

  const bool active = model->tonTiming(selectedBlockIndexV11()) ||
                      model->blockValue(selectedBlockIndexV11());
  tft.setTextColor(active ? COLOR_OK : COLOR_MUTED,
                   COLOR_BACKGROUND);
  tft.setCursor(TON_PANEL_X_V13 + 4, TON_PANEL_Y_V13 + 19);
  tft.print("Ta ");
  tft.print(elapsed);
}
