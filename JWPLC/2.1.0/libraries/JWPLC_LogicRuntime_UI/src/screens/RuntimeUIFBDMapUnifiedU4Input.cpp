#define JWPLC_UNIFIED_STRONG_IMPLEMENTATION
#include "RuntimeUIFBDMapUnifiedU4Internal.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;
using namespace JWPLCUnifiedU4;

extern "C" void jwplcUnifiedU4InputAnchor() {}

void RuntimeUIFBDMapUnified::handleWizardInput()
{
  ensureU4(this, _model);

  if (gU4.applying && _applyCompleted)
  {
    const bool success = _applySuccess;
    _applyCompleted = false;
    _applySuccess = false;
    _awaitingApply = false;
    gU4.applying = false;

    if (!success)
    {
      _editSession.cancel();
      gU4.error = true;
      _contentDirty = true;
      gateInputUntilRelease();
      return;
    }

    _editSession.cancel();
    gU4.error = false;
    gU4.addSelected = false;
    invalidateLayout();
    buildLayout();
    _selectedIndex = gU4.newIndex < _model->blockCount()
                         ? gU4.newIndex
                         : static_cast<uint16_t>(_model->blockCount() - 1U);
    normalizeSelection();
    ensureSelectionVisible();
    invalidateMapCache();
    gU4.previewDrawn = false;
    transitionTo(View::Map);
    return;
  }

  if (gU4.applying)
    return;

  if (_view == View::AddType)
  {
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
      transitionTo(View::Map);
      return;
    }
    if (JWPLC_Buttons.pressed(BTN_LEFT))
    {
      JWPLC_Display.notifyActivity();
      return;
    }
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
      gU4.sourceA = gU4.addOrigin < _model->blockCount()
                           ? gU4.addOrigin
                           : JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
      gU4.sourceB = JWPLC_LOGIC_V2_SOURCE_CONST_TRUE;
      gU4.resource = gU4.type == U4Type::DigitalOutput
                         ? firstAvailableOutput(_model)
                         : 0;
      gU4.tonMajor = 1;
      gU4.tonMinor = 0;
      gU4.tonBase = U4TonBase::Seconds;
      gU4.sourceInput = 0;
      gU4.parameterIndex = 0;
      gU4.error = false;
      normalizeMainFocus();
      transitionTo(View::Configure);
      return;
    }

    const bool up = JWPLC_Buttons.pressed(BTN_UP);
    const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
    if (up || down)
    {
      const U4Type previous = gU4.type;
      uint8_t index = static_cast<uint8_t>(gU4.type);
      const uint8_t count = static_cast<uint8_t>(U4Type::Count);
      index = up
                  ? (index == 0 ? static_cast<uint8_t>(count - 1U)
                                : static_cast<uint8_t>(index - 1U))
                  : static_cast<uint8_t>((index + 1U) % count);
      gU4.type = static_cast<U4Type>(index);
      drawTypeChoice(previous, false);
      drawTypeChoice(gU4.type, true);
      _headerDirty = true;
      JWPLC_Display.notifyActivity();
    }
    return;
  }

  if (_view == View::Configure)
  {
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
      transitionTo(View::AddType);
      return;
    }

    const bool left = JWPLC_Buttons.pressed(BTN_LEFT);
    const bool right = !left && JWPLC_Buttons.pressed(BTN_RIGHT);
    if (left || right)
    {
      const U4MainFocus previous = gU4.mainFocus;
      moveMainFocus(right);
      drawMainGroup(previous);
      drawMainGroup(gU4.mainFocus);

      if (gU4.mainFocus == U4MainFocus::Sources)
        drawContextPanel(_levels, _lanes, _maxLevel, CONTEXT_Y, CONTEXT_H, true, gU4.sourceA, nullptr);
      else if (gU4.mainFocus == U4MainFocus::Parameters)
      {
        char summary[48];
        if (gU4.type == U4Type::Ton)
        {
          char value[16];
          formatTonConfigured(value, sizeof(value));
          std::snprintf(summary, sizeof(summary), "T CONFIGURADO: %s", value);
        }
        else
        {
          char value[20];
          formatResource(value, sizeof(value));
          std::snprintf(summary, sizeof(summary), "RECURSO: %s", value);
        }
        drawContextPanel(_levels, _lanes, _maxLevel, CONTEXT_Y, CONTEXT_H, false, 0, summary, COLOR_ACCENT);
      }
      else
        drawContextPanel(_levels, _lanes, _maxLevel, CONTEXT_Y, CONTEXT_H, false, 0,
                         "LISTO PARA CREAR - RAM / SIN FRAM", COLOR_ACCENT);
      _headerDirty = true;
      JWPLC_Display.notifyActivity();
      return;
    }

    if (JWPLC_Buttons.pressed(BTN_OK))
    {
      if (gU4.mainFocus == U4MainFocus::Sources)
      {
        gU4.sourceInput = 0;
        transitionTo(View::SourceList);
      }
      else if (gU4.mainFocus == U4MainFocus::Parameters)
      {
        gU4.parameterIndex = 0;
        transitionTo(View::ParameterList);
      }
      else
      {
        if (!_editSession.begin())
        {
          gU4.error = true;
          drawFooter("ERROR AL ABRIR BORRADOR", COLOR_ERROR);
          return;
        }

        LogicV2InputLink inputs[2];
        uint8_t inputCount = 0;
        uint16_t resource = 0;
        uint32_t parameter = 0;

        switch (gU4.type)
        {
        case U4Type::DigitalInput:
          resource = gU4.resource;
          break;
        case U4Type::Not:
          inputs[0] = makeLink(gU4.sourceA);
          inputCount = 1;
          break;
        case U4Type::And2:
          inputs[0] = makeLink(gU4.sourceA);
          inputs[1] = makeLink(gU4.sourceB);
          inputCount = 2;
          break;
        case U4Type::Ton:
          inputs[0] = makeLink(gU4.sourceA);
          inputCount = 1;
          resource = static_cast<uint16_t>(gU4.tonBase);
          parameter = tonMilliseconds();
          if (parameter < 20UL) parameter = 20UL;
          break;
        case U4Type::DigitalOutput:
          inputs[0] = makeLink(gU4.sourceA);
          inputCount = 1;
          resource = gU4.resource;
          break;
        default:
          break;
        }

        uint16_t newIndex = 0;
        const bool prepared =
            resource != 0xFFU &&
            _editSession.appendBlock(logicType(gU4.type),
                                     inputCount > 0 ? inputs : nullptr,
                                     inputCount,
                                     resource,
                                     parameter,
                                     &newIndex);
        const bool valid = prepared &&
                           _editSession.validate() == LogicV2PrototypeError::None;
        if (!valid)
        {
          _editSession.cancel();
          gU4.error = true;
          drawFooter("CONFIGURACION NO VALIDA", COLOR_ERROR);
          return;
        }

        gU4.newIndex = newIndex;
        gU4.applying = true;
        gU4.error = false;
        _awaitingApply = true;
        _applyRequested = true;
        drawFooter("APLICANDO CAMBIOS...", COLOR_WARNING);
        JWPLC_Display.notifyActivity();
        gateInputUntilRelease();
      }
      return;
    }
    return;
  }

  if (_view == View::SourceList)
  {
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
      transitionTo(View::Configure);
      return;
    }
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
      gU4.sourceBackup = sourceValue(gU4.sourceInput);
      transitionTo(View::SourceEdit);
      return;
    }
    const bool up = JWPLC_Buttons.pressed(BTN_UP);
    const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
    const uint8_t count = sourceCountForType(gU4.type);
    if ((up || down) && count > 1)
    {
      const uint8_t previous = gU4.sourceInput;
      gU4.sourceInput = up
                            ? (gU4.sourceInput == 0
                                   ? static_cast<uint8_t>(count - 1U)
                                   : static_cast<uint8_t>(gU4.sourceInput - 1U))
                            : static_cast<uint8_t>((gU4.sourceInput + 1U) % count);
      drawListRow(previous, false);
      drawListRow(gU4.sourceInput, true);
      drawContextPanel(_levels,
                       _lanes,
                       _maxLevel,
                       sourceCountForType(gU4.type) > 1 ? 100 : CONTEXT_Y,
                       sourceCountForType(gU4.type) > 1 ? 50 : CONTEXT_H,
                       true,
                       sourceValue(gU4.sourceInput),
                       nullptr);
      _headerDirty = true;
      JWPLC_Display.notifyActivity();
    }
    return;
  }

  if (_view == View::SourceEdit)
  {
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
      sourceReference(gU4.sourceInput) = gU4.sourceBackup;
      transitionTo(View::SourceList);
      return;
    }
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
      transitionTo(View::SourceList);
      return;
    }
    const bool up = JWPLC_Buttons.pressed(BTN_UP);
    const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
    if (up || down)
    {
      uint16_t &source = sourceReference(gU4.sourceInput);
      moveSource(_model, source, down);
      char value[28];
      formatSource(_model, source, value, sizeof(value));
      updateTextField(JWPLC_Display.tft(), 112, 75, 28, value,
                      COLOR_WARNING, COLOR_SELECTED);
      drawContextPanel(_levels, _lanes, _maxLevel, 100, 50, true, source, nullptr);
      JWPLC_Display.notifyActivity();
    }
    return;
  }

  if (_view == View::ParameterList)
  {
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
      transitionTo(View::Configure);
      return;
    }
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
      gU4.resourceBackup = gU4.resource;
      gU4.tonMajorBackup = gU4.tonMajor;
      gU4.tonMinorBackup = gU4.tonMinor;
      gU4.tonBaseBackup = gU4.tonBase;
      gU4.tonFocus = U4TonField::Major;
      transitionTo(View::ParameterEdit);
      return;
    }
    return;
  }

  if (_view == View::ParameterEdit)
  {
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
      gU4.resource = gU4.resourceBackup;
      gU4.tonMajor = gU4.tonMajorBackup;
      gU4.tonMinor = gU4.tonMinorBackup;
      gU4.tonBase = gU4.tonBaseBackup;
      transitionTo(View::ParameterList);
      return;
    }
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
      transitionTo(View::ParameterList);
      return;
    }

    if (gU4.type != U4Type::Ton)
    {
      const bool up = JWPLC_Buttons.pressed(BTN_UP);
      const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
      if (up || down)
      {
        moveResource(_model, down);
        char value[20];
        formatResource(value, sizeof(value));
        updateTextField(JWPLC_Display.tft(), 112, 73, 24, value,
                        COLOR_WARNING, COLOR_SELECTED);
        JWPLC_Display.notifyActivity();
      }
      return;
    }

    const bool left = JWPLC_Buttons.pressed(BTN_LEFT);
    const bool right = !left && JWPLC_Buttons.pressed(BTN_RIGHT);
    if (left || right)
    {
      const U4TonField previous = gU4.tonFocus;
      uint8_t focus = static_cast<uint8_t>(gU4.tonFocus);
      focus = right
                  ? static_cast<uint8_t>((focus + 1U) % 3U)
                  : (focus == 0 ? 2U : static_cast<uint8_t>(focus - 1U));
      gU4.tonFocus = static_cast<U4TonField>(focus);

      char major[12], minor[12];
      formatTwoDigit(gU4.tonMajor, major, sizeof(major));
      formatTwoDigit(gU4.tonMinor, minor, sizeof(minor));
      const char *labels[3] = {tonMajorLabel(), tonMinorLabel(), "BASE"};
      const char *values[3] = {major, minor, tonBaseText()};
      drawTonField(static_cast<uint8_t>(previous),
                   labels[static_cast<uint8_t>(previous)],
                   values[static_cast<uint8_t>(previous)],
                   false,
                   true);
      drawTonField(static_cast<uint8_t>(gU4.tonFocus),
                   labels[static_cast<uint8_t>(gU4.tonFocus)],
                   values[static_cast<uint8_t>(gU4.tonFocus)],
                   true,
                   true);
      JWPLC_Display.notifyActivity();
      return;
    }

    bool changed = false;
    if (gU4.tonFocus == U4TonField::Major)
    {
      changed = JWPLC_Buttons.applyAxis(
          gU4.tonMajor, 0UL, 99UL, BTN_DOWN, BTN_UP, false, true);
    }
    else if (gU4.tonFocus == U4TonField::Minor)
    {
      changed = JWPLC_Buttons.applyAxis(
          gU4.tonMinor,
          0UL,
          u4TonMinorMaximum(gU4.tonBase),
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
        changeTonBase(up);
        char major[12], minor[12];
        formatTwoDigit(gU4.tonMajor, major, sizeof(major));
        formatTwoDigit(gU4.tonMinor, minor, sizeof(minor));
        drawTonField(0, tonMajorLabel(), major, false, false);
        drawTonField(1, tonMinorLabel(), minor, false, false);
        drawTonField(2, "BASE", tonBaseText(), true, false);
        changed = true;
      }
    }

    if (changed)
    {
      if (gU4.tonFocus == U4TonField::Major)
      {
        char value[12];
        formatTwoDigit(gU4.tonMajor, value, sizeof(value));
        drawTonField(0, tonMajorLabel(), value, true, false);
      }
      else if (gU4.tonFocus == U4TonField::Minor)
      {
        char value[12];
        formatTwoDigit(gU4.tonMinor, value, sizeof(value));
        drawTonField(1, tonMinorLabel(), value, true, false);
      }
      char configured[16];
      formatTonConfigured(configured, sizeof(configured));
      updateTextField(JWPLC_Display.tft(), 112, 70, 16, configured,
                      COLOR_WARNING, COLOR_PANEL);
      JWPLC_Display.notifyActivity();
    }
  }
}
