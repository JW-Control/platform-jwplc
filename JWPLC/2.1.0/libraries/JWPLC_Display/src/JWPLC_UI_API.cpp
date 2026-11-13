#include "JWPLC_UI.h"
#include <JWPLC_TFT.h>
#include "JWPLC_UI_Pages.h"
#include "JWPLC_UI_PixelMap.h"
#include "JWPLC_UI_RuntimeHooks.h"

// Los constructores y helpers de campo son constexpr inline en JWPLC_UI.h
// para que HMI_FIELDS[] resida en Flash (.rodata) sin consumir DRAM.

// Hooks del MENU (Alpha15). JWPLC_UI_Menu.cpp aporta las versiones strong y
// sólo se enlaza si el sketch registra menús (setMenus / JWPLC_UIMenu*).
extern "C" void __attribute__((weak)) jwplcUIMenuServiceInput(void) {}
extern "C" void __attribute__((weak)) jwplcUIMenuPrepareEnter(void) {}
extern "C" void __attribute__((weak)) jwplcUIMenuInvalidateAll(void) {}
extern "C" void __attribute__((weak)) jwplcUIMenuDrawStatic(JWPLC_TFTClass *tft) { (void)tft; }
extern "C" void __attribute__((weak)) jwplcUIMenuDrawDirty(JWPLC_TFTClass *tft) { (void)tft; }

// Hooks de EVENT_LOG (Alpha16). JWPLC_UI_EventLog.cpp aporta las versiones strong.
extern "C" void __attribute__((weak)) jwplcUIEventLogServiceInput(void) {}
extern "C" void __attribute__((weak)) jwplcUIEventLogPrepareEnter(void) {}
extern "C" void __attribute__((weak)) jwplcUIEventLogInvalidateAll(void) {}
extern "C" void __attribute__((weak)) jwplcUIEventLogDrawStatic(JWPLC_TFTClass *tft) { (void)tft; }
extern "C" void __attribute__((weak)) jwplcUIEventLogDrawDirty(JWPLC_TFTClass *tft) { (void)tft; }

// Implementaciones strong de los hooks internos. Este TU se enlaza cuando
// cualquier entrada HMI (setFields, setPageCount, setPage, setMenus,
// setPixelMaps) referencia jwplcUIRuntimeEnsureLinked().
extern "C" void jwplcUIRuntimeEnsureLinked(void) {}

extern "C" void jwplcUIRuntimeServiceInput(void)
{
    // EVENT_LOG y MENU atienden sus teclas con prioridad antes del selector de páginas.
    jwplcUIEventLogServiceInput();
    jwplcUIMenuServiceInput();
    JWPLCUIPages::serviceNavigation();
    JWPLCUI::pollClockFields();
}

extern "C" bool jwplcUIRuntimeRefreshNeeded(void)
{
    // Compatibilidad con callbacks USER manuales/legacy:
    // si el motor HMI queda enlazado pero no hay fields ni PixelMaps
    // declarativos registrados, no debe filtrar jwplcUserDisplayRefreshCallback().
    // El periodo USER sigue siendo responsabilidad de JWPLC_Display.
    if (JWPLCUI::fieldCount() == 0 && JWPLCUI::pixelMapCount() == 0)
    {
        return true;
    }

    return JWPLCUI::refreshNeeded();
}

extern "C" bool jwplcUIRuntimeBackgroundTrendDue(void)
{
    return JWPLCUI::anyTrendSampleDue();
}

extern "C" void jwplcUIRuntimeTickDueTrends(void)
{
    JWPLCUI::tickDueTrends();
}

extern "C" void jwplcUIRuntimeInvalidateAll(bool redrawStatic)
{
    jwplcUIEventLogInvalidateAll();
    jwplcUIMenuInvalidateAll();
    JWPLCUI::invalidateAll(redrawStatic);
}

extern "C" void jwplcUIRuntimePrepareEnter(void)
{
    JWPLCUI::prepareEnter();
    jwplcUIMenuPrepareEnter();
    jwplcUIEventLogPrepareEnter();
    JWPLCUIPages::prepareEnter();
}

extern "C" uint8_t jwplcUIRuntimeCurrentPage(void)
{
    return JWPLCUI::currentPage();
}

extern "C" bool jwplcUIRuntimePageRedrawPending(void)
{
    return JWPLCUI::pageRedrawPending();
}

extern "C" void jwplcUIRuntimeConsumePageRedrawPending(void)
{
    JWPLCUI::consumePageRedrawPending();
}

extern "C" void jwplcUIRuntimeConsumeRefreshRequest(void)
{
    JWPLCUI::consumeRefreshRequest();
}

extern "C" void jwplcUIRuntimeDrawStatic(JWPLC_TFTClass *tft)
{
    if (tft != nullptr)
    {
        // PixelMaps son fondo estático de la página; los fields declarativos se
        // dibujan después y por tanto conservan prioridad visual.
        JWPLCUI::drawPixelMapsStatic(*tft);
        JWPLCUI::drawStatic(*tft);
        jwplcUIMenuDrawStatic(tft);
        jwplcUIEventLogDrawStatic(tft);
    }
}

extern "C" void jwplcUIRuntimeDrawDirty(JWPLC_TFTClass *tft)
{
    if (tft != nullptr)
    {
        JWPLCUI::drawDirty(*tft);
        jwplcUIMenuDrawDirty(tft);
        jwplcUIEventLogDrawDirty(tft);
        JWPLCUIPages::drawModeChange(*tft);
        JWPLCUIPages::drawIndicator(*tft);
    }
}
