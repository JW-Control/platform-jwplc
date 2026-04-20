#include "JWPLC_Display_ST7789.h"
#include "JWPLC_IdleScreen.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {
  #include "jwplc_peripherals.h"
}

// =====================================================
// SPI compartido del JWPLC Basic
// =====================================================
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK  18

#define TFT_CS   33
#define TFT_DC   25
#define TFT_RST  14

#define SD_CS    32
#define FRAM_CS  13
#define ETH_CS   5

static Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// =====================================================
// RTC global del ecosistema JWPLC
// =====================================================
// Esta instancia NO debe crearla el usuario en su sketch.
// La librería la publica y además la usa para proveer el estado RTC al core.
JW_RTC JWPLC_RTC;

// Habilita o deshabilita internamente el backend RTC.
// Por ahora se deja siempre activo. Más adelante puede exponerse una API
// pública para deshabilitarlo sin tocar el core.
static bool g_rtcEnabled = true;

// Bit OSF del DS3232/DS3232M dentro del registro STATUS.
static constexpr uint8_t RTC_STATUS_OSF = 0x80u;

// =====================================================
// Botonera real
// =====================================================
JW_MatrixButtons JWPLC_Buttons;

static const uint8_t ROWS[] = {12, 2};
static const uint8_t COLS[] = {35, 34, 36};

static const JW_MatrixButtons::BtnMapItem BUTTON_MAP[] = {
  {BTN_LEFT,  0, 0},
  {BTN_UP,    0, 1},
  {BTN_RIGHT, 0, 2},
  {BTN_ESC,   1, 0},
  {BTN_OK,    1, 1},
  {BTN_DOWN,  1, 2}
};

static bool g_buttonsReady = false;
static TaskHandle_t g_buttonTaskHandle = nullptr;

// =====================================================
// Estado general del display
// =====================================================
enum DisplayMode : uint8_t
{
    DISPLAY_MODE_IDLE = 0,
    DISPLAY_MODE_USER = 1
};

static bool g_tftReady = false;
static bool g_forceFullRedraw = true;
static DisplayMode g_displayMode = DISPLAY_MODE_IDLE;

static JWPLCDisplay::IdleReturnMode g_idleReturnMode = JWPLCDisplay::IDLE_RETURN_ESC_ONLY;
static uint32_t g_idleTimeoutMs = 15000;
static uint32_t g_idleRefreshPeriodMs = 50;
static uint32_t g_userRefreshPeriodMs = 20;
static uint32_t g_lastActivityMs = 0;

// Indicadores laterales mostrados en la pantalla IDLE.
static bool g_runLed = true;
static bool g_errLed = false;
static bool g_busLed = false;
static bool g_ethLed = false;

// =====================================================
// Hooks débiles de UI usuario
// =====================================================
// Estos callbacks los puede implementar el sketch del usuario.
extern "C" bool __attribute__((weak)) jwplcCanReturnToIdle(void) { return true; }

extern "C" void __attribute__((weak)) jwplcUserDisplayEnterCallback(void) {}

extern "C" void __attribute__((weak)) jwplcUserDisplayRefreshCallback(const JWPLC_IOState* io, const JWPLC_RTCState* rtc)
{
  (void)io;
  (void)rtc;
}

extern "C" void __attribute__((weak)) jwplcUserDisplayExitCallback(void) {}

// =====================================================
// RTC provider callbacks para el core
// =====================================================
// El core NO conoce JW_RTC directamente.
// En su lugar llama a estos callbacks para:
// 1) inicializar el RTC
// 2) pedir un snapshot actualizado del RTC

// Inicializa el RTC usando la instancia global JWPLC_RTC.
extern "C" bool jwplcRTCBeginCallback(void)
{
    if (!g_rtcEnabled)
    {
        return false;
    }

    return JWPLC_RTC.begin();
}

