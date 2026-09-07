#include "JWPLC_UI_PixelMap.h"

#include "JWPLC_Display_API.h"
#include "JWPLC_UI.h"

#include <Adafruit_ST7789.h>

namespace
{
    const JWPLC_UIPixelMap *g_pixelMaps = nullptr;
    size_t g_pixelMapCount = 0;

    const JWPLC_UIPixelPackedMap *g_packedPixelMaps = nullptr;
    size_t g_packedPixelMapCount = 0;

    bool g_pixelMapVisible[JWPLC_UI_MAX_PIXEL_MAPS] = {};

    size_t activePixelMapCount()
    {
        return g_packedPixelMaps != nullptr ? g_packedPixelMapCount : g_pixelMapCount;
    }

    void resetPixelMapVisibility(size_t count)
    {
        for (size_t i = 0; i < JWPLC_UI_MAX_PIXEL_MAPS; ++i)
        {
            g_pixelMapVisible[i] = (i < count);
        }
    }

    uint16_t packedX(uint32_t span)
    {
        return static_cast<uint16_t>(span & 0x01FFu);
    }

    uint16_t packedY(uint32_t span)
    {
        return static_cast<uint16_t>((span >> 9) & 0x00FFu);
    }

    uint16_t packedLength(uint32_t span)
    {
        return static_cast<uint16_t>(((span >> 17) & 0x01FFu) + 1u);
    }

    uint8_t packedColorIndex(uint32_t span)
    {
        return static_cast<uint8_t>((span >> 26) & 0x0Fu);
    }

    bool packedVertical(uint32_t span)
    {
        return (span & 0x40000000u) != 0;
    }

    void drawHorizontalClipped(
        Adafruit_ST7789 &tft,
        int16_t x,
        int16_t y,
        int16_t width,
        uint16_t color)
    {
        const int16_t tftWidth = tft.width();
        const int16_t tftHeight = tft.height();

        if (width <= 0 || y < 0 || y >= tftHeight)
        {
            return;
        }

        if (x < 0)
        {
            width += x;
            x = 0;
        }

        if (x >= tftWidth || width <= 0)
        {
            return;
        }

        if ((int32_t)x + width > tftWidth)
        {
            width = tftWidth - x;
        }

        if (width > 0)
        {
            tft.drawFastHLine(x, y, width, color);
        }
    }

    void drawVerticalClipped(
        Adafruit_ST7789 &tft,
        int16_t x,
        int16_t y,
        int16_t height,
        uint16_t color)
    {
        const int16_t tftWidth = tft.width();
        const int16_t tftHeight = tft.height();

        if (height <= 0 || x < 0 || x >= tftWidth)
        {
            return;
        }

        if (y < 0)
        {
            height += y;
            y = 0;
        }

        if (y >= tftHeight || height <= 0)
        {
            return;
        }

        if ((int32_t)y + height > tftHeight)
        {
            height = tftHeight - y;
        }

        if (height > 0)
        {
            tft.drawFastVLine(x, y, height, color);
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

JWPLC_UIPixelPackedMap::JWPLC_UIPixelPackedMap(
    uint8_t pageValue,
    const uint16_t *paletteValue,
    uint8_t paletteCountValue,
    const uint32_t *spansValue,
    size_t spanCountValue)
    : page(pageValue),
      palette(paletteValue),
      paletteCount(paletteCountValue),
      spans(spansValue),
      spanCount(spanCountValue)
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
        g_packedPixelMaps = nullptr;
        g_packedPixelMapCount = 0;
        resetPixelMapVisibility(count);
        JWPLCUI::invalidateAll(true);
        return true;
    }

    bool setPackedPixelMaps(const JWPLC_UIPixelPackedMap *maps, size_t count)
    {
        if ((maps == nullptr && count != 0) || count > JWPLC_UI_MAX_PIXEL_MAPS)
        {
            return false;
        }

        for (size_t i = 0; i < count; ++i)
        {
            const JWPLC_UIPixelPackedMap &map = maps[i];

            if (map.paletteCount > JWPLC_UI_PIXEL_PACKED_MAX_COLORS)
            {
                return false;
            }

            if ((map.palette == nullptr && map.paletteCount != 0) ||
                (map.spans == nullptr && map.spanCount != 0) ||
                (map.spanCount != 0 && map.paletteCount == 0))
            {
                return false;
            }

            for (size_t spanIndex = 0; spanIndex < map.spanCount; ++spanIndex)
            {
                if (packedColorIndex(map.spans[spanIndex]) >= map.paletteCount)
                {
                    return false;
                }
            }
        }

        g_pixelMaps = nullptr;
        g_pixelMapCount = 0;
        g_packedPixelMaps = maps;
        g_packedPixelMapCount = count;
        resetPixelMapVisibility(count);
        JWPLCUI::invalidateAll(true);
        return true;
    }

    void clearPixelMaps()
    {
        g_pixelMaps = nullptr;
        g_pixelMapCount = 0;
        g_packedPixelMaps = nullptr;
        g_packedPixelMapCount = 0;
        resetPixelMapVisibility(0);
        JWPLCUI::invalidateAll(true);
    }

    size_t pixelMapCount()
    {
        return activePixelMapCount();
    }

    bool setPixelMapVisible(size_t index, bool visible)
    {
        if (index >= activePixelMapCount())
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
        return index < activePixelMapCount() && g_pixelMapVisible[index];
    }

    void drawPixelMapsStatic(Adafruit_ST7789 &tft)
    {
        const uint8_t page = JWPLCUI::currentPage();

        if (g_packedPixelMaps != nullptr && g_packedPixelMapCount != 0)
        {
            for (size_t mapIndex = 0; mapIndex < g_packedPixelMapCount; ++mapIndex)
            {
                if (!g_pixelMapVisible[mapIndex])
                {
                    continue;
                }

                const JWPLC_UIPixelPackedMap &map = g_packedPixelMaps[mapIndex];
                if (map.page != page || map.spans == nullptr || map.palette == nullptr)
                {
                    continue;
                }

                for (size_t spanIndex = 0; spanIndex < map.spanCount; ++spanIndex)
                {
                    const uint32_t span = map.spans[spanIndex];
                    const uint8_t colorIndex = packedColorIndex(span);
                    if (colorIndex >= map.paletteCount)
                    {
                        continue;
                    }

                    const int16_t x = static_cast<int16_t>(packedX(span));
                    const int16_t y = static_cast<int16_t>(packedY(span));
                    const int16_t length = static_cast<int16_t>(packedLength(span));
                    const uint16_t color = map.palette[colorIndex];

                    if (packedVertical(span))
                    {
                        drawVerticalClipped(tft, x, y, length, color);
                    }
                    else
                    {
                        drawHorizontalClipped(tft, x, y, length, color);
                    }
                }
            }
            return;
        }

        if (g_pixelMaps == nullptr || g_pixelMapCount == 0)
        {
            return;
        }

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
                drawHorizontalClipped(tft, run.x, run.y, run.width, run.color);
            }
        }
    }
}

bool JWPLC_DisplayClass::setPixelMaps(const JWPLC_UIPixelMap *maps, size_t count)
{
    return JWPLCUI::setPixelMaps(maps, count);
}

bool JWPLC_DisplayClass::setPackedPixelMaps(const JWPLC_UIPixelPackedMap *maps, size_t count)
{
    return JWPLCUI::setPackedPixelMaps(maps, count);
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
