#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V11_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V11_H

#include "RuntimeUIFBDMapV10.h"

/**
 * @brief Revisión v0.5.8 del asistente estructural FBD.
 *
 * El nodo + deja de tener preview superpuesto. CONFIGURAR utiliza una fila fija
 * FUENTE / PARAMETROS / CREAR y abre subpantallas específicas para escoger
 * entradas, fuentes y parámetros sin reducir el mini mapa contextual.
 */
class RuntimeUIFBDMapV11 : public RuntimeUIFBDMapV10
{
public:
  RuntimeUIFBDMapV11();

  void attach(RuntimeUIV2ReadModel &model);
  void detach();
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();

protected:
  enum class ConfigView : uint8_t
  {
    Main = 0,
    SourceList,
    SourceEdit,
    ParameterList,
    ParameterEdit
  };

  enum class MainFocus : uint8_t
  {
    Sources = 0,
    Parameters,
    Create
  };

  enum class ParameterField : uint8_t
  {
    Value = 0,
    Unit
  };

  static constexpr int16_t GROUP_Y = 42;
  static constexpr int16_t GROUP_H = 24;
  static constexpr int16_t GROUP_W = 98;
  static constexpr int16_t GROUP_GAP = 4;
  static constexpr int16_t GROUP_X0 = 8;

  static constexpr int16_t FIXED_CONTEXT_X = 10;
  static constexpr int16_t FIXED_CONTEXT_Y = 72;
  static constexpr int16_t FIXED_CONTEXT_W = 300;
  static constexpr int16_t FIXED_CONTEXT_H = 78;

  static constexpr int16_t LIST_X = 10;
  static constexpr int16_t LIST_Y = 42;
  static constexpr int16_t LIST_W = 300;
  static constexpr int16_t LIST_ROW_H = 24;
  static constexpr int16_t LIST_STEP = 28;

  void resetV11State();
  bool handleAddBackV11();
  bool handleTypeScreenV11();
  virtual void enterConfigureMain();
  void handleApplyCompletedV11();

  bool hasSourceGroup() const;
  uint8_t sourceInputCount() const;
  uint16_t &sourceReference(uint8_t inputIndex);
  uint16_t sourceReference(uint8_t inputIndex) const;
  const char *sourceInputName(uint8_t inputIndex) const;

  bool hasParameterGroup() const;
  uint8_t parameterCount() const;
  const char *parameterName(uint8_t parameterIndex) const;
  virtual void formatParameterValue(uint8_t parameterIndex,
                                    char *destination,
                                    size_t capacity) const;

  void normalizeMainFocus();
  void moveMainFocus(bool forward);
  bool mainFocusSelectable(MainFocus focus) const;

  void drawDisabledButton(int16_t x,
                          int16_t y,
                          int16_t width,
                          int16_t height,
                          const char *label);
  void drawMainGroup(MainFocus focus);
  void drawMainScreen();
  virtual bool handleMainInput();

  void drawSourceListScreen();
  void drawSourceListRow(uint8_t inputIndex, bool selected);
  bool handleSourceListInput();

  void beginSourceEdit();
  void drawSourceEditScreen();
  void drawSourceEditRow();
  bool handleSourceEditInput();
  void returnFromSourceEdit(bool accept);

  virtual void drawParameterListScreen();
  virtual void drawParameterListRow(uint8_t parameterIndex, bool selected);
  virtual bool handleParameterListInput();

  void beginParameterEdit();
  virtual void drawParameterEditScreen();
  void drawParameterEditFields();
  void drawParameterEditValue();
  void drawParameterEditUnit();
  virtual bool handleParameterEditInput();
  virtual void returnFromParameterEdit(bool accept);

  void drawFixedContextForCurrent();
  void drawFixedContext(uint16_t source);
  void drawFixedContextMessage(
      const char *message,
      uint16_t color = JWPLCLogicRuntimeUIWidgets::COLOR_MUTED);
  uint16_t contextualSource() const;

  void drawConfigHeader(const char *subtitle);
  void drawConfigFooter(
      const char *text,
      uint16_t color = JWPLCLogicRuntimeUIWidgets::COLOR_MUTED);

  // Hooks protegidos para revisiones posteriores sin volver a duplicar la
  // cadena V8 -> V11 ni exponer estado del asistente en la API pública.
  bool normalMapRootActiveV11() const
  {
    return _wizardPage == WizardPage::None &&
           !_addSelected &&
           _mode == Mode::Map;
  }

  bool detailModeActiveV11() const
  {
    return _wizardPage == WizardPage::None &&
           !_addSelected &&
           _mode == Mode::Detail;
  }

  uint16_t selectedBlockIndexV11() const
  {
    return _selectedIndex;
  }

  const LogicV2BlockRecord *selectedBlockV11() const
  {
    return _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  }

  RuntimeUIV2ReadModel *readModelV11() const
  {
    return _model;
  }

  bool consumeInputReleaseGateV11()
  {
    return consumeInputReleaseGate();
  }

