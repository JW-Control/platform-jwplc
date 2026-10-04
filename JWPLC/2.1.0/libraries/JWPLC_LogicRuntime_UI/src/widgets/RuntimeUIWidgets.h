#ifndef JWPLC_LOGIC_RUNTIME_UI_WIDGETS_H
#define JWPLC_LOGIC_RUNTIME_UI_WIDGETS_H

#include <Arduino.h>
#include <JWPLC_TFT.h>

namespace JWPLCLogicRuntimeUIWidgets
{
  static constexpr int16_t SCREEN_W = 320;
  static constexpr int16_t SCREEN_H = 170;

  // Paleta base de JW Control: verde, blanco y negro.
  static constexpr uint16_t COLOR_BACKGROUND = JWPLC_TFT_BLACK;
  static constexpr uint16_t COLOR_TEXT = JWPLC_TFT_WHITE;
  static constexpr uint16_t COLOR_MUTED = 0x9CF3;
  static constexpr uint16_t COLOR_BORDER = 0x7BEF;
  static constexpr uint16_t COLOR_ACCENT = 0x5FE0;
  static constexpr uint16_t COLOR_OK = 0x07E0;
  static constexpr uint16_t COLOR_WARNING = 0xFFE0;
  static constexpr uint16_t COLOR_ERROR = JWPLC_TFT_RED;
  static constexpr uint16_t COLOR_PANEL = 0x1082;
  static constexpr uint16_t COLOR_SELECTED = 0x0200;

  void clearScreen(JWPLC_TFTClass &tft);

  /** Dibuja una sola vez el fondo, titulo y separador del encabezado. */
  void drawHeaderStatic(JWPLC_TFTClass &tft,
                        const char *title);

  /** Actualiza solamente la insignia de estado del encabezado. */
  void updateHeaderState(JWPLC_TFTClass &tft,
                         const char *stateText,
                         uint16_t stateColor);

  /** Helper completo conservado para composiciones puntuales. */
  void drawHeader(JWPLC_TFTClass &tft,
                  const char *title,
                  const char *stateText,
                  uint16_t stateColor);

  void drawPanel(JWPLC_TFTClass &tft,
                 int16_t x,
                 int16_t y,
                 int16_t w,
                 int16_t h,
                 const char *title);

  /** Dibuja una etiqueta estatica. No debe llamarse en cada refresh. */
  void drawFieldLabel(JWPLC_TFTClass &tft,
                      int16_t x,
                      int16_t y,
                      const char *label,
                      uint16_t foreground = COLOR_MUTED,
                      uint16_t background = COLOR_PANEL);

  /**
   * Actualiza un campo de ancho fijo solo cuando su valor cambia.
   *
   * Primero limpia la region completa mediante una unica operacion continua y
   * luego imprime solamente los caracteres utiles en modo transparente. Esto
   * evita enviar espacios como glifos y acelera especialmente la carga inicial.
   */
  void updateTextField(JWPLC_TFTClass &tft,
                       int16_t x,
                       int16_t y,
                       uint8_t columns,
                       const char *value,
                       uint16_t foreground = COLOR_TEXT,
                       uint16_t background = COLOR_PANEL);

  /** Helper compatible para dibujar etiqueta y valor en una sola llamada. */
  void drawLabelValue(JWPLC_TFTClass &tft,
                      int16_t x,
                      int16_t y,
                      const char *label,
                      const char *value,
                      int16_t clearWidth);

  void drawMenuButton(JWPLC_TFTClass &tft,
                      int16_t x,
                      int16_t y,
                      int16_t w,
                      int16_t h,
                      const char *label,
                      bool selected);

  void drawFooter(JWPLC_TFTClass &tft, const char *text);
}

#endif