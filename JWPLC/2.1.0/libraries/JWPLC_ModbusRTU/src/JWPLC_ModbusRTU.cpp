#include "JWPLC_ModbusRTU.h"

JWPLC_ModbusRTUClass JWPLC_ModbusRTU;

JWPLC_ModbusRTUClass::JWPLC_ModbusRTUClass()
    : _ready(false),
      _slaveId(JWPLC_MODBUS_RTU_DEFAULT_SLAVE_ID),
      _baud(JWPLC_MODBUS_RTU_DEFAULT_BAUD),
      _config(JWPLC_MODBUS_RTU_DEFAULT_CONFIG),
      _frameGapMs(5),
      _holdingRegisters(nullptr),
      _holdingCount(0),
      _rxLength(0),
      _lastByteMs(0),
      _lastError(JWPLC_MODBUS_NOT_STARTED),
      _stats{0, 0, 0, 0, 0, 0},
      _masterState(JWPLC_MODBUS_MASTER_IDLE),
      _masterOperation(JWPLC_MODBUS_MASTER_OP_NONE),
      _masterResult(JWPLC_MODBUS_OK),
      _masterTargetSlaveId(0),
      _masterExpectedFunction(0),
      _masterStartAddress(0),
      _masterQuantity(0),
      _masterDestination(nullptr),
      _masterWriteValue(0),
      _masterStartMs(0),
      _masterTimeoutMs(0)
{
}

bool JWPLC_ModbusRTUClass::begin()
{
    return begin(JWPLC_MODBUS_RTU_DEFAULT_SLAVE_ID,
                 JWPLC_MODBUS_RTU_DEFAULT_BAUD,
                 JWPLC_MODBUS_RTU_DEFAULT_CONFIG);
}

bool JWPLC_ModbusRTUClass::begin(uint8_t slaveId)
{
    return begin(slaveId,
                 JWPLC_MODBUS_RTU_DEFAULT_BAUD,
                 JWPLC_MODBUS_RTU_DEFAULT_CONFIG);
}

bool JWPLC_ModbusRTUClass::begin(uint8_t slaveId, uint32_t baud, uint32_t config)
{
    if (slaveId == 0 || slaveId > 247)
    {
        _ready = false;
        setError(JWPLC_MODBUS_INVALID_SLAVE_ID);
        return false;
    }

    _slaveId = slaveId;
    _baud = baud;
    _config = config;

    if (!JWPLC_RS485.begin(_baud, _config))
    {
        _ready = false;
        setError(JWPLC_MODBUS_DISABLED);
        return false;
    }

    clearRxBuffer();
    resetMasterContext();
    _ready = true;
    clearError();

    return true;
}

void JWPLC_ModbusRTUClass::end()
{
    _ready = false;
    JWPLC_RS485.end();
    clearRxBuffer();
    resetMasterContext();
    setError(JWPLC_MODBUS_NOT_STARTED);
}

bool JWPLC_ModbusRTUClass::isReady() const
{
    return _ready && JWPLC_RS485.isReady();
}

uint8_t JWPLC_ModbusRTUClass::slaveId() const
{
    return _slaveId;
}

uint32_t JWPLC_ModbusRTUClass::baudRate() const
{
    return _baud;
}

uint32_t JWPLC_ModbusRTUClass::config() const
{
    return _config;
}

void JWPLC_ModbusRTUClass::setFrameGapMs(uint16_t gapMs)
{
    _frameGapMs = gapMs;
}

uint16_t JWPLC_ModbusRTUClass::frameGapMs() const
{
    return _frameGapMs;
}

void JWPLC_ModbusRTUClass::setHoldingRegisters(uint16_t *registers, uint16_t count)
{
    _holdingRegisters = registers;
    _holdingCount = count;
}

uint16_t JWPLC_ModbusRTUClass::holdingRegisterCount() const
{
    return _holdingCount;
}

bool JWPLC_ModbusRTUClass::getHoldingRegister(uint16_t address, uint16_t &value) const
{
    if (_holdingRegisters == nullptr || address >= _holdingCount)
    {
        return false;
    }

    value = _holdingRegisters[address];
    return true;
}

bool JWPLC_ModbusRTUClass::setHoldingRegister(uint16_t address, uint16_t value)
{
    if (_holdingRegisters == nullptr || address >= _holdingCount)
    {
        return false;
    }

    _holdingRegisters[address] = value;
    return true;
}