// Lee el RTC y llena la estructura que el core usa internamente.
extern "C" bool jwplcRTCReadCallback(JWPLC_RTCState* rtc)
{
    if (!g_rtcEnabled || rtc == nullptr)
    {
        return false;
    }

    JWRTCDateTime dt = {};
    uint8_t status = 0xFF;

    // Si no se puede leer fecha/hora, el callback falla y el core
    // se encargará de limpiar su snapshot interno.
    if (!JWPLC_RTC.read(dt))
    {
        return false;
    }

    const bool statusOk = JWPLC_RTC.readStatus(status);

    rtc->present = true;
    rtc->valid = dt.valid;
    rtc->lost_power = statusOk ? ((status & RTC_STATUS_OSF) != 0u) : false;
    rtc->status = status;
    rtc->day_of_week = dt.dayOfWeek;
    rtc->second = dt.second;
    rtc->minute = dt.minute;
    rtc->hour = dt.hour;
    rtc->day = dt.day;
    rtc->month = dt.month;
    rtc->year = dt.year;
    rtc->last_update_ms = millis();

    return true;
}

// =====================================================
// Helpers SPI
// =====================================================
// Fuerza a todos los periféricos SPI compartidos a estado no seleccionado.
static void deselectAllSPI()
{
    pinMode(TFT_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    pinMode(FRAM_CS, OUTPUT);
    pinMode(ETH_CS, OUTPUT);

    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(FRAM_CS, HIGH);
    digitalWrite(ETH_CS, HIGH);
}

// Prepara el bus SPI para hablar únicamente con la TFT.
static void prepareForTFT()
{
    digitalWrite(SD_CS, HIGH);
    digitalWrite(FRAM_CS, HIGH);
    digitalWrite(ETH_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
}

// =====================================================
// Botonera
// =====================================================
// Tarea dedicada al escaneo periódico de la matriz de botones.
static void buttonScanTask(void* pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        if (g_buttonsReady)
        {
            JWPLC_Buttons.update();

            // Si hubo eventos, pedimos refresco del display.
            if (JWPLC_Buttons.eventCount() > 0)
            {
                jwplcSystemForceDisplayRefresh();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Inicializa la librería de botonera con el mapeo físico del JWPLC Basic.
static void initButtons()
{
    bool ok = JWPLC_Buttons.begin(
        ROWS, 2,
        COLS, 3,
        BUTTON_MAP, sizeof(BUTTON_MAP) / sizeof(BUTTON_MAP[0]),
        BTN_COUNT,
        false,
        20
    );

    if (!ok)
    {
        g_buttonsReady = false;
        return;
    }

    // Repetición solo para teclas de navegación.
    JWPLC_Buttons.setRepeatEnabled(BTN_LEFT,  true);
    JWPLC_Buttons.setRepeatEnabled(BTN_UP,    true);
    JWPLC_Buttons.setRepeatEnabled(BTN_RIGHT, true);
    JWPLC_Buttons.setRepeatEnabled(BTN_DOWN,  true);
    JWPLC_Buttons.setRepeatEnabled(BTN_OK,    false);
    JWPLC_Buttons.setRepeatEnabled(BTN_ESC,   false);

    // Perfil ya validado en hardware.
    JWPLC_Buttons.setRepeatInitialDelay(220);
    JWPLC_Buttons.setRepeatProfile(6, 12, 20, 1, 1, 1, 1, 120, 90, 70, 50);

    JWPLC_Buttons.clearPendingInput();
    g_buttonsReady = true;

    if (g_buttonTaskHandle == nullptr)
    {
        xTaskCreatePinnedToCore(
            buttonScanTask,
            "jwplcBtnScan",
            4096,
            nullptr,
            2,
            &g_buttonTaskHandle,
            ARDUINO_RUNNING_CORE
        );
    }
}

// Devuelve true si hubo pulsación o repetición en cualquier tecla.
static bool anyButtonPressedOrRepeated()
{
    if (!g_buttonsReady) return false;

    return
        JWPLC_Buttons.pressed(BTN_LEFT)  ||
        JWPLC_Buttons.pressed(BTN_UP)    ||
        JWPLC_Buttons.pressed(BTN_RIGHT) ||
        JWPLC_Buttons.pressed(BTN_ESC)   ||
        JWPLC_Buttons.pressed(BTN_OK)    ||
        JWPLC_Buttons.pressed(BTN_DOWN)  ||
        (JWPLC_Buttons.eventCount() > 0);
}

// =====================================================
// Periodo dinámico del display
// =====================================================
// El core consulta este callback para saber cada cuánto refrescar la pantalla.
extern "C" uint32_t jwplcDisplayDesiredPeriod_ms(void)
{
    return (g_displayMode == DISPLAY_MODE_IDLE) ? g_idleRefreshPeriodMs
                                                : g_userRefreshPeriodMs;
}

// =====================================================
// Modo REPOSO / USER
// =====================================================
// Gestiona la transición desde IDLE hacia USER y el posible retorno automático.
static void handleIdleWakeAndTimeout()
{
    if (g_displayMode == DISPLAY_MODE_IDLE)
    {
        if (anyButtonPressedOrRepeated())
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
// API pública de la librería
// =====================================================
namespace JWPLCDisplay
{
    // Indica si la TFT ya fue inicializada correctamente.
    bool isReady()
    {
        return g_tftReady;
    }

    // Indica si actualmente se está mostrando la pantalla IDLE.
    bool isIdleMode()
    {
        return (g_displayMode == DISPLAY_MODE_IDLE);
    }

    // Indica si la botonera quedó operativa.
    bool buttonsReady()
    {
        return g_buttonsReady;
    }

    // Fuerza un redibujado completo de la pantalla IDLE.
    void forceRedraw()
    {
        g_forceFullRedraw = true;
        JWPLCIdleScreen::forceFullRedraw();
        jwplcSystemForceDisplayRefresh();
    }

    // Registra actividad del usuario para timeout / retorno a IDLE.
    void notifyActivity()
    {
        g_lastActivityMs = millis();
    }

    // Entra al modo USER y llama al callback del sketch.
    void enterUserUI()
    {
        if (g_displayMode == DISPLAY_MODE_USER)
        {
            g_lastActivityMs = millis();
            return;
        }

        g_displayMode = DISPLAY_MODE_USER;
        g_lastActivityMs = millis();

        if (g_buttonsReady)
        {
            JWPLC_Buttons.clearPendingInput();
        }

        prepareForTFT();
        tft.fillScreen(ST77XX_BLACK);

        jwplcUserDisplayEnterCallback();
        jwplcSystemForceDisplayRefresh();
    }

    // Retorna al modo IDLE y reinicia el refresco visual.
    void goIdle()
    {
        if (g_displayMode == DISPLAY_MODE_USER)
        {
            jwplcUserDisplayExitCallback();
        }

        g_displayMode = DISPLAY_MODE_IDLE;
        g_forceFullRedraw = true;
        g_lastActivityMs = millis();

        JWPLCIdleScreen::forceFullRedraw();

        if (g_buttonsReady)
        {
            JWPLC_Buttons.clearPendingInput();
        }

        jwplcSystemForceDisplayRefresh();
    }

    // Configura el modo de retorno a IDLE.
    void setIdleReturnMode(IdleReturnMode mode)
    {
        g_idleReturnMode = mode;
    }

    // Devuelve el modo de retorno a IDLE actual.
    IdleReturnMode idleReturnMode()
    {
        return g_idleReturnMode;
    }

    // Configura el timeout de inactividad antes de volver a IDLE.
    void setIdleTimeoutMs(uint32_t timeoutMs)
    {
        g_idleTimeoutMs = timeoutMs;
    }

    // Devuelve el timeout de inactividad actual.
    uint32_t idleTimeoutMs()
    {
        return g_idleTimeoutMs;
    }

    // Configura el periodo de refresco cuando el sistema está en IDLE.
    void setIdleRefreshPeriodMs(uint32_t ms)
    {
        g_idleRefreshPeriodMs = (ms == 0) ? 1 : ms;
    }

    // Devuelve el periodo de refresco en IDLE.
    uint32_t idleRefreshPeriodMs()
    {
        return g_idleRefreshPeriodMs;
    }

    // Configura el periodo de refresco cuando el sistema está en USER.
    void setUserRefreshPeriodMs(uint32_t ms)
    {
        g_userRefreshPeriodMs = (ms == 0) ? 1 : ms;
    }

    // Devuelve el periodo de refresco en USER.
    uint32_t userRefreshPeriodMs()
    {
        return g_userRefreshPeriodMs;
    }

    // Limpia cualquier input retenido en la librería de botones.
    void clearPendingInput()
    {
        if (g_buttonsReady)
        {
            JWPLC_Buttons.clearPendingInput();
        }
    }

    // Devuelve la referencia directa al objeto TFT para uso del sketch.
    Adafruit_ST7789& display()
    {
        return tft;
    }

    // Controla el estado del LED lógico RUN mostrado en la pantalla IDLE.
    void setRunLed(bool state)
    {
        g_runLed = state;
        if (g_displayMode == DISPLAY_MODE_IDLE) jwplcSystemForceDisplayRefresh();
    }

    // Devuelve el estado lógico de RUN.
    bool runLed()
    {
        return g_runLed;
    }

    // Controla el estado del LED lógico ERR mostrado en la pantalla IDLE.
    void setErrLed(bool state)
    {
        g_errLed = state;
        if (g_displayMode == DISPLAY_MODE_IDLE) jwplcSystemForceDisplayRefresh();
    }

    // Devuelve el estado lógico de ERR.
    bool errLed()
    {
        return g_errLed;
    }

    // Controla el estado del LED lógico BUS mostrado en la pantalla IDLE.
    void setBusLed(bool state)
    {
        g_busLed = state;
        if (g_displayMode == DISPLAY_MODE_IDLE) jwplcSystemForceDisplayRefresh();
    }

    // Devuelve el estado lógico de BUS.
    bool busLed()
    {
        return g_busLed;
    }

    // Controla el estado del LED lógico ETH mostrado en la pantalla IDLE.
    void setEthLed(bool state)
    {
        g_ethLed = state;
        if (g_displayMode == DISPLAY_MODE_IDLE) jwplcSystemForceDisplayRefresh();
    }

    // Devuelve el estado lógico de ETH.
    bool ethLed()
    {
        return g_ethLed;
    }
}

// =====================================================
// Hooks del sistema: DISPLAY
// =====================================================
// Inicializa el subsistema visual del JWPLC Basic.
extern "C" bool jwplcDisplayBeginCallback(void)
{
    deselectAllSPI();
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

    prepareForTFT();
    digitalWrite(TFT_CS, LOW);

    tft.init(170, 320);
    tft.setRotation(3);

    digitalWrite(TFT_CS, HIGH);

    initButtons();

    JWPLCIdleScreen::begin(&tft);
    JWPLCIdleScreen::setTitle("JWPLC Basic");

    g_tftReady = true;
    g_forceFullRedraw = true;
    g_displayMode = DISPLAY_MODE_IDLE;
    g_idleReturnMode = JWPLCDisplay::IDLE_RETURN_ESC_ONLY;
    g_idleTimeoutMs = 15000;
    g_idleRefreshPeriodMs = 50;
    g_userRefreshPeriodMs = 20;
    g_lastActivityMs = millis();

    g_runLed = true;
    g_errLed = false;
    g_busLed = false;
    g_ethLed = false;

    JWPLCIdleScreen::forceFullRedraw();

    Serial.println("JWPLC_Display_ST7789 idle layout actualizado");
    return true;
}

// Refresca la pantalla en función del modo actual (IDLE o USER).
extern "C" void jwplcDisplayRefreshCallback(const JWPLC_IOState* io, const JWPLC_RTCState* rtc)
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
        g_forceFullRedraw = false;
        return;
    }

    jwplcUserDisplayRefreshCallback(io, rtc);
}
