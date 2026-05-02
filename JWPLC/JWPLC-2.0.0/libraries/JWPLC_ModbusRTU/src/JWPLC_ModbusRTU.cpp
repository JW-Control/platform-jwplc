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
      _stats{0, 0, 0, 0, 0, 0}
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
    _ready = true;
    clearError();

    return true;
}

void JWPLC_ModbusRTUClass::end()
{
    _ready = false;
    JWPLC_RS485.end();
    clearRxBuffer();
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
            setError(JWPLC_MODBUS_BUFFER_OVERFLOW);
            return;
        }

        _rxBuffer[_rxLength++] = (uint8_t)value;
        _lastByteMs = millis();
    }

    if (_rxLength > 0 && (uint32_t)(millis() - _lastByteMs) >= _frameGapMs)
    {
        processServerFrame(_rxBuffer, _rxLength);
        clearRxBuffer();
    }
}

bool JWPLC_ModbusRTUClass::readHoldingRegisters(uint8_t targetSlaveId,
                                                uint16_t startAddress,
                                                uint16_t quantity,
                                                uint16_t *destination,
                                                uint32_t timeoutMs)
{
    if (!isReady() || destination == nullptr || targetSlaveId == 0 || quantity == 0 || quantity > 125)
    {
        setError(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }

    while (JWPLC_RS485.available() > 0)
    {
        JWPLC_RS485.read();
    }

    uint8_t request[8];
    request[0] = targetSlaveId;
    request[1] = 0x03;
    request[2] = highByte(startAddress);
    request[3] = lowByte(startAddress);
    request[4] = highByte(quantity);
    request[5] = lowByte(quantity);
    appendCRC(request, 6);

    JWPLC_RS485.write(request, sizeof(request));

    uint8_t response[JWPLC_MODBUS_RTU_MAX_FRAME];
    uint16_t responseLength = 0;

    if (!waitMasterResponse(response, sizeof(response), responseLength, timeoutMs))
    {
        _stats.masterTimeouts++;
        setError(JWPLC_MODBUS_TIMEOUT);
        return false;
    }

    if (responseLength < 5 || !checkCRC(response, responseLength))
    {
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

    if (response[1] != 0x03 || response[2] != quantity * 2 || responseLength != (uint16_t)(5 + quantity * 2))
    {
        setError(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        destination[i] = ((uint16_t)response[3 + i * 2] << 8) | response[4 + i * 2];
    }

    clearError();
    return true;
}

bool JWPLC_ModbusRTUClass::writeSingleRegister(uint8_t targetSlaveId,
                                               uint16_t address,
                                               uint16_t value,
                                               uint32_t timeoutMs)
{
    if (!isReady() || targetSlaveId == 0)
    {
        setError(JWPLC_MODBUS_INVALID_RESPONSE);
        return false;
    }

    while (JWPLC_RS485.available() > 0)
    {
        JWPLC_RS485.read();
    }

    uint8_t request[8];
    request[0] = targetSlaveId;
    request[1] = 0x06;
    request[2] = highByte(address);
    request[3] = lowByte(address);
    request[4] = highByte(value);
    request[5] = lowByte(value);
    appendCRC(request, 6);

    JWPLC_RS485.write(request, sizeof(request));

    uint8_t response[JWPLC_MODBUS_RTU_MAX_FRAME];
    uint16_t responseLength = 0;

    if (!waitMasterResponse(response, sizeof(response), responseLength, timeoutMs))
    {
        _stats.masterTimeouts++;
        setError(JWPLC_MODBUS_TIMEOUT);
        return false;
    }

    if (responseLength != 8 || !checkCRC(response, responseLength))
    {
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

    clearError();
    return true;
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

    uint16_t received = (uint16_t)frame[length - 2] | ((uint16_t)frame[length - 1] << 8);
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

bool JWPLC_ModbusRTUClass::processServerFrame(const uint8_t *frame, uint16_t length)
{
    if (frame == nullptr || length < 4)
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

    uint8_t address = frame[0];
    uint8_t functionCode = frame[1];
    bool broadcast = (address == 0);

    if (address != _slaveId && !broadcast)
    {
        return false;
    }

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

void JWPLC_ModbusRTUClass::sendException(uint8_t functionCode, uint8_t exceptionCode)
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

void JWPLC_ModbusRTUClass::sendFrame(uint8_t *frame, uint16_t payloadLength)
{
    appendCRC(frame, payloadLength);
    JWPLC_RS485.write(frame, payloadLength + 2);
    _stats.txFrames++;
}

void JWPLC_ModbusRTUClass::handleReadHoldingRegisters(const uint8_t *frame, uint16_t length, bool broadcast)
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

    if (start >= _holdingCount || (uint32_t)start + quantity > _holdingCount)
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

void JWPLC_ModbusRTUClass::handleWriteSingleRegister(const uint8_t *frame, uint16_t length, bool broadcast)
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

void JWPLC_ModbusRTUClass::handleWriteMultipleRegisters(const uint8_t *frame, uint16_t length, bool broadcast)
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

    if (quantity == 0 || quantity > 123 || byteCount != quantity * 2 || length != (uint16_t)(9 + byteCount))
    {
        if (!broadcast)
        {
            sendException(0x10, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        return;
    }

    if (start >= _holdingCount || (uint32_t)start + quantity > _holdingCount)
    {
        if (!broadcast)
        {
            sendException(0x10, JWPLC_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        }
        return;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        _holdingRegisters[start + i] = ((uint16_t)frame[7 + i * 2] << 8) | frame[8 + i * 2];
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

bool JWPLC_ModbusRTUClass::waitMasterResponse(uint8_t *buffer,
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

        if (receivedAny && (uint32_t)(millis() - lastByteMs) >= _frameGapMs)
        {
            return true;
        }

        yield();
    }

    return false;
}
