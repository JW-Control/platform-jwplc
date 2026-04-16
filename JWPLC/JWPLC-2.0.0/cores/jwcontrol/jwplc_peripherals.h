#ifndef JWCONTROL_JWPLC_PERIPHERALS_H
#define JWCONTROL_JWPLC_PERIPHERALS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void jwplc_pinMode(uint16_t pin, uint8_t mode);
void jwplc_digitalWrite(uint16_t pin, uint8_t val);
int  jwplc_digitalRead(uint16_t pin);

#ifdef __cplusplus
}
#endif

#endif