#include "peripheral-tca6424a.h"
#include "WireC.h"

bool TCA6424A_init(uint8_t address) {
    return TCA6424A_testConnection(address);
}

bool TCA6424A_testConnection(uint8_t address) {
    uint8_t buffer = 0;
    return WireC_readByte(address, TCA6424A_RA_INPUT0, &buffer) == 0;
}

bool TCA6424A_readPin(uint8_t address, uint16_t pin, uint8_t *state) {
    uint8_t regAddr = TCA6424A_RA_INPUT0 + (pin / 8);
    uint8_t bitNum  = pin % 8;
    return WireC_readBit(address, regAddr, bitNum, state) == 0;
}

uint8_t TCA6424A_readBank(uint8_t address, uint8_t bank, uint8_t *state) {
    uint8_t regAddr = TCA6424A_RA_INPUT0 + bank;
    if (WireC_readByte(address, regAddr, state) == 0) {
        return *state;
    }
    return 0xFF;
}

bool TCA6424A_writePin(uint8_t address, uint16_t pin, bool state) {
    uint8_t regAddr = TCA6424A_RA_OUTPUT0 + (pin / 8);
    uint8_t bitNum  = pin % 8;
    return WireC_writeBit(address, regAddr, bitNum, state ? 1 : 0) == 0;
}

bool TCA6424A_writeBank(uint8_t address, uint8_t bank, uint8_t state) {
    uint8_t regAddr = TCA6424A_RA_OUTPUT0 + bank;
    return WireC_writeByte(address, regAddr, state) == 0;
}

bool TCA6424A_setPinDirection(uint8_t address, uint16_t pin, uint8_t direction) {
    uint8_t regAddr = TCA6424A_RA_CONFIG0 + (pin / 8);
    uint8_t bitNum  = pin % 8;
    return WireC_writeBit(address, regAddr, bitNum, direction) == 0;
}

bool TCA6424A_setBankDirection(uint8_t address, uint8_t bank, uint8_t direction) {
    uint8_t regAddr = TCA6424A_RA_CONFIG0 + bank;
    return WireC_writeByte(address, regAddr, direction) == 0;
}