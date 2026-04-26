#ifndef JWPLC_I2C_BRIDGE_H
#define JWPLC_I2C_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int jwplcI2C_begin(void);
int jwplcI2C_beginWithPins(uint8_t sdaPin, uint8_t sclPin, uint32_t frequencyHz);
int jwplcI2C_setClock(uint32_t frequencyHz);
int jwplcI2C_probe(uint8_t address);

int jwplcI2C_readReg8(uint8_t address, uint8_t reg, uint8_t *data);
int jwplcI2C_readRegs(uint8_t address, uint8_t startReg, uint8_t length, uint8_t *data);

int jwplcI2C_writeReg8(uint8_t address, uint8_t reg, uint8_t data);
int jwplcI2C_writeRegs(uint8_t address, uint8_t startReg, uint8_t length, const uint8_t *data);

int jwplcI2C_updateBit(uint8_t address, uint8_t reg, uint8_t bitNum, uint8_t bitValue);

#ifdef __cplusplus
}
#endif

#endif