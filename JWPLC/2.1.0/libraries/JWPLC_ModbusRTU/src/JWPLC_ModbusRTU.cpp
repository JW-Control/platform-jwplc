#include "JWPLC_ModbusRTU.h"

JWPLC_ModbusRTUClass JWPLC_ModbusRTU;

JWPLC_ModbusRTUClass::JWPLC_ModbusRTUClass()
    : _ready(false),
      _slaveId(JWPLC_MODBUS_RTU_DEFAULT_SLAVE_ID),
      _baud(JWPLC_MODBUS_RTU_DEFAULT_BAUD),
      _config(JWPLC_MODBUS_RTU_DEFAULT_CONFIG),
      _frameGapMs(5),
      _coils(nullptr),
      _coilCount(0),
      _discreteInputs(nullptr),
      _discreteInputCount(0),
      _holdingRegisters(nullptr),
      _holdingCount(0),
      _inputRegisters(nullptr),
      _inputCount(0),
      _validRequestSeen(false),
      _lastValidRequestMs(0),
      _coilWriteSeen(false),
      _lastCoilWriteMs(0),
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
      _masterRegisterDestination(nullptr),
      _masterBitDestination(nullptr),
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

bool JWPLC_ModbusRTUClass::begin(
    uint8_t slaveId,
    uint32_t baud,
    uint32_t config)
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
    _validRequestSeen = false;
    _lastValidRequestMs = 0;
    _coilWriteSeen = false;
    _lastCoilWriteMs = 0;
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

void JWPLC_ModbusRTUClass::setCoils(uint8_t *bits, uint16_t count)
{
    _coils = bits;
    _coilCount = bits == nullptr ? 0 : count;
}

uint16_t JWPLC_ModbusRTUClass::coilCount() const
{
    return _coilCount;
}

bool JWPLC_ModbusRTUClass::getCoil(uint16_t address, bool &value) const
{
    if (_coils == nullptr || address >= _coilCount)
    {
        return false;
    }

    value = packedBit(_coils, address);
    return true;
}

bool JWPLC_ModbusRTUClass::setCoil(uint16_t address, bool value)
{
    if (_coils == nullptr || address >= _coilCount)
    {
        return false;
    }

    setPackedBit(_coils, address, value);
    return true;
}

void JWPLC_ModbusRTUClass::setDiscreteInputs(
    const uint8_t *bits,
    uint16_t count)
{
    _discreteInputs = bits;
    _discreteInputCount = bits == nullptr ? 0 : count;
}

uint16_t JWPLC_ModbusRTUClass::discreteInputCount() const
{
    return _discreteInputCount;
}

bool JWPLC_ModbusRTUClass::getDiscreteInput(
    uint16_t address,
    bool &value) const
{
    if (_discreteInputs == nullptr || address >= _discreteInputCount)
    {
        return false;
    }

    value = packedBit(_discreteInputs, address);
    return true;
}

void JWPLC_ModbusRTUClass::setHoldingRegisters(
    uint16_t *registers,
    uint16_t count)
{
    _holdingRegisters = registers;
    _holdingCount = registers == nullptr ? 0 : count;
}

uint16_t JWPLC_ModbusRTUClass::holdingRegisterCount() const
{
    return _holdingCount;
}

bool JWPLC_ModbusRTUClass::getHoldingRegister(
    uint16_t address,
    uint16_t &value) const
{
    if (_holdingRegisters == nullptr || address >= _holdingCount)
    {
        return false;
    }

    value = _holdingRegisters[address];
    return true;
}

bool JWPLC_ModbusRTUClass::setHoldingRegister(
    uint16_t address,
    uint16_t value)
{
    if (_holdingRegisters == nullptr || address >= _holdingCount)
    {
        return false;
    }

    _holdingRegisters[address] = value;
    return true;
}

void JWPLC_ModbusRTUClass::setInputRegisters(
    const uint16_t *registers,
    uint16_t count)
{
    _inputRegisters = registers;
    _inputCount = registers == nullptr ? 0 : count;
}

uint16_t JWPLC_ModbusRTUClass::inputRegisterCount() const
{
    return _inputCount;
}