void JWPLC_ModbusRTUClass::task()
{
    poll();
}

void JWPLC_ModbusRTUClass::poll()
{
    if (!isReady())
    {
        return;
    }

    if (masterBusy())
    {
        pollMaster();
        return;
    }

    pollServer();
}

void JWPLC_ModbusRTUClass::pollServer()
{
    int bytesProcessed = 0;

    while (JWPLC_RS485.available() > 0 && bytesProcessed < 64)
    {
        int value = JWPLC_RS485.read();

        if (value < 0)
        {
            break;
        }

        bytesProcessed++;

        if (_rxLength >= JWPLC_MODBUS_RTU_MAX_FRAME)
        {
            clearRxBuffer();
            setError(JWPLC_MODBUS_BUFFER_OVERFLOW);
            return;
        }

        _rxBuffer[_rxLength++] = (uint8_t)value;
        _lastByteMs = millis();
    }

    if (_rxLength > 0 &&
        (uint32_t)(millis() - _lastByteMs) >= _frameGapMs)
    {
        uint16_t offset = 0;

        while (offset < _rxLength)
        {
            const uint16_t remaining = _rxLength - offset;
            const uint8_t *frame = &_rxBuffer[offset];

            uint16_t frameLength = 0;

            if (remaining >= 5)
            {
                const uint8_t functionCode = frame[1];

                // Respuesta Modbus de excepcion:
                // address + (function | 0x80) + exception + CRC16.
                if ((functionCode & 0x80) != 0)
                {
                    if (checkCRC(frame, 5))
                    {
                        frameLength = 5;
                    }
                }
                else
                {
                    switch (functionCode)
                    {
                    // Read Coils / Discrete Inputs /
                    // Holding Registers / Input Registers.
                    //
                    // Request:  8 bytes.
                    // Response: address + function + byteCount +
                    //           data + CRC16 = 5 + byteCount.
                    case 0x01:
                    case 0x02:
                    case 0x03:
                    case 0x04:
                    {
                        if (remaining >= 8 &&
                            checkCRC(frame, 8))
                        {
                            frameLength = 8;
                            break;
                        }

                        const uint8_t byteCount = frame[2];
                        bool validResponseShape = (byteCount > 0);

                        if (functionCode == 0x03 ||
                            functionCode == 0x04)
                        {
                            validResponseShape =
                                validResponseShape &&
                                ((byteCount & 0x01) == 0);
                        }

                        const uint16_t responseLength =
                            (uint16_t)5 + byteCount;

                        if (validResponseShape &&
                            responseLength <= remaining &&
                            checkCRC(frame, responseLength))
                        {
                            frameLength = responseLength;
                        }

                        break;
                    }

                    // Write Single Coil / Register.
                    // Request y response tienen exactamente 8 bytes.
                    case 0x05:
                    case 0x06:
                        if (remaining >= 8 &&
                            checkCRC(frame, 8))
                        {
                            frameLength = 8;
                        }
                        break;

                    // Write Multiple Coils / Registers.
                    // Request: 9 + byteCount. Response: 8 bytes.
                    case 0x0F:
                    case 0x10:
                    {
                        if (remaining >= 9)
                        {
                            const uint16_t quantity =
                                ((uint16_t)frame[4] << 8) |
                                frame[5];
                            const uint8_t byteCount = frame[6];
                            bool validRequestShape = false;

                            if (functionCode == 0x10)
                            {
                                validRequestShape =
                                    quantity >= 1 &&
                                    quantity <= 123 &&
                                    byteCount == quantity * 2;
                            }
                            else
                            {
                                validRequestShape =
                                    quantity >= 1 &&
                                    quantity <= 1968 &&
                                    byteCount ==
                                        (uint8_t)((quantity + 7) / 8);
                            }

                            const uint16_t requestLength =
                                (uint16_t)9 + byteCount;

                            if (validRequestShape &&
                                requestLength <= remaining &&
                                checkCRC(frame, requestLength))
                            {
                                frameLength = requestLength;
                            }
                        }

                        if (frameLength == 0 &&
                            remaining >= 8 &&
                            checkCRC(frame, 8))
                        {
                            frameLength = 8;
                        }

                        break;
                    }

                    default:
                        // Para una funcion no reconocida solo conservar el
                        // comportamiento previo si todo el remanente forma
                        // una ADU CRC-valida.
                        if (checkCRC(frame, remaining))
                        {
                            frameLength = remaining;
                        }
                        break;
                    }
                }
            }

            if (frameLength == 0)
            {
                // Corrupcion real o trama no delimitable: mantener el
                // diagnostico previo sobre el remanente.
                processServerFrame(frame, remaining);
                break;
            }

            processServerFrame(frame, frameLength);
            offset += frameLength;
        }

        clearRxBuffer();
    }
}

