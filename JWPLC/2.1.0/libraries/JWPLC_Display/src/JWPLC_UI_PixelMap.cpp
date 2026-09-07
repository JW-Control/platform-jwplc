#include "JWPLC_UI_PixelMap.h"

#include "JWPLC_Display_API.h"
#include "JWPLC_UI.h"

#include <Adafruit_ST7789.h>

namespace
{
    const JWPLC_UIPixelMap *g_pixelMaps = nullptr;
    size_t g_pixelMapCount = 0;
}

JWPLC_UIPixelRun::JWPLC_UIPixelRun(
    int16_t xValue,
    int16_t yValue,
    int16_t widthValue,
    uint16_t colorValue)
    : x(xValue),
      y(yValue),
      width(widthValue),
      color(colorValue)
{
}

JWPLC_UIPixelMap::JWPLC_UIPixelMap(
    uint8_t pageValue,
    const JWPLC_UIPixelRun *runsValue,
    size_t runCountValue)
    : page(pageValue),
      runs(runsValue),
      runCount(runCountValue)
{
}

namespace JWPLCUI
{
    bool setPixelMaps(const JWPLC_UIPixelMap *maps, size_t count)
    {
        if ((maps == nullptr && count != 0) || count > JWPLC_UI_MAX_PIXEL_MAPS)
        {
            return false;
        }

        for (size_t i = 0; i < count; ++i)
        {
            if (maps[i].runs == nullptr && maps[i].runCount != 0)
            {
                return false;
            }
        }

        g_pixelMaps = maps;
        g_pixelMapCount = count;
        JWPLCUI::invalidateAll(true);
        return true;
    }

    void clearPixelMaps()
    {
        g_pixelMaps = nullptr;
        g_pixelMapCount = 0;
        JWPLCUI::invalidateAll(true);
    }

    size_t pixelMapCount()
    {
        return g_pixelMapCount;
    }

    void drawPixelMapsStatic(Adafruit_ST7789 &tft)
    {
        if (g_pixelMaps == nullptr || g_pixelMapCount == 0)
        {
            return;
        }

        const uint8_t page = JWPLCUI::currentPage();
        const int16_t tftWidth = tft.width();
        const int16_t tftHeight = tft.height();

        for (size_t mapIndex = 0; mapIndex < g_pixelMapCount; ++mapIndex)
        {
            const JWPLC_UIPixelMap &map = g_pixelMaps[mapIndex];

            if (map.page != page || map.runs == nullptr)
            {
                continue;
            }

            for (size_t runIndex = 0; runIndex < map.runCount; ++runIndex)
            {
                const JWPLC_UIPixelRun &run = map.runs[runIndex];

                if (run.width <= 0 || run.y < 0 || run.y >= tftHeight)
                {
                    continue;
                }

                int16_t x = run.x;
                int16_t width = run.width;

                if (x < 0)
                {
                    width += x;
                    x = 0;
                }

                if (x >= tftWidth || width <= 0)
                {
                    continue;
                }

                if ((int32_t)x + width > tftWidth)
                {
                    width = tftWidth - x;
                }

                if (width > 0)
                {
                    tft.drawFastHLine(x, run.y, width, run.color);
                }
            }
        }
    }
}

bool JWPLC_DisplayClass::setPixelMaps(const JWPLC_UIPixelMap *maps, size_t count)
{
    return JWPLCUI::setPixelMaps(maps, count);
}

void JWPLC_DisplayClass::clearPixelMaps()
{
    JWPLCUI::clearPixelMaps();
}

size_t JWPLC_DisplayClass::pixelMapCount() const
{
    return JWPLCUI::pixelMapCount();
}
