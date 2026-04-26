#include "JWPLC_Display.h"
#include "JWPLC_IdleScreen.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#include "jwplc_spi_bus.h"

extern "C"
{
#include "jwplc_peripherals.h"
}

// =====================================================
// SPI compartido
// =====================================================
// Los pines y frecuencias SPI se centralizan en jwplc_spi_bus.
static Adafruit_ST7789 tft(JWPLC_TFT_CS, JWPLC_TFT_DC, JWPLC_TFT_RST);

// =====================================================
// Estado general del módulo display
// =====================================================
enum DisplayMode : uint8_t
{
    DISPLAY_MODE_IDLE = 0,
    DISPLAY_MODE_USER = 1
};

static bool g_tftReady = false;
static DisplayMode g_displayMode = DISPLAY_MODE_IDLE;

static JWPLCDisplay::IdleReturnMode g_idleReturnMode = JWPLCDisplay::IDLE_RETURN_ESC_ONLY;
static uint32_t g_idleTimeoutMs = 15000;
static uint32_t g_idleRefreshPeriodMs = 50;

// Alpha.17:
// Se sube el periodo por defecto del modo usuario a 40 ms para aliviar
// la presión de refresco mientras la UI se navega con botones.
static uint32_t g_userRefreshPeriodMs = 40;

static uint32_t g_lastActivityMs = 0;

// Indicadores laterales del panel IDLE
static bool g_runLed = true;
static bool g_errLed = false;
static bool g_busLed = false;
static bool g_ethLed = false;

// =====================================================
// Hooks débiles de UI de usuario
// =====================================================
// El sketch puede redefinir estos hooks para crear sus propias pantallas.
extern "C" bool __attribute__((weak)) jwplcCanReturnToIdle(void) { return true; }

extern "C" void __attribute__((weak)) jwplcUserDisplayEnterCallback(void) {}

extern "C" void __attribute__((weak)) jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;
}

extern "C" void __attribute__((weak)) jwplcUserDisplayExitCallback(void) {}

// =====================================================
// Helpers SPI
// =====================================================
// Pone todos los esclavos SPI en estado inactivo para evitar conflictos.
static void deselectAllSPI()
{
    jwplcSPI_deselectAll();
}

// Toma el mutex del bus SPI y selecciona el dispositivo solicitado de forma segura.
// En alpha20 no se toma mutex para la TFT, porque Adafruit_ST7789 ya maneja internamente sus transacciones.
static void prepareForTFT()
{
    jwplcSPI_prepareForTFT();
}

// =====================================================
// Helpers de refresh / invalidación
// =====================================================
// Reinicia el estado visual básico del módulo al entrar/salir de pantallas.
static void resetDisplayState()
{
    g_lastActivityMs = millis();
}

// =====================================================
// Periodo dinámico del display
// =====================================================
// El runtime consulta este hook para decidir con qué frecuencia evaluar
// el display. El modo usuario ahora usa un periodo más relajado por defecto.
extern "C" uint32_t jwplcDisplayDesiredPeriod_ms(void)
{
    return (g_displayMode == DISPLAY_MODE_IDLE) ? g_idleRefreshPeriodMs
                                                : g_userRefreshPeriodMs;
}

// =====================================================
// Gestión de transiciones REPOSO / USER
// =====================================================
// Maneja:
// - despertar desde IDLE al detectar actividad
// - regreso automático a IDLE por timeout, si está habilitado
static void handleIdleWakeAndTimeout()
{
    if (g_displayMode == DISPLAY_MODE_IDLE)
    {
        if (JWPLCButtons::anyPressedOrRepeated())
        {
            JWPLCDisplay::notifyActivity();
            JWPLCDisplay::enterUserUI();
            return;
        }
    }

    if ((g_displayMode == DISPLAY_MODE_USER) &&
        (g_idleReturnMode == JWPLCDisplay::IDLE_RETURN_TIMEOUT) &&
        (g_idleTimeoutMs > 0) &&
        jwplcCanReturnToIdle())
    {
        uint32_t now = millis();
        if ((uint32_t)(now - g_lastActivityMs) >= g_idleTimeoutMs)
        {
            JWPLCDisplay::goIdle();
            return;
        }
    }
}

// =====================================================
// API pública JWPLCDisplay
// =====================================================
namespace JWPLCDisplay
{
    bool isReady()
    {
        return g_tftReady;
    }

    bool isIdleMode()
    {
        return (g_displayMode == DISPLAY_MODE_IDLE);
    }

    bool buttonsReady()
    {
        return JWPLCButtons::isReady();
    }

    // Fuerza redibujado completo de la pantalla actual.
    void forceRedraw()
    {
        JWPLCIdleScreen::forceFullRedraw();
        jwplcSystemForceDisplayRefresh();
    }

    // Registra actividad del usuario para lógica de timeout.
    void notifyActivity()
    {
        g_lastActivityMs = millis();
    }

    // Entra al modo USER. Aquí el sketch puede dibujar su propia UI.
    void enterUserUI()
    {
        if (g_displayMode == DISPLAY_MODE_USER)
        {
            g_lastActivityMs = millis();
            return;
        }

        g_displayMode = DISPLAY_MODE_USER;
        resetDisplayState();

        JWPLCButtons::clearPendingInput();

        prepareForTFT();
        tft.fillScreen(ST77XX_BLACK);

        jwplcUserDisplayEnterCallback();
        jwplcSystemForceDisplayRefresh();
    }

