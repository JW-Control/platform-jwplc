#ifndef JWPLC_DISPLAY_API_H
#define JWPLC_DISPLAY_API_H

#include <Arduino.h>
#include <JWPLC_UI.h>

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

    void setUserRefreshMode(JWPLC_UIRefreshMode mode);
    JWPLC_UIRefreshMode userRefreshMode() const;
    void requestUserRefresh();

    void setUserPage(uint8_t page);
    uint8_t userPage() const;

    // Navegación compacta Alpha11. Un conteo > 1 habilita el selector físico
    // NN/TT en la esquina superior derecha. El ID interno de página sigue
    // siendo 0-based; el indicador visible es 1-based.
    void setUserPageCount(uint8_t count);
    uint8_t userPageCount() const;
    bool isUserPageSelection() const;

    bool setFields(const JWPLC_UIField *fields, size_t count);
    void clearFields();
    size_t fieldCount() const;

    template <typename T>
    bool setValue(uint8_t fieldId, T value)
    {
        return setNumericValue(fieldId, static_cast<double>(value));
    }

    bool setValue(uint8_t fieldId, bool value)
    {
        return setBool(fieldId, value);
    }

    bool setValue(uint8_t fieldId, const char *value)
    {
        return setText(fieldId, value);
    }

    bool setText(uint8_t fieldId, const char *value);
    bool setBool(uint8_t fieldId, bool value);
    bool setBar(uint8_t fieldId, float value);

    void invalidateField(uint8_t fieldId);
    void invalidateAllFields();

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

    // Código ERR definido por la aplicación.
    //
    // Formato:
    //   - 1..4 caracteres A-Z / 0-9.
    //   - minúsculas se normalizan a mayúsculas.
    //   - vacío, nullptr o sólo ceros significan "sin error".
    //   - una entrada inválida conserva el estado anterior.
    //
    // setErrLed(bool) se conserva por compatibilidad; para código nuevo
    // se recomienda setErrCode().
    bool setErrCode(const char *code);
    const char *errCode() const;
    void setBusLed(bool state);
    bool busLed() const;

    void setBusLedAuto(bool enabled);
    bool busLedAuto() const;

    void setEthLed(bool state);
    bool ethLed() const;
    void setEthLedAuto(bool enabled);
    bool ethLedAuto() const;

private:
    bool setNumericValue(uint8_t fieldId, double value);
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
