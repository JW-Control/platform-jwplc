#ifndef JWPLC_IDLESCREEN_H
#define JWPLC_IDLESCREEN_H

#include <Arduino.h>
#include <Adafruit_ST7789.h>

extern "C"
{
#include "jwplc_peripherals.h"
}

namespace JWPLCIdleScreen
{
    enum StatusLedState : uint8_t
    {
        STATUS_LED_OFF = 0,
        STATUS_LED_GREEN,
        STATUS_LED_RED
    };

    struct StatusPanel
    {
        bool pwr = true;
        bool run = true;
        bool err = false;
        bool bus = false;

        // ETH necesita 3 estados:
        // OFF   = sin link / disabled
        // GREEN = link + Ethernet OK
        // RED   = link/hardware con falla
        StatusLedState eth = STATUS_LED_OFF;
    };

    void begin(Adafruit_ST7789 *display);
    void setTitle(const char *title);
    void setStatusPanel(const StatusPanel &panel);
    const StatusPanel &statusPanel();

    void forceFullRedraw();
    void draw(const JWPLC_IOState *io, const JWPLC_RTCState *rtc);
}

#endif // JWPLC_IDLESCREEN_H