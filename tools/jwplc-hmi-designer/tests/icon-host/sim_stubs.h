// Stubs compartidos de las simulaciones ICON en host (icon_sim.cpp,
// header_sim.cpp): resto del firmware y TFT en framebuffer con la fuente GLCD.
// Se incluye una sola vez, desde el .cpp principal de cada simulación.
#pragma once
#include <JWPLC_Display_API.h>
#include "JWPLC_Display_H3E1_Profile.h"
#include "JWPLC_UI_Pages.h"
#include "font_data.h"
extern "C"
{
#include "jwplc_peripherals.h"
}

// ---- stubs del resto del firmware ------------------------------------------
JWPLC_DisplayClass JWPLC_Display;
bool JWPLC_DisplayClass::isIdleMode() const { return false; }
extern "C" void jwplcSystemForceDisplayRefresh(void) {}
const JWPLC_RTCState *jwplcGetRTCState(void) { return nullptr; }
bool jwplcH3E1ProfilerEnabled(void) { return false; }
void jwplcH3E1RecordSetter(uint8_t, bool) {}
void jwplcH3E1RecordDirtyPass(uint32_t) {}
void jwplcH3E1RecordStage(JWPLCH3E1StageId, uint32_t) {}
namespace JWPLCUIPages
{
  void serviceNavigation() {}
  void prepareEnter() {}
  void drawModeChange(JWPLC_TFTClass &) {}
  void drawIndicator(JWPLC_TFTClass &) {}
}
void jwplcH3E1RecordFieldDraw(uint8_t, uint8_t, uint16_t, uint16_t, uint8_t, uint32_t, uint32_t, uint32_t, uint32_t) {}

// ---- TFT en framebuffer ------------------------------------------------------
bool JWPLC_TFTClass::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint32_t)
{
  ++fillRects;
  for (int yy = y; yy < y + h; ++yy) for (int xx = x; xx < x + w; ++xx) pixel(xx, yy, c);
  return true;
}
bool JWPLC_TFTClass::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint32_t)
{
  for (int xx = x; xx < x + w; ++xx) { pixel(xx, y, c); pixel(xx, y + h - 1, c); }
  for (int yy = y; yy < y + h; ++yy) { pixel(x, yy, c); pixel(x + w - 1, yy, c); }
  return true;
}
bool JWPLC_TFTClass::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t c, uint32_t)
{
  for (int yy = -r; yy <= r; ++yy) for (int xx = -r; xx <= r; ++xx) if (xx * xx + yy * yy <= r * r) pixel(x + xx, y + yy, c);
  return true;
}
bool JWPLC_TFTClass::pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *p, uint32_t)
{
  ++pushImages;
  for (int yy = 0; yy < h; ++yy) for (int xx = 0; xx < w; ++xx) pixel(x + xx, y + yy, p[yy * w + xx]);
  return true;
}
void JWPLC_TFTClass::getTextBounds(const char *s, int16_t, int16_t, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)
{
  *x1 = 0; *y1 = 0;
  const size_t n = strlen(s);
  *w = (uint16_t)(n * 6 * size);
  *h = (uint16_t)(n ? 8 * size : 0);
}
size_t JWPLC_TFTClass::write(uint8_t c)
{
  const uint8_t *g = jwplcSimGlyph(c);
  for (int col = 0; col < 6; ++col)
    for (int row = 0; row < 8; ++row)
    {
      const bool on = col < 5 && ((g[col] >> row) & 1);
      if (!on && fg == bg) continue; // setTextColor(fg): texto sin fondo, como TFT_eSPI
      for (int dy = 0; dy < size; ++dy) for (int dx = 0; dx < size; ++dx)
        pixel(cx + col * size + dx, cy + row * size + dy, on ? fg : bg);
    }
  cx += 6 * size;
  return 1;
}

inline void savePpm(const JWPLC_TFTClass &tft, const char *path)
{
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6 320 170 255 ");
  for (uint16_t c : tft.fb)
  {
    const uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 0x1F) * 255 / 31), (uint8_t)(((c >> 5) & 0x3F) * 255 / 63), (uint8_t)((c & 0x1F) * 255 / 31) };
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

// Framebuffer crudo RGB565 little-endian (lo compara icon-smoke.cjs con el lienzo).
inline void saveRaw(const JWPLC_TFTClass &tft, const char *path)
{
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fwrite(tft.fb, sizeof(uint16_t), 320 * 170, f);
  fclose(f);
}
