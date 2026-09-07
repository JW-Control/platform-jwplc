#ifndef JWPLC_UI_PIXELMAP_H
#define JWPLC_UI_PIXELMAP_H

#include <Arduino.h>
#include <stddef.h>

class Adafruit_ST7789;

static constexpr uint8_t JWPLC_UI_MAX_PIXEL_MAPS = 16;

// Run horizontal estático RGB565. El Designer agrupa píxeles consecutivos del
// mismo color para reducir el tamaño del header y el número de transacciones TFT.
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

// PixelMap estático asociado a una página HMI. Sus runs no cambian en runtime,
// pero cada mapa puede mostrarse u ocultarse por índice para soportar frames,
// estados gráficos y animaciones declarativas sin tocar la TFT directamente.
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

namespace JWPLCUI
{
    bool setPixelMaps(const JWPLC_UIPixelMap *maps, size_t count);
    void clearPixelMaps();
    size_t pixelMapCount();

    bool setPixelMapVisible(size_t index, bool visible);
    bool pixelMapVisible(size_t index);

    // Hook interno invocado antes de drawStatic(fields), de modo que los
    // campos declarativos permanezcan por encima del PixelMap.
    void drawPixelMapsStatic(Adafruit_ST7789 &tft);
}

#endif // JWPLC_UI_PIXELMAP_H
