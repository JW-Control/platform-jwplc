#include "peripheral-tca6424a.h"
#include "jwplc_i2c_bridge.h"

static bool TCA6424A_isValidBank(uint8_t bank)
{
    return bank < 3;
}

bool TCA6424A_init(uint8_t address)
{
    return TCA6424A_testConnection(address);
}

bool TCA6424A_testConnection(uint8_t address)
{
    uint8_t buffer = 0;
    return jwplcI2C_readReg8(address, TCA6424A_RA_INPUT0, &buffer) == 0;
}

bool TCA6424A_readPin(uint8_t address, uint16_t pin, uint8_t *state)
{
    if (state == 0 || pin >= TCA6424A_NUM_IO)
    {
        return false;
    }

    uint8_t regAddr = (uint8_t)(TCA6424A_RA_INPUT0 + (pin / 8));
    uint8_t bitNum  = (uint8_t)(pin % 8);
    uint8_t regVal  = 0;

    if (jwplcI2C_readReg8(address, regAddr, &regVal) != 0)
    {
        return false;
    }

    *state = (uint8_t)((regVal >> bitNum) & 0x01);
    return true;
}

uint8_t TCA6424A_readBank(uint8_t address, uint8_t bank, uint8_t *state)
{
    if (!TCA6424A_isValidBank(bank) || state == 0)
    {
        return 0xFF;
    }

    uint8_t regAddr = (uint8_t)(TCA6424A_RA_INPUT0 + bank);
    if (jwplcI2C_readReg8(address, regAddr, state) == 0)
    {
        return *state;
    }

    return 0xFF;
}

bool TCA6424A_writePin(uint8_t address, uint16_t pin, bool state)
{
    if (pin >= TCA6424A_NUM_IO)
    {
        return false;
    }

    uint8_t regAddr = (uint8_t)(TCA6424A_RA_OUTPUT0 + (pin / 8));
    uint8_t bitNum  = (uint8_t)(pin % 8);

    return jwplcI2C_updateBit(address, regAddr, bitNum, state ? 1 : 0) == 0;
}

bool TCA6424A_writeBank(uint8_t address, uint8_t bank, uint8_t state)
{
    if (!TCA6424A_isValidBank(bank))
    {
        return false;
    }

    uint8_t regAddr = (uint8_t)(TCA6424A_RA_OUTPUT0 + bank);
    return jwplcI2C_writeReg8(address, regAddr, state) == 0;
}

bool TCA6424A_setPinDirection(uint8_t address, uint16_t pin, uint8_t direction)
{
    if (pin >= TCA6424A_NUM_IO)
    {
        return false;
    }

    uint8_t regAddr = (uint8_t)(TCA6424A_RA_CONFIG0 + (pin / 8));
    uint8_t bitNum  = (uint8_t)(pin % 8);

    return jwplcI2C_updateBit(address, regAddr, bitNum, direction ? 1 : 0) == 0;
}

bool TCA6424A_setBankDirection(uint8_t address, uint8_t bank, uint8_t direction)
{
    if (!TCA6424A_isValidBank(bank))
    {
        return false;
    }

    uint8_t regAddr = (uint8_t)(TCA6424A_RA_CONFIG0 + bank);
    return jwplcI2C_writeReg8(address, regAddr, direction) == 0;
}