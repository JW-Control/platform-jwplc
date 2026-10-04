#include "JWPLC_ModbusTCP_Client.h"

bool JWPLC_ModbusTCPClientClass::startWriteMultipleRequest(
    Operation operation,
    uint8_t functionCode,
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t byteCount,
    uint32_t timeoutMs)
{
    if (busy())
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_BUSY;
        return false;
    }

    if (!_configured)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_NOT_CONFIGURED;
        return false;
    }

    if (timeoutMs == 0)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        return false;
    }

    const uint16_t totalLength = (uint16_t)(13U + byteCount);
    if (totalLength > JWPLC_MODBUS_TCP_CLIENT_MAX_ADU)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        return false;
    }

    if (done())
    {
        clearResult();
    }

    _operation = operation;
    _expectedFunction = functionCode;
    _startAddress = startAddress;
    _quantity = quantity;
    _writeValue = 0;
    _registerDestination = nullptr;
    _bitDestination = nullptr;
    _exceptionCode = 0;
    _activeTransactionId = _nextTransactionId++;
    _requestStartMs = millis();
    _timeoutMs = timeoutMs;
    _connectStarted = false;
    _sendStarted = false;
    resetRx();

    writeU16BE(&_txBuffer[0], _activeTransactionId);
    writeU16BE(&_txBuffer[2], 0);
    writeU16BE(&_txBuffer[4], (uint16_t)(7U + byteCount));
    _txBuffer[6] = _unitId;
    _txBuffer[7] = functionCode;
    writeU16BE(&_txBuffer[8], startAddress);
    writeU16BE(&_txBuffer[10], quantity);
    _txBuffer[12] = byteCount;
    _txLength = totalLength;

    _result = JWPLC_MODBUS_TCP_CLIENT_OK;
    _state = JWPLC_Ethernet.isReady()
                 ? JWPLC_MODBUS_TCP_CLIENT_CONNECTING
                 : JWPLC_MODBUS_TCP_CLIENT_WAIT_ETHERNET;
    return true;
}

bool JWPLC_ModbusTCPClientClass::requestWriteMultipleCoils(
    uint16_t startAddress,
    uint16_t quantity,
    const uint8_t *sourcePacked,
    uint32_t timeoutMs)
{
    if (sourcePacked == nullptr || quantity == 0 || quantity > 1968)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        return false;
    }

    const uint16_t byteCount16 = (uint16_t)((quantity + 7U) / 8U);
    if (byteCount16 > 246U)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        return false;
    }

    const uint8_t byteCount = (uint8_t)byteCount16;

    if (!startWriteMultipleRequest(
            OP_WRITE_MULTIPLE_COILS,
            0x0F,
            startAddress,
            quantity,
            byteCount,
            timeoutMs))
    {
        return false;
    }

    for (uint16_t i = 0; i < byteCount16; ++i)
    {
        _txBuffer[13 + i] = sourcePacked[i];
    }

    // Igual que las lecturas: mantener deterministas los bits de padding del
    // último byte y no transmitir basura fuera de quantity.
    if ((quantity & 0x07U) != 0)
    {
        const uint8_t validBits = (uint8_t)(quantity & 0x07U);
        _txBuffer[13 + byteCount16 - 1U] &=
            (uint8_t)((1U << validBits) - 1U);
    }

    return true;
}

bool JWPLC_ModbusTCPClientClass::requestWriteMultipleRegisters(
    uint16_t startAddress,
    uint16_t quantity,
    const uint16_t *source,
    uint32_t timeoutMs)
{
    if (source == nullptr || quantity == 0 || quantity > 123)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        return false;
    }

    const uint16_t byteCount16 = (uint16_t)(quantity * 2U);
    if (byteCount16 > 246U)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        return false;
    }

    const uint8_t byteCount = (uint8_t)byteCount16;

    if (!startWriteMultipleRequest(
            OP_WRITE_MULTIPLE_REGISTERS,
            0x10,
            startAddress,
            quantity,
            byteCount,
            timeoutMs))
    {
        return false;
    }

    for (uint16_t i = 0; i < quantity; ++i)
    {
        writeU16BE(&_txBuffer[13 + (i * 2U)], source[i]);
    }

    return true;
}
