#include "JWPLC_Display.h"
#include "JWPLC_IdleScreen.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <cstring>

#include "jwplc_spi_bus.h"

extern "C"
{
#include "jwplc_peripherals.h"
}

enum DisplayMode : uint8_t
{
    DISPLAY_MODE_IDLE = 0,
    DISPLAY_MODE_USER = 1
};

static Adafruit_ST7789 tft(JWPLC_TFT_CS, JWPLC_TFT_DC, JWPLC_TFT_RST);

static bool g_tftReady = false;
static DisplayMode g_displayMode = DISPLAY_MODE_IDLE;

static JWPLCDisplay::IdleWakeMode g_idleWakeMode = JWPLCDisplay::IDLE_WAKE_ANY_BUTTON;
static uint8_t g_idleWakeButton = BTN_OK;

static JWPLCDisplay::IdleReturnMode g_idleReturnMode = JWPLCDisplay::IDLE_RETURN_ESC_ONLY;
static uint8_t g_idleReturnButton = BTN_ESC;

static uint32_t g_idleTimeoutMs = 15000;
static uint32_t g_idleRefreshPeriodMs = 10;
static uint32_t g_userRefreshPeriodMs = 40;

static uint32_t g_lastActivityMs = 0;
static bool g_waitButtonReleaseBeforeWake = false;

static bool g_runLed = true;
static bool g_errLed = false;
static bool g_busLed = false;
static bool g_ethLedAuto = true;
static JWPLCIdleScreen::StatusLedState g_ethLedState = JWPLCIdleScreen::STATUS_LED_OFF;
static uint32_t g_lastEthLedAutoUpdateMs = 0;
static constexpr uint32_t ETH_LED_AUTO_PERIOD_MS = 500;

extern "C" bool __attribute__((weak)) jwplcCanReturnToIdle(void) { return true; }
extern "C" void __attribute__((weak)) jwplcUserDisplayEnterCallback(void) {}
extern "C" void __attribute__((weak)) jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;
}
extern "C" void __attribute__((weak)) jwplcUserDisplayExitCallback(void) {}

static void deselectAllSPI()
{
    jwplcSPI_deselectAll();
}

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

static void resetDisplayState()
{
    g_lastActivityMs = millis();
}

extern "C" uint32_t jwplcDisplayDesiredPeriod_ms(void)
{
    return (g_displayMode == DISPLAY_MODE_IDLE) ? g_idleRefreshPeriodMs : g_userRefreshPeriodMs;
}

static bool shouldWakeFromIdle()
{
    switch (g_idleWakeMode)
    {
    case JWPLCDisplay::IDLE_WAKE_ANY_BUTTON:
        return JWPLCButtons::anyPressedOrRepeated();

    case JWPLCDisplay::IDLE_WAKE_BUTTON_ONLY:
        return JWPLCButtons::isReady() && JWPLC_Buttons.pressed(g_idleWakeButton);

    case JWPLCDisplay::IDLE_WAKE_DISABLED:
        return false;

    default:
        return JWPLCButtons::anyPressedOrRepeated();
    }
}

static bool shouldReturnToIdleByButton()
{
    switch (g_idleReturnMode)
    {
    case JWPLCDisplay::IDLE_RETURN_ESC_ONLY:
        return JWPLCButtons::escPressed();

    case JWPLCDisplay::IDLE_RETURN_BUTTON_ONLY:
        return JWPLCButtons::isReady() && JWPLC_Buttons.pressed(g_idleReturnButton);

    default:
        return false;
    }
}

