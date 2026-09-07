#ifndef JWPLC_UI_PIXELMAP_H
#define JWPLC_UI_PIXELMAP_H

#include <Arduino.h>
#include <stddef.h>

class Adafruit_ST7789;

static constexpr uint8_t JWPLC_UI_MAX_PIXEL_MAPS = 16;
static constexpr uint8_t JWPLC_UI_PIXEL_PACKED_MAX_COLORS = 16;

// Run horizontal estático RGB565. Se conserva como formato compatible/fallback
// para mapas que no conviene representar mediante la codificación compacta.
struct JWPLC_UIPixelRun
{
    int16_t x;
    int16_t y;
    int16_t width;
    uint16_t color;

    JWPLC_UIPixelRun(
        int16_t xValue = 0,
        int16_t yValue = 0,
        int16_t widthValue = 1,
        uint16_t colorValue = 0xFFFF);
};

// PixelMap estático asociado a una página HMI mediante runs RGB565 horizontales.
struct JWPLC_UIPixelMap
{
    uint8_t page;
    const JWPLC_UIPixelRun *runs;
    size_t runCount;

    JWPLC_UIPixelMap(
        uint8_t pageValue = 0,
        const JWPLC_UIPixelRun *runsValue = nullptr,
        size_t runCountValue = 0);
};

// Span compacto de 32 bits generado por el Designer.
//
// Layout PACKED_SPAN16:
//   bits  0.. 8 : X        (0..319)
//   bits  9..16 : Y        (0..169)
//   bits 17..25 : LEN-1    (0..319 => 1..320 px)
//   bits 26..29 : COLOR    (índice 0..15 en palette)
//   bit      30 : DIR      (0=horizontal, 1=vertical)
//   bit      31 : reservado (0)
//
// La paleta RGB565 se almacena una sola vez por PixelMap. El formato permite
// comprimir líneas horizontales y verticales sin perder ningún píxel.
struct JWPLC_UIPixelPackedMap
{
    uint8_t page;
    const uint16_t *palette;
    uint8_t paletteCount;
    const uint32_t *spans;
    size_t spanCount;

    JWPLC_UIPixelPackedMap(
        uint8_t pageValue = 0,
        const uint16_t *paletteValue = nullptr,
        uint8_t paletteCountValue = 0,
        const uint32_t *spansValue = nullptr,
        size_t spanCountValue = 0);
};

namespace JWPLCUI
{
    bool setPixelMaps(const JWPLC_UIPixelMap *maps, size_t count);
    bool setPackedPixelMaps(const JWPLC_UIPixelPackedMap *maps, size_t count);
    void clearPixelMaps();
    size_t pixelMapCount();

    // Visibilidad común a cualquiera de los dos formatos activos. El sketch no
    // necesita conocer qué codificación eligió el Designer.
    bool setPixelMapVisible(size_t index, bool visible);
    bool pixelMapVisible(size_t index);

    // Hook interno invocado antes de drawStatic(fields), de modo que los
    // campos declarativos permanezcan por encima del PixelMap.
    void drawPixelMapsStatic(Adafruit_ST7789 &tft);
}

#endif // JWPLC_UI_PIXELMAP_H
