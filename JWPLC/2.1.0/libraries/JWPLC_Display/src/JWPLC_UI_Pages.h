#ifndef JWPLC_UI_PAGES_H
#define JWPLC_UI_PAGES_H

#include <Arduino.h>

class JWPLC_TFTClass;

namespace JWPLCUIPages
{
    static constexpr uint8_t MAX_PAGE_COUNT = 99;

    void setPageCount(uint8_t count);
    uint8_t pageCount();

    bool navigationEnabled();
    bool selectionMode();

    // Se ejecuta antes de consultar dirty/refresh. Lee estados físicos sin
    // consumir pressed()/released() salvo las teclas que pertenecen al sistema
    // de navegación de páginas.
    void serviceNavigation();

    // Restablece PAGE_SELECT al entrar nuevamente en USER.
    void prepareEnter();

    // Alpha15: si OK/ESC cambió el modo (selector <-> contenido), llama una vez
    // a jwplcUIPageModeChanged() con el bus tomado. Va antes del overlay NN/TT.
    void drawModeChange(JWPLC_TFTClass &tft);

    // Overlay global NN/TT. Se dibuja al final del refresh para quedar siempre
    // por encima de los fields de la página.
    void drawIndicator(JWPLC_TFTClass &tft);

    // Alpha15: visibilidad y esquina superior izquierda del indicador (36×12).
    void setIndicator(bool visible, int16_t x, int16_t y);
}

#endif // JWPLC_UI_PAGES_H
