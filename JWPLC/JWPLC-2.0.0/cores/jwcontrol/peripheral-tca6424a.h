#ifndef _PERIPHERAL_TCA6424A_H_
#define _PERIPHERAL_TCA6424A_H_

#include <stdint.h>
#include <stdbool.h>

#define TCA6424A_NUM_IO 24

#define TCA6424A_ADDRESS_ADDR_LOW   0x22
#define TCA6424A_ADDRESS_ADDR_HIGH  0x23
#define TCA6424A_DEFAULT_ADDRESS    TCA6424A_ADDRESS_ADDR_LOW

#define TCA6424A_RA_INPUT0          0x00
#define TCA6424A_RA_INPUT1          0x01
#define TCA6424A_RA_INPUT2          0x02
#define TCA6424A_RA_OUTPUT0         0x04
#define TCA6424A_RA_OUTPUT1         0x05
#define TCA6424A_RA_OUTPUT2         0x06
#define TCA6424A_RA_POLARITY0       0x08
#define TCA6424A_RA_POLARITY1       0x09
#define TCA6424A_RA_POLARITY2       0x0A
#define TCA6424A_RA_CONFIG0         0x0C
#define TCA6424A_RA_CONFIG1         0x0D
#define TCA6424A_RA_CONFIG2         0x0E

#define TCA6424A_AUTO_INCREMENT     0x80

#define TCA6424A_LOW                0
#define TCA6424A_HIGH               1

#define TCA6424A_POLARITY_NORMAL    0
#define TCA6424A_POLARITY_INVERTED  1

#define TCA6424A_OUTPUT             0
#define TCA6424A_INPUT              1

bool TCA6424A_init(uint8_t address);
bool TCA6424A_testConnection(uint8_t address);

bool TCA6424A_readPin(uint8_t address, uint16_t pin, uint8_t *state);
bool TCA6424A_writePin(uint8_t address, uint16_t pin, bool state);

uint8_t TCA6424A_readBank(uint8_t address, uint8_t bank, uint8_t *state);
bool TCA6424A_writeBank(uint8_t address, uint8_t bank, uint8_t state);

bool TCA6424A_setPinDirection(uint8_t address, uint16_t pin, uint8_t direction);
bool TCA6424A_setBankDirection(uint8_t address, uint8_t bank, uint8_t direction);

#endif /* _PERIPHERAL_TCA6424A_H_ */