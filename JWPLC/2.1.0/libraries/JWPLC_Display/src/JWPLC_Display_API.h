#ifndef JWPLC_DISPLAY_API_H
#define JWPLC_DISPLAY_API_H

#include <Arduino.h>

class Adafruit_ST7789;

// =====================================================
// JWPLC_DisplayClass
// API pública estilo objeto para el display JWPLC.
//
// Esta clase es liviana y puede exponerse desde JWPLC_Display_Auto.h
// sin incluir headers pesados del display.
// =====================================================

class JWPLC_DisplayClass
{
public:
    enum IdleWakeMode : uint8_t
    {
        IDLE_WAKE_ANY_BUTTON = 0,
        IDLE_WAKE_BUTTON_ONLY = 1,
        IDLE_WAKE_DISABLED = 2
    };

    enum IdleReturnMode : uint8_t
    {
        IDLE_RETURN_TIMEOUT = 0,
        IDLE_RETURN_ESC_ONLY = 1,
        IDLE_RETURN_DISABLED = 2,
        IDLE_RETURN_BUTTON_ONLY = 3
    };

    bool isReady() const;
    bool isIdleMode() const;
    bool buttonsReady() const;

    void forceRedraw();

    void enterUserUI();
    void goIdle();
    void notifyActivity();

    void setIdleWakeMode(IdleWakeMode mode);
    IdleWakeMode idleWakeMode() const;

    void setIdleWakeButton(uint8_t buttonId);
    uint8_t idleWakeButton() const;

    void setIdleReturnMode(IdleReturnMode mode);
    IdleReturnMode idleReturnMode() const;

    void setIdleReturnButton(uint8_t buttonId);
    uint8_t idleReturnButton() const;

    void setIdleTimeoutMs(uint32_t timeoutMs);
    uint32_t idleTimeoutMs() const;

    void setIdleRefreshPeriodMs(uint32_t ms);
    uint32_t idleRefreshPeriodMs() const;

    void setUserRefreshPeriodMs(uint32_t ms);
    uint32_t userRefreshPeriodMs() const;

    void clearPendingInput();

    // Alias principal recomendado.
    Adafruit_ST7789 &tft();

    // Alias compatible/legible.
    Adafruit_ST7789 &display();

    // Indicadores laterales de la pantalla IDLE.
    void setRunLed(bool state);
    bool runLed() const;

    void setErrLed(bool state);
    bool errLed() const;

    void setBusLed(bool state);
    bool busLed() const;

    void setEthLed(bool state);
    bool ethLed() const;
    void setEthLedAuto(bool enabled);
    bool ethLedAuto() const;
};

// Objeto global recomendado para sketches.
extern JWPLC_DisplayClass JWPLC_Display;

// =====================================================
// Alias globales para evitar uso de JWPLC_DisplayClass::
// en sketches de usuario.
// =====================================================

using JWPLC_DisplayIdleWakeMode = JWPLC_DisplayClass::IdleWakeMode;

static constexpr JWPLC_DisplayIdleWakeMode IDLE_WAKE_ANY_BUTTON =
    JWPLC_DisplayClass::IDLE_WAKE_ANY_BUTTON;

static constexpr JWPLC_DisplayIdleWakeMode IDLE_WAKE_BUTTON_ONLY =
    JWPLC_DisplayClass::IDLE_WAKE_BUTTON_ONLY;

static constexpr JWPLC_DisplayIdleWakeMode IDLE_WAKE_DISABLED =
    JWPLC_DisplayClass::IDLE_WAKE_DISABLED;

using JWPLC_DisplayIdleReturnMode = JWPLC_DisplayClass::IdleReturnMode;

static constexpr JWPLC_DisplayIdleReturnMode IDLE_RETURN_TIMEOUT =
    JWPLC_DisplayClass::IDLE_RETURN_TIMEOUT;

static constexpr JWPLC_DisplayIdleReturnMode IDLE_RETURN_ESC_ONLY =
    JWPLC_DisplayClass::IDLE_RETURN_ESC_ONLY;

static constexpr JWPLC_DisplayIdleReturnMode IDLE_RETURN_DISABLED =
    JWPLC_DisplayClass::IDLE_RETURN_DISABLED;

static constexpr JWPLC_DisplayIdleReturnMode IDLE_RETURN_BUTTON_ONLY =
    JWPLC_DisplayClass::IDLE_RETURN_BUTTON_ONLY;

#endif // JWPLC_DISPLAY_API_H
