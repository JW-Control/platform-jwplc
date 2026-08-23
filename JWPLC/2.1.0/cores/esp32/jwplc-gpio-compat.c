#include <stdint.h>
#include "esp32-hal-gpio.h"

// Alpha5: bridge ABI para archives compartidos generados bajo JWPLC_BASIC.
//
// Este archivo vive exclusivamente en el core generico `esp32`. Los targets
// JWPLC Basic y JWPLC Basic Core usan `jwcontrol_p2` y `jwcontrol`, donde las
// implementaciones reales de jwplc_* conservan soporte para pines virtuales.
//
// En el target generico estas funciones deben comportarse exactamente como
// GPIO Arduino normal. Se delega a las primitivas internas del core ESP32 para
// evitar depender de posibles remapeos de macros de Arduino.h.

extern void ARDUINO_ISR_ATTR __pinMode(uint8_t pin, uint8_t mode);
extern void ARDUINO_ISR_ATTR __digitalWrite(uint8_t pin, uint8_t val);
extern int ARDUINO_ISR_ATTR __digitalRead(uint8_t pin);

void jwplc_pinMode(uint16_t pin, uint8_t mode)
{
    __pinMode((uint8_t)pin, mode);
}

void jwplc_digitalWrite(uint16_t pin, uint8_t val)
{
    __digitalWrite((uint8_t)pin, val);
}

int jwplc_digitalRead(uint16_t pin)
{
    return __digitalRead((uint8_t)pin);
}
