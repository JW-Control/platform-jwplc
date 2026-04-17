#include <Arduino.h>
#include "esp32-hal-gpio.h"

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

const JWPLC_IOState* jwplcGetIOState(void)
{
    return &g_ioState;
}

const JWPLC_RTCState* jwplcGetRTCState(void)
{
    return &g_rtcState;
}

void jwplcSystemInitState(void)
{
    g_ioState.di_raw_bank0 = 0;
    g_ioState.di_logical_bank0 = 0;
    g_ioState.do_bank1 = 0;
    g_ioState.do_bank2 = 0;
    g_ioState.initialized = true;
    g_ioState.last_scan_ms = millis();

    g_rtcState.valid = false;
    g_rtcState.second = 0;
    g_rtcState.minute = 0;
    g_rtcState.hour = 0;
    g_rtcState.day = 0;
    g_rtcState.month = 0;
    g_rtcState.year = 0;
    g_rtcState.last_update_ms = 0;
}

void jwplcSystemSetOutputShadow(uint8_t bank1, uint8_t bank2)
{
    g_ioState.do_bank1 = bank1;
    g_ioState.do_bank2 = bank2;
}

void jwplcSystemClearOutputShadow(void)
{
    g_ioState.do_bank1 = 0;
    g_ioState.do_bank2 = 0;
}

void jwplcSystemScanIO(void)
{
    uint8_t bank0 = 0;

    if (TCA6424A_readBank(TCA6424A_DEFAULT_ADDRESS, 0, &bank0) != 0xFF)
    {
        g_ioState.di_raw_bank0 = bank0;
        g_ioState.di_logical_bank0 = jwplc_reverseBits8(bank0);
        g_ioState.last_scan_ms = millis();
    }
}

void jwplcSystemTickRTC(void)
{
    // Stub por ahora.
    // Aquí luego irá la lectura del RTC sobre WireC / JW_RTC.
}

void jwplcSystemDisplayHook(void)
{
    // Stub por ahora.
    // Aquí luego irá el refresco de la TFT usando solo el cache.
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

        if (TCA6424A_writePin(
                TCA6424A_DEFAULT_ADDRESS,
                channel,
                high))
        {
            if (jwplc_isOutputBank1Channel(channel))
            {
                uint8_t bit = (uint8_t)(channel - 8);
                if (high)
                    g_ioState.do_bank1 |= (uint8_t)(1u << bit);
                else
                    g_ioState.do_bank1 &= (uint8_t)~(1u << bit);
            }
            else if (jwplc_isOutputBank2Channel(channel))
            {
                uint8_t bit = (uint8_t)(channel - 16);
                if (high)
                    g_ioState.do_bank2 |= (uint8_t)(1u << bit);
                else
                    g_ioState.do_bank2 &= (uint8_t)~(1u << bit);
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

        // Entradas virtuales: leer SIEMPRE desde cache
        if (jwplc_isInputChannel(channel))
        {
            uint8_t logicalBit = (uint8_t)(7u - channel);
            return (g_ioState.di_logical_bank0 & (1u << logicalBit)) ? HIGH : LOW;
        }

        // Salidas virtuales: devolver shadow
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