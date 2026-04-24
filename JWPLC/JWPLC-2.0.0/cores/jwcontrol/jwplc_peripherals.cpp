#include <Arduino.h>
#include "esp32-hal-gpio.h"
#include "jwplc_hardware_config.h"

extern "C"
{
#include "jwplc_peripherals.h"
#include "peripheral-tca6424a.h"
}

extern "C" void ARDUINO_ISR_ATTR __pinMode(uint8_t pin, uint8_t mode);
extern "C" void ARDUINO_ISR_ATTR __digitalWrite(uint8_t pin, uint8_t val);
extern "C" int ARDUINO_ISR_ATTR __digitalRead(uint8_t pin);

static JWPLC_IOState g_ioState = {};
static JWPLC_RTCState g_rtcState = {};

static bool g_displayInitialized = false;
static uint32_t g_lastDisplayBeginAttemptMs = 0;

static inline bool jwplc_isExpanderPin(uint16_t pin)
{
    return (pin & 0xFF00u) == 0x2200u;
}

static inline uint8_t jwplc_virtualChannel(uint16_t pin)
{
    return (uint8_t)(pin & 0x00FFu);
}

static inline uint8_t jwplc_reverseBits8(uint8_t x)
{
    x = (uint8_t)(((x & 0xF0u) >> 4) | ((x & 0x0Fu) << 4));
    x = (uint8_t)(((x & 0xCCu) >> 2) | ((x & 0x33u) << 2));
    x = (uint8_t)(((x & 0xAAu) >> 1) | ((x & 0x55u) << 1));
    return x;
}

static inline bool jwplc_isInputChannel(uint8_t channel)
{
    return channel <= 7;
}

static inline bool jwplc_isOutputBank1Channel(uint8_t channel)
{
    return (channel >= 8) && (channel <= 15);
}

static inline bool jwplc_isOutputBank2Channel(uint8_t channel)
{
    return (channel >= 16) && (channel <= 23);
}

// Hooks débiles por defecto
bool jwplcDisplayBeginCallback(void) __attribute__((weak));
bool jwplcDisplayBeginCallback(void)
{
    return false;
}

void jwplcDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc) __attribute__((weak));
void jwplcDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;
}

bool jwplcRTCBeginCallback(void) __attribute__((weak));
bool jwplcRTCBeginCallback(void)
{
    return false;
}

bool jwplcRTCReadCallback(JWPLC_RTCState *rtc) __attribute__((weak));
bool jwplcRTCReadCallback(JWPLC_RTCState *rtc)
{
    (void)rtc;
    return false;
}

const JWPLC_IOState *jwplcGetIOState(void)
{
    return &g_ioState;
}

const JWPLC_RTCState *jwplcGetRTCState(void)
{
    return &g_rtcState;
}

void jwplcSystemMarkDisplayDirty(void)
{
    g_ioState.display_dirty = true;
}

void jwplcSystemForceDisplayRefresh(void)
{
    g_ioState.display_dirty = true;
    g_ioState.last_display_refresh_ms = 0;
}

bool jwplcSystemConsumeDisplayDirty(void)
{
    bool dirty = g_ioState.display_dirty;
    g_ioState.display_dirty = false;
    return dirty;
}

void jwplcSystemInitState(void)
{
    g_ioState.di_raw_bank0 = 0;
    g_ioState.di_logical_bank0 = 0;
    g_ioState.do_bank1 = 0;
    g_ioState.do_bank2 = 0;
    g_ioState.initialized = true;
    g_ioState.display_dirty = true;
    g_ioState.last_scan_ms = millis();
    g_ioState.last_display_refresh_ms = 0;

    g_rtcState.present = false;
    g_rtcState.valid = false;
    g_rtcState.lost_power = false;
    g_rtcState.status = 0xFF;
    g_rtcState.day_of_week = 0;
    g_rtcState.second = 0;
    g_rtcState.minute = 0;
    g_rtcState.hour = 0;
    g_rtcState.day = 0;
    g_rtcState.month = 0;
    g_rtcState.year = 0;
    g_rtcState.last_update_ms = 0;

    g_displayInitialized = false;
    g_lastDisplayBeginAttemptMs = 0;
}

