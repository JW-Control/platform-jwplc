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
// SPI compartido
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

static constexpr uint32_t TFT_SPI_HZ = 80000000;

static Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// =====================================================
// RTC global del ecosistema JWPLC
// =====================================================
// Esta instancia queda accesible desde el sketch a través del header.
// La inicialización real del RTC se hace por callback cuando el runtime
// del package lo solicita.
JW_RTC JWPLC_RTC;

// Flags básicos para futura expansión.
// Por ahora el RTC está habilitado por defecto y siempre se muestra en IDLE.
static bool g_rtcEnabled = true;

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
// Estado general del módulo display
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
// Optimización Alpha.17: throttle de refresh por botones
// =====================================================
// Antes, cada evento pendiente de la botonera forzaba refresh inmediato.
// Eso generaba demasiado tráfico visual cuando el usuario mantenía botones
// o navegaba rápido. Ahora se limita la frecuencia máxima de refresh
// provocado por input.
static uint32_t g_buttonRefreshThrottleMs = 25;
static uint32_t g_lastButtonRefreshRequestMs = 0;

// =====================================================
// Hooks débiles de UI de usuario
// =====================================================
// El sketch puede redefinir estos hooks para crear sus propias pantallas.
extern "C" bool __attribute__((weak)) jwplcCanReturnToIdle(void) { return true; }

extern "C" void __attribute__((weak)) jwplcUserDisplayEnterCallback(void) {}

extern "C" void __attribute__((weak)) jwplcUserDisplayRefreshCallback(const JWPLC_IOState* io, const JWPLC_RTCState* rtc)
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
    pinMode(TFT_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    pinMode(FRAM_CS, OUTPUT);
    pinMode(ETH_CS, OUTPUT);

    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(FRAM_CS, HIGH);
    digitalWrite(ETH_CS, HIGH);
}

// Prepara el bus para uso de la TFT dejando deseleccionados los demás
// periféricos SPI.
static void prepareForTFT()
{
    digitalWrite(SD_CS, HIGH);
    digitalWrite(FRAM_CS, HIGH);
    digitalWrite(ETH_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
}

// =====================================================
// Helpers de refresh / invalidación
// =====================================================
// Fuerza un refresh del display, pero respetando un intervalo mínimo
// para que la botonera no dispare redibujados masivos demasiado seguido.
static void requestDisplayRefreshThrottled()
{
    uint32_t now = millis();

    if ((uint32_t)(now - g_lastButtonRefreshRequestMs) >= g_buttonRefreshThrottleMs)
    {
        g_lastButtonRefreshRequestMs = now;
        jwplcSystemForceDisplayRefresh();
    }
}

// Reinicia el estado visual básico del módulo al entrar/salir de pantallas.
static void resetDisplayState()
{
    g_forceFullRedraw = true;
    g_lastActivityMs = millis();
}

// =====================================================
// Botonera
// =====================================================
// Tarea dedicada de escaneo. Mantiene la lectura de la matriz desacoplada
// del loop del usuario y del refresco gráfico.
static void buttonScanTask(void* pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        if (g_buttonsReady)
        {
            JWPLC_Buttons.update();

            // Si hay eventos pendientes, no forzamos refresh inmediato sin control.
            // En su lugar, pedimos refresh con throttle para evitar penalizar la UI
            // y afectar el resto del sistema cuando el usuario navega rápido.
            if (JWPLC_Buttons.eventCount() > 0)
            {
                requestDisplayRefreshThrottled();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Inicializa la botonera física y su perfil de repetición.
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

    JWPLC_Buttons.setRepeatEnabled(BTN_LEFT,  true);
    JWPLC_Buttons.setRepeatEnabled(BTN_UP,    true);
    JWPLC_Buttons.setRepeatEnabled(BTN_RIGHT, true);
    JWPLC_Buttons.setRepeatEnabled(BTN_DOWN,  true);
    JWPLC_Buttons.setRepeatEnabled(BTN_OK,    false);
    JWPLC_Buttons.setRepeatEnabled(BTN_ESC,   false);

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
            1,
            &g_buttonTaskHandle,
            ARDUINO_RUNNING_CORE
        );
    }
}

// Retorna true si hay pulsaciones/repeticiones pendientes.
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
        return g_buttonsReady;
    }

    // Fuerza redibujado completo de la pantalla actual.
    void forceRedraw()
    {
        g_forceFullRedraw = true;
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

        if (g_buttonsReady)
        {
            JWPLC_Buttons.clearPendingInput();
        }

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

        if (g_buttonsReady)
        {
            JWPLC_Buttons.clearPendingInput();
        }

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
        if (g_buttonsReady)
        {
            JWPLC_Buttons.clearPendingInput();
        }
    }

    Adafruit_ST7789& display()
    {
        return tft;
    }

    void setRunLed(bool state)
    {
        g_runLed = state;
        if (g_displayMode == DISPLAY_MODE_IDLE) jwplcSystemForceDisplayRefresh();
    }

    bool runLed()
    {
        return g_runLed;
    }

    void setErrLed(bool state)
    {
        g_errLed = state;
        if (g_displayMode == DISPLAY_MODE_IDLE) jwplcSystemForceDisplayRefresh();
    }

    bool errLed()
    {
        return g_errLed;
    }

    void setBusLed(bool state)
    {
        g_busLed = state;
        if (g_displayMode == DISPLAY_MODE_IDLE) jwplcSystemForceDisplayRefresh();
    }

    bool busLed()
    {
        return g_busLed;
    }

    void setEthLed(bool state)
    {
        g_ethLed = state;
        if (g_displayMode == DISPLAY_MODE_IDLE) jwplcSystemForceDisplayRefresh();
    }

    bool ethLed()
    {
        return g_ethLed;
    }
}

