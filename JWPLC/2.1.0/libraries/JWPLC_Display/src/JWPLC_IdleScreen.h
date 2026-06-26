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
        STATUS_LED_DISABLED = 0, // Gris: periférico no disponible o no iniciado
        STATUS_LED_OFF,          // Negro: disponible, pero inactivo
        STATUS_LED_GREEN,        // Verde: OK / actividad
        STATUS_LED_RED           // Rojo: error
    };

    struct StatusPanel
    {
        bool pwr = true;
        bool run = true;
        bool err = false;
        StatusLedState bus = STATUS_LED_DISABLED;
        StatusLedState eth = STATUS_LED_DISABLED;
    };

    void begin(Adafruit_ST7789 *display);
    void setTitle(const char *title);
    void setStatusPanel(const StatusPanel &panel);
    const StatusPanel &statusPanel();

    void forceFullRedraw();
    void draw(const JWPLC_IOState *io, const JWPLC_RTCState *rtc);
}

#endif // JWPLC_IDLESCREEN_H