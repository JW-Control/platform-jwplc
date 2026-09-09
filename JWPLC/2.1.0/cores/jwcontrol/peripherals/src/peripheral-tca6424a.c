#include "peripheral-tca6424a.h"
#include "jwplc_i2c_bridge.h"

// Shadow interno de los registros OUTPUT del TCA6424A.
//
// Objetivo Alpha11:
// - una escritura repetida del mismo estado no debe generar tráfico I2C;
// - una escritura que realmente cambia conserva el camino validado existente
//   (read-modify-write mediante jwplcI2C_updateBit);
// - writeBank() mantiene el shadow sincronizado para que, tras el arranque
//   normal del JWPLC Basic, digitalWrite(Qx_y, mismo_estado) sea un no-op real.
//
// El driver usa un único TCA6424A en JWPLC Basic. Si se cambia de dirección,
// el shadow se invalida antes de reutilizarlo para no mezclar dispositivos.
static uint8_t g_outputShadow[3] = {0, 0, 0};
static bool g_outputShadowValid[3] = {false, false, false};
static uint8_t g_outputShadowAddress = 0;
static bool g_outputShadowAddressValid = false;

static bool TCA6424A_isValidBank(uint8_t bank)
{
    return bank < 3;
}

static void TCA6424A_invalidateOutputShadow(void)
{
    g_outputShadowValid[0] = false;
    g_outputShadowValid[1] = false;
    g_outputShadowValid[2] = false;
}

static void TCA6424A_prepareOutputShadowAddress(uint8_t address)
{
    if (!g_outputShadowAddressValid || g_outputShadowAddress != address)
    {
        TCA6424A_invalidateOutputShadow();
        g_outputShadowAddress = address;
        g_outputShadowAddressValid = true;
    }
}

bool TCA6424A_init(uint8_t address)
{
    TCA6424A_prepareOutputShadowAddress(address);
    TCA6424A_invalidateOutputShadow();
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

bool TCA6424A_readBank(uint8_t address, uint8_t bank, uint8_t *state)
{
    if (!TCA6424A_isValidBank(bank) || state == 0)
    {
        return false;
    }

    uint8_t regAddr = (uint8_t)(TCA6424A_RA_INPUT0 + bank);
    return jwplcI2C_readReg8(address, regAddr, state) == 0;
}

bool TCA6424A_writePin(uint8_t address, uint16_t pin, bool state)
{
    if (pin >= TCA6424A_NUM_IO)
    {
        return false;
    }

    uint8_t bank    = (uint8_t)(pin / 8);
    uint8_t regAddr = (uint8_t)(TCA6424A_RA_OUTPUT0 + bank);
    uint8_t bitNum  = (uint8_t)(pin % 8);
    uint8_t bitMask = (uint8_t)(1u << bitNum);

    TCA6424A_prepareOutputShadowAddress(address);

    if (g_outputShadowValid[bank])
    {
        bool currentState = (g_outputShadow[bank] & bitMask) != 0;

        if (currentState == state)
        {
            // No-op real: no mutex I2C, no read y no write.
            return true;
        }
    }

    if (jwplcI2C_updateBit(address, regAddr, bitNum, state ? 1 : 0) != 0)
    {
        return false;
    }

    if (g_outputShadowValid[bank])
    {
        if (state)
        {
            g_outputShadow[bank] |= bitMask;
        }
        else
        {
            g_outputShadow[bank] &= (uint8_t)~bitMask;
        }
    }

    return true;
}

bool TCA6424A_writeBank(uint8_t address, uint8_t bank, uint8_t state)
{
    if (!TCA6424A_isValidBank(bank))
    {
        return false;
    }

    TCA6424A_prepareOutputShadowAddress(address);

    if (g_outputShadowValid[bank] && g_outputShadow[bank] == state)
    {
        // El banco ya está en el estado solicitado: cero tráfico I2C.
        return true;
    }

    uint8_t regAddr = (uint8_t)(TCA6424A_RA_OUTPUT0 + bank);

    if (jwplcI2C_writeReg8(address, regAddr, state) != 0)
    {
        return false;
    }

    g_outputShadow[bank] = state;
    g_outputShadowValid[bank] = true;
    return true;
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