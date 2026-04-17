#ifndef JWPLC_DISPLAY_ST7789_H
#define JWPLC_DISPLAY_ST7789_H

#include <Arduino.h>

namespace JWPLCDisplay
{
    bool isReady();
    void forceRedraw();
}

#endif