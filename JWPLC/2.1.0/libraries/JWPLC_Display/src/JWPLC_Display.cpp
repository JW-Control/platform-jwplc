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

static JWPLCDisplay::IdleWakeMode g_idleWakeMode = JWPLCDisplay::IDLE_WAKE_DISABLED;
static uint8_t g_idleWakeButton = BTN_OK;

static JWPLCDisplay::IdleReturnMode g_idleReturnMode = JWPLCDisplay::IDLE_RETURN_ESC_ONLY;
static uint8_t g_idleReturnButton = BTN_ESC;

static uint32_t g_idleTimeoutMs = 15000;
static uint32_t g_idleRefreshPeriodMs = 10;
static uint32_t g_userRefreshPeriodMs = 40;

static uint32_t g_lastActivityMs = 0;
static bool g_waitButtonReleaseBeforeWake = false;
static bool g_userRefreshForced = false;
// La navegacion de la TFT observa estados fisicos y detecta sus propios
// flancos. No consume pressed()/released() del sketch.
static uint8_t g_navigationPreviousMask = 0;

static bool g_runLed = true;

// ERR pertenece exclusivamente a la aplicación.
// Código vacío = sin código visible.
static bool g_errLed = false;
static char g_errCode[5] = {'\0', '\0', '\0', '\0', '\0'};

static JWPLCIdleScreen::StatusLedState g_busLedState = JWPLCIdleScreen::STATUS_LED_DISABLED;
static char g_busDiagnosticCode[4] = "---";
static bool g_busLedAuto = true;
static uint32_t g_lastBusLedAutoUpdateMs = 0;
static constexpr uint32_t BUS_LED_ACTIVE_HOLD_MS = 800;
static constexpr uint32_t BUS_LED_AUTO_PERIOD_MS = 100;

static bool g_ethLedAuto = true;
static JWPLCIdleScreen::StatusLedState g_ethLedState = JWPLCIdleScreen::STATUS_LED_OFF;
static char g_ethDiagnosticCode[4] = "---";
static uint32_t g_lastEthLedAutoUpdateMs = 0;
static constexpr uint32_t ETH_LED_AUTO_PERIOD_MS = 500;

extern "C" bool __attribute__((weak)) jwplcCanReturnToIdle(void) { return true; }

// Callbacks cortos recomendados desde Alpha8.
extern "C" void __attribute__((weak)) jwplcUIEnter(void) {}
extern "C" void __attribute__((weak)) jwplcUIPageEnter(uint8_t page) { (void)page; }
extern "C" void __attribute__((weak)) jwplcUIUpdate(void) {}
extern "C" void __attribute__((weak)) jwplcUIExit(void) {}

// Callbacks legacy conservados sin romper sketches existentes.
extern "C" void __attribute__((weak)) jwplcUserDisplayEnterCallback(void) {}
extern "C" bool __attribute__((weak)) jwplcUserDisplayRefreshNeededCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;
    return JWPLCUI::refreshNeeded();
}
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

static bool setDiagnosticCode(char destination[4], const char *source)
{
    char next[4] = {'-', '-', '-', '\0'};

    if (source != nullptr)
    {
        for (uint8_t i = 0; i < 3 && source[i] != '\0'; i++)
        {
            next[i] = source[i];
        }
    }

    if (strncmp(destination, next, 4) == 0)
    {
        return false;
    }

    memcpy(destination, next, 4);
    return true;
}

static bool normalizeApplicationErrCode(
    const char *source,
    char destination[5],
    bool &hasError)
{
    destination[0] = '\0';
    hasError = false;

    // nullptr y cadena vacía significan "sin error".
    if (source == nullptr || source[0] == '\0')
    {
        return true;
    }

    const size_t length = strlen(source);

    if (length > 4)
    {
        return false;
    }

    bool onlyZeros = true;

    for (size_t i = 0; i < length; i++)
    {
        char c = source[i];

        if (c >= 'a' && c <= 'z')
        {
            c = (char)(c - 'a' + 'A');
        }

        const bool valid =
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9');

        if (!valid)
        {
            return false;
        }

        if (c != '0')
        {
            onlyZeros = false;
        }

        destination[i] = c;
    }

    destination[length] = '\0';

    // "0", "00", "000" y "0000" equivalen a OK / sin error.
    if (onlyZeros)
    {
        destination[0] = '\0';
        hasError = false;
        return true;
    }

    hasError = true;
    return true;
}

