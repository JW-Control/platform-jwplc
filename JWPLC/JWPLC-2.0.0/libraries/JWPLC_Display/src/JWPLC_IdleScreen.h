#ifndef JWPLC_IDLESCREEN_H
#define JWPLC_IDLESCREEN_H

#include <Arduino.h>
#include <Adafruit_ST7789.h>

extern "C" {
  #include "jwplc_peripherals.h"
}

namespace JWPLCIdleScreen
{
    struct StatusPanel
    {
        bool pwr = true;
        bool run = true;
        bool err = false;
        bool bus = false;
        bool eth = false;
    };

    void begin(Adafruit_ST7789* display);
    void setTitle(const char* title);
    void setStatusPanel(const StatusPanel& panel);
    const StatusPanel& statusPanel();

    void forceFullRedraw();
    void draw(const JWPLC_IOState* io, const JWPLC_RTCState* rtc);
}

#endif // JWPLC_IDLESCREEN_H