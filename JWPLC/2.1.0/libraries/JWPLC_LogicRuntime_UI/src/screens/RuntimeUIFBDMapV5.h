#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V5_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V5_H

#include <cstdio>
#include <cstdint>
#include <cstring>

#include "RuntimeUIFBDMapV4.h"
#include "../widgets/RuntimeUIWidgets.h"

/**
 * @brief Extensión v0.5.2 del mapa FBD con navegación Entrada/Parámetros.
 *
 * Conserva el editor transaccional RAM de v0.5.1 y agrega:
 * - LEFT/RIGHT para mover el foco entre entradas y parámetros;
 * - UP/DOWN para elegir la entrada o parámetro;
 * - OK para abrir el editor correspondiente;
 * - edición del tiempo T del TON;
 * - visualización dinámica de Ta sin mostrar Q como texto redundante.
 */
class RuntimeUIFBDMapV5 : public RuntimeUIFBDMapV4
{
  friend class RuntimeUIFBDMapV7;

public:
  RuntimeUIFBDMapV5();

  void attach(RuntimeUIV2ReadModel &model);
  void detach();
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();

private:
  enum class DetailFocus : uint8_t
  {
    Inputs = 0,
    Parameters
  };

  enum class ParameterEditFocus : uint8_t
  {
    Value = 0,
    Unit
  };

  enum class TimeUnit : uint8_t
  {
    Milliseconds = 0,
    Seconds,
    Minutes,
    Hours
  };

  static constexpr uint8_t TON_PARAMETER_COUNT = 1;
  static constexpr int16_t TON_PANEL_X =
      DETAIL_BLOCK_X + DETAIL_GUTTER_W + 4;
  static constexpr int16_t TON_PANEL_Y = DETAIL_BLOCK_Y + 57;
  static constexpr int16_t TON_PANEL_W =
      DETAIL_BLOCK_W - DETAIL_GUTTER_W - 8;
  static constexpr int16_t TON_PANEL_H = 31;

  void resetExtensionState();
  bool selectedBlockHasParameters() const;
  uint8_t selectedParameterCount() const;
  void normalizeDetailFocus();

  void refreshDetailV5();
  void handleDetailInputV5();
  void drawDetailV5(bool force);
  void drawTonParameterPanel(bool selected);
  void updateDetailHeader();

  bool beginParameterEdit();
  void cancelParameterEdit();
  void refreshParameterEditor();
  void handleParameterEditInput();
  void handleParameterApplyCompleted();
  void drawParameterEditStatic();
  void drawParameterEdit();

  static uint32_t unitMultiplier(TimeUnit unit);
  static const char *unitText(TimeUnit unit);
  static TimeUnit preferredUnit(uint32_t milliseconds);
  static void formatDurationCompact(uint32_t milliseconds,
                                    char *destination,
                                    size_t capacity);

  uint32_t parameterMilliseconds() const;
  void moveParameterValue(bool increase);
  void moveParameterUnit(bool forward);

  DetailFocus _detailFocus;
  uint8_t _detailParameterIndex;
  bool _parameterEditorActive;
  bool _parameterFullRedraw;
  ParameterEditFocus _parameterEditFocus;
  TimeUnit _parameterUnit;
  uint32_t _parameterValue;

protected:
  /**
   * @brief Punto de extensión mínimo para revisiones posteriores del editor.
   *
   * Mantiene encapsulados el motor, la sesión transaccional y el resto de la
   * navegación. Las revisiones derivadas solo pueden acelerar el campo VALOR y
   * actualizar la lectura viva de Ta.
   */
  bool parameterEditorActiveForExtension() const
  {
    return _parameterEditorActive;
  }

  bool parameterValueFocusedForExtension() const
  {
    return _parameterEditFocus == ParameterEditFocus::Value;
  }

  bool applyParameterValueAxisForExtension()
  {
    if (!_parameterEditorActive ||
        _awaitingApply ||
        _parameterEditFocus != ParameterEditFocus::Value)
    {
      return false;
    }

    const uint32_t multiplier = unitMultiplier(_parameterUnit);
    const uint32_t maximum =
        multiplier == 0 ? UINT32_MAX : UINT32_MAX / multiplier;

    const bool changed = JWPLC_Buttons.applyAxis(
        &_parameterValue,
        0,
        maximum,
        BTN_DOWN,
        BTN_UP,
        false,
        true);

    if (!changed)
    {
      return false;
    }

    const bool feedbackChanged = _editFeedback != EditFeedback::None;
    _editFeedback = EditFeedback::None;
    JWPLC_Display.notifyActivity();

    char valueField[24];
    std::snprintf(valueField,
                  sizeof(valueField),
                  "VALOR <%lu>",
                  static_cast<unsigned long>(_parameterValue));

    // Durante repeat no se reconstruye el botón de 145 x 38 px. Solo se limpia
    // y repinta la línea de texto dentro de la superficie ya seleccionada.
    Adafruit_ST7789 &tft = JWPLC_Display.tft();
    static constexpr int16_t valueX = 10;
    static constexpr int16_t valueY = 103;
    static constexpr int16_t valueW = 145;
    static constexpr int16_t valueH = 38;
    static constexpr int16_t innerX = valueX + 5;
    static constexpr int16_t textY = valueY + (valueH - 8) / 2;

    tft.fillRect(innerX,
                 textY,
                 valueW - 10,
                 8,
                 JWPLCLogicRuntimeUIWidgets::COLOR_SELECTED);

    const int16_t textWidth = static_cast<int16_t>(
        std::strlen(valueField) * 6U);
    const int16_t textX = static_cast<int16_t>(
        valueX + (valueW - textWidth) / 2);

    tft.setTextWrap(false);
    tft.setTextSize(1);
    tft.setTextColor(JWPLCLogicRuntimeUIWidgets::COLOR_ACCENT,
                     JWPLCLogicRuntimeUIWidgets::COLOR_SELECTED);
    tft.setCursor(textX, textY);
    tft.print(valueField);

    // El pie solo necesita restaurarse si antes mostraba un error.
    if (feedbackChanged)
    {
      JWPLCLogicRuntimeUIWidgets::updateTextField(
          tft,
          78,
          154,
          28,
          "OK GUARDAR   ESC CANCELAR",
          JWPLCLogicRuntimeUIWidgets::COLOR_MUTED,
          JWPLCLogicRuntimeUIWidgets::COLOR_PANEL);
    }
    return true;
  }

  void refreshParameterElapsedForExtension()
  {
    if (!_parameterEditorActive ||
        _model == nullptr ||
        !_model->isTon(_selectedIndex))
    {
      return;
    }

    char elapsed[18];
    formatDurationCompact(
        _model->tonElapsedMs(_selectedIndex, millis()),
        elapsed,
        sizeof(elapsed));

    const bool timing = _model->tonTiming(_selectedIndex);
    const bool active = _model->blockValue(_selectedIndex);
    JWPLCLogicRuntimeUIWidgets::updateTextField(
        JWPLC_Display.tft(),
        88,
        83,
        18,
        elapsed,
        (timing || active)
            ? JWPLCLogicRuntimeUIWidgets::COLOR_OK
            : JWPLCLogicRuntimeUIWidgets::COLOR_MUTED,
        JWPLCLogicRuntimeUIWidgets::COLOR_PANEL);
  }
};

#endif
