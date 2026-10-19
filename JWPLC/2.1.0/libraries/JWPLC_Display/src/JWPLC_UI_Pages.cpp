#include "JWPLC_UI_Pages.h"
#include <JWPLC_TFT.h>

#include "JWPLC_Display_API.h"
#include "JWPLC_UI.h"
#include "JWPLC_Display_H3E1_Profile.h"

#include <JWPLC_GlobalPeripherals.h>
#include <Arduino.h>

#include <cstdio>

namespace
{
    static constexpr int16_t INDICATOR_W = 36;
    static constexpr int16_t INDICATOR_H = 12;
    static constexpr int16_t INDICATOR_TEXT_DX = 4;
    static constexpr int16_t INDICATOR_TEXT_DY = 2;
    static constexpr int16_t SCREEN_W = 320;
    static constexpr int16_t SCREEN_H = 170;

    // Alpha15: posición y visibilidad configurables (setPageIndicator). Por
    // defecto, el overlay de Alpha11 arriba a la derecha.
    int16_t g_indicatorX = 282;
    int16_t g_indicatorY = 3;
    bool g_indicatorVisible = true;

    uint8_t g_pageCount = 1;
    bool g_selectionMode = true;
    uint8_t g_previousMask = 0;
    bool g_indicatorDirty = true;
    // Alpha15: el modo cambió sin cambiar de página (la barra redibuja su pestaña).
    bool g_modeDirty = false;

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

    void markIndicatorDirty()
    {
        g_indicatorDirty = true;
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

        // En PAGE_CONTENT ESC pertenece al contenido (vuelve al selector), así
        // que sólo se suspende la vuelta por tecla. La vuelta por inactividad
        // sigue activa en cualquier sección (Alpha15).
        g_savedIdleReturnMode = JWPLC_Display.idleReturnMode();
        if (g_savedIdleReturnMode == IDLE_RETURN_ESC_ONLY ||
            g_savedIdleReturnMode == IDLE_RETURN_BUTTON_ONLY)
        {
            JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
            g_idleReturnOverridden = true;
        }
        g_selectionMode = false;
        g_modeDirty = true;
        markIndicatorDirty();
        JWPLCUI::requestRefresh();
    }

    void enterSelectionMode()
    {
        if (g_selectionMode)
        {
            return;
        }

        g_selectionMode = true;
        g_modeDirty = true;
        restoreIdleReturnMode();

        // Al salir de PAGE_CONTENT no deben sobrevivir PRESS/RELEASE/REPEAT
        // de teclas que pertenecían a la aplicación. En particular, un OK
        // pendiente no puede provocar un reingreso inmediato al selector.
        consumeNavigationInput();

        // Sincroniza también el snapshot físico: si una tecla sigue sostenida
        // al volver al selector, no debe aparecer como un flanco nuevo.
        g_previousMask = readButtonDownMask();

        markIndicatorDirty();
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
        markIndicatorDirty();
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

        if (!navigationEnabled())
        {
            return;
        }

        if (g_selectionMode)
        {
            // PAGE_SELECT es propiedad del sistema HMI. Usamos los latches
            // pressed() para no perder pulsaciones entre dos ticks de Display.
            // En este modo es correcto consumir LEFT/RIGHT/UP/DOWN/OK.
            const bool leftPressed = JWPLC_Buttons.pressed(BTN_LEFT);
            const bool rightPressed = JWPLC_Buttons.pressed(BTN_RIGHT);
            const bool okPressed = JWPLC_Buttons.pressed(BTN_OK);
            const bool upPressed = JWPLC_Buttons.pressed(BTN_UP);
            const bool downPressed = JWPLC_Buttons.pressed(BTN_DOWN);

            bool consumed = false;
            uint8_t current = JWPLCUI::currentPage();

            if (leftPressed)
            {
                consumed = true;
                if (current > 0)
                {
                    current = (uint8_t)(current - 1);
                    JWPLCUI::setPage(current);
                    markIndicatorDirty();
                }
            }

            if (rightPressed)
            {
                consumed = true;
                if ((uint8_t)(current + 1) < g_pageCount)
                {
                    current = (uint8_t)(current + 1);
                    JWPLCUI::setPage(current);
                    markIndicatorDirty();
                }
            }

            if (okPressed)
            {
                consumed = true;
                enterContentMode();
            }

            // UP/DOWN no hacen nada en PAGE_SELECT y tampoco deben llegar a la
            // aplicación: en este modo la botonera pertenece al selector.
            if (upPressed || downPressed)
            {
                consumed = true;
            }

            // ESC se deja al comportamiento IDLE existente. El resto de las
            // teclas del selector se absorben completamente al cambiar de modo.
            if (consumed)
            {
                consumeNavigationInput();
            }

            return;
        }

        // PAGE_CONTENT: LEFT/RIGHT/UP/DOWN/OK quedan intactos para el usuario.
        // Para ON_DEMAND mantenemos el wake por flanco físico sin consumir sus
        // latches. En PERIODIC este request es inocuo y no añade redibujos si
        // los setters no modifican ningún field.
        const uint8_t userOwnedMask =
            buttonMask(BTN_LEFT) |
            buttonMask(BTN_RIGHT) |
            buttonMask(BTN_UP) |
            buttonMask(BTN_DOWN) |
            buttonMask(BTN_OK);

        if ((pressedEdges & userOwnedMask) != 0)
        {
            JWPLCUI::requestRefresh();
        }

        // ESC sí pertenece al sistema HMI en PAGE_CONTENT. Se usa el latch
        // pressed() para que la salida al selector no dependa de acertar el
        // intervalo de sondeo físico del Display.
        if (JWPLC_Buttons.pressed(BTN_ESC))
        {
            enterSelectionMode();
        }
    }