void JWPLC_ModbusRTUClass::pollMaster()
{
    while (JWPLC_RS485.available() > 0)
    {
        int value = JWPLC_RS485.read();

        if (value < 0)
        {
            break;
        }

        if (_rxLength >= JWPLC_MODBUS_RTU_MAX_FRAME)
        {
            clearRxBuffer();
            completeMasterTransaction(JWPLC_MODBUS_BUFFER_OVERFLOW);
            return;
        }

        _rxBuffer[_rxLength++] = (uint8_t)value;
        _lastByteMs = millis();
    }

    uint16_t expectedLength = 0;

    if (_rxLength >= 2 &&
        _rxBuffer[0] == _masterTargetSlaveId &&
        _rxBuffer[1] == (uint8_t)(_masterExpectedFunction | 0x80))
    {
        // Respuesta de excepcion Modbus:
        // address + function|0x80 + exception + CRC16.
        expectedLength = 5;
    }
    else
    {
        switch (_masterOperation)
        {
        case JWPLC_MODBUS_MASTER_OP_READ_HOLDING_REGISTERS:
            // address + function + byteCount + data + CRC16.
            expectedLength =
                (uint16_t)(5 + _masterQuantity * 2U);
            break;

        case JWPLC_MODBUS_MASTER_OP_WRITE_SINGLE_REGISTER:
            // Echo FC06 completo.
            expectedLength = 8;
            break;

        default:
            break;
        }
    }

    // Para operaciones conocidas, la longitud esperada manda. Un frame gap
    // intermedio no puede cerrar prematuramente una FC03 larga.
    if (expectedLength > 0)
    {
        if (_rxLength >= expectedLength)
        {
            processMasterFrame(_rxBuffer, expectedLength);
            clearRxBuffer();
            return;
        }
    }
    else if (_rxLength > 0 &&
             (uint32_t)(millis() - _lastByteMs) >= _frameGapMs)
    {
        // Fallback reservado para operaciones cuyo largo no puede derivarse.
        processMasterFrame(_rxBuffer, _rxLength);
        clearRxBuffer();
        return;
    }

    if ((uint32_t)(millis() - _masterStartMs) >= _masterTimeoutMs)
    {
        _stats.masterTimeouts++;
        clearRxBuffer();
        completeMasterTransaction(JWPLC_MODBUS_TIMEOUT);
    }
}