static void handleIdleWakeAndTimeout()
{
    if (g_displayMode == DISPLAY_MODE_IDLE)
    {
        if (g_waitButtonReleaseBeforeWake)
        {
            if (JWPLCButtons::anyPressed())
            {
                JWPLCButtons::clearPendingInput();
                return;
            }

            JWPLCButtons::clearPendingInput();
            g_waitButtonReleaseBeforeWake = false;
            return;
        }

        if (shouldWakeFromIdle())
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

    if (shouldReturnToIdleByButton() && jwplcCanReturnToIdle())
    {
        JWPLCDisplay::goIdle();
        return;
    }

    if (JWPLCButtons::anyPressed())
    {
        JWPLCDisplay::notifyActivity();
    }

    if ((g_idleReturnMode == JWPLCDisplay::IDLE_RETURN_TIMEOUT) &&
        (g_idleTimeoutMs > 0) &&
        jwplcCanReturnToIdle())
    {
        const uint32_t now = millis();

        if ((uint32_t)(now - g_lastActivityMs) >= g_idleTimeoutMs)
        {
            JWPLCDisplay::goIdle();
            return;
        }
    }
}

static JWPLCIdleScreen::StatusLedState computeAutomaticEthLedState()
{
    if (!JWPLC_Ethernet.isEnabled())
    {
        return JWPLCIdleScreen::STATUS_LED_OFF;
    }

    if (!JWPLC_Ethernet.isBeginAttempted())
    {
        return JWPLCIdleScreen::STATUS_LED_OFF;
    }

    const char *status = JWPLC_Ethernet.statusString();

    if (strcmp(status, "OK") == 0)
    {
        return JWPLCIdleScreen::STATUS_LED_GREEN;
    }

    if (strcmp(status, "Link OFF") == 0 ||
        strcmp(status, "Not started") == 0 ||
        strcmp(status, "Ethernet disabled") == 0)
    {
        return JWPLCIdleScreen::STATUS_LED_OFF;
    }

    if (strcmp(status, "SPI lock timeout") == 0)
    {
        return g_ethLedState;
    }

    return JWPLCIdleScreen::STATUS_LED_RED;
}

static void updateAutomaticEthLed()
{
    if (!g_ethLedAuto)
    {
        return;
    }

    const uint32_t now = millis();

    if ((uint32_t)(now - g_lastEthLedAutoUpdateMs) < ETH_LED_AUTO_PERIOD_MS)
    {
        return;
    }

    g_lastEthLedAutoUpdateMs = now;

    JWPLCIdleScreen::StatusLedState newState = computeAutomaticEthLedState();

    if (newState != g_ethLedState)
    {
        g_ethLedState = newState;

        if (g_displayMode == DISPLAY_MODE_IDLE)
        {
            jwplcSystemForceDisplayRefresh();
        }
    }
}

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

    void forceRedraw()
    {
        JWPLCIdleScreen::forceFullRedraw();
        jwplcSystemForceDisplayRefresh();
    }

    void notifyActivity()
    {
        g_lastActivityMs = millis();
    }

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

    void goIdle()
    {
        if (g_displayMode == DISPLAY_MODE_USER)
        {
            jwplcUserDisplayExitCallback();
        }

        g_displayMode = DISPLAY_MODE_IDLE;
        resetDisplayState();
        g_waitButtonReleaseBeforeWake = true;

        JWPLCIdleScreen::forceFullRedraw();
        JWPLCButtons::clearPendingInput();
        jwplcSystemForceDisplayRefresh();
    }

    void setIdleWakeMode(IdleWakeMode mode)
    {
        g_idleWakeMode = mode;
    }

    IdleWakeMode idleWakeMode()
    {
        return g_idleWakeMode;
    }

    void setIdleWakeButton(uint8_t buttonId)
    {
        g_idleWakeButton = buttonId;
    }

    uint8_t idleWakeButton()
    {
        return g_idleWakeButton;
    }

    void setIdleReturnMode(IdleReturnMode mode)
    {
        g_idleReturnMode = mode;
    }

    IdleReturnMode idleReturnMode()
    {
        return g_idleReturnMode;
    }

    void setIdleReturnButton(uint8_t buttonId)
    {
        g_idleReturnButton = buttonId;
    }

    uint8_t idleReturnButton()
    {
        return g_idleReturnButton;
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
        g_ethLedAuto = false;
        g_ethLedState = state ? JWPLCIdleScreen::STATUS_LED_GREEN : JWPLCIdleScreen::STATUS_LED_OFF;

        if (g_displayMode == DISPLAY_MODE_IDLE)
        {
            jwplcSystemForceDisplayRefresh();
        }
    }

    bool ethLed()
    {
        return g_ethLedState == JWPLCIdleScreen::STATUS_LED_GREEN;
    }

    void setEthLedAuto(bool enabled)
    {
        g_ethLedAuto = enabled;
        g_lastEthLedAutoUpdateMs = 0;

        if (enabled)
        {
            g_ethLedState = computeAutomaticEthLedState();
        }

        if (g_displayMode == DISPLAY_MODE_IDLE)
        {
            jwplcSystemForceDisplayRefresh();
        }
    }

    bool ethLedAuto()
    {
        return g_ethLedAuto;
    }
}

extern "C" bool jwplcDisplayBeginCallback(void)
{
    if (!jwplcSPI_begin())
    {
        return false;
    }

    deselectAllSPI();
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

    // No reiniciamos configuración de transición/refresh aquí.
    // Los setters pueden llamarse desde setup() antes de que la TFT esté lista.
    g_runLed = true;
    g_errLed = false;
    g_busLed = false;
    g_ethLedAuto = true;
    g_ethLedState = JWPLCIdleScreen::STATUS_LED_OFF;
    g_lastEthLedAutoUpdateMs = 0;

    resetDisplayState();
    JWPLCIdleScreen::forceFullRedraw();

    Serial.println("JWPLC_Display inicializado");
    return true;
}

extern "C" void jwplcDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    if (!g_tftReady)
    {
        return;
    }

    handleIdleWakeAndTimeout();
    updateAutomaticEthLed();

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
        panel.eth = g_ethLedState;

        JWPLCIdleScreen::setStatusPanel(panel);
        JWPLCIdleScreen::draw(io, rtc);

        releaseTFTBus();
        return;
    }

    jwplcUserDisplayRefreshCallback(io, rtc);
    releaseTFTBus();
}

JWPLC_DisplayClass JWPLC_Display;

static JWPLCDisplay::IdleWakeMode toLegacyIdleWakeMode(JWPLC_DisplayClass::IdleWakeMode mode)
{
    return static_cast<JWPLCDisplay::IdleWakeMode>(mode);
}

