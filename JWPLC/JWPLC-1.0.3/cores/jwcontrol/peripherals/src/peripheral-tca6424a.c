#include "peripheral-tca6424a.h"
#include "WireC.h"

/** Default constructor, uses default I2C address.
 * @see TCA6424A_DEFAULT_ADDRESS
 */
bool TCA6424A_init(uint8_t address) {
    return TCA6424A_testConnection(address);
}

/** Verify the I2C connection.
 * Make sure the device is connected and responds as expected.
 * @return True if connection is valid, false otherwise
 */
bool TCA6424A_testConnection(uint8_t address) {
    uint8_t buffer;
    return WireC_readByte(address, TCA6424A_RA_INPUT0, &buffer) == 0;
}

// INPUT* registers (x0h - x2h)

/** Get a single INPUT pin's logic level.
 * @return Pin logic level (0 or 1)
 */
bool TCA6424A_readPin(uint8_t address, uint16_t pin, uint8_t *state) {
    uint8_t regAddr = TCA6424A_RA_INPUT0 + (pin / 8);
    uint8_t bitNum = pin % 8;
    return WireC_readBit(address, regAddr, bitNum, state) == 0;
}

/** Get all pin logic levels from one bank.
 * @param bank Which bank to read (0/1/2 for P0*, P1*, P2* respectively)
 * @return 8 pins' logic levels (0 or 1 for each pin)
 */
uint8_t TCA6424A_readBank(uint8_t address, uint8_t bank, uint8_t *state) {
    uint8_t regAddr = TCA6424A_RA_INPUT0 + bank;
    if (WireC_readByte(address, regAddr, state) == 0) {
        return *state;
    }
    return 0xFF; // Retorna un valor no válido si falla
}

/** Set a single OUTPUT pin's logic level.
 * @param pin Which pin to write (0-23)
 * @param value New pin output logic level (0 or 1)
 * @return Status of operation (true = success)
 */
bool TCA6424A_writePin(uint8_t address, uint16_t pin, bool state) {
    uint8_t regAddr = TCA6424A_RA_OUTPUT0 + (pin / 8);
    uint8_t bitNum = pin % 8;
    return WireC_writeBit(address, regAddr, bitNum, state) == 0;
}

/** Set all OUTPUT pins' logic levels in one bank.
 * @param bank Which bank to write (0/1/2 for P0*, P1*, P2* respectively)
 * @param value New pins' output logic level (0 or 1 for each pin)
 */
bool TCA6424A_writeBank(uint8_t address, uint8_t bank, uint8_t state) {
    uint8_t regAddr = TCA6424A_RA_OUTPUT0 + bank;
    return WireC_writeByte(address, regAddr, state) == 0;
}


/** Set a single pin's direction (I/O) setting.
 * @param pin Which pin to write (0-23)
 * @param direction Pin direction setting (0 or 1)
 * @return Status of operation (true = success)
 */
bool TCA6424A_setPinDirection(uint8_t address, uint16_t pin, uint8_t direction) {
    uint8_t regAddr = TCA6424A_RA_CONFIG0 + (pin / 8);
    uint8_t bitNum = pin % 8;
    return WireC_writeBit(address, regAddr, bitNum, direction) == 0;
}

/** Set all pin direction (I/O) settings in one bank.
 * @param bank Which bank to read (0/1/2 for P0*, P1*, P2* respectively)
 * @param direction New pins' direction settings (0 or 1 for each pin)
 */
bool TCA6424A_setBankDirection(uint8_t address, uint8_t bank, uint8_t direction) {
    uint8_t regAddr = TCA6424A_RA_CONFIG0 + bank;
    return WireC_writeByte(address, regAddr, direction) == 0;
}