extern "C" uint32_t jwplcDisplayDesiredPeriod_ms(void)
{
    uint32_t periodMs =
        (g_displayMode == DISPLAY_MODE_IDLE) ? g_idleRefreshPeriodMs : g_userRefreshPeriodMs;

    if ((g_displayMode == DISPLAY_MODE_IDLE) &&
        g_busLedAuto &&
        (g_busLedState == JWPLCIdleScreen::STATUS_LED_GREEN) &&
        periodMs > BUS_LED_AUTO_PERIOD_MS)
    {
        return BUS_LED_AUTO_PERIOD_MS;
    }

    return periodMs;
}

static uint8_t readButtonDownMask()
{
    if (!JWPLCButtons::isReady())
    {
        return 0;
    }

    uint8_t mask = 0;

    if (JWPLC_Buttons.isDown(BTN_LEFT))
        mask |= (uint8_t)(1u << BTN_LEFT);
    if (JWPLC_Buttons.isDown(BTN_UP))
        mask |= (uint8_t)(1u << BTN_UP);
    if (JWPLC_Buttons.isDown(BTN_RIGHT))
        mask |= (uint8_t)(1u << BTN_RIGHT);
    if (JWPLC_Buttons.isDown(BTN_ESC))
        mask |= (uint8_t)(1u << BTN_ESC);
    if (JWPLC_Buttons.isDown(BTN_OK))
        mask |= (uint8_t)(1u << BTN_OK);
    if (JWPLC_Buttons.isDown(BTN_DOWN))
        mask |= (uint8_t)(1u << BTN_DOWN);

    return mask;
}

static uint8_t buttonMaskForId(uint8_t buttonId)
{
    return (buttonId < 8)
               ? (uint8_t)(1u << buttonId)
               : 0;
}

static bool shouldWakeFromIdle(uint8_t pressedEdges)
{
    switch (g_idleWakeMode)
    {
    case JWPLCDisplay::IDLE_WAKE_ANY_BUTTON:
        return pressedEdges != 0;

    case JWPLCDisplay::IDLE_WAKE_BUTTON_ONLY:
        return (pressedEdges & buttonMaskForId(g_idleWakeButton)) != 0;

    case JWPLCDisplay::IDLE_WAKE_DISABLED:
    default:
        return false;
    }
}

static bool shouldReturnToIdleByButton(uint8_t pressedEdges)
{
    switch (g_idleReturnMode)
    {
    case JWPLCDisplay::IDLE_RETURN_ESC_ONLY:
        return (pressedEdges & buttonMaskForId(BTN_ESC)) != 0;

    case JWPLCDisplay::IDLE_RETURN_BUTTON_ONLY:
        return (pressedEdges & buttonMaskForId(g_idleReturnButton)) != 0;

    default:
        return false;
    }
}

