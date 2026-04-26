#include "JWPLC_GlobalPeripherals.h"

#include <SPI.h>

#include "jwplc_hardware_config.h"
#include "jwplc_spi_bus.h"

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