  void suppressCompactAddPreviewV11()
  {
    _addPreviewDrawn = true;
  }

  const char *wizardTypeLabelV11() const
  {
    return wizardTypeName(_wizardType);
  }

  bool wizardIsTonV11() const
  {
    return _wizardType == WizardType::Ton;
  }

  uint16_t wizardPrimarySourceV11() const
  {
    return _wizardSourceA;
  }

  /**
   * @brief Cambia la unidad del TON sin alterar la duración real.
   *
   * Se conserva para compatibilidad con V12. La revisión LOGO! posterior
   * reemplaza este flujo por una base de tiempo y dos componentes independientes.
   */
  void moveWizardTimeUnit(bool forward)
  {
    const uint32_t milliseconds = wizardTimeMilliseconds();
    uint8_t unit = static_cast<uint8_t>(_wizardTimeUnit);
    unit = forward
               ? static_cast<uint8_t>((unit + 1U) % 4U)
               : (unit == 0 ? 3U : static_cast<uint8_t>(unit - 1U));

    const WizardTimeUnit candidate = static_cast<WizardTimeUnit>(unit);
    const uint32_t multiplier = wizardUnitMultiplier(candidate);
    if (multiplier == 0 || milliseconds % multiplier != 0)
    {
      drawConfigFooter(
          "UNIDAD NO EXACTA",
          JWPLCLogicRuntimeUIWidgets::COLOR_WARNING);
      return;
    }

    _wizardTimeUnit = candidate;
    _wizardTimeValue = milliseconds / multiplier;
    drawConfigFooter("OK ACEPTAR   ESC CANCELAR");
  }

  virtual void requestWizardCreateV11()
  {
    requestWizardCreate();
  }

  bool requestLogoTonCreateV11(uint32_t parameterMs,
                               uint16_t timeBaseResource)
  {
    if (!_editSession.begin())
    {
      _wizardError = true;
      drawConfigFooter("ERROR AL ABRIR BORRADOR", JWPLCLogicRuntimeUIWidgets::COLOR_ERROR);
      return false;
    }

    LogicV2InputLink input;
    if (_wizardSourceA == JWPLC_LOGIC_V2_SOURCE_OPEN)
    {
      input = LogicV2InputLink::open();
    }
    else if (_wizardSourceA == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
    {
      input = LogicV2InputLink::constantTrue();
    }
    else if (_wizardSourceA == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
    {
      input = LogicV2InputLink::constantFalse();
    }
    else
    {
      input = LogicV2InputLink::block(_wizardSourceA);
    }

    uint16_t newIndex = 0;
    const bool prepared = _editSession.appendBlock(
        LogicV2BlockType::Ton,
        &input,
        1,
        timeBaseResource,
        parameterMs,
        &newIndex);
    const bool valid =
        prepared &&
        _editSession.validate() == LogicV2PrototypeError::None;

    if (!valid)
    {
      _editSession.cancel();
      _wizardError = true;
      drawConfigFooter("CONFIGURACION NO VALIDA", JWPLCLogicRuntimeUIWidgets::COLOR_ERROR);
      return false;
    }

    _wizardNewIndex = newIndex;
    _wizardApplying = true;
    _wizardError = false;
    _awaitingApply = true;
    _applyRequested = true;
    JWPLC_Display.notifyActivity();
    gateInputUntilRelease(false);
    drawConfigFooter("APLICANDO CAMBIOS...", JWPLCLogicRuntimeUIWidgets::COLOR_WARNING);
    return true;
  }

  void returnConfigureToTypeV11()
  {
    restoreWizardRepeatProfile();
    _wizardPage = WizardPage::Type;
    _wizardError = false;
    JWPLC_Display.notifyActivity();
    gateInputUntilRelease(false);
    drawWizardTypeScreen();
  }

  void restoreWizardRepeatProfileV11()
  {
    restoreWizardRepeatProfile();
  }

  void restoreParameterBackupV11()
  {
    _wizardResource = _resourceBackup;
    _wizardTimeUnit = _timeUnitBackup;
    _wizardTimeValue = _timeValueBackup;
  }

  /**
   * @brief Expone a revisiones derivadas el gate privado de V4.
   *
   * V11 ya es friend explícita de RuntimeUIFBDMapV4. La llamada cualificada
   * evita ampliar la visibilidad de V4 y mantiene el acceso encapsulado.
   */
  void gateInputUntilRelease(bool restoreIdleReturn = false)
  {
    RuntimeUIFBDMapV4::gateInputUntilRelease(restoreIdleReturn);
  }

  ConfigView _configView;
  MainFocus _mainFocus;
  uint8_t _sourceInputIndex;
  uint16_t _sourceBackup;
  uint8_t _parameterIndex;
  ParameterField _parameterField;
  uint8_t _resourceBackup;
  WizardTimeUnit _timeUnitBackup;
  uint32_t _timeValueBackup;
};

#endif