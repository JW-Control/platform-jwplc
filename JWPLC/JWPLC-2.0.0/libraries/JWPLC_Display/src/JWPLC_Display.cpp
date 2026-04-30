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

// Toma el mutex global del bus SPI antes de usar la TFT.
// Adafruit_ST7789 maneja transacciones SPI, pero no conoce el mutex
// compartido del ecosistema JWPLC. Por eso protegemos la TFT aquí.
static bool acquireTFTBus(uint32_t timeoutMs = 50)
{
    if (!jwplcSPI_acquire(timeoutMs))
    {
        return false;
    }

    jwplcSPI_prepareForTFT();
    return true;
}

static void releaseTFTBus()
{
    jwplcSPI_release();
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
        // En IDLE sí usamos anyPressedOrRepeated() porque queremos
        // despertar la pantalla con una pulsación, evento pendiente
        // o repetición generada por la botonera.
        if (JWPLCButtons::anyPressedOrRepeated())
        {
            JWPLCDisplay::notifyActivity();
            JWPLCDisplay::enterUserUI();
            return;
        }

        return;
    }

    if (g_displayMode != DISPLAY_MODE_USER)
    {
        return;
    }

    // En USER solo una tecla físicamente presionada debe contar como actividad.
    // No usamos eventCount() aquí porque una cola con eventos pendientes puede
    // impedir que el timeout expire correctamente.
    if (JWPLCButtons::anyPressed())
    {
        JWPLCDisplay::notifyActivity();
    }

    if ((g_idleReturnMode == JWPLCDisplay::IDLE_RETURN_ESC_ONLY) &&
        JWPLCButtons::escPressed() &&
        jwplcCanReturnToIdle())
    {
        JWPLCDisplay::goIdle();
        return;
    }

    if ((g_idleReturnMode == JWPLCDisplay::IDLE_RETURN_TIMEOUT) &&
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

    // IDLE_RETURN_DISABLED:
    // No retorna automáticamente.
    // El sketch debe llamar manualmente a JWPLC_Display.goIdle().
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

        if (acquireTFTBus(100))
        {
            tft.fillScreen(ST77XX_BLACK);
            jwplcUserDisplayEnterCallback();
            releaseTFTBus();
        }

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

    if (!acquireTFTBus(100))
    {
        return false;
    }

    digitalWrite(JWPLC_TFT_CS, LOW);

    tft.init(170, 320);
    tft.setRotation(3);
    tft.setSPISpeed(JWPLC_SPI_TFT_HZ);

    digitalWrite(JWPLC_TFT_CS, HIGH);

    releaseTFTBus();

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

    if (!acquireTFTBus(20))
    {
        return;
    }

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

        releaseTFTBus();
        return;
    }

    // En modo usuario, la UI concreta la define el sketch mediante callback.
    // IMPORTANTE: dentro del callback USER no se deben consultar periféricos SPI.
    // Solo dibujar variables previamente cacheadas en loop().
    jwplcUserDisplayRefreshCallback(io, rtc);

    releaseTFTBus();
}

// =====================================================
// API pública estilo objeto: JWPLC_Display
// =====================================================
// Esta capa permite usar el display con sintaxis de punto:
//
//   JWPLC_Display.isReady();
//   JWPLC_Display.enterUserUI();
//   JWPLC_Display.tft();
//
// Se mantiene JWPLCDisplay:: como compatibilidad interna/legacy.
// =====================================================

JWPLC_DisplayClass JWPLC_Display;

static JWPLCDisplay::IdleReturnMode toLegacyIdleReturnMode(JWPLC_DisplayClass::IdleReturnMode mode)
{
    return static_cast<JWPLCDisplay::IdleReturnMode>(mode);
}

static JWPLC_DisplayClass::IdleReturnMode fromLegacyIdleReturnMode(JWPLCDisplay::IdleReturnMode mode)
{
    return static_cast<JWPLC_DisplayClass::IdleReturnMode>(mode);
}

bool JWPLC_DisplayClass::isReady() const
{
    return JWPLCDisplay::isReady();
}

bool JWPLC_DisplayClass::isIdleMode() const
{
    return JWPLCDisplay::isIdleMode();
}

bool JWPLC_DisplayClass::buttonsReady() const
{
    return JWPLCDisplay::buttonsReady();
}

void JWPLC_DisplayClass::forceRedraw()
{
    JWPLCDisplay::forceRedraw();
}

void JWPLC_DisplayClass::enterUserUI()
{
    JWPLCDisplay::enterUserUI();
}

void JWPLC_DisplayClass::goIdle()
{
    JWPLCDisplay::goIdle();
}

void JWPLC_DisplayClass::notifyActivity()
{
    JWPLCDisplay::notifyActivity();
}

void JWPLC_DisplayClass::setIdleReturnMode(IdleReturnMode mode)
{
    JWPLCDisplay::setIdleReturnMode(toLegacyIdleReturnMode(mode));
}

JWPLC_DisplayClass::IdleReturnMode JWPLC_DisplayClass::idleReturnMode() const
{
    return fromLegacyIdleReturnMode(JWPLCDisplay::idleReturnMode());
}

void JWPLC_DisplayClass::setIdleTimeoutMs(uint32_t timeoutMs)
{
    JWPLCDisplay::setIdleTimeoutMs(timeoutMs);
}

uint32_t JWPLC_DisplayClass::idleTimeoutMs() const
{
    return JWPLCDisplay::idleTimeoutMs();
}

void JWPLC_DisplayClass::setIdleRefreshPeriodMs(uint32_t ms)
{
    JWPLCDisplay::setIdleRefreshPeriodMs(ms);
}

uint32_t JWPLC_DisplayClass::idleRefreshPeriodMs() const
{
    return JWPLCDisplay::idleRefreshPeriodMs();
}

void JWPLC_DisplayClass::setUserRefreshPeriodMs(uint32_t ms)
{
    JWPLCDisplay::setUserRefreshPeriodMs(ms);
}

uint32_t JWPLC_DisplayClass::userRefreshPeriodMs() const
{
    return JWPLCDisplay::userRefreshPeriodMs();
}

void JWPLC_DisplayClass::clearPendingInput()
{
    JWPLCDisplay::clearPendingInput();
}

Adafruit_ST7789 &JWPLC_DisplayClass::tft()
{
    return JWPLCDisplay::display();
}

Adafruit_ST7789 &JWPLC_DisplayClass::display()
{
    return JWPLCDisplay::display();
}

void JWPLC_DisplayClass::setRunLed(bool state)
{
    JWPLCDisplay::setRunLed(state);
}

bool JWPLC_DisplayClass::runLed() const
{
    return JWPLCDisplay::runLed();
}

void JWPLC_DisplayClass::setErrLed(bool state)
{
    JWPLCDisplay::setErrLed(state);
}

bool JWPLC_DisplayClass::errLed() const
{
    return JWPLCDisplay::errLed();
}

void JWPLC_DisplayClass::setBusLed(bool state)
{
    JWPLCDisplay::setBusLed(state);
}

bool JWPLC_DisplayClass::busLed() const
{
    return JWPLCDisplay::busLed();
}

void JWPLC_DisplayClass::setEthLed(bool state)
{
    JWPLCDisplay::setEthLed(state);
}

bool JWPLC_DisplayClass::ethLed() const
{
    return JWPLCDisplay::ethLed();
}