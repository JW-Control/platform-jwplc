#include "JWPLC_GlobalPeripherals.h"

#include <SPI.h>

#include "jwplc_hardware_config.h"
#include "jwplc_spi_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C"
{
#include "jwplc_peripherals.h"
}

// =====================================================
// RTC global del ecosistema JWPLC
// =====================================================
// Esta instancia queda accesible desde el sketch a través del header.
// La inicialización real del RTC se hace por callback cuando el runtime
// del package lo solicita.
JW_RTC JWPLC_RTC;

// =====================================================
// FRAM global del ecosistema JWPLC
// =====================================================
// Esta instancia queda accesible desde el sketch a través del header.
// La inicialización se realiza automáticamente si JWPLC_HAS_FRAM=1.
JW_FRAM JWPLC_FRAM(JWPLC_FRAM_CS, &SPI, JWPLC_SPI_FRAM_HZ);

static bool g_framReady = false;
static bool g_framBeginAttempted = false;

// Flags básicos para futura expansión.
// Por ahora el RTC está habilitado por defecto y siempre se muestra en IDLE.
static bool g_rtcEnabled = true;

// =====================================================
// Botonera global del ecosistema JWPLC
// =====================================================
// La botonera ya no pertenece al display. El display solo la consume.

JW_MatrixButtons JWPLC_Buttons;

static const uint8_t ROWS[] = {12, 2};
static const uint8_t COLS[] = {35, 34, 36};

static const JW_MatrixButtons::BtnMapItem BUTTON_MAP[] = {
    {BTN_LEFT, 0, 0},
    {BTN_UP, 0, 1},
    {BTN_RIGHT, 0, 2},
    {BTN_ESC, 1, 0},
    {BTN_OK, 1, 1},
    {BTN_DOWN, 1, 2}};

static bool g_buttonsReady = false;
static TaskHandle_t g_buttonTaskHandle = nullptr;

static uint32_t g_buttonRefreshThrottleMs = 25;
static uint32_t g_lastButtonRefreshRequestMs = 0;

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

// Tarea dedicada de escaneo. Mantiene la lectura de la matriz desacoplada
// del loop del usuario y del refresco gráfico.
static void buttonScanTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        if (g_buttonsReady)
        {
            JWPLC_Buttons.update();

            if (JWPLC_Buttons.eventCount() > 0)
            {
                requestDisplayRefreshThrottled();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

namespace JWPLCButtons
{
    bool begin()
    {
        if (g_buttonsReady)
        {
            return true;
        }

        bool ok = JWPLC_Buttons.begin(
            ROWS, 2,
            COLS, 3,
            BUTTON_MAP, sizeof(BUTTON_MAP) / sizeof(BUTTON_MAP[0]),
            BTN_COUNT,
            false,
            20);

        if (!ok)
        {
            g_buttonsReady = false;
            return false;
        }

        JWPLC_Buttons.setRepeatEnabled(BTN_LEFT, true);
        JWPLC_Buttons.setRepeatEnabled(BTN_UP, true);
        JWPLC_Buttons.setRepeatEnabled(BTN_RIGHT, true);
        JWPLC_Buttons.setRepeatEnabled(BTN_DOWN, true);
        JWPLC_Buttons.setRepeatEnabled(BTN_OK, false);
        JWPLC_Buttons.setRepeatEnabled(BTN_ESC, false);

        JWPLC_Buttons.setRepeatInitialDelay(220);
        JWPLC_Buttons.setRepeatProfile(6, 12, 20, 1, 1, 1, 1, 120, 90, 70, 50);

        JWPLC_Buttons.clearPendingInput();

        g_lastButtonRefreshRequestMs = 0;
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
                ARDUINO_RUNNING_CORE);
        }

        return true;
    }

    bool isReady()
    {
        return g_buttonsReady;
    }

    bool anyPressedOrRepeated()
    {
        if (!g_buttonsReady)
        {
            return false;
        }

        return JWPLC_Buttons.pressed(BTN_LEFT) ||
               JWPLC_Buttons.pressed(BTN_UP) ||
               JWPLC_Buttons.pressed(BTN_RIGHT) ||
               JWPLC_Buttons.pressed(BTN_ESC) ||
               JWPLC_Buttons.pressed(BTN_OK) ||
               JWPLC_Buttons.pressed(BTN_DOWN) ||
               (JWPLC_Buttons.eventCount() > 0);
    }

    void clearPendingInput()
    {
        if (g_buttonsReady)
        {
            JWPLC_Buttons.clearPendingInput();
        }
    }
}

extern "C" bool jwplcButtonsBeginCallback(void)
{
    return JWPLCButtons::begin();
}

// =====================================================
// FRAM provider interno
// =====================================================
// JW_FRAM sigue siendo una librería reusable. Aquí solo se le inyecta
// el control del bus SPI compartido del ecosistema JWPLC.

static bool jwplcFRAMLockBus(uint32_t timeoutMs, void *userData)
{
    (void)userData;

    if (!jwplcSPI_acquire(timeoutMs))
    {
        return false;
    }

    jwplcSPI_deselectAll();
    return true;
}

static void jwplcFRAMUnlockBus(void *userData)
{
    (void)userData;
    jwplcSPI_release();
}

static bool initFRAM()
{
#if JWPLC_HAS_FRAM
    if (g_framBeginAttempted)
    {
        return g_framReady;
    }

    g_framBeginAttempted = true;

    // SPI.begin() se llama aquí porque este módulo sí tiene acceso a la
    // librería Arduino SPI. Esto permite inicializar FRAM antes de que
    // el sketch del usuario la consulte.
    SPI.begin(JWPLC_SPI_SCK, JWPLC_SPI_MISO, JWPLC_SPI_MOSI);

    if (!jwplcSPI_begin())
    {
        g_framReady = false;
        return false;
    }

    JWPLC_FRAM.setBusLockCallbacks(
        jwplcFRAMLockBus,
        jwplcFRAMUnlockBus,
        nullptr,
        50);

    g_framReady = JWPLC_FRAM.begin(JWPLC_FRAM_SIZE_BYTES);

    return g_framReady;
#else
    g_framBeginAttempted = true;
    g_framReady = false;
    return false;
#endif
}

extern "C" bool jwplcFRAMBeginCallback(void)
{
    return initFRAM();
}

// =====================================================
// Hooks RTC provider
// =====================================================
// Estos callbacks permiten que el runtime/core no dependa directamente
// de JW_RTC. Este módulo actúa como proveedor del snapshot RTC.

extern "C" bool jwplcRTCBeginCallback(void)
{
#if !JWPLC_HAS_RTC
    return false;
#else
    if (!g_rtcEnabled)
    {
        return false;
    }

    return JWPLC_RTC.begin();
#endif
}

extern "C" bool jwplcRTCReadCallback(JWPLC_RTCState *rtc)
{
#if !JWPLC_HAS_RTC
    (void)rtc;
    return false;
#else
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
#endif
}