bool JWPLC_ModbusRTUClass::readHoldingRegistersSync(uint8_t targetSlaveId,
                                                    uint16_t startAddress,
                                                    uint16_t quantity,
                                                    uint16_t *destination,
                                                    uint32_t timeoutMs)
{
    if (masterBusy())
    {
        setError(JWPLC_MODBUS_BUSY);
        return false;
    }

    if (masterDone())
    {
        clearMasterResult();
    }

    if (!isReady() ||
        destination == nullptr ||
        targetSlaveId == 0 ||
        targetSlaveId > 247 ||
        quantity == 0 ||
        quantity > 125 ||
        timeoutMs == 0)
    {
        setError(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }

    drainRs485();
    clearRxBuffer();

    uint8_t request[8];
    request[0] = targetSlaveId;
    request[1] = 0x03;
    request[2] = highByte(startAddress);
    request[3] = lowByte(startAddress);
    request[4] = highByte(quantity);
    request[5] = lowByte(quantity);
    appendCRC(request, 6);

    const size_t written = JWPLC_RS485.write(request, sizeof(request));

    if (written != sizeof(request))
    {
        setError(JWPLC_MODBUS_TRANSPORT_ERROR);
        return false;
    }

    _stats.txFrames++;

    uint8_t response[JWPLC_MODBUS_RTU_MAX_FRAME];
    uint16_t responseLength = 0;

    if (!waitMasterResponse(response,
                            sizeof(response),
                            responseLength,
                            timeoutMs))
    {
        _stats.masterTimeouts++;
        setError(JWPLC_MODBUS_TIMEOUT);
        return false;
    }

    _stats.rxFrames++;

    if (responseLength < 5 || !checkCRC(response, responseLength))
    {
        _stats.crcErrors++;
        setError(JWPLC_MODBUS_CRC_ERROR);
        return false;
    }

    if (response[0] != targetSlaveId)
    {
        setError(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }

    if (response[1] & 0x80)
    {
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (response[1] != 0x03 ||
        response[2] != quantity * 2 ||
        responseLength != (uint16_t)(5 + quantity * 2))
    {
        setError(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        destination[i] =
            ((uint16_t)response[3 + i * 2] << 8) |
            response[4 + i * 2];
    }

    _stats.requestsOk++;
    clearError();
    return true;
}

bool JWPLC_ModbusRTUClass::writeSingleRegisterSync(uint8_t targetSlaveId,
                                                   uint16_t address,
                                                   uint16_t value,
                                                   uint32_t timeoutMs)
{
    if (masterBusy())
    {
        setError(JWPLC_MODBUS_BUSY);
        return false;
    }

    if (masterDone())
    {
        clearMasterResult();
    }

    if (!isReady() ||
        targetSlaveId == 0 ||
        targetSlaveId > 247 ||
        timeoutMs == 0)
    {
        setError(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }

    drainRs485();
    clearRxBuffer();

    uint8_t request[8];
    request[0] = targetSlaveId;
    request[1] = 0x06;
    request[2] = highByte(address);
    request[3] = lowByte(address);
    request[4] = highByte(value);
    request[5] = lowByte(value);
    appendCRC(request, 6);

    const size_t written = JWPLC_RS485.write(request, sizeof(request));

    if (written != sizeof(request))
    {
        setError(JWPLC_MODBUS_TRANSPORT_ERROR);
        return false;
    }

    _stats.txFrames++;

    uint8_t response[JWPLC_MODBUS_RTU_MAX_FRAME];
    uint16_t responseLength = 0;

    if (!waitMasterResponse(response,
                            sizeof(response),
                            responseLength,
                            timeoutMs))
    {
        _stats.masterTimeouts++;
        setError(JWPLC_MODBUS_TIMEOUT);
        return false;
    }

    _stats.rxFrames++;

    if (responseLength != 8 || !checkCRC(response, responseLength))
    {
        _stats.crcErrors++;
        setError(JWPLC_MODBUS_CRC_ERROR);
        return false;
    }

    for (uint8_t i = 0; i < 6; i++)
    {
        if (response[i] != request[i])
        {
            setError(JWPLC_MODBUS_INVALID_RESPONSE);
            return false;
        }
    }

    _stats.requestsOk++;
    clearError();
    return true;
}

bool JWPLC_ModbusRTUClass::requestReadHoldingRegisters(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    if (masterBusy())
    {
        setError(JWPLC_MODBUS_BUSY);
        return false;
    }

    if (!isReady() ||
        destination == nullptr ||
        targetSlaveId == 0 ||
        targetSlaveId > 247 ||
        quantity == 0 ||
        quantity > 125 ||
        timeoutMs == 0)
    {
        setError(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }

    if (masterDone())
    {
        clearMasterResult();
    }

    drainRs485();
    clearRxBuffer();

    _masterOperation = JWPLC_MODBUS_MASTER_OP_READ_HOLDING_REGISTERS;
    _masterTargetSlaveId = targetSlaveId;
    _masterExpectedFunction = 0x03;
    _masterStartAddress = startAddress;
    _masterQuantity = quantity;
    _masterDestination = destination;
    _masterWriteValue = 0;
    _masterTimeoutMs = timeoutMs;
    _masterResult = JWPLC_MODBUS_OK;
    _masterState = JWPLC_MODBUS_MASTER_WAIT_RESPONSE;

    uint8_t request[8];
    request[0] = targetSlaveId;
    request[1] = 0x03;
    request[2] = highByte(startAddress);
    request[3] = lowByte(startAddress);
    request[4] = highByte(quantity);
    request[5] = lowByte(quantity);
    appendCRC(request, 6);

    const size_t written = JWPLC_RS485.write(request, sizeof(request));

    if (written != sizeof(request))
    {
        completeMasterTransaction(JWPLC_MODBUS_TRANSPORT_ERROR);
        return false;
    }

    _stats.txFrames++;
    _masterStartMs = millis();
    clearError();
    return true;
}

bool JWPLC_ModbusRTUClass::requestWriteSingleRegister(
    uint8_t targetSlaveId,
    uint16_t address,
    uint16_t value,
    uint32_t timeoutMs)
{
    if (masterBusy())
    {
        setError(JWPLC_MODBUS_BUSY);
        return false;
    }

    if (!isReady() ||
        targetSlaveId == 0 ||
        targetSlaveId > 247 ||
        timeoutMs == 0)
    {
        setError(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }

    if (masterDone())
    {
        clearMasterResult();
    }

    drainRs485();
    clearRxBuffer();

    _masterOperation = JWPLC_MODBUS_MASTER_OP_WRITE_SINGLE_REGISTER;
    _masterTargetSlaveId = targetSlaveId;
    _masterExpectedFunction = 0x06;
    _masterStartAddress = address;
    _masterQuantity = 1;
    _masterDestination = nullptr;
    _masterWriteValue = value;
    _masterTimeoutMs = timeoutMs;
    _masterResult = JWPLC_MODBUS_OK;
    _masterState = JWPLC_MODBUS_MASTER_WAIT_RESPONSE;

    uint8_t request[8];
    request[0] = targetSlaveId;
    request[1] = 0x06;
    request[2] = highByte(address);
    request[3] = lowByte(address);
    request[4] = highByte(value);
    request[5] = lowByte(value);
    appendCRC(request, 6);

    const size_t written = JWPLC_RS485.write(request, sizeof(request));

    if (written != sizeof(request))
    {
        completeMasterTransaction(JWPLC_MODBUS_TRANSPORT_ERROR);
        return false;
    }

    _stats.txFrames++;
    _masterStartMs = millis();
    clearError();
    return true;
}

bool JWPLC_ModbusRTUClass::masterBusy() const
{
    return _masterState == JWPLC_MODBUS_MASTER_WAIT_RESPONSE;
}

bool JWPLC_ModbusRTUClass::masterDone() const
{
    return _masterState == JWPLC_MODBUS_MASTER_DONE ||
           _masterState == JWPLC_MODBUS_MASTER_ERROR;
}

bool JWPLC_ModbusRTUClass::masterSucceeded() const
{
    return _masterState == JWPLC_MODBUS_MASTER_DONE &&
           _masterResult == JWPLC_MODBUS_OK;
}

JWPLCModbusMasterState JWPLC_ModbusRTUClass::masterState() const
{
    return _masterState;
}

JWPLCModbusRTUError JWPLC_ModbusRTUClass::masterResult() const
{
    return _masterResult;
}

void JWPLC_ModbusRTUClass::clearMasterResult()
{
    if (masterBusy())
    {
        return;
    }

    resetMasterContext();
}

void JWPLC_ModbusRTUClass::resetMasterContext()
{
    _masterState = JWPLC_MODBUS_MASTER_IDLE;
    _masterOperation = JWPLC_MODBUS_MASTER_OP_NONE;
    _masterResult = JWPLC_MODBUS_OK;
    _masterTargetSlaveId = 0;
    _masterExpectedFunction = 0;
    _masterStartAddress = 0;
    _masterQuantity = 0;
    _masterDestination = nullptr;
    _masterWriteValue = 0;
    _masterStartMs = 0;
    _masterTimeoutMs = 0;
}

void JWPLC_ModbusRTUClass::completeMasterTransaction(
    JWPLCModbusRTUError result)
{
    _masterResult = result;

    if (result == JWPLC_MODBUS_OK)
    {
        _masterState = JWPLC_MODBUS_MASTER_DONE;
        _stats.requestsOk++;
        clearError();
    }
    else
    {
        _masterState = JWPLC_MODBUS_MASTER_ERROR;
        setError(result);
    }
}

bool JWPLC_ModbusRTUClass::processMasterFrame(
    const uint8_t *frame,
    uint16_t length)
{
    _stats.rxFrames++;

    if (frame == nullptr || length < 5)
    {
        completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }

    if (!checkCRC(frame, length))
    {
        _stats.crcErrors++;
        completeMasterTransaction(JWPLC_MODBUS_CRC_ERROR);
        return false;
    }

    if (frame[0] != _masterTargetSlaveId)
    {
        completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }

    if (frame[1] == (uint8_t)(_masterExpectedFunction | 0x80))
    {
        completeMasterTransaction(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (frame[1] != _masterExpectedFunction)
    {
        completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }

    switch (_masterOperation)
    {
    case JWPLC_MODBUS_MASTER_OP_READ_HOLDING_REGISTERS:
    {
        if (_masterDestination == nullptr ||
            _masterQuantity == 0 ||
            _masterQuantity > 125)
        {
            completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
            return false;
        }

        const uint16_t expectedByteCount = _masterQuantity * 2;
        const uint16_t expectedLength = 5 + expectedByteCount;

        if (frame[2] != expectedByteCount || length != expectedLength)
        {
            completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
            return false;
        }

        for (uint16_t i = 0; i < _masterQuantity; i++)
        {
            _masterDestination[i] =
                ((uint16_t)frame[3 + i * 2] << 8) |
                frame[4 + i * 2];
        }

        completeMasterTransaction(JWPLC_MODBUS_OK);
        return true;
    }

    case JWPLC_MODBUS_MASTER_OP_WRITE_SINGLE_REGISTER:
    {
        if (length != 8)
        {
            completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
            return false;
        }

        if (frame[2] != highByte(_masterStartAddress) ||
            frame[3] != lowByte(_masterStartAddress) ||
            frame[4] != highByte(_masterWriteValue) ||
            frame[5] != lowByte(_masterWriteValue))
        {
            completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
            return false;
        }

        completeMasterTransaction(JWPLC_MODBUS_OK);
        return true;
    }

    default:
        completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }
}

void JWPLC_ModbusRTUClass::drainRs485()
{
    while (JWPLC_RS485.available() > 0)
    {
        JWPLC_RS485.read();
    }
}

uint16_t JWPLC_ModbusRTUClass::crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

bool JWPLC_ModbusRTUClass::checkCRC(const uint8_t *frame, size_t length)
{
    if (frame == nullptr || length < 4)
    {
        return false;
    }

    uint16_t received =
        (uint16_t)frame[length - 2] |
        ((uint16_t)frame[length - 1] << 8);
    uint16_t calculated = crc16(frame, length - 2);

    return received == calculated;
}

void JWPLC_ModbusRTUClass::appendCRC(uint8_t *frame, size_t payloadLength)
{
    if (frame == nullptr)
    {
        return;
    }

    uint16_t crc = crc16(frame, payloadLength);
    frame[payloadLength] = lowByte(crc);
    frame[payloadLength + 1] = highByte(crc);
}

JWPLCModbusRTUError JWPLC_ModbusRTUClass::lastError() const
{
    return _lastError;
}

const char *JWPLC_ModbusRTUClass::lastErrorString() const
{
    switch (_lastError)
    {
    case JWPLC_MODBUS_OK:
        return "OK";
    case JWPLC_MODBUS_DISABLED:
        return "Modbus disabled";
    case JWPLC_MODBUS_NOT_STARTED:
        return "Modbus not started";
    case JWPLC_MODBUS_INVALID_SLAVE_ID:
        return "Invalid slave ID";
    case JWPLC_MODBUS_INVALID_REGISTER_MAP:
        return "Invalid register map";
    case JWPLC_MODBUS_TIMEOUT:
        return "Timeout";
    case JWPLC_MODBUS_CRC_ERROR:
        return "CRC error";
    case JWPLC_MODBUS_EXCEPTION:
        return "Modbus exception";
    case JWPLC_MODBUS_INVALID_RESPONSE:
        return "Invalid response";
    case JWPLC_MODBUS_BUFFER_OVERFLOW:
        return "Buffer overflow";
    case JWPLC_MODBUS_UNSUPPORTED_FUNCTION:
        return "Unsupported function";
    case JWPLC_MODBUS_BUSY:
        return "Master busy";
    case JWPLC_MODBUS_TRANSPORT_ERROR:
        return "Transport error";
    default:
        return "Unknown Modbus error";
    }
}

const char *JWPLC_ModbusRTUClass::configString() const
{
    if (_config == SERIAL_8N1)
    {
        return "SERIAL_8N1";
    }
    if (_config == SERIAL_8E1)
    {
        return "SERIAL_8E1";
    }
    if (_config == SERIAL_8O1)
    {
        return "SERIAL_8O1";
    }
    if (_config == SERIAL_8N2)
    {
        return "SERIAL_8N2";
    }
    if (_config == SERIAL_8E2)
    {
        return "SERIAL_8E2";
    }
    if (_config == SERIAL_8O2)
    {
        return "SERIAL_8O2";
    }

    return "CUSTOM";
}

const JWPLCModbusRTUStats &JWPLC_ModbusRTUClass::stats() const
{
    return _stats;
}

void JWPLC_ModbusRTUClass::resetStats()
{
    _stats = {0, 0, 0, 0, 0, 0};
}

void JWPLC_ModbusRTUClass::printStatus(Print &out) const
{
    out.print("Modbus RTU ready: ");
    out.println(isReady() ? "yes" : "no");

    out.print("Slave ID: ");
    out.println(_slaveId);

    out.print("Baud: ");
    out.println(_baud);

    out.print("Config: ");
    out.println(configString());

    out.print("Frame gap ms: ");
    out.println(_frameGapMs);

    out.print("Holding registers: ");
    out.println(_holdingCount);

    out.print("Last error: ");
    out.println(lastErrorString());
}

void JWPLC_ModbusRTUClass::clearRxBuffer()
{
    _rxLength = 0;
}

void JWPLC_ModbusRTUClass::setError(JWPLCModbusRTUError error)
{
    _lastError = error;
}

void JWPLC_ModbusRTUClass::clearError()
{
    _lastError = JWPLC_MODBUS_OK;
}

bool JWPLC_ModbusRTUClass::processServerFrame(
    const uint8_t *frame,
    uint16_t length)
{
    if (frame == nullptr || length < 4)
    {
        return false;
    }

    const uint8_t address = frame[0];
    const bool broadcast = (address == 0);

    // En un bus multidrop todos los nodos reciben fisicamente el trafico.
    // Las tramas destinadas a otro Slave no deben contaminar crcErrors,
    // rxFrames ni lastError del nodo local.
    if (address != _slaveId && !broadcast)
    {
        return false;
    }

    _stats.rxFrames++;

    if (!checkCRC(frame, length))
    {
        _stats.crcErrors++;
        setError(JWPLC_MODBUS_CRC_ERROR);
        return false;
    }

    const uint8_t functionCode = frame[1];

    switch (functionCode)
    {
    case 0x03:
        handleReadHoldingRegisters(frame, length, broadcast);
        break;

    case 0x06:
        handleWriteSingleRegister(frame, length, broadcast);
        break;

    case 0x10:
        handleWriteMultipleRegisters(frame, length, broadcast);
        break;

    default:
        if (!broadcast)
        {
            sendException(functionCode, JWPLC_MODBUS_EX_ILLEGAL_FUNCTION);
        }
        setError(JWPLC_MODBUS_UNSUPPORTED_FUNCTION);
        return false;
    }

    _stats.requestsOk++;
    clearError();
    return true;
}

void JWPLC_ModbusRTUClass::sendException(
    uint8_t functionCode,
    uint8_t exceptionCode)
{
    uint8_t response[5];
    response[0] = _slaveId;
    response[1] = functionCode | 0x80;
    response[2] = exceptionCode;
    appendCRC(response, 3);

    JWPLC_RS485.write(response, sizeof(response));

    _stats.exceptionsSent++;
    _stats.txFrames++;
}

void JWPLC_ModbusRTUClass::sendFrame(
    uint8_t *frame,
    uint16_t payloadLength)
{
    appendCRC(frame, payloadLength);
    JWPLC_RS485.write(frame, payloadLength + 2);
    _stats.txFrames++;
}

void JWPLC_ModbusRTUClass::handleReadHoldingRegisters(
    const uint8_t *frame,
    uint16_t length,
    bool broadcast)
{
    if (broadcast)
    {
        return;
    }

    if (length != 8)
    {
        sendException(0x03, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    if (_holdingRegisters == nullptr || _holdingCount == 0)
    {
        sendException(0x03, JWPLC_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        return;
    }

    uint16_t start = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t quantity = ((uint16_t)frame[4] << 8) | frame[5];

    if (quantity == 0 || quantity > 125)
    {
        sendException(0x03, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    if (start >= _holdingCount ||
        (uint32_t)start + quantity > _holdingCount)
    {
        sendException(0x03, JWPLC_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return;
    }

    uint8_t response[JWPLC_MODBUS_RTU_MAX_FRAME];
    response[0] = _slaveId;
    response[1] = 0x03;
    response[2] = quantity * 2;

    for (uint16_t i = 0; i < quantity; i++)
    {
        uint16_t value = _holdingRegisters[start + i];
        response[3 + i * 2] = highByte(value);
        response[4 + i * 2] = lowByte(value);
    }

    sendFrame(response, 3 + quantity * 2);
}

void JWPLC_ModbusRTUClass::handleWriteSingleRegister(
    const uint8_t *frame,
    uint16_t length,
    bool broadcast)
{
    if (length != 8)
    {
        if (!broadcast)
        {
            sendException(0x06, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        return;
    }

    if (_holdingRegisters == nullptr || _holdingCount == 0)
    {
        if (!broadcast)
        {
            sendException(0x06, JWPLC_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        }
        return;
    }

    uint16_t address = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t value = ((uint16_t)frame[4] << 8) | frame[5];

    if (address >= _holdingCount)
    {
        if (!broadcast)
        {
            sendException(0x06, JWPLC_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        }
        return;
    }

    _holdingRegisters[address] = value;

    if (!broadcast)
    {
        uint8_t response[8];

        for (uint8_t i = 0; i < 6; i++)
        {
            response[i] = frame[i];
        }

        sendFrame(response, 6);
    }
}

void JWPLC_ModbusRTUClass::handleWriteMultipleRegisters(
    const uint8_t *frame,
    uint16_t length,
    bool broadcast)
{
    if (length < 9)
    {
        if (!broadcast)
        {
            sendException(0x10, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        return;
    }

    if (_holdingRegisters == nullptr || _holdingCount == 0)
    {
        if (!broadcast)
        {
            sendException(0x10, JWPLC_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        }
        return;
    }

    uint16_t start = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t quantity = ((uint16_t)frame[4] << 8) | frame[5];
    uint8_t byteCount = frame[6];

    if (quantity == 0 ||
        quantity > 123 ||
        byteCount != quantity * 2 ||
        length != (uint16_t)(9 + byteCount))
    {
        if (!broadcast)
        {
            sendException(0x10, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        return;
    }

    if (start >= _holdingCount ||
        (uint32_t)start + quantity > _holdingCount)
    {
        if (!broadcast)
        {
            sendException(0x10, JWPLC_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        }
        return;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        _holdingRegisters[start + i] =
            ((uint16_t)frame[7 + i * 2] << 8) |
            frame[8 + i * 2];
    }

    if (!broadcast)
    {
        uint8_t response[8];
        response[0] = _slaveId;
        response[1] = 0x10;
        response[2] = frame[2];
        response[3] = frame[3];
        response[4] = frame[4];
        response[5] = frame[5];

        sendFrame(response, 6);
    }
}

bool JWPLC_ModbusRTUClass::waitMasterResponse(
    uint8_t *buffer,
    uint16_t maxLength,
    uint16_t &length,
    uint32_t timeoutMs)
{
    if (buffer == nullptr || maxLength == 0)
    {
        length = 0;
        return false;
    }

    length = 0;
    uint32_t startMs = millis();
    uint32_t lastByteMs = 0;
    bool receivedAny = false;

    while ((uint32_t)(millis() - startMs) < timeoutMs)
    {
        while (JWPLC_RS485.available() > 0)
        {
            int value = JWPLC_RS485.read();

            if (value < 0)
            {
                break;
            }

            if (length >= maxLength)
            {
                setError(JWPLC_MODBUS_BUFFER_OVERFLOW);
                return false;
            }

            buffer[length++] = (uint8_t)value;
            lastByteMs = millis();
            receivedAny = true;
        }

        if (receivedAny &&
            (uint32_t)(millis() - lastByteMs) >= _frameGapMs)
        {
            return true;
        }

        yield();
    }

    return false;
}