void jwplcSystemSetOutputShadow(uint8_t bank1, uint8_t bank2)
{
    if ((g_ioState.do_bank1 != bank1) || (g_ioState.do_bank2 != bank2))
    {
        g_ioState.do_bank1 = bank1;
        g_ioState.do_bank2 = bank2;
        jwplcSystemMarkDisplayDirty();
    }
}

void jwplcSystemClearOutputShadow(void)
{
    jwplcSystemSetOutputShadow(0, 0);
}

void jwplcSystemScanIO(void)
{
    uint8_t bank0 = 0;

    if (TCA6424A_readBank(TCA6424A_DEFAULT_ADDRESS, 0, &bank0) != 0xFF)
    {
        uint8_t logical = jwplc_reverseBits8(bank0);

        if ((g_ioState.di_raw_bank0 != bank0) || (g_ioState.di_logical_bank0 != logical))
        {
            g_ioState.di_raw_bank0 = bank0;
            g_ioState.di_logical_bank0 = logical;
            jwplcSystemMarkDisplayDirty();
        }

        g_ioState.last_scan_ms = millis();
    }
}

void jwplcSystemTickRTC(void)
{
#if JWPLC_HAS_RTC
    JWPLC_RTCState snapshot = {};
    snapshot.status = 0xFF;

    if (jwplcRTCReadCallback(&snapshot))
    {
        bool changed =
            (g_rtcState.present != snapshot.present) ||
            (g_rtcState.valid != snapshot.valid) ||
            (g_rtcState.lost_power != snapshot.lost_power) ||
            (g_rtcState.status != snapshot.status) ||
            (g_rtcState.day_of_week != snapshot.day_of_week) ||
            (g_rtcState.second != snapshot.second) ||
            (g_rtcState.minute != snapshot.minute) ||
            (g_rtcState.hour != snapshot.hour) ||
            (g_rtcState.day != snapshot.day) ||
            (g_rtcState.month != snapshot.month) ||
            (g_rtcState.year != snapshot.year);

        snapshot.last_update_ms = millis();
        g_rtcState = snapshot;

        if (changed)
        {
            jwplcSystemMarkDisplayDirty();
        }
    }
    else
    {
        bool changed =
            g_rtcState.present ||
            g_rtcState.valid ||
            g_rtcState.lost_power ||
            (g_rtcState.status != 0xFFu) ||
            (g_rtcState.day_of_week != 0u) ||
            (g_rtcState.second != 0u) ||
            (g_rtcState.minute != 0u) ||
            (g_rtcState.hour != 0u) ||
            (g_rtcState.day != 0u) ||
            (g_rtcState.month != 0u) ||
            (g_rtcState.year != 0u);

        g_rtcState.present = false;
        g_rtcState.valid = false;
        g_rtcState.lost_power = false;
        g_rtcState.status = 0xFF;
        g_rtcState.day_of_week = 0;
        g_rtcState.second = 0;
        g_rtcState.minute = 0;
        g_rtcState.hour = 0;
        g_rtcState.day = 0;
        g_rtcState.month = 0;
        g_rtcState.year = 0;
        g_rtcState.last_update_ms = millis();

        if (changed)
        {
            jwplcSystemMarkDisplayDirty();
        }
    }
#else
    bool changed =
        g_rtcState.present ||
        g_rtcState.valid ||
        g_rtcState.lost_power ||
        (g_rtcState.status != 0xFFu) ||
        (g_rtcState.day_of_week != 0u) ||
        (g_rtcState.second != 0u) ||
        (g_rtcState.minute != 0u) ||
        (g_rtcState.hour != 0u) ||
        (g_rtcState.day != 0u) ||
        (g_rtcState.month != 0u) ||
        (g_rtcState.year != 0u);

    g_rtcState.present = false;
    g_rtcState.valid = false;
    g_rtcState.lost_power = false;
    g_rtcState.status = 0xFF;
    g_rtcState.day_of_week = 0;
    g_rtcState.second = 0;
    g_rtcState.minute = 0;
    g_rtcState.hour = 0;
    g_rtcState.day = 0;
    g_rtcState.month = 0;
    g_rtcState.year = 0;
    g_rtcState.last_update_ms = millis();

    if (changed)
    {
        jwplcSystemMarkDisplayDirty();
    }
#endif
}

