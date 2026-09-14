#include "JWPLC_ModbusTCP_Client.h"

bool JWPLC_ModbusTCPClientClass::requestReadCoils(
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t *destinationPacked,
    uint32_t timeoutMs)
{
    if (destinationPacked == nullptr || quantity == 0 || quantity > 2000)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        return false;
    }

    const bool started = startRequest(
        OP_READ_BITS,
        0x01,
        startAddress,
        quantity,
        0,
        nullptr,
        timeoutMs);

    if (!started)
    {
        return false;
    }

    _bitDestination = destinationPacked;
    _txBuffer[7] = 0x01;
    return true;
}

bool JWPLC_ModbusTCPClientClass::requestReadDiscreteInputs(
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t *destinationPacked,
    uint32_t timeoutMs)
{
    if (destinationPacked == nullptr || quantity == 0 || quantity > 2000)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        return false;
    }

    const bool started = startRequest(
        OP_READ_BITS,
        0x02,
        startAddress,
        quantity,
        0,
        nullptr,
        timeoutMs);

    if (!started)
    {
        return false;
    }

    _bitDestination = destinationPacked;
    _txBuffer[7] = 0x02;
    return true;
}
