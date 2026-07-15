#ifndef JWPLC_LOGIC_RUNTIME_UI_WIDGETS_H
#define JWPLC_LOGIC_RUNTIME_UI_WIDGETS_H

#include <Arduino.h>
#include <Adafruit_ST7789.h>

namespace JWPLCLogicRuntimeUIWidgets
{
  static constexpr int16_t SCREEN_W = 320;
  static constexpr int16_t SCREEN_H = 170;

  static constexpr uint16_t COLOR_BACKGROUND = ST77XX_BLACK;
  static constexpr uint16_t COLOR_TEXT = ST77XX_WHITE;
  static constexpr uint16_t COLOR_MUTED = 0x9CF3;
  static constexpr uint16_t COLOR_BORDER = 0x7BEF;
  static constexpr uint16_t COLOR_ACCENT = 0x07FF;
  static constexpr uint16_t COLOR_OK = 0x07E0;
  static constexpr uint16_t COLOR_WARNING = 0xFFE0;
  static constexpr uint16_t COLOR_ERROR = ST77XX_RED;
  static constexpr uint16_t COLOR_PANEL = 0x1082;
  static constexpr uint16_t COLOR_SELECTED = 0x18E3;

  void clearScreen(Adafruit_ST7789 &tft);

  /** Dibuja una sola vez el fondo, titulo y separador del encabezado. */
  void drawHeaderStatic(Adafruit_ST7789 &tft,
                        const char *title);

  /** Actualiza solamente la insignia de estado del encabezado. */
  void updateHeaderState(Adafruit_ST7789 &tft,
                         const char *stateText,
                         uint16_t stateColor);

  /** Helper completo conservado para composiciones puntuales. */
  void drawHeader(Adafruit_ST7789 &tft,
                  const char *title,
                  const char *stateText,
                  uint16_t stateColor);

  void drawPanel(Adafruit_ST7789 &tft,
                 int16_t x,
                 int16_t y,
                 int16_t w,
                 int16_t h,
                 const char *title);

  /** Dibuja una etiqueta estatica. No debe llamarse en cada refresh. */
  void drawFieldLabel(Adafruit_ST7789 &tft,
                      int16_t x,
                      int16_t y,
                      const char *label,
                      uint16_t foreground = COLOR_MUTED,
                      uint16_t background = COLOR_PANEL);

  /**
   * Actualiza un campo de ancho fijo.
   *
   * El texto se trunca o rellena con espacios hasta columns. Adafruit_GFX
   * escribe el fondo de cada celda junto con el glifo, evitando el ciclo
   * visible "borrar rectangulo -> escribir texto".
   */
  void updateTextField(Adafruit_ST7789 &tft,
                       int16_t x,
                       int16_t y,
                       uint8_t columns,
                       const char *value,
                       uint16_t foreground = COLOR_TEXT,
                       uint16_t background = COLOR_PANEL);

  /** Helper compatible para dibujar etiqueta y valor en una sola llamada. */
  void drawLabelValue(Adafruit_ST7789 &tft,
                      int16_t x,
                      int16_t y,
                      const char *label,
                      const char *value,
                      int16_t clearWidth);

  void drawMenuButton(Adafruit_ST7789 &tft,
                      int16_t x,
                      int16_t y,
                      int16_t w,
                      int16_t h,
                      const char *label,
                      bool selected);

  void drawFooter(Adafruit_ST7789 &tft, const char *text);
}

#endif
