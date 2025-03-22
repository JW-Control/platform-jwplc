#ifndef _WIREC_H_
#define _WIREC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


int WireC_begin();
int WireC_readByte(uint8_t devAddr, uint8_t regAddr, uint8_t *data);
int WireC_readBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data);
int WireC_readBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t *data);
int WireC_writeByte(uint8_t devAddr, uint8_t regAddr, uint8_t data);
int WireC_writeBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data);
int WireC_writeBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t data);

#ifdef __cplusplus
}
#endif

#endif /* _WIREC_H_ */