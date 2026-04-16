#include <Arduino.h>
#include "esp32-hal-gpio.h"

extern "C" {
#include "jwplc_peripherals.h"
#include "peripheral-tca6424a.h"
}

extern "C" void ARDUINO_ISR_ATTR __pinMode(uint8_t pin, uint8_t mode);
extern "C" void ARDUINO_ISR_ATTR __digitalWrite(uint8_t pin, uint8_t val);
extern "C" int  ARDUINO_ISR_ATTR __digitalRead(uint8_t pin);

static inline bool jwplc_isExpanderPin(uint16_t pin) {
    return (pin & 0xFF00u) == 0x2200u;
}

static inline uint8_t jwplc_virtualChannel(uint16_t pin) {
    return (uint8_t)(pin & 0x00FFu);
}

void jwplc_pinMode(uint16_t pin, uint8_t mode) {
    if (jwplc_isExpanderPin(pin)) {
        uint8_t dir = (mode == OUTPUT) ? TCA6424A_OUTPUT : TCA6424A_INPUT;
        (void)TCA6424A_setPinDirection(
            TCA6424A_DEFAULT_ADDRESS,
            jwplc_virtualChannel(pin),
            dir
        );
        return;
    }

    __pinMode((uint8_t)pin, mode);
}

void jwplc_digitalWrite(uint16_t pin, uint8_t val) {
    if (jwplc_isExpanderPin(pin)) {
        (void)TCA6424A_writePin(
            TCA6424A_DEFAULT_ADDRESS,
            jwplc_virtualChannel(pin),
            (val != LOW)
        );
        return;
    }

    __digitalWrite((uint8_t)pin, val);
}

int jwplc_digitalRead(uint16_t pin) {
    if (jwplc_isExpanderPin(pin)) {
        uint8_t state = 0;
        if (TCA6424A_readPin(
                TCA6424A_DEFAULT_ADDRESS,
                jwplc_virtualChannel(pin),
                &state)) {
            return state ? HIGH : LOW;
        }
        return LOW;
    }

    return __digitalRead((uint8_t)pin);
}