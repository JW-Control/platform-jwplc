// Stub host de JWPLC_TFT: framebuffer RGB565 320x170 con la fuente GLCD 5x7
// (celda 6x8) que usa TFT_eSPI con LOAD_GLCD.
#pragma once
#include <Arduino.h>

struct JWPLC_TFTFont { uint8_t kind; const void *data; };
static constexpr uint16_t JWPLC_TFT_BLACK = 0x0000;

class JWPLC_TFTClass : public Print
{
public:
  static constexpr int W = 320, H = 170;
  uint16_t fb[W * H] = {};
  int fillRects = 0;
  int16_t cx = 0, cy = 0;
  uint8_t size = 1;
  uint16_t fg = 0xFFFF, bg = 0x0000;

  bool fillScreen(uint16_t c, uint32_t = 50) { for (auto &p : fb) p = c; return true; }
  bool fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint32_t = 50);
  bool drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint32_t = 50);
  void setTextSize(uint8_t s) { size = s ? s : 1; }
  void setTextColor(uint16_t f) { fg = f; bg = f; }
  void setTextColor(uint16_t f, uint16_t b) { fg = f; bg = b; }
  void setCursor(int16_t x, int16_t y) { cx = x; cy = y; }
  size_t write(uint8_t c) override;
  void pixel(int x, int y, uint16_t c) { if (x >= 0 && y >= 0 && x < W && y < H) fb[y * W + x] = c; }
};
