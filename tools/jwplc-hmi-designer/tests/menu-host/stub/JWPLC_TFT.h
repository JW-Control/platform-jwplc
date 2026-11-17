// Stub host de JWPLC_TFT: framebuffer RGB565 320x170 con la fuente GLCD 5x7
// (celda 6x8) que usa TFT_eSPI con LOAD_GLCD.
#pragma once
#include <Arduino.h>

struct JWPLC_TFTFont { uint8_t kind; const void *data; };
inline constexpr JWPLC_TFTFont JWPLC_FONT_GLCD = { 0, nullptr };
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
  // Menú CARDS (#239): esquinas redondeadas como en TFT_eSPI (cuarto de círculo de radio r).
  bool fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c, uint32_t = 50)
  {
    fillRects++;
    for (int j = 0; j < h; ++j)
      for (int i = 0; i < w; ++i)
        if (inRound(i, j, w, h, r)) pixel(x + i, y + j, c);
    return true;
  }
  bool drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c, uint32_t = 50)
  {
    for (int j = 0; j < h; ++j)
      for (int i = 0; i < w; ++i)
        if (inRound(i, j, w, h, r) &&
            !(inRound(i - 1, j - 1, w - 2, h - 2, r > 0 ? r - 1 : 0) && i > 0 && j > 0 && i < w - 1 && j < h - 1))
          pixel(x + i, y + j, c);
    return true;
  }
  static bool inRound(int i, int j, int w, int h, int r)
  {
    if (i < 0 || j < 0 || i >= w || j >= h) return false;
    if (r <= 0) return true;
    int cx = i < r ? r : (i >= w - r ? w - r - 1 : i);
    int cy = j < r ? r : (j >= h - r ? h - r - 1 : j);
    int dx = i - cx, dy = j - cy;
    return dx * dx + dy * dy <= r * r;
  }
  void setFont(const JWPLC_TFTFont &) {} // el stub sólo dibuja GLCD
  void setTextSize(uint8_t s) { size = s ? s : 1; }
  void setTextColor(uint16_t f) { fg = f; bg = f; }
  void setTextColor(uint16_t f, uint16_t b) { fg = f; bg = b; }
  void setCursor(int16_t x, int16_t y) { cx = x; cy = y; }
  size_t write(uint8_t c) override;
  void pixel(int x, int y, uint16_t c) { if (x >= 0 && y >= 0 && x < W && y < H) fb[y * W + x] = c; }
};