static JWPLC_DisplayClass::IdleWakeMode fromLegacyIdleWakeMode(JWPLCDisplay::IdleWakeMode mode)
{
    return static_cast<JWPLC_DisplayClass::IdleWakeMode>(mode);
}

static JWPLCDisplay::IdleReturnMode toLegacyIdleReturnMode(JWPLC_DisplayClass::IdleReturnMode mode)
{
    return static_cast<JWPLCDisplay::IdleReturnMode>(mode);
}

static JWPLC_DisplayClass::IdleReturnMode fromLegacyIdleReturnMode(JWPLCDisplay::IdleReturnMode mode)
{
    return static_cast<JWPLC_DisplayClass::IdleReturnMode>(mode);
}

bool JWPLC_DisplayClass::isReady() const { return JWPLCDisplay::isReady(); }
bool JWPLC_DisplayClass::isIdleMode() const { return JWPLCDisplay::isIdleMode(); }
bool JWPLC_DisplayClass::buttonsReady() const { return JWPLCDisplay::buttonsReady(); }
void JWPLC_DisplayClass::forceRedraw() { JWPLCDisplay::forceRedraw(); }
void JWPLC_DisplayClass::enterUserUI() { JWPLCDisplay::enterUserUI(); }
void JWPLC_DisplayClass::goIdle() { JWPLCDisplay::goIdle(); }
void JWPLC_DisplayClass::notifyActivity() { JWPLCDisplay::notifyActivity(); }
void JWPLC_DisplayClass::setIdleWakeMode(IdleWakeMode mode) { JWPLCDisplay::setIdleWakeMode(toLegacyIdleWakeMode(mode)); }
JWPLC_DisplayClass::IdleWakeMode JWPLC_DisplayClass::idleWakeMode() const { return fromLegacyIdleWakeMode(JWPLCDisplay::idleWakeMode()); }
void JWPLC_DisplayClass::setIdleWakeButton(uint8_t buttonId) { JWPLCDisplay::setIdleWakeButton(buttonId); }
uint8_t JWPLC_DisplayClass::idleWakeButton() const { return JWPLCDisplay::idleWakeButton(); }
void JWPLC_DisplayClass::setIdleReturnMode(IdleReturnMode mode) { JWPLCDisplay::setIdleReturnMode(toLegacyIdleReturnMode(mode)); }
JWPLC_DisplayClass::IdleReturnMode JWPLC_DisplayClass::idleReturnMode() const { return fromLegacyIdleReturnMode(JWPLCDisplay::idleReturnMode()); }
void JWPLC_DisplayClass::setIdleReturnButton(uint8_t buttonId) { JWPLCDisplay::setIdleReturnButton(buttonId); }
uint8_t JWPLC_DisplayClass::idleReturnButton() const { return JWPLCDisplay::idleReturnButton(); }
void JWPLC_DisplayClass::setIdleTimeoutMs(uint32_t timeoutMs) { JWPLCDisplay::setIdleTimeoutMs(timeoutMs); }
uint32_t JWPLC_DisplayClass::idleTimeoutMs() const { return JWPLCDisplay::idleTimeoutMs(); }
void JWPLC_DisplayClass::setIdleRefreshPeriodMs(uint32_t ms) { JWPLCDisplay::setIdleRefreshPeriodMs(ms); }
uint32_t JWPLC_DisplayClass::idleRefreshPeriodMs() const { return JWPLCDisplay::idleRefreshPeriodMs(); }
void JWPLC_DisplayClass::setUserRefreshPeriodMs(uint32_t ms) { JWPLCDisplay::setUserRefreshPeriodMs(ms); }
uint32_t JWPLC_DisplayClass::userRefreshPeriodMs() const { return JWPLCDisplay::userRefreshPeriodMs(); }
void JWPLC_DisplayClass::clearPendingInput() { JWPLCDisplay::clearPendingInput(); }
Adafruit_ST7789 &JWPLC_DisplayClass::tft() { return JWPLCDisplay::display(); }
Adafruit_ST7789 &JWPLC_DisplayClass::display() { return JWPLCDisplay::display(); }
void JWPLC_DisplayClass::setRunLed(bool state) { JWPLCDisplay::setRunLed(state); }
bool JWPLC_DisplayClass::runLed() const { return JWPLCDisplay::runLed(); }
void JWPLC_DisplayClass::setErrLed(bool state) { JWPLCDisplay::setErrLed(state); }
bool JWPLC_DisplayClass::errLed() const { return JWPLCDisplay::errLed(); }
void JWPLC_DisplayClass::setBusLed(bool state) { JWPLCDisplay::setBusLed(state); }
bool JWPLC_DisplayClass::busLed() const { return JWPLCDisplay::busLed(); }
void JWPLC_DisplayClass::setEthLed(bool state) { JWPLCDisplay::setEthLed(state); }
bool JWPLC_DisplayClass::ethLed() const { return JWPLCDisplay::ethLed(); }
void JWPLC_DisplayClass::setEthLedAuto(bool enabled) { JWPLCDisplay::setEthLedAuto(enabled); }
bool JWPLC_DisplayClass::ethLedAuto() const { return JWPLCDisplay::ethLedAuto(); }