    void prepareEnter()
    {
        if (!navigationEnabled())
        {
            g_previousMask = readButtonDownMask();
            markIndicatorDirty();
            return;
        }

        restoreIdleReturnMode();
        g_selectionMode = true;
        g_modeDirty = false; // la entrada dibuja la página completa en este modo

        // La tecla que despertó la IDLE ya cumplió su función: su PRESS no debe
        // actuar además sobre el selector (un OK entraba directo al contenido y
        // un LEFT/RIGHT cambiaba de página). En PAGE_SELECT la botonera es del
        // sistema, igual que en enterSelectionMode().
        consumeNavigationInput();

        g_previousMask = readButtonDownMask();
        markIndicatorDirty();
        JWPLCUI::requestRefresh();
    }

    void setIndicator(bool visible, int16_t x, int16_t y)
    {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x > SCREEN_W - INDICATOR_W) x = SCREEN_W - INDICATOR_W;
        if (y > SCREEN_H - INDICATOR_H) y = SCREEN_H - INDICATOR_H;

        g_indicatorVisible = visible;
        g_indicatorX = x;
        g_indicatorY = y;
        markIndicatorDirty();
        JWPLCUI::requestRefresh();
    }

    void drawIndicator(JWPLC_TFTClass &tft)
    {
        if (!navigationEnabled() || !g_indicatorVisible || !g_indicatorDirty)
        {
            return;
        }

        const bool profile =
            jwplcH3E1ProfilerEnabled();

        const uint32_t startUs =
            profile ? micros() : 0U;

        const uint16_t background = g_selectionMode ? 0x0000 : 0xFFFF;
        const uint16_t foreground = g_selectionMode ? 0xFFFF : 0x0000;

        tft.fillRect(
            g_indicatorX,
            g_indicatorY,
            INDICATOR_W,
            INDICATOR_H,
            background);

        tft.drawRect(
            g_indicatorX,
            g_indicatorY,
            INDICATOR_W,
            INDICATOR_H,
            0xFFFF);

        char text[8] = {}; // "NN/TT" + margen: el compilador no sabe que pageCount <= 99
        snprintf(
            text,
            sizeof(text),
            "%02u/%02u",
            (unsigned)(JWPLCUI::currentPage() + 1),
            (unsigned)g_pageCount);

        tft.setTextSize(1);
        tft.setTextColor(foreground, background);
        tft.setCursor(g_indicatorX + INDICATOR_TEXT_DX, g_indicatorY + INDICATOR_TEXT_DY);
        tft.print(text);

        g_indicatorDirty = false;

        if (profile)
        {
            jwplcH3E1RecordStage(
                JWPLC_H3E1_STAGE_INDICATOR,
                (uint32_t)(micros() - startUs));
        }
    }
}

// Alpha15: sin header del Designer (o sin barra de pestañas) no hay nada que
// redibujar al cambiar de modo; el NN/TT ya se repinta solo.
extern "C" void __attribute__((weak)) jwplcUIPageModeChanged(uint8_t page, bool selecting)
{
    (void)page;
    (void)selecting;
}

void JWPLCUIPages::drawModeChange(JWPLC_TFTClass &tft)
{
    (void)tft;
    if (!g_modeDirty)
    {
        return;
    }
    g_modeDirty = false;
    if (!navigationEnabled())
    {
        return;
    }
    jwplcUIPageModeChanged(JWPLCUI::currentPage(), g_selectionMode);
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

void JWPLC_DisplayClass::setPageIndicator(bool visible, int16_t x, int16_t y)
{
    JWPLCUIPages::setIndicator(visible, x, y);
}