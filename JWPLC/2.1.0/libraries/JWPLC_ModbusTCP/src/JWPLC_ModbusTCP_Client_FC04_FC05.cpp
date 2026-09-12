#include "JWPLC_ModbusTCP_Client.h"

bool JWPLC_ModbusTCPClientClass::requestReadInputRegisters(
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    if (destination == nullptr || quantity == 0 || quantity > 125)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        return false;
    }

    // FC04 tiene la misma forma request/response que FC03. Se reutiliza el
    // camino validado de lectura de registros y sólo se cambia el Function Code
    // antes de que la state machine entregue el ADU al transporte.
    const bool started = startRequest(
        OP_READ_HOLDING_REGISTERS,
        0x04,
        startAddress,
        quantity,
        0,
        destination,
        timeoutMs);

    if (!started)
    {
        return false;
    }

    _expectedFunction = 0x04;
    _txBuffer[7] = 0x04;
    return true;
}

bool JWPLC_ModbusTCPClientClass::requestWriteSingleCoil(
    uint16_t address,
    bool value,
    uint32_t timeoutMs)
{
    const uint16_t rawValue = value ? 0xFF00U : 0x0000U;

    // FC05 responde con echo address + value, igual que FC06. Se reutiliza el
    // validador de write-single ya probado conservando el valor raw Modbus.
    const bool started = startRequest(
        OP_WRITE_SINGLE_REGISTER,
        0x05,
        address,
        1,
        rawValue,
        nullptr,
        timeoutMs);

    if (!started)
    {
        return false;
    }

    _expectedFunction = 0x05;
    _txBuffer[7] = 0x05;
    return true;
}
