#ifndef JWPLC_UI_PAGES_H
#define JWPLC_UI_PAGES_H

#include <Arduino.h>

class Adafruit_ST7789;

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

    // Overlay global NN/TT. Se dibuja al final del refresh para quedar siempre
    // por encima de los fields de la página.
    void drawIndicator(Adafruit_ST7789 &tft);
}

#endif // JWPLC_UI_PAGES_H