bool JWPLC_ModbusRTUClass::getInputRegister(
    uint16_t address,
    uint16_t &value) const
{
    if (_inputRegisters == nullptr || address >= _inputCount)
    {
        return false;
    }

    value = _inputRegisters[address];
    return true;
}

bool JWPLC_ModbusRTUClass::hasValidRequest() const
{
    return _validRequestSeen;
}

uint32_t JWPLC_ModbusRTUClass::lastValidRequestMs() const
{
    return _lastValidRequestMs;
}

bool JWPLC_ModbusRTUClass::hasCoilWrite() const
{
    return _coilWriteSeen;
}

uint32_t JWPLC_ModbusRTUClass::lastCoilWriteMs() const
{
    return _lastCoilWriteMs;
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

    if (_rxLength == 0 ||
        (uint32_t)(millis() - _lastByteMs) < _frameGapMs)
    {
        return;
    }

    uint16_t offset = 0;

    while (offset < _rxLength)
    {
        const uint16_t remaining = _rxLength - offset;
        const uint8_t *frame = &_rxBuffer[offset];
        uint16_t frameLength = 0;

        if (remaining >= 5)
        {
            const uint8_t functionCode = frame[1];

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
                case 0x01:
                case 0x02:
                case 0x03:
                case 0x04:
                {
                    if (remaining >= 8 && checkCRC(frame, 8))
                    {
                        frameLength = 8;
                        break;
                    }

                    const uint8_t byteCount = frame[2];
                    bool validResponseShape = byteCount > 0;

                    if (functionCode == 0x03 || functionCode == 0x04)
                    {
                        validResponseShape =
                            validResponseShape && ((byteCount & 0x01) == 0);
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

                case 0x05:
                case 0x06:
                    if (remaining >= 8 && checkCRC(frame, 8))
                    {
                        frameLength = 8;
                    }
                    break;

                case 0x0F:
                case 0x10:
                {
                    if (remaining >= 9)
                    {
                        const uint16_t quantity =
                            ((uint16_t)frame[4] << 8) | frame[5];
                        const uint8_t byteCount = frame[6];
                        bool validRequestShape = false;

                        if (functionCode == 0x10)
                        {
                            validRequestShape =
                                quantity >= 1 &&
                                quantity <= 123 &&
                                byteCount == quantity * 2U;
                        }
                        else
                        {
                            validRequestShape =
                                quantity >= 1 &&
                                quantity <= 1968 &&
                                byteCount ==
                                    (uint8_t)((quantity + 7U) / 8U);
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
            // Solo clasificar como CRC local un remanente que coincida
            // exactamente con la forma estructural de una request local.
            uint16_t localRequestLength = 0;

            if (remaining >= 2 &&
                (frame[0] == _slaveId || frame[0] == 0))
            {
                switch (frame[1])
                {
                case 0x01:
                case 0x02:
                case 0x03:
                case 0x04:
                case 0x05:
                case 0x06:
                    if (remaining == 8)
                    {
                        localRequestLength = 8;
                    }
                    break;

                case 0x0F:
                case 0x10:
                    if (remaining >= 7)
                    {
                        const uint16_t candidateLength =
                            (uint16_t)9 + frame[6];

                        if (candidateLength == remaining &&
                            candidateLength <= JWPLC_MODBUS_RTU_MAX_FRAME)
                        {
                            localRequestLength = candidateLength;
                        }
                    }
                    break;

                default:
                    break;
                }
            }

            if (localRequestLength > 0)
            {
                processServerFrame(frame, localRequestLength);
                offset += localRequestLength;
                continue;
            }

            // Trafico ajeno o tail ambiguo: no contaminar CRC del Slave local.
            break;
        }

        processServerFrame(frame, frameLength);
        offset += frameLength;
    }

    clearRxBuffer();
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
        expectedLength = 5;
    }
    else
    {
        switch (_masterOperation)
        {
        case JWPLC_MODBUS_MASTER_OP_READ_COILS:
        case JWPLC_MODBUS_MASTER_OP_READ_DISCRETE_INPUTS:
            expectedLength =
                (uint16_t)(5 + ((_masterQuantity + 7U) / 8U));
            break;

        case JWPLC_MODBUS_MASTER_OP_READ_HOLDING_REGISTERS:
        case JWPLC_MODBUS_MASTER_OP_READ_INPUT_REGISTERS:
            expectedLength =
                (uint16_t)(5 + _masterQuantity * 2U);
            break;

        case JWPLC_MODBUS_MASTER_OP_WRITE_SINGLE_COIL:
        case JWPLC_MODBUS_MASTER_OP_WRITE_SINGLE_REGISTER:
        case JWPLC_MODBUS_MASTER_OP_WRITE_MULTIPLE_COILS:
            expectedLength = 8;
            break;

        default:
            break;
        }
    }

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

bool JWPLC_ModbusRTUClass::requestReadCoils(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t *destinationPacked,
    uint32_t timeoutMs)
{
    return startReadBitsRequest(
        0x01,
        JWPLC_MODBUS_MASTER_OP_READ_COILS,
        targetSlaveId,
        startAddress,
        quantity,
        destinationPacked,
        timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestReadDiscreteInputs(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t *destinationPacked,
    uint32_t timeoutMs)
{
    return startReadBitsRequest(
        0x02,
        JWPLC_MODBUS_MASTER_OP_READ_DISCRETE_INPUTS,
        targetSlaveId,
        startAddress,
        quantity,
        destinationPacked,
        timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestReadHoldingRegisters(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    return startReadRegistersRequest(
        0x03,
        JWPLC_MODBUS_MASTER_OP_READ_HOLDING_REGISTERS,
        targetSlaveId,
        startAddress,
        quantity,
        destination,
        timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestReadInputRegisters(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    return startReadRegistersRequest(
        0x04,
        JWPLC_MODBUS_MASTER_OP_READ_INPUT_REGISTERS,
        targetSlaveId,
        startAddress,
        quantity,
        destination,
        timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestWriteSingleCoil(
    uint8_t targetSlaveId,
    uint16_t address,
    bool value,
    uint32_t timeoutMs)
{
    return startWriteSingleRequest(
        0x05,
        JWPLC_MODBUS_MASTER_OP_WRITE_SINGLE_COIL,
        targetSlaveId,
        address,
        value ? 0xFF00 : 0x0000,
        timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestWriteSingleRegister(
    uint8_t targetSlaveId,
    uint16_t address,
    uint16_t value,
    uint32_t timeoutMs)
{
    return startWriteSingleRequest(
        0x06,
        JWPLC_MODBUS_MASTER_OP_WRITE_SINGLE_REGISTER,
        targetSlaveId,
        address,
        value,
        timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestWriteMultipleCoils(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    const uint8_t *sourcePacked,
    uint32_t timeoutMs)
{
    if (masterBusy())
    {
        setError(JWPLC_MODBUS_BUSY);
        return false;
    }

    const uint16_t byteCount = (uint16_t)((quantity + 7U) / 8U);
    const uint16_t requestLength = (uint16_t)(9U + byteCount);

    if (!isReady() ||
        sourcePacked == nullptr ||
        targetSlaveId == 0 ||
        targetSlaveId > 247 ||
        quantity == 0 ||
        quantity > 1968 ||
        requestLength > JWPLC_MODBUS_RTU_MAX_FRAME ||
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

    _masterOperation = JWPLC_MODBUS_MASTER_OP_WRITE_MULTIPLE_COILS;
    _masterTargetSlaveId = targetSlaveId;
    _masterExpectedFunction = 0x0F;
    _masterStartAddress = startAddress;
    _masterQuantity = quantity;
    _masterRegisterDestination = nullptr;
    _masterBitDestination = nullptr;
    _masterWriteValue = 0;
    _masterTimeoutMs = timeoutMs;
    _masterResult = JWPLC_MODBUS_OK;
    _masterState = JWPLC_MODBUS_MASTER_WAIT_RESPONSE;

    uint8_t request[JWPLC_MODBUS_RTU_MAX_FRAME];
    request[0] = targetSlaveId;
    request[1] = 0x0F;
    request[2] = highByte(startAddress);
    request[3] = lowByte(startAddress);
    request[4] = highByte(quantity);
    request[5] = lowByte(quantity);
    request[6] = (uint8_t)byteCount;

    for (uint16_t i = 0; i < byteCount; i++)
    {
        request[7 + i] = sourcePacked[i];
    }

    if ((quantity & 0x07U) != 0)
    {
        const uint8_t validBits = (uint8_t)(quantity & 0x07U);
        request[7 + byteCount - 1] &= (uint8_t)((1U << validBits) - 1U);
    }

    appendCRC(request, 7 + byteCount);

    const size_t written = JWPLC_RS485.write(request, requestLength);

    if (written != requestLength)
    {
        completeMasterTransaction(JWPLC_MODBUS_TRANSPORT_ERROR);
        return false;
    }

    _stats.txFrames++;
    _masterStartMs = millis();
    clearError();
    return true;
}

bool JWPLC_ModbusRTUClass::startReadBitsRequest(
    uint8_t functionCode,
    JWPLCModbusMasterOperation operation,
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t *destinationPacked,
    uint32_t timeoutMs)
{
    if (masterBusy())
    {
        setError(JWPLC_MODBUS_BUSY);
        return false;
    }

    if (!isReady() ||
        destinationPacked == nullptr ||
        targetSlaveId == 0 ||
        targetSlaveId > 247 ||
        quantity == 0 ||
        quantity > 2000 ||
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

    _masterOperation = operation;
    _masterTargetSlaveId = targetSlaveId;
    _masterExpectedFunction = functionCode;
    _masterStartAddress = startAddress;
    _masterQuantity = quantity;
    _masterRegisterDestination = nullptr;
    _masterBitDestination = destinationPacked;
    _masterWriteValue = 0;
    _masterTimeoutMs = timeoutMs;
    _masterResult = JWPLC_MODBUS_OK;
    _masterState = JWPLC_MODBUS_MASTER_WAIT_RESPONSE;

    uint8_t request[8];
    request[0] = targetSlaveId;
    request[1] = functionCode;
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

bool JWPLC_ModbusRTUClass::startReadRegistersRequest(
    uint8_t functionCode,
    JWPLCModbusMasterOperation operation,
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

    _masterOperation = operation;
    _masterTargetSlaveId = targetSlaveId;
    _masterExpectedFunction = functionCode;
    _masterStartAddress = startAddress;
    _masterQuantity = quantity;
    _masterRegisterDestination = destination;
    _masterBitDestination = nullptr;
    _masterWriteValue = 0;
    _masterTimeoutMs = timeoutMs;
    _masterResult = JWPLC_MODBUS_OK;
    _masterState = JWPLC_MODBUS_MASTER_WAIT_RESPONSE;

    uint8_t request[8];
    request[0] = targetSlaveId;
    request[1] = functionCode;
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

bool JWPLC_ModbusRTUClass::startWriteSingleRequest(
    uint8_t functionCode,
    JWPLCModbusMasterOperation operation,
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

    _masterOperation = operation;
    _masterTargetSlaveId = targetSlaveId;
    _masterExpectedFunction = functionCode;
    _masterStartAddress = address;
    _masterQuantity = 1;
    _masterRegisterDestination = nullptr;
    _masterBitDestination = nullptr;
    _masterWriteValue = value;
    _masterTimeoutMs = timeoutMs;
    _masterResult = JWPLC_MODBUS_OK;
    _masterState = JWPLC_MODBUS_MASTER_WAIT_RESPONSE;

    uint8_t request[8];
    request[0] = targetSlaveId;
    request[1] = functionCode;
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
    if (!masterBusy())
    {
        resetMasterContext();
    }
}

bool JWPLC_ModbusRTUClass::finishSyncRequest(bool started)
{
    if (!started)
    {
        return false;
    }

    while (masterBusy())
    {
        task();
        yield();
    }

    const bool ok = masterSucceeded();

    if (masterDone())
    {
        clearMasterResult();
    }

    return ok;
}

bool JWPLC_ModbusRTUClass::readCoilsSync(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t *destinationPacked,
    uint32_t timeoutMs)
{
    return finishSyncRequest(requestReadCoils(
        targetSlaveId,
        startAddress,
        quantity,
        destinationPacked,
        timeoutMs));
}

bool JWPLC_ModbusRTUClass::readDiscreteInputsSync(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t *destinationPacked,
    uint32_t timeoutMs)
{
    return finishSyncRequest(requestReadDiscreteInputs(
        targetSlaveId,
        startAddress,
        quantity,
        destinationPacked,
        timeoutMs));
}

bool JWPLC_ModbusRTUClass::readHoldingRegistersSync(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    return finishSyncRequest(requestReadHoldingRegisters(
        targetSlaveId,
        startAddress,
        quantity,
        destination,
        timeoutMs));
}

bool JWPLC_ModbusRTUClass::readInputRegistersSync(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    return finishSyncRequest(requestReadInputRegisters(
        targetSlaveId,
        startAddress,
        quantity,
        destination,
        timeoutMs));
}

bool JWPLC_ModbusRTUClass::writeSingleCoilSync(
    uint8_t targetSlaveId,
    uint16_t address,
    bool value,
    uint32_t timeoutMs)
{
    return finishSyncRequest(requestWriteSingleCoil(
        targetSlaveId,
        address,
        value,
        timeoutMs));
}

bool JWPLC_ModbusRTUClass::writeSingleRegisterSync(
    uint8_t targetSlaveId,
    uint16_t address,
    uint16_t value,
    uint32_t timeoutMs)
{
    return finishSyncRequest(requestWriteSingleRegister(
        targetSlaveId,
        address,
        value,
        timeoutMs));
}

bool JWPLC_ModbusRTUClass::writeMultipleCoilsSync(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    const uint8_t *sourcePacked,
    uint32_t timeoutMs)
{
    return finishSyncRequest(requestWriteMultipleCoils(
        targetSlaveId,
        startAddress,
        quantity,
        sourcePacked,
        timeoutMs));
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
    _masterRegisterDestination = nullptr;
    _masterBitDestination = nullptr;
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
    case JWPLC_MODBUS_MASTER_OP_READ_COILS:
    case JWPLC_MODBUS_MASTER_OP_READ_DISCRETE_INPUTS:
    {
        if (_masterBitDestination == nullptr ||
            _masterQuantity == 0 ||
            _masterQuantity > 2000)
        {
            completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
            return false;
        }

        const uint16_t expectedByteCount =
            (uint16_t)((_masterQuantity + 7U) / 8U);
        const uint16_t expectedLength =
            (uint16_t)(5U + expectedByteCount);

        if (frame[2] != expectedByteCount || length != expectedLength)
        {
            completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
            return false;
        }

        for (uint16_t i = 0; i < expectedByteCount; i++)
        {
            _masterBitDestination[i] = frame[3 + i];
        }

        if ((_masterQuantity & 0x07U) != 0)
        {
            const uint8_t validBits =
                (uint8_t)(_masterQuantity & 0x07U);
            _masterBitDestination[expectedByteCount - 1] &=
                (uint8_t)((1U << validBits) - 1U);
        }

        completeMasterTransaction(JWPLC_MODBUS_OK);
        return true;
    }

    case JWPLC_MODBUS_MASTER_OP_READ_HOLDING_REGISTERS:
    case JWPLC_MODBUS_MASTER_OP_READ_INPUT_REGISTERS:
    {
        if (_masterRegisterDestination == nullptr ||
            _masterQuantity == 0 ||
            _masterQuantity > 125)
        {
            completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
            return false;
        }

        const uint16_t expectedByteCount = _masterQuantity * 2U;
        const uint16_t expectedLength =
            (uint16_t)(5U + expectedByteCount);

        if (frame[2] != expectedByteCount || length != expectedLength)
        {
            completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
            return false;
        }

        for (uint16_t i = 0; i < _masterQuantity; i++)
        {
            _masterRegisterDestination[i] =
                ((uint16_t)frame[3 + i * 2U] << 8) |
                frame[4 + i * 2U];
        }

        completeMasterTransaction(JWPLC_MODBUS_OK);
        return true;
    }

    case JWPLC_MODBUS_MASTER_OP_WRITE_SINGLE_COIL:
    case JWPLC_MODBUS_MASTER_OP_WRITE_SINGLE_REGISTER:
        if (length != 8 ||
            frame[2] != highByte(_masterStartAddress) ||
            frame[3] != lowByte(_masterStartAddress) ||
            frame[4] != highByte(_masterWriteValue) ||
            frame[5] != lowByte(_masterWriteValue))
        {
            completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
            return false;
        }

        completeMasterTransaction(JWPLC_MODBUS_OK);
        return true;

    case JWPLC_MODBUS_MASTER_OP_WRITE_MULTIPLE_COILS:
        if (length != 8 ||
            frame[2] != highByte(_masterStartAddress) ||
            frame[3] != lowByte(_masterStartAddress) ||
            frame[4] != highByte(_masterQuantity) ||
            frame[5] != lowByte(_masterQuantity))
        {
            completeMasterTransaction(JWPLC_MODBUS_INVALID_RESPONSE);
            return false;
        }

        completeMasterTransaction(JWPLC_MODBUS_OK);
        return true;

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

uint16_t JWPLC_ModbusRTUClass::crc16(
    const uint8_t *data,
    size_t length)
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

bool JWPLC_ModbusRTUClass::checkCRC(
    const uint8_t *frame,
    size_t length)
{
    if (frame == nullptr || length < 4)
    {
        return false;
    }

    const uint16_t received =
        (uint16_t)frame[length - 2] |
        ((uint16_t)frame[length - 1] << 8);
    const uint16_t calculated = crc16(frame, length - 2);

    return received == calculated;
}

void JWPLC_ModbusRTUClass::appendCRC(
    uint8_t *frame,
    size_t payloadLength)
{
    if (frame == nullptr)
    {
        return;
    }

    const uint16_t crc = crc16(frame, payloadLength);
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
        return "SERIAL_8N1";
    if (_config == SERIAL_8E1)
        return "SERIAL_8E1";
    if (_config == SERIAL_8O1)
        return "SERIAL_8O1";
    if (_config == SERIAL_8N2)
        return "SERIAL_8N2";
    if (_config == SERIAL_8E2)
        return "SERIAL_8E2";
    if (_config == SERIAL_8O2)
        return "SERIAL_8O2";

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

    out.print("Coils: ");
    out.println(_coilCount);

    out.print("Discrete inputs: ");
    out.println(_discreteInputCount);

    out.print("Holding registers: ");
    out.println(_holdingCount);

    out.print("Input registers: ");
    out.println(_inputCount);

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
    const bool broadcast = address == 0;

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
    bool handled = false;

    switch (functionCode)
    {
    case 0x01:
        handled = handleReadBits(
            frame, length, broadcast, 0x01, _coils, _coilCount);
        break;

    case 0x02:
        handled = handleReadBits(
            frame,
            length,
            broadcast,
            0x02,
            _discreteInputs,
            _discreteInputCount);
        break;

    case 0x03:
        handled = handleReadRegisters(
            frame,
            length,
            broadcast,
            0x03,
            _holdingRegisters,
            _holdingCount);
        break;

    case 0x04:
        handled = handleReadRegisters(
            frame,
            length,
            broadcast,
            0x04,
            _inputRegisters,
            _inputCount);
        break;

    case 0x05:
        handled = handleWriteSingleCoil(frame, length, broadcast);
        break;

    case 0x06:
        handled = handleWriteSingleRegister(frame, length, broadcast);
        break;

    case 0x0F:
        handled = handleWriteMultipleCoils(frame, length, broadcast);
        break;

    case 0x10:
        handled = handleWriteMultipleRegisters(frame, length, broadcast);
        break;

    default:
        if (!broadcast)
        {
            sendException(functionCode, JWPLC_MODBUS_EX_ILLEGAL_FUNCTION);
        }
        setError(JWPLC_MODBUS_UNSUPPORTED_FUNCTION);
        return false;
    }

    if (!handled)
    {
        return false;
    }

    _stats.requestsOk++;
    _validRequestSeen = true;
    _lastValidRequestMs = millis();
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

bool JWPLC_ModbusRTUClass::handleReadBits(
    const uint8_t *frame,
    uint16_t length,
    bool broadcast,
    uint8_t functionCode,
    const uint8_t *map,
    uint16_t count)
{
    if (broadcast)
    {
        return false;
    }

    if (length != 8)
    {
        sendException(functionCode, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (map == nullptr || count == 0)
    {
        sendException(functionCode, JWPLC_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    const uint16_t start = ((uint16_t)frame[2] << 8) | frame[3];
    const uint16_t quantity = ((uint16_t)frame[4] << 8) | frame[5];

    if (quantity == 0 || quantity > 2000)
    {
        sendException(functionCode, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (start >= count || (uint32_t)start + quantity > count)
    {
        sendException(functionCode, JWPLC_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    const uint16_t byteCount = (uint16_t)((quantity + 7U) / 8U);
    uint8_t response[JWPLC_MODBUS_RTU_MAX_FRAME] = {};
    response[0] = _slaveId;
    response[1] = functionCode;
    response[2] = (uint8_t)byteCount;

    for (uint16_t bit = 0; bit < quantity; bit++)
    {
        if (packedBit(map, start + bit))
        {
            response[3 + (bit / 8U)] |=
                (uint8_t)(1U << (bit & 0x07U));
        }
    }

    sendFrame(response, (uint16_t)(3U + byteCount));
    return true;
}

bool JWPLC_ModbusRTUClass::handleReadRegisters(
    const uint8_t *frame,
    uint16_t length,
    bool broadcast,
    uint8_t functionCode,
    const uint16_t *map,
    uint16_t count)
{
    if (broadcast)
    {
        return false;
    }

    if (length != 8)
    {
        sendException(functionCode, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (map == nullptr || count == 0)
    {
        sendException(functionCode, JWPLC_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    const uint16_t start = ((uint16_t)frame[2] << 8) | frame[3];
    const uint16_t quantity = ((uint16_t)frame[4] << 8) | frame[5];

    if (quantity == 0 || quantity > 125)
    {
        sendException(functionCode, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (start >= count || (uint32_t)start + quantity > count)
    {
        sendException(functionCode, JWPLC_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    uint8_t response[JWPLC_MODBUS_RTU_MAX_FRAME];
    response[0] = _slaveId;
    response[1] = functionCode;
    response[2] = (uint8_t)(quantity * 2U);

    for (uint16_t i = 0; i < quantity; i++)
    {
        const uint16_t value = map[start + i];
        response[3 + i * 2U] = highByte(value);
        response[4 + i * 2U] = lowByte(value);
    }

    sendFrame(response, (uint16_t)(3U + quantity * 2U));
    return true;
}

bool JWPLC_ModbusRTUClass::handleWriteSingleCoil(
    const uint8_t *frame,
    uint16_t length,
    bool broadcast)
{
    if (length != 8)
    {
        if (!broadcast)
        {
            sendException(0x05, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (_coils == nullptr || _coilCount == 0)
    {
        if (!broadcast)
        {
            sendException(0x05, JWPLC_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    const uint16_t address = ((uint16_t)frame[2] << 8) | frame[3];
    const uint16_t value = ((uint16_t)frame[4] << 8) | frame[5];

    if (address >= _coilCount)
    {
        if (!broadcast)
        {
            sendException(0x05, JWPLC_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (value != 0xFF00 && value != 0x0000)
    {
        if (!broadcast)
        {
            sendException(0x05, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    setPackedBit(_coils, address, value == 0xFF00);
    _coilWriteSeen = true;
    _lastCoilWriteMs = millis();

    if (!broadcast)
    {
        uint8_t response[8];
        for (uint8_t i = 0; i < 6; i++)
        {
            response[i] = frame[i];
        }
        sendFrame(response, 6);
    }

    return true;
}

bool JWPLC_ModbusRTUClass::handleWriteSingleRegister(
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
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (_holdingRegisters == nullptr || _holdingCount == 0)
    {
        if (!broadcast)
        {
            sendException(0x06, JWPLC_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    const uint16_t address = ((uint16_t)frame[2] << 8) | frame[3];
    const uint16_t value = ((uint16_t)frame[4] << 8) | frame[5];

    if (address >= _holdingCount)
    {
        if (!broadcast)
        {
            sendException(0x06, JWPLC_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
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

    return true;
}

bool JWPLC_ModbusRTUClass::handleWriteMultipleCoils(
    const uint8_t *frame,
    uint16_t length,
    bool broadcast)
{
    if (length < 10)
    {
        if (!broadcast)
        {
            sendException(0x0F, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (_coils == nullptr || _coilCount == 0)
    {
        if (!broadcast)
        {
            sendException(0x0F, JWPLC_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    const uint16_t start = ((uint16_t)frame[2] << 8) | frame[3];
    const uint16_t quantity = ((uint16_t)frame[4] << 8) | frame[5];
    const uint8_t byteCount = frame[6];
    const uint8_t expectedByteCount =
        (uint8_t)((quantity + 7U) / 8U);

    if (quantity == 0 ||
        quantity > 1968 ||
        byteCount != expectedByteCount ||
        length != (uint16_t)(9U + byteCount))
    {
        if (!broadcast)
        {
            sendException(0x0F, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (start >= _coilCount ||
        (uint32_t)start + quantity > _coilCount)
    {
        if (!broadcast)
        {
            sendException(0x0F, JWPLC_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    for (uint16_t bit = 0; bit < quantity; bit++)
    {
        const bool value =
            (frame[7 + (bit / 8U)] &
             (uint8_t)(1U << (bit & 0x07U))) != 0;
        setPackedBit(_coils, start + bit, value);
    }

    _coilWriteSeen = true;
    _lastCoilWriteMs = millis();

    if (!broadcast)
    {
        uint8_t response[8];
        response[0] = _slaveId;
        response[1] = 0x0F;
        response[2] = frame[2];
        response[3] = frame[3];
        response[4] = frame[4];
        response[5] = frame[5];
        sendFrame(response, 6);
    }

    return true;
}

bool JWPLC_ModbusRTUClass::handleWriteMultipleRegisters(
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
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (_holdingRegisters == nullptr || _holdingCount == 0)
    {
        if (!broadcast)
        {
            sendException(0x10, JWPLC_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    const uint16_t start = ((uint16_t)frame[2] << 8) | frame[3];
    const uint16_t quantity = ((uint16_t)frame[4] << 8) | frame[5];
    const uint8_t byteCount = frame[6];

    if (quantity == 0 ||
        quantity > 123 ||
        byteCount != quantity * 2U ||
        length != (uint16_t)(9U + byteCount))
    {
        if (!broadcast)
        {
            sendException(0x10, JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    if (start >= _holdingCount ||
        (uint32_t)start + quantity > _holdingCount)
    {
        if (!broadcast)
        {
            sendException(0x10, JWPLC_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        }
        setError(JWPLC_MODBUS_EXCEPTION);
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        _holdingRegisters[start + i] =
            ((uint16_t)frame[7 + i * 2U] << 8) |
            frame[8 + i * 2U];
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

    return true;
}

bool JWPLC_ModbusRTUClass::packedBit(
    const uint8_t *map,
    uint16_t address)
{
    if (map == nullptr)
    {
        return false;
    }

    return (map[address / 8U] &
            (uint8_t)(1U << (address & 0x07U))) != 0;
}

void JWPLC_ModbusRTUClass::setPackedBit(
    uint8_t *map,
    uint16_t address,
    bool value)
{
    if (map == nullptr)
    {
        return;
    }

    const uint8_t mask =
        (uint8_t)(1U << (address & 0x07U));

    if (value)
    {
        map[address / 8U] |= mask;
    }
    else
    {
        map[address / 8U] &= (uint8_t)~mask;
    }
}
