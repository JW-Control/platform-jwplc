#ifndef JWPLC_DISPLAY_ST7789_H
#define JWPLC_DISPLAY_ST7789_H

#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include <JW_MatrixButtons.h>
#include <JW_RTC.h>

extern "C" {
  #include "jwplc_peripherals.h"
}

enum JWPLCButtonId : uint8_t
{
    BTN_LEFT = 0,
    BTN_UP,
    BTN_RIGHT,
    BTN_ESC,
    BTN_OK,
    BTN_DOWN,
    BTN_COUNT
};

extern JW_MatrixButtons JWPLC_Buttons;
extern JW_RTC JWPLC_RTC;

namespace JWPLCDisplay
{
    enum IdleReturnMode : uint8_t
    {
        IDLE_RETURN_TIMEOUT  = 0,
        IDLE_RETURN_ESC_ONLY = 1,
        IDLE_RETURN_DISABLED = 2
    };

    bool isReady();
    bool isIdleMode();
    bool buttonsReady();

    void forceRedraw();

    void enterUserUI();
    void goIdle();
    void notifyActivity();

    void setIdleReturnMode(IdleReturnMode mode);
    IdleReturnMode idleReturnMode();

    void setIdleTimeoutMs(uint32_t timeoutMs);
    uint32_t idleTimeoutMs();

    void setIdleRefreshPeriodMs(uint32_t ms);
    uint32_t idleRefreshPeriodMs();

    void setUserRefreshPeriodMs(uint32_t ms);
    uint32_t userRefreshPeriodMs();

    void clearPendingInput();

    Adafruit_ST7789& display();

    // Indicadores laterales
    void setRunLed(bool state);
    bool runLed();

    void setErrLed(bool state);
    bool errLed();

    void setBusLed(bool state);
    bool busLed();

    void setEthLed(bool state);
    bool ethLed();
}

#ifdef __cplusplus
extern "C" {
#endif

bool jwplcCanReturnToIdle(void);

void jwplcUserDisplayEnterCallback(void);
void jwplcUserDisplayRefreshCallback(const JWPLC_IOState* io, const JWPLC_RTCState* rtc);
void jwplcUserDisplayExitCallback(void);

#ifdef __cplusplus
}
#endif

#endif