void jwplcSystemDisplayHook(void)
{
    uint32_t now = millis();

    if (!g_displayInitialized)
    {
        if ((uint32_t)(now - g_lastDisplayBeginAttemptMs) >= 500u)
        {
            g_lastDisplayBeginAttemptMs = now;

            if (jwplcDisplayBeginCallback())
            {
                g_displayInitialized = true;
                jwplcSystemForceDisplayRefresh();
            }
        }
        return;
    }

    bool dirty = jwplcSystemConsumeDisplayDirty();
    bool periodicRefresh = ((uint32_t)(now - g_ioState.last_display_refresh_ms) >= 1000u);

    if (!dirty && !periodicRefresh)
    {
        return;
    }

    jwplcDisplayRefreshCallback(&g_ioState, &g_rtcState);
    g_ioState.last_display_refresh_ms = now;
}

void jwplc_pinMode(uint16_t pin, uint8_t mode)
{
    if (jwplc_isExpanderPin(pin))
    {
        uint8_t dir = (mode == OUTPUT) ? TCA6424A_OUTPUT : TCA6424A_INPUT;
        (void)TCA6424A_setPinDirection(
            TCA6424A_DEFAULT_ADDRESS,
            jwplc_virtualChannel(pin),
            dir);
        return;
    }

    __pinMode((uint8_t)pin, mode);
}

void jwplc_digitalWrite(uint16_t pin, uint8_t val)
{
    if (jwplc_isExpanderPin(pin))
    {
        uint8_t channel = jwplc_virtualChannel(pin);
        bool high = (val != LOW);

        if (TCA6424A_writePin(TCA6424A_DEFAULT_ADDRESS, channel, high))
        {
            if (jwplc_isOutputBank1Channel(channel))
            {
                uint8_t bit = (uint8_t)(channel - 8);
                uint8_t oldVal = g_ioState.do_bank1;

                if (high)
                    g_ioState.do_bank1 |= (uint8_t)(1u << bit);
                else
                    g_ioState.do_bank1 &= (uint8_t)~(1u << bit);

                if (g_ioState.do_bank1 != oldVal)
                {
                    jwplcSystemMarkDisplayDirty();
                }
            }
            else if (jwplc_isOutputBank2Channel(channel))
            {
                uint8_t bit = (uint8_t)(channel - 16);
                uint8_t oldVal = g_ioState.do_bank2;

                if (high)
                    g_ioState.do_bank2 |= (uint8_t)(1u << bit);
                else
                    g_ioState.do_bank2 &= (uint8_t)~(1u << bit);

                if (g_ioState.do_bank2 != oldVal)
                {
                    jwplcSystemMarkDisplayDirty();
                }
            }
        }
        return;
    }

    __digitalWrite((uint8_t)pin, val);
}

int jwplc_digitalRead(uint16_t pin)
{
    if (jwplc_isExpanderPin(pin))
    {
        uint8_t channel = jwplc_virtualChannel(pin);

        if (jwplc_isInputChannel(channel))
        {
            uint8_t logicalBit = (uint8_t)(7u - channel);
            return (g_ioState.di_logical_bank0 & (1u << logicalBit)) ? HIGH : LOW;
        }

        if (jwplc_isOutputBank1Channel(channel))
        {
            uint8_t bit = (uint8_t)(channel - 8);
            return (g_ioState.do_bank1 & (1u << bit)) ? HIGH : LOW;
        }

        if (jwplc_isOutputBank2Channel(channel))
        {
            uint8_t bit = (uint8_t)(channel - 16);
            return (g_ioState.do_bank2 & (1u << bit)) ? HIGH : LOW;
        }

        return LOW;
    }

    return __digitalRead((uint8_t)pin);
}