// =====================================================
// Hooks RTC provider
// =====================================================
// Estos callbacks permiten que el runtime/core no dependa directamente
// de JW_RTC. La librería display actúa como proveedor del snapshot RTC.

extern "C" bool jwplcRTCBeginCallback(void)
{
    if (!g_rtcEnabled)
    {
        return false;
    }

    return JWPLC_RTC.begin();
}

extern "C" bool jwplcRTCReadCallback(JWPLC_RTCState* rtc)
{
    if (!g_rtcEnabled || rtc == nullptr)
    {
        return false;
    }

    JWRTCDateTime dt = {};
    uint8_t status = 0xFF;

    if (!JWPLC_RTC.read(dt))
    {
        return false;
    }

    bool statusOk = JWPLC_RTC.readStatus(status);

    rtc->present = true;
    rtc->valid = dt.valid;
    rtc->lost_power = statusOk ? ((status & 0x80u) != 0u) : false;
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
// Hooks display del sistema
// =====================================================
// Inicialización real de la TFT y de la botonera. El runtime llama a este
// hook automáticamente cuando corresponde.
extern "C" bool jwplcDisplayBeginCallback(void)
{
    deselectAllSPI();
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

    prepareForTFT();
    digitalWrite(TFT_CS, LOW);

    tft.init(170, 320);
    tft.setRotation(3);
    tft.setSPISpeed(TFT_SPI_HZ); //Probado y validado a 80 MHz sin problemas, incluso con otros periféricos SPI activos. La pantalla responde bien y no se detectan errores visuales ni de comunicación.

    digitalWrite(TFT_CS, HIGH);

    initButtons();

    JWPLCIdleScreen::begin(&tft);
    JWPLCIdleScreen::setTitle("JWPLC Basic");

    g_tftReady = true;
    g_displayMode = DISPLAY_MODE_IDLE;

    // Se reinician valores base del módulo al completar la inicialización.
    g_idleReturnMode = JWPLCDisplay::IDLE_RETURN_ESC_ONLY;
    g_idleTimeoutMs = 15000;
    g_idleRefreshPeriodMs = 10;
    g_userRefreshPeriodMs = 40;
    g_lastButtonRefreshRequestMs = 0;

    g_runLed = true;
    g_errLed = false;
    g_busLed = false;
    g_ethLed = false;

    resetDisplayState();
    JWPLCIdleScreen::forceFullRedraw();

    Serial.println("JWPLC_Display_ST7789 inicializado (alpha.17)");
    return true;
}

// Refresco principal del display. El runtime le entrega snapshots del sistema
// ya procesados. Aquí solo se decide cómo representarlos visualmente.
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

    // En modo usuario, la UI concreta la define el sketch mediante callback.
    // El objetivo es que el sketch actualice solo lo que cambie, evitando
    // redibujados globales innecesarios.
    jwplcUserDisplayRefreshCallback(io, rtc);
}