static void handleIdleWakeAndTimeout()
{
    const uint8_t downMask = readButtonDownMask();
    const uint8_t pressedEdges =
        (uint8_t)(downMask & (uint8_t)~g_navigationPreviousMask);

    g_navigationPreviousMask = downMask;

    if (g_displayMode == DISPLAY_MODE_IDLE)
    {
        if (g_waitButtonReleaseBeforeWake)
        {
            if (downMask != 0)
            {
                return;
            }

            g_waitButtonReleaseBeforeWake = false;
            g_navigationPreviousMask = 0;
            return;
        }

        if (shouldWakeFromIdle(pressedEdges))
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

    if (shouldReturnToIdleByButton(pressedEdges) && jwplcCanReturnToIdle())
    {
        JWPLCDisplay::goIdle();
        return;
    }

    if (downMask != 0)
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

static const char *computeAutomaticEthDiagnosticCode()
{
    return JWPLC_Ethernet.diagnosticCode();
}

static JWPLCIdleScreen::StatusLedState computeAutomaticEthLedState()
{
    const char *code = computeAutomaticEthDiagnosticCode();

    if (strcmp(code, "DIS") == 0)
    {
        return JWPLCIdleScreen::STATUS_LED_DISABLED;
    }

    if (strcmp(code, "---") == 0)
    {
        return JWPLCIdleScreen::STATUS_LED_GREEN;
    }

    if (strcmp(code, "INI") == 0 ||
        strcmp(code, "PHY") == 0 ||
        strcmp(code, "LNK") == 0)
    {
        return JWPLCIdleScreen::STATUS_LED_OFF;
    }

    if (strcmp(code, "DHC") == 0)
    {
        if (JWPLC_Ethernet.runtimeState() == JWPLC_ETH_STATE_ERROR ||
            JWPLC_Ethernet.lastError() == JWPLC_ETH_DHCP_FAILED)
        {
            return JWPLCIdleScreen::STATUS_LED_RED;
        }

        // Durante renew/rebind el lease vigente sigue operativo.
        if (JWPLC_Ethernet.isReady())
        {
            return JWPLCIdleScreen::STATUS_LED_GREEN;
        }

        return JWPLCIdleScreen::STATUS_LED_OFF;
    }

    // HW, IP, SPI y cualquier código inesperado representan una causa que
    // requiere atención en el indicador ETH, sin tocar ERR de aplicación.
    return JWPLCIdleScreen::STATUS_LED_RED;
}

static const char *computeAutomaticBusDiagnosticCode()
{
    if (!JWPLC_RS485.isEnabled())
    {
        return "DIS";
    }

    if (!JWPLC_RS485.isReady())
    {
        switch (JWPLC_RS485.lastError())
        {
        case JWPLC_RS485_DISABLED:
            return "DIS";
        case JWPLC_RS485_INVALID_SERIAL:
        case JWPLC_RS485_UNKNOWN_ERROR:
            return "SER";
        case JWPLC_RS485_OK:
        case JWPLC_RS485_NOT_STARTED:
        default:
            return "INI";
        }
    }

    // Modbus es opcional. NOT_STARTED/DISABLED significan que RS-485 crudo
    // puede estar perfectamente operativo, por lo que no se marcan como error.
    switch (JWPLC_ModbusRTU.lastError())
    {
    case JWPLC_MODBUS_OK:
    case JWPLC_MODBUS_DISABLED:
    case JWPLC_MODBUS_NOT_STARTED:
        return "---";
    case JWPLC_MODBUS_INVALID_SLAVE_ID:
        return "SID";
    case JWPLC_MODBUS_INVALID_REGISTER_MAP:
        return "MAP";
    case JWPLC_MODBUS_TIMEOUT:
        return "TMO";
    case JWPLC_MODBUS_CRC_ERROR:
        return "CRC";
    case JWPLC_MODBUS_EXCEPTION:
        return "EXC";
    case JWPLC_MODBUS_INVALID_RESPONSE:
        return "RSP";
    case JWPLC_MODBUS_BUFFER_OVERFLOW:
        return "OVF";
    case JWPLC_MODBUS_UNSUPPORTED_FUNCTION:
        return "FUN";
    default:
        return "SER";
    }
}

static JWPLCIdleScreen::StatusLedState computeAutomaticBusLedState()
{
    const char *code = computeAutomaticBusDiagnosticCode();

    if (strcmp(code, "DIS") == 0)
    {
        return JWPLCIdleScreen::STATUS_LED_DISABLED;
    }

    if (strcmp(code, "INI") == 0)
    {
        return JWPLCIdleScreen::STATUS_LED_OFF;
    }

    if (strcmp(code, "---") != 0)
    {
        return JWPLCIdleScreen::STATUS_LED_RED;
    }

    if (JWPLC_RS485.hasRecentActivity(BUS_LED_ACTIVE_HOLD_MS))
    {
        return JWPLCIdleScreen::STATUS_LED_GREEN;
    }

    return JWPLCIdleScreen::STATUS_LED_OFF;
}

static void updateAutomaticBusLed()
{
    if (!g_busLedAuto)
    {
        return;
    }

    const uint32_t now = millis();

    if ((uint32_t)(now - g_lastBusLedAutoUpdateMs) < BUS_LED_AUTO_PERIOD_MS)
    {
        return;
    }

    g_lastBusLedAutoUpdateMs = now;

    JWPLCIdleScreen::StatusLedState newState = computeAutomaticBusLedState();
    const bool codeChanged = setDiagnosticCode(g_busDiagnosticCode, computeAutomaticBusDiagnosticCode());

    if (newState != g_busLedState || codeChanged)
    {
        g_busLedState = newState;

        if (g_displayMode == DISPLAY_MODE_IDLE)
        {
            jwplcSystemForceDisplayRefresh();
        }
    }
}

extern "C" void jwplcRs485ActivityCallback(void)
{
    if (!g_busLedAuto)
    {
        return;
    }

    JWPLCIdleScreen::StatusLedState newState = computeAutomaticBusLedState();
    const bool codeChanged = setDiagnosticCode(g_busDiagnosticCode, computeAutomaticBusDiagnosticCode());

    if (newState != g_busLedState || codeChanged)
    {
        g_busLedState = newState;
    }

    if (g_displayMode == DISPLAY_MODE_IDLE)
    {
        jwplcSystemForceDisplayRefresh();
    }
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
    const bool codeChanged = setDiagnosticCode(g_ethDiagnosticCode, computeAutomaticEthDiagnosticCode());

    if (newState != g_ethLedState || codeChanged)
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
        if (g_displayMode == DISPLAY_MODE_IDLE)
        {
            JWPLCIdleScreen::forceFullRedraw();
        }
        else
        {
            g_userRefreshForced = true;
            JWPLCUI::invalidateAll(true);
        }
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
        g_userRefreshForced = true;
        resetDisplayState();
        // Absorbe el estado fisico actual solo para la navegacion interna.
        // Los latches pressed()/released() del usuario permanecen intactos.
        g_navigationPreviousMask = readButtonDownMask();

        JWPLCUI::prepareEnter();

        if (acquireTFTBus(100))
        {
            tft.fillScreen(ST77XX_BLACK);

            jwplcUserDisplayEnterCallback();
            jwplcUIEnter();
            jwplcUIPageEnter(JWPLCUI::currentPage());

            JWPLCUI::drawStatic(tft);
            JWPLCUI::drawDirty(tft);
            JWPLCUI::consumePageRedrawPending();
            JWPLCUI::consumeRefreshRequest();

            g_userRefreshForced = false;
            releaseTFTBus();
        }

        jwplcSystemForceDisplayRefresh();
    }

    void goIdle()
    {
        if (g_displayMode == DISPLAY_MODE_USER)
        {
            jwplcUserDisplayExitCallback();
            jwplcUIExit();
        }

        g_displayMode = DISPLAY_MODE_IDLE;
        g_userRefreshForced = false;
        resetDisplayState();
        g_waitButtonReleaseBeforeWake = true;
        g_navigationPreviousMask = readButtonDownMask();

        JWPLCIdleScreen::forceFullRedraw();
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
        // API histórica conservada por compatibilidad.
        // El modo manual no muestra código alfanumérico.
        g_errLed = state;
        memset(g_errCode, 0, sizeof(g_errCode));

        if (g_displayMode == DISPLAY_MODE_IDLE)
            jwplcSystemForceDisplayRefresh();
    }

    bool errLed()
    {
        return g_errLed;
    }

    bool setErrCode(const char *code)
    {
        char normalized[5] = {'\0', '\0', '\0', '\0', '\0'};
        bool hasError = false;

        if (!normalizeApplicationErrCode(code, normalized, hasError))
        {
            // Código inválido: conservar diagnóstico anterior.
            return false;
        }

        g_errLed = hasError;

        if (hasError)
            memcpy(g_errCode, normalized, sizeof(g_errCode));
        else
            memset(g_errCode, 0, sizeof(g_errCode));

        if (g_displayMode == DISPLAY_MODE_IDLE)
            jwplcSystemForceDisplayRefresh();

        return true;
    }

    const char *errCode()
    {
        return g_errCode;
    }
    void setBusLed(bool state)
    {
        g_busLedAuto = false;
        g_busLedState = state ? JWPLCIdleScreen::STATUS_LED_GREEN
                              : JWPLCIdleScreen::STATUS_LED_OFF;
        setDiagnosticCode(g_busDiagnosticCode, "---");

        if (g_displayMode == DISPLAY_MODE_IDLE)
        {
            jwplcSystemForceDisplayRefresh();
        }
    }

    bool busLed()
    {
        return g_busLedState == JWPLCIdleScreen::STATUS_LED_GREEN;
    }

    void setBusLedAuto(bool enabled)
    {
        g_busLedAuto = enabled;
        g_lastBusLedAutoUpdateMs = 0;

        if (enabled)
        {
            g_busLedState = computeAutomaticBusLedState();
            setDiagnosticCode(g_busDiagnosticCode, computeAutomaticBusDiagnosticCode());
        }
        else
        {
            setDiagnosticCode(g_busDiagnosticCode, "---");
        }

        if (g_displayMode == DISPLAY_MODE_IDLE)
        {
            jwplcSystemForceDisplayRefresh();
        }
    }

    bool busLedAuto()
    {
        return g_busLedAuto;
    }

    void setEthLed(bool state)
    {
        g_ethLedAuto = false;
        g_ethLedState = state ? JWPLCIdleScreen::STATUS_LED_GREEN : JWPLCIdleScreen::STATUS_LED_OFF;
        setDiagnosticCode(g_ethDiagnosticCode, "---");

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
            setDiagnosticCode(g_ethDiagnosticCode, computeAutomaticEthDiagnosticCode());
        }
        else
        {
            setDiagnosticCode(g_ethDiagnosticCode, "---");
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
    // Alpha5 PILOTO: la inicializacion fisica puede ejecutarse durante
    // initPeripherals(), antes de setup(). La llamada posterior del
    // runtime debe reconocer la TFT ya inicializada y no repetir tft.init().
    if (g_tftReady)
    {
        return true;
    }

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
    g_userRefreshForced = false;

    // No reiniciamos configuración de transición, refresh ni LEDs aquí.
    // Los setters pueden llamarse desde setup() antes de que la TFT esté lista.
    if (g_busLedAuto)
    {
        g_busLedState = computeAutomaticBusLedState();
        setDiagnosticCode(g_busDiagnosticCode, computeAutomaticBusDiagnosticCode());
    }

    if (g_ethLedAuto)
    {
        g_ethLedState = computeAutomaticEthLedState();
        setDiagnosticCode(g_ethDiagnosticCode, computeAutomaticEthDiagnosticCode());
    }

    g_lastBusLedAutoUpdateMs = 0;
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
    updateAutomaticBusLed();
    updateAutomaticEthLed();

    // La consulta USER ocurre antes del lock SPI. La implementación weak devuelve
    // true, por lo que sketches existentes conservan exactamente su frecuencia.
    // Una UI optimizada puede omitir la adquisición cuando no tiene regiones
    // sucias ni eventos pendientes.
    if (g_displayMode == DISPLAY_MODE_USER &&
        !g_userRefreshForced &&
        !jwplcUserDisplayRefreshNeededCallback(io, rtc))
    {
        return;
    }

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
        memcpy(panel.errCode, g_errCode, sizeof(panel.errCode));

        panel.bus = g_busLedState;
        panel.eth = g_ethLedState;
        memcpy(panel.busCode, g_busDiagnosticCode, sizeof(panel.busCode));
        memcpy(panel.ethCode, g_ethDiagnosticCode, sizeof(panel.ethCode));

        JWPLCIdleScreen::setStatusPanel(panel);
        JWPLCIdleScreen::draw(io, rtc);

        releaseTFTBus();
        return;
    }

    if (JWPLCUI::pageRedrawPending())
    {
        tft.fillScreen(ST77XX_BLACK);

        jwplcUIPageEnter(JWPLCUI::currentPage());

        JWPLCUI::drawStatic(tft);
        JWPLCUI::consumePageRedrawPending();
    }

    // Legacy y API corta se ejecutan con el bus TFT adquirido.
    // Para HMI basada en campos no es obligatorio implementar callbacks:
    // setValue()/setText()/setBool()/setBar() disparan el refresh por si solos.
    jwplcUserDisplayRefreshCallback(io, rtc);
    jwplcUIUpdate();
    // Solo las regiones VALUE marcadas dirty se redibujan.
    JWPLCUI::drawDirty(tft);
    JWPLCUI::consumeRefreshRequest();

    g_userRefreshForced = false;
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
bool JWPLC_DisplayClass::setErrCode(const char *code) { return JWPLCDisplay::setErrCode(code); }
const char *JWPLC_DisplayClass::errCode() const { return JWPLCDisplay::errCode(); }void JWPLC_DisplayClass::setBusLed(bool state) { JWPLCDisplay::setBusLed(state); }
bool JWPLC_DisplayClass::busLed() const { return JWPLCDisplay::busLed(); }
void JWPLC_DisplayClass::setBusLedAuto(bool enabled) { JWPLCDisplay::setBusLedAuto(enabled); }
bool JWPLC_DisplayClass::busLedAuto() const { return JWPLCDisplay::busLedAuto(); }
void JWPLC_DisplayClass::setEthLed(bool state) { JWPLCDisplay::setEthLed(state); }
bool JWPLC_DisplayClass::ethLed() const { return JWPLCDisplay::ethLed(); }
void JWPLC_DisplayClass::setEthLedAuto(bool enabled) { JWPLCDisplay::setEthLedAuto(enabled); }
bool JWPLC_DisplayClass::ethLedAuto() const { return JWPLCDisplay::ethLedAuto(); }
