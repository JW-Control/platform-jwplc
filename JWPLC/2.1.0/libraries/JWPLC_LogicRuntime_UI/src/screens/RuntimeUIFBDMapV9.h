#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V9_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V9_H

#include "RuntimeUIFBDMapV8.h"
#include "../widgets/RuntimeUIWidgets.h"

/**
 * @brief Ajuste de integración de botones para v0.5.6.
 *
 * `pressed()` es un latch consumible. Esta capa intercepta RIGHT antes de que
 * lo lea el mapa base y procesa UP/DOWN del asistente una sola vez, conservando
 * correctamente el sentido de avance.
 */
class RuntimeUIFBDMapV9 : public RuntimeUIFBDMapV8
{
public:
  RuntimeUIFBDMapV9()
      : RuntimeUIFBDMapV8()
  {
  }

  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc)
  {
    if (interceptAddRightPressForExtension())
    {
      return;
    }

    if (interceptWizardDirectionalPress())
    {
      return;
    }

    RuntimeUIFBDMapV8::refresh(io, rtc);
  }

private:
  bool interceptWizardDirectionalPress()
  {
    if (_wizardPage != WizardPage::Configure ||
        _wizardApplying ||
        inputReleaseGateActiveForExtension() ||
        wizardFieldIsTimeValue(_wizardField))
    {
      return false;
    }

    const bool up = JWPLC_Buttons.pressed(BTN_UP);
    const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
    if (!up && !down)
    {
      return false;
    }

    const bool forward = down;
    bool secondSource = false;
    if (wizardFieldIsSource(_wizardField, secondSource))
    {
      const bool allowOpen = _wizardType == WizardType::And2;
      uint16_t &source = secondSource ? _wizardSourceB : _wizardSourceA;
      moveWizardSource(source, forward, allowOpen);
    }
    else if (wizardFieldIsResource(_wizardField))
    {
      moveWizardResource(forward);
    }
    else if (wizardFieldIsTimeUnit(_wizardField))
    {
      moveWizardTimeUnit(forward);
    }
    else
    {
      return false;
    }

    _wizardError = false;
    JWPLC_Display.notifyActivity();
    drawWizardConfigFields();
    drawWizardFooter(
        "OK CREAR   ESC ATRAS",
        JWPLCLogicRuntimeUIWidgets::COLOR_MUTED);
    return true;
  }
};

#endif
