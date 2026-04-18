#ifndef JWPLC_DISPLAY_ST7789_H
#define JWPLC_DISPLAY_ST7789_H

#include <Arduino.h>
#include <Adafruit_ST7789.h>

extern "C" {
  #include "jwplc_peripherals.h"
}

namespace JWPLCDisplay
{
    enum IdleReturnMode : uint8_t
    {
        IDLE_RETURN_TIMEOUT = 0,
        IDLE_RETURN_ESC_ONLY = 1,
        IDLE_RETURN_DISABLED = 2
    };

    bool isReady();
    bool isIdleMode();

    void forceRedraw();

    void enterUserUI();
    void goIdle();
    void notifyActivity();

    void setIdleReturnMode(IdleReturnMode mode);
    IdleReturnMode idleReturnMode();

    void setIdleTimeoutMs(uint32_t timeoutMs);
    uint32_t idleTimeoutMs();

    Adafruit_ST7789& display();
}

// -----------------------------------------------------
// Hooks débiles de navegación / integración
// -----------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

bool jwplcNavLeftPressed(void);
bool jwplcNavRightPressed(void);
bool jwplcNavUpPressed(void);
bool jwplcNavDownPressed(void);
bool jwplcNavOkPressed(void);
bool jwplcNavEscPressed(void);

// Debe devolver true SOLO cuando la UI del usuario esté
// en la raíz y sea válido volver a REPOSO.
bool jwplcCanReturnToIdle(void);

// Se llama al entrar en modo usuario
void jwplcUserDisplayEnterCallback(void);

// Se llama continuamente mientras la UI del usuario está activa
void jwplcUserDisplayRefreshCallback(const JWPLC_IOState* io, const JWPLC_RTCState* rtc);

// Se llama al salir del modo usuario y volver a REPOSO
void jwplcUserDisplayExitCallback(void);

#ifdef __cplusplus
}
#endif

#endif