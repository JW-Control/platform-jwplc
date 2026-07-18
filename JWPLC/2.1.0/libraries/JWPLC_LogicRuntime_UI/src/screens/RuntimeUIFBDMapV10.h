#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V10_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V10_H

#include "RuntimeUIFBDMapV9.h"

/**
 * @brief Revisión v0.5.7 del asistente NUEVO BLOQUE.
 *
 * Corrige el preview invasivo del nodo +, actualiza únicamente los botones que
 * cambian en la selección de tipo, reserva ESC como única salida de esa pantalla
 * y agrega un mini mapa FBD contextual al configurar fuentes reales.
 */
class RuntimeUIFBDMapV10 : public RuntimeUIFBDMapV9
{
  friend class RuntimeUIFBDMapV11;

public:
  RuntimeUIFBDMapV10();

  void attach(RuntimeUIV2ReadModel &model);
  void detach();
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();

private:
  static constexpr int16_t TYPE_BUTTON_X = 8;
  static constexpr int16_t TYPE_BUTTON_Y = 33;
  static constexpr int16_t TYPE_BUTTON_W = 132;
  static constexpr int16_t TYPE_BUTTON_H = 22;
  static constexpr int16_t TYPE_BUTTON_STEP = 25;

  static constexpr int16_t CONFIG_FIELD_X = 10;
  static constexpr int16_t CONFIG_FIELD_W = 300;
  static constexpr int16_t CONFIG_FIELD_H = 22;
  static constexpr int16_t CONFIG_FIELD_STEP = 26;

  static constexpr int16_t CONTEXT_X = 10;
  static constexpr int16_t CONTEXT_W = 300;
  static constexpr int16_t CONTEXT_BOTTOM = 152;

  bool handleAddBackWithoutPreview();
  bool handleWizardTypeInputIncremental();
  bool handleWizardConfigInputIncremental();
  void handleWizardApplyCompletedV10();

  void drawWizardTypeChoice(WizardType type, bool selected);
  void drawWizardConfigScreenV10();
  void drawWizardConfigFieldsV10();
  void drawWizardConfigFieldV10(uint8_t field);
  int16_t wizardConfigFieldY(uint8_t field) const;
  int16_t wizardContextY() const;
  int16_t wizardContextHeight() const;

  void drawWizardContextPanel();
  bool wizardContextSource(uint16_t &source) const;
  void drawMiniFBD(uint16_t selectedSource,
                   int16_t x,
                   int16_t y,
                   int16_t width,
                   int16_t height);

  void configureWizardRepeatProfile();
  void restoreWizardRepeatProfile();
  void clearWizardErrorFooterIfNeeded();

  bool unsafeAddPreview() const;
  void suppressUnsafeAddPreview(bool forceCleanup);

  bool _unsafePreviewClean;
  bool _wizardRepeatActive;
  bool _wizardRepeatUnitValid;
  WizardTimeUnit _wizardRepeatUnit;
};

#endif
