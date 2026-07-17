#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V5_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V5_H

#include "RuntimeUIFBDMapV4.h"

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
};

#endif
