#include "JWPLC_UI_Pages.h"

#include "JWPLC_Display_API.h"
#include "JWPLC_UI.h"

#include <JWPLC_GlobalPeripherals.h>
#include <Adafruit_ST7789.h>

#include <cstdio>

namespace
{
    static constexpr int16_t INDICATOR_X = 282;
    static constexpr int16_t INDICATOR_Y = 3;
    static constexpr int16_t INDICATOR_W = 36;
    static constexpr int16_t INDICATOR_H = 12;
    static constexpr int16_t INDICATOR_TEXT_X = 286;
    static constexpr int16_t INDICATOR_TEXT_Y = 5;

    uint8_t g_pageCount = 1;
    bool g_selectionMode = true;
    uint8_t g_previousMask = 0;

    JWPLC_DisplayIdleReturnMode g_savedIdleReturnMode = IDLE_RETURN_ESC_ONLY;
    bool g_idleReturnOverridden = false;

    uint8_t buttonMask(uint8_t buttonId)
    {
        return (buttonId < 8)
                   ? (uint8_t)(1u << buttonId)
                   : 0;
    }

    uint8_t readButtonDownMask()
    {
        if (!JWPLCButtons::isReady())
        {
            return 0;
        }

        uint8_t mask = 0;

        if (JWPLC_Buttons.isDown(BTN_LEFT))
            mask |= buttonMask(BTN_LEFT);
        if (JWPLC_Buttons.isDown(BTN_UP))
            mask |= buttonMask(BTN_UP);
        if (JWPLC_Buttons.isDown(BTN_RIGHT))
            mask |= buttonMask(BTN_RIGHT);
        if (JWPLC_Buttons.isDown(BTN_ESC))
            mask |= buttonMask(BTN_ESC);
        if (JWPLC_Buttons.isDown(BTN_OK))
            mask |= buttonMask(BTN_OK);
        if (JWPLC_Buttons.isDown(BTN_DOWN))
            mask |= buttonMask(BTN_DOWN);

        return mask;
    }

    void consumeNavigationInput()
    {
        JWPLCButtons::clearPendingInput();
    }

    void restoreIdleReturnMode()
    {
        if (!g_idleReturnOverridden)
        {
            return;
        }

        JWPLC_Display.setIdleReturnMode(g_savedIdleReturnMode);
        g_idleReturnOverridden = false;
    }

    void enterContentMode()
    {
        if (g_pageCount <= 1 || !g_selectionMode)
        {
            return;
        }

        g_savedIdleReturnMode = JWPLC_Display.idleReturnMode();
        JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
        g_idleReturnOverridden = true;
        g_selectionMode = false;
        JWPLCUI::requestRefresh();
    }

    void enterSelectionMode()
    {
        if (g_selectionMode)
        {
            return;
        }

        g_selectionMode = true;
        restoreIdleReturnMode();
        JWPLCUI::requestRefresh();
    }
}

namespace JWPLCUIPages
{
    void setPageCount(uint8_t count)
    {
        if (count == 0)
        {
            count = 1;
        }
        else if (count > MAX_PAGE_COUNT)
        {
            count = MAX_PAGE_COUNT;
        }

        if (g_pageCount == count)
        {
            return;
        }

        g_pageCount = count;

        if (JWPLCUI::currentPage() >= g_pageCount)
        {
            JWPLCUI::setPage((uint8_t)(g_pageCount - 1));
        }

        if (g_pageCount <= 1)
        {
            g_selectionMode = true;
            restoreIdleReturnMode();
        }
        else
        {
            g_selectionMode = true;
        }

        g_previousMask = readButtonDownMask();
        JWPLCUI::requestRefresh();
    }

    uint8_t pageCount()
    {
        return g_pageCount;
    }

    bool navigationEnabled()
    {
        return g_pageCount > 1;
    }

    bool selectionMode()
    {
        return navigationEnabled() && g_selectionMode;
    }

    void serviceNavigation()
    {
        const uint8_t downMask = readButtonDownMask();
        const uint8_t pressedEdges =
            (uint8_t)(downMask & (uint8_t)~g_previousMask);
        g_previousMask = downMask;

        if (!navigationEnabled() || pressedEdges == 0)
        {
            return;
        }

        if (g_selectionMode)
        {
            bool consumed = false;
            const uint8_t current = JWPLCUI::currentPage();

            if ((pressedEdges & buttonMask(BTN_LEFT)) != 0)
            {
                consumed = true;
                if (current > 0)
                {
                    JWPLCUI::setPage((uint8_t)(current - 1));
                }
            }

            if ((pressedEdges & buttonMask(BTN_RIGHT)) != 0)
            {
                consumed = true;
                if ((uint8_t)(current + 1) < g_pageCount)
                {
                    JWPLCUI::setPage((uint8_t)(current + 1));
                }
            }

            if ((pressedEdges & buttonMask(BTN_OK)) != 0)
            {
                consumed = true;
                enterContentMode();
            }

            // UP/DOWN no hacen nada en PAGE_SELECT y tampoco deben llegar a la
            // aplicación: en este modo la botonera pertenece al selector.
            if ((pressedEdges & (buttonMask(BTN_UP) | buttonMask(BTN_DOWN))) != 0)
            {
                consumed = true;
            }

            // ESC se deja al comportamiento IDLE existente. Si el usuario
            // mantiene IDLE_RETURN_ESC_ONLY, un segundo ESC desde el selector
            // puede regresar a IDLE sin introducir una ruta paralela.
            if (consumed)
            {
                consumeNavigationInput();
            }

            return;
        }

        // PAGE_CONTENT: LEFT/RIGHT/UP/DOWN/OK quedan intactos para el usuario.
        // Sólo ESC pertenece al sistema HMI y vuelve a PAGE_SELECT.
        if ((pressedEdges & buttonMask(BTN_ESC)) != 0)
        {
            consumeNavigationInput();
            enterSelectionMode();
        }
    }

    void prepareEnter()
    {
        if (!navigationEnabled())
        {
            g_previousMask = readButtonDownMask();
            return;
        }

        restoreIdleReturnMode();
        g_selectionMode = true;
        g_previousMask = readButtonDownMask();
        JWPLCUI::requestRefresh();
    }

    void drawIndicator(Adafruit_ST7789 &tft)
    {
        if (!navigationEnabled())
        {
            return;
        }

        const uint16_t background = g_selectionMode ? 0x0000 : 0xFFFF;
        const uint16_t foreground = g_selectionMode ? 0xFFFF : 0x0000;

        tft.fillRect(
            INDICATOR_X,
            INDICATOR_Y,
            INDICATOR_W,
            INDICATOR_H,
            background);

        tft.drawRect(
            INDICATOR_X,
            INDICATOR_Y,
            INDICATOR_W,
            INDICATOR_H,
            0xFFFF);

        char text[6] = {};
        snprintf(
            text,
            sizeof(text),
            "%02u/%02u",
            (unsigned)(JWPLCUI::currentPage() + 1),
            (unsigned)g_pageCount);

        tft.setTextSize(1);
        tft.setTextColor(foreground, background);
        tft.setCursor(INDICATOR_TEXT_X, INDICATOR_TEXT_Y);
        tft.print(text);
    }
}

void JWPLC_DisplayClass::setUserPageCount(uint8_t count)
{
    JWPLCUIPages::setPageCount(count);
}

uint8_t JWPLC_DisplayClass::userPageCount() const
{
    return JWPLCUIPages::pageCount();
}

bool JWPLC_DisplayClass::isUserPageSelection() const
{
    return JWPLCUIPages::selectionMode();
}
