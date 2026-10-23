// Stub host de JWPLC_TFT para JWPLC_UI.cpp: framebuffer RGB565 320x170 con la
// fuente GLCD 5x7 (celda 6x8) y las primitivas que usan los campos. Cuenta
// fillScreen/fillRect/pushImage para verificar que un ICON sólo repinta su recuadro.
#pragma once
#include <Arduino.h>

struct JWPLC_TFTFont { uint8_t kind; const void *data; };

class JWPLC_TFTClass : public Print
{
public:
  static constexpr int W = 320, H = 170;
  uint16_t fb[W * H] = {};
  int fillScreens = 0, fillRects = 0, pushImages = 0;
  int16_t cx = 0, cy = 0;
  uint8_t size = 1;
  uint16_t fg = 0xFFFF, bg = 0x0000;

  bool fillScreen(uint16_t c, uint32_t = 50) { ++fillScreens; for (auto &p : fb) p = c; return true; }
  bool fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint32_t = 50);
  bool drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint32_t = 50);
  bool fillCircle(int16_t x, int16_t y, int16_t r, uint16_t c, uint32_t = 50);
  bool pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *pixels, uint32_t = 50);
  bool drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c, uint32_t = 50) { for (int i = 0; i < w; ++i) pixel(x + i, y, c); return true; }
  bool drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c, uint32_t = 50) { for (int i = 0; i < h; ++i) pixel(x, y + i, c); return true; }
  int16_t width() const { return W; }
  int16_t height() const { return H; }
  void getTextBounds(const char *s, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h);
  void setTextSize(uint8_t s) { size = s ? s : 1; }
  void setTextColor(uint16_t f) { fg = f; bg = f; }
  void setTextColor(uint16_t f, uint16_t b) { fg = f; bg = b; }
  void setCursor(int16_t x, int16_t y) { cx = x; cy = y; }
  size_t write(uint8_t c) override;
  void pixel(int x, int y, uint16_t c) { if (x >= 0 && y >= 0 && x < W && y < H) fb[y * W + x] = c; }
};