    // Retorna al modo IDLE mostrando nuevamente la pantalla base del sistema.
    void goIdle()
    {
        if (g_displayMode == DISPLAY_MODE_USER)
        {
            jwplcUserDisplayExitCallback();
        }

        g_displayMode = DISPLAY_MODE_IDLE;
        resetDisplayState();

        JWPLCIdleScreen::forceFullRedraw();

        JWPLCButtons::clearPendingInput();

        jwplcSystemForceDisplayRefresh();
    }

    void setIdleReturnMode(IdleReturnMode mode)
    {
        g_idleReturnMode = mode;
    }

    IdleReturnMode idleReturnMode()
    {
        return g_idleReturnMode;
    }

    void setIdleTimeoutMs(uint32_t timeoutMs)
    {
        g_idleTimeoutMs = timeoutMs;
    }

    uint32_t idleTimeoutMs()
    {
        return g_idleTimeoutMs;
    }

    void setIdleRefreshPeriodMs(uint32_t ms)
    {
        g_idleRefreshPeriodMs = (ms == 0) ? 1 : ms;
    }

    uint32_t idleRefreshPeriodMs()
    {
        return g_idleRefreshPeriodMs;
    }

    void setUserRefreshPeriodMs(uint32_t ms)
    {
        g_userRefreshPeriodMs = (ms == 0) ? 1 : ms;
    }

    uint32_t userRefreshPeriodMs()
    {
        return g_userRefreshPeriodMs;
    }

    void clearPendingInput()
    {
        JWPLCButtons::clearPendingInput();
    }

    Adafruit_ST7789 &display()
    {
        return tft;
    }

    void setRunLed(bool state)
    {
        g_runLed = state;
        if (g_displayMode == DISPLAY_MODE_IDLE)
            jwplcSystemForceDisplayRefresh();
    }

    bool runLed()
    {
        return g_runLed;
    }

    void setErrLed(bool state)
    {
        g_errLed = state;
        if (g_displayMode == DISPLAY_MODE_IDLE)
            jwplcSystemForceDisplayRefresh();
    }

    bool errLed()
    {
        return g_errLed;
    }

    void setBusLed(bool state)
    {
        g_busLed = state;
        if (g_displayMode == DISPLAY_MODE_IDLE)
            jwplcSystemForceDisplayRefresh();
    }

    bool busLed()
    {
        return g_busLed;
    }

    void setEthLed(bool state)
    {
        g_ethLed = state;
        if (g_displayMode == DISPLAY_MODE_IDLE)
            jwplcSystemForceDisplayRefresh();
    }

    bool ethLed()
    {
        return g_ethLed;
    }
}

// =====================================================
// Hooks display del sistema
// =====================================================
// Inicialización real de la TFT y de la botonera. El runtime llama a este
// hook automáticamente cuando corresponde.
extern "C" bool jwplcDisplayBeginCallback(void)
{
    if (!jwplcSPI_begin())
    {
        return false;
    }

    deselectAllSPI();

    // SPI.begin() se mantiene aquí porque JWPLC_Display.cpp
    // sí tiene acceso a la librería Arduino SPI.
    SPI.begin(JWPLC_SPI_SCK, JWPLC_SPI_MISO, JWPLC_SPI_MOSI);

    prepareForTFT();
    digitalWrite(JWPLC_TFT_CS, LOW);

    tft.init(170, 320);
    tft.setRotation(3);
    tft.setSPISpeed(JWPLC_SPI_TFT_HZ);

    digitalWrite(JWPLC_TFT_CS, HIGH);

    JWPLCIdleScreen::begin(&tft);
    JWPLCIdleScreen::setTitle("JWPLC Basic");

    g_tftReady = true;
    g_displayMode = DISPLAY_MODE_IDLE;

    // Se reinician valores base del módulo al completar la inicialización.
    g_idleReturnMode = JWPLCDisplay::IDLE_RETURN_ESC_ONLY;
    g_idleTimeoutMs = 15000;
    g_idleRefreshPeriodMs = 10;
    g_userRefreshPeriodMs = 40;

    g_runLed = true;
    g_errLed = false;
    g_busLed = false;
    g_ethLed = false;

    resetDisplayState();
    JWPLCIdleScreen::forceFullRedraw();

    Serial.println("JWPLC_Display inicializado");
    return true;
}

// Refresco principal del display. El runtime le entrega snapshots del sistema
// ya procesados. Aquí solo se decide cómo representarlos visualmente.
extern "C" void jwplcDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    if (!g_tftReady)
    {
        return;
    }

    handleIdleWakeAndTimeout();

    prepareForTFT();

    if (g_displayMode == DISPLAY_MODE_IDLE)
    {
        JWPLCIdleScreen::StatusPanel panel;
        panel.pwr = true;
        panel.run = g_runLed;
        panel.err = g_errLed;
        panel.bus = g_busLed;
        panel.eth = g_ethLed;

        JWPLCIdleScreen::setStatusPanel(panel);
        JWPLCIdleScreen::draw(io, rtc);
        return;
    }

    // En modo usuario, la UI concreta la define el sketch mediante callback.
    // El objetivo es que el sketch actualice solo lo que cambie, evitando
    // redibujados globales innecesarios.
    jwplcUserDisplayRefreshCallback(io, rtc);
}
