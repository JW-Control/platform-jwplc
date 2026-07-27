#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V8_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V8_H

#include "RuntimeUIFBDMapV7.h"

/**
 * @brief Revisión v0.5.6 con bloque virtual + y creación guiada en RAM.
 *
 * El símbolo + no forma parte del programa ni consume un registro de bloque.
 * Aparece después de la última columna lógica y abre un asistente que agrega al
 * final del orden topológico uno de cinco tipos iniciales: DI, NOT, AND2, TON o
 * DO. La aplicación continúa diferida fuera del callback TFT.
 */
class RuntimeUIFBDMapV8 : public RuntimeUIFBDMapV7
{
  friend class RuntimeUIFBDMapV9;
  friend class RuntimeUIFBDMapV10;
  friend class RuntimeUIFBDMapV11;

public:
  RuntimeUIFBDMapV8();

  void attach(RuntimeUIV2ReadModel &model);
  void detach();
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();

protected:
  /**
   * @brief Intercepta RIGHT antes del renderer v0.5.4, porque pressed() es
   * consumible y el mapa base no conoce el nodo virtual +.
   */
  bool interceptAddRightPressForExtension()
  {
    if (_wizardPage != WizardPage::None ||
        _addSelected ||
        _mode != Mode::Map ||
        _inputReleaseGate ||
        !selectedAtLastLevel())
    {
      return false;
    }

    if (!JWPLC_Buttons.pressed(BTN_RIGHT))
    {
      return false;
    }

    selectAddNode();
    return true;
  }

  bool inputReleaseGateActiveForExtension() const
  {
    return _inputReleaseGate;
  }

private:
  enum class WizardPage : uint8_t
  {
    None = 0,
    Type,
    Configure
  };

  enum class WizardType : uint8_t
  {
    DigitalInput = 0,
    Not,
    And2,
    Ton,
    DigitalOutput,
    Count
  };

  enum class WizardTimeUnit : uint8_t
  {
    Milliseconds = 0,
    Seconds,
    Minutes,
    Hours
  };

  static constexpr int16_t ADD_PREVIEW_W = 28;
  static constexpr int16_t ADD_PREVIEW_H = 24;
  static constexpr int16_t ADD_EDGE_W = 20;
  static constexpr uint32_t WIZARD_DEFAULT_T_MS = 1000UL;

  void resetAddState();
  bool canAddBlock() const;
  bool selectedAtLastLevel() const;

  void refreshNormalMap(const JWPLC_IOState *io,
                        const JWPLC_RTCState *rtc);
  void selectAddNode();
  void leaveAddNode();
  void refreshAddNode();
  void drawAddSelectedMap();
  void drawAddSelectedLive();
  void drawAddPreview();
  void drawAddNode(bool selected,
                   bool compact,
                   int16_t x,
                   int16_t y,
                   int16_t width,
                   int16_t height);
  int16_t addNodeY() const;
  bool addPreviewVisible() const;
  void updateAddHeader(bool selected);

  void openWizard();
  void closeWizardToAdd();
  void refreshWizard();
  void handleWizardApplyCompleted();
  void handleWizardTypeInput();
  void handleWizardConfigInput();
  void initializeWizardConfig();
  void requestWizardCreate();

  void drawWizardTypeScreen();
  void drawWizardConfigScreen();
  void drawWizardConfigFields();
  void drawWizardFooter(const char *text,
                        uint16_t color);

  static const char *wizardTypeName(WizardType type);
  static LogicV2BlockType wizardLogicType(WizardType type);
  uint8_t wizardFieldCount() const;
  const char *wizardFieldName(uint8_t field) const;
  void formatWizardFieldValue(uint8_t field,
                              char *destination,
                              size_t capacity) const;

  bool wizardFieldIsSource(uint8_t field,
                           bool &secondSource) const;
  bool wizardFieldIsResource(uint8_t field) const;
  bool wizardFieldIsTimeValue(uint8_t field) const;
  bool wizardFieldIsTimeUnit(uint8_t field) const;

  uint16_t wizardSourceCandidateCount(bool allowOpen) const;
  uint16_t wizardSourceCandidateAt(uint16_t candidate,
                                   bool allowOpen) const;
  uint16_t wizardSourceCandidateIndex(uint16_t source,
                                      bool allowOpen) const;
  void moveWizardSource(uint16_t &source,
                        bool forward,
                        bool allowOpen);
  void formatWizardSource(uint16_t source,
                          char *destination,
                          size_t capacity) const;

  uint8_t digitalInputCount() const;
  uint8_t digitalOutputCount() const;
  bool outputResourceUsed(uint8_t resource) const;
  uint8_t firstAvailableOutputResource() const;
  void moveWizardResource(bool forward);

  static uint32_t wizardUnitMultiplier(WizardTimeUnit unit);
  static const char *wizardUnitText(WizardTimeUnit unit);
  uint32_t wizardTimeMilliseconds() const;
  void moveWizardTimeValue(bool increase);
  void moveWizardTimeUnit(bool forward);

  bool _addSelected;
  bool _addPreviewDrawn;
  uint16_t _addOriginIndex;
  uint8_t _previewMaxLevel;
  HorizontalWindowMode _previewWindowMode;
  uint8_t _previewCentralStart;
  int16_t _previewViewportY;

  WizardPage _wizardPage;
  WizardType _wizardType;
  uint8_t _wizardField;
  uint16_t _wizardSourceA;
  uint16_t _wizardSourceB;
  uint8_t _wizardResource;
  WizardTimeUnit _wizardTimeUnit;
  uint32_t _wizardTimeValue;
  bool _wizardApplying;
  bool _wizardError;
  uint16_t _wizardNewIndex;
};

#endif
