#include "JWPLC_UI_PixelMap.h"

#include "JWPLC_Display_API.h"
#include "JWPLC_UI.h"

#include <Adafruit_ST7789.h>

namespace
{
    const JWPLC_UIPixelMap *g_pixelMaps = nullptr;
    size_t g_pixelMapCount = 0;
    bool g_pixelMapVisible[JWPLC_UI_MAX_PIXEL_MAPS] = {};

    void resetPixelMapVisibility(size_t count)
    {
        for (size_t i = 0; i < JWPLC_UI_MAX_PIXEL_MAPS; ++i)
        {
            g_pixelMapVisible[i] = (i < count);
        }
    }
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
        resetPixelMapVisibility(count);
        JWPLCUI::invalidateAll(true);
        return true;
    }

    void clearPixelMaps()
    {
        g_pixelMaps = nullptr;
        g_pixelMapCount = 0;
        resetPixelMapVisibility(0);
        JWPLCUI::invalidateAll(true);
    }

    size_t pixelMapCount()
    {
        return g_pixelMapCount;
    }

    bool setPixelMapVisible(size_t index, bool visible)
    {
        if (index >= g_pixelMapCount)
        {
            return false;
        }

        if (g_pixelMapVisible[index] == visible)
        {
            return true;
        }

        g_pixelMapVisible[index] = visible;
        JWPLCUI::invalidateAll(true);
        return true;
    }

    bool pixelMapVisible(size_t index)
    {
        return index < g_pixelMapCount && g_pixelMapVisible[index];
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
            if (!g_pixelMapVisible[mapIndex])
            {
                continue;
            }

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

bool JWPLC_DisplayClass::setPixelMapVisible(size_t index, bool visible)
{
    return JWPLCUI::setPixelMapVisible(index, visible);
}

bool JWPLC_DisplayClass::isPixelMapVisible(size_t index) const
{
    return JWPLCUI::pixelMapVisible(index);
}
