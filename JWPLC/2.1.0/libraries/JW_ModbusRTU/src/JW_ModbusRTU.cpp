#include "JW_ModbusRTU.h"
#include <string.h>

static constexpr uint32_t JW_MODBUS_FAST_PARTIAL_HOLD_US = 15000UL;

static constexpr uint16_t JW_MODBUS_CRC16_TABLE[256] = {
    0x0000U, 0xC0C1U, 0xC181U, 0x0140U, 0xC301U, 0x03C0U, 0x0280U, 0xC241U,
    0xC601U, 0x06C0U, 0x0780U, 0xC741U, 0x0500U, 0xC5C1U, 0xC481U, 0x0440U,
    0xCC01U, 0x0CC0U, 0x0D80U, 0xCD41U, 0x0F00U, 0xCFC1U, 0xCE81U, 0x0E40U,
    0x0A00U, 0xCAC1U, 0xCB81U, 0x0B40U, 0xC901U, 0x09C0U, 0x0880U, 0xC841U,
    0xD801U, 0x18C0U, 0x1980U, 0xD941U, 0x1B00U, 0xDBC1U, 0xDA81U, 0x1A40U,
    0x1E00U, 0xDEC1U, 0xDF81U, 0x1F40U, 0xDD01U, 0x1DC0U, 0x1C80U, 0xDC41U,
    0x1400U, 0xD4C1U, 0xD581U, 0x1540U, 0xD701U, 0x17C0U, 0x1680U, 0xD641U,
    0xD201U, 0x12C0U, 0x1380U, 0xD341U, 0x1100U, 0xD1C1U, 0xD081U, 0x1040U,
    0xF001U, 0x30C0U, 0x3180U, 0xF141U, 0x3300U, 0xF3C1U, 0xF281U, 0x3240U,
    0x3600U, 0xF6C1U, 0xF781U, 0x3740U, 0xF501U, 0x35C0U, 0x3480U, 0xF441U,
    0x3C00U, 0xFCC1U, 0xFD81U, 0x3D40U, 0xFF01U, 0x3FC0U, 0x3E80U, 0xFE41U,
    0xFA01U, 0x3AC0U, 0x3B80U, 0xFB41U, 0x3900U, 0xF9C1U, 0xF881U, 0x3840U,
    0x2800U, 0xE8C1U, 0xE981U, 0x2940U, 0xEB01U, 0x2BC0U, 0x2A80U, 0xEA41U,
    0xEE01U, 0x2EC0U, 0x2F80U, 0xEF41U, 0x2D00U, 0xEDC1U, 0xEC81U, 0x2C40U,
    0xE401U, 0x24C0U, 0x2580U, 0xE541U, 0x2700U, 0xE7C1U, 0xE681U, 0x2640U,
    0x2200U, 0xE2C1U, 0xE381U, 0x2340U, 0xE101U, 0x21C0U, 0x2080U, 0xE041U,
    0xA001U, 0x60C0U, 0x6180U, 0xA141U, 0x6300U, 0xA3C1U, 0xA281U, 0x6240U,
    0x6600U, 0xA6C1U, 0xA781U, 0x6740U, 0xA501U, 0x65C0U, 0x6480U, 0xA441U,
    0x6C00U, 0xACC1U, 0xAD81U, 0x6D40U, 0xAF01U, 0x6FC0U, 0x6E80U, 0xAE41U,
    0xAA01U, 0x6AC0U, 0x6B80U, 0xAB41U, 0x6900U, 0xA9C1U, 0xA881U, 0x6840U,
    0x7800U, 0xB8C1U, 0xB981U, 0x7940U, 0xBB01U, 0x7BC0U, 0x7A80U, 0xBA41U,
    0xBE01U, 0x7EC0U, 0x7F80U, 0xBF41U, 0x7D00U, 0xBDC1U, 0xBC81U, 0x7C40U,
    0xB401U, 0x74C0U, 0x7580U, 0xB541U, 0x7700U, 0xB7C1U, 0xB681U, 0x7640U,
    0x7200U, 0xB2C1U, 0xB381U, 0x7340U, 0xB101U, 0x71C0U, 0x7080U, 0xB041U,
    0x5000U, 0x90C1U, 0x9181U, 0x5140U, 0x9301U, 0x53C0U, 0x5280U, 0x9241U,
    0x9601U, 0x56C0U, 0x5780U, 0x9741U, 0x5500U, 0x95C1U, 0x9481U, 0x5440U,
    0x9C01U, 0x5CC0U, 0x5D80U, 0x9D41U, 0x5F00U, 0x9FC1U, 0x9E81U, 0x5E40U,
    0x5A00U, 0x9AC1U, 0x9B81U, 0x5B40U, 0x9901U, 0x59C0U, 0x5880U, 0x9841U,
    0x8801U, 0x48C0U, 0x4980U, 0x8941U, 0x4B00U, 0x8BC1U, 0x8A81U, 0x4A40U,
    0x4E00U, 0x8EC1U, 0x8F81U, 0x4F40U, 0x8D01U, 0x4DC0U, 0x4C80U, 0x8C41U,
    0x4400U, 0x84C1U, 0x8581U, 0x4540U, 0x8701U, 0x47C0U, 0x4680U, 0x8641U,
    0x8201U, 0x42C0U, 0x4380U, 0x8341U, 0x4100U, 0x81C1U, 0x8081U, 0x4040U,
};

bool JW_ModbusRTU::_crcLookupEnabled = false;


JW_ModbusRTU::JW_ModbusRTU(JW_RS485 &port)
    : _port(&port),
      _ready(false),
      _slaveId(JW_MODBUS_RTU_DEFAULT_SLAVE_ID),
      _baud(JW_MODBUS_RTU_DEFAULT_BAUD),
      _config(JW_MODBUS_RTU_DEFAULT_CONFIG),
      _frameGapMs(5),
      _frameGapUs(5000UL),
      _motor(JW_MODBUS_ASYNC),
      _queuedTxEnabled(true),
      _bulkRxEnabled(false),
      _earlyServerDispatchEnabled(false),
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
      _lastByteUs(0),
      _lastError(JW_MODBUS_NOT_STARTED),
      _stats{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      _masterState(JW_MODBUS_MASTER_IDLE),
      _masterOperation(JW_MODBUS_MASTER_OP_NONE),
      _masterResult(JW_MODBUS_OK),
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

bool JW_ModbusRTU::begin()
{
    return begin(JW_MODBUS_RTU_DEFAULT_SLAVE_ID,
                 JW_MODBUS_RTU_DEFAULT_BAUD,
                 JW_MODBUS_RTU_DEFAULT_CONFIG);
}

bool JW_ModbusRTU::begin(uint8_t slaveId)
{
    return begin(slaveId,
                 JW_MODBUS_RTU_DEFAULT_BAUD,
                 JW_MODBUS_RTU_DEFAULT_CONFIG);
}

bool JW_ModbusRTU::begin(
    uint8_t slaveId,
    uint32_t baud,
    uint32_t config)
{
    if (slaveId == 0 || slaveId > 247)
    {
        _ready = false;
        setError(JW_MODBUS_INVALID_SLAVE_ID);
        return false;
    }

    _slaveId = slaveId;
    _baud = baud;
    _config = config;

    if (!_port->begin(_baud, _config))
    {
        _ready = false;
        setError(JW_MODBUS_DISABLED);
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

void JW_ModbusRTU::end()
{
    _ready = false;
    _port->end();
    clearRxBuffer();
    resetMasterContext();
    setError(JW_MODBUS_NOT_STARTED);
}

JW_RS485 &JW_ModbusRTU::port()
{
    return *_port;
}

bool JW_ModbusRTU::isReady() const
{
    return _ready && _port->isReady();
}

uint8_t JW_ModbusRTU::slaveId() const
{
    return _slaveId;
}

uint32_t JW_ModbusRTU::baudRate() const
{
    return _baud;
}

uint32_t JW_ModbusRTU::effectiveBaudRate() const
{
    return _port->effectiveBaudRate();
}

uint32_t JW_ModbusRTU::config() const
{
    return _config;
}

bool JW_ModbusRTU::motor(JWModbusMotor mode)
{
    if (masterBusy())
    {
        setError(JW_MODBUS_BUSY);
        return false;
    }

    if (mode != JW_MODBUS_ASYNC && mode != JW_MODBUS_SYNC)
    {
        setError(JW_MODBUS_INVALID_RESPONSE);
        return false;
    }

    _motor = mode;

    // El motor ASYNC aprovecha la ruta TX encolada cuando el hardware
    // AutoDirection la soporta. SYNC conserva el transporte historico.
    _queuedTxEnabled = (_motor == JW_MODBUS_ASYNC);

    clearError();
    return true;
}

JWModbusMotor JW_ModbusRTU::motor() const
{
    return _motor;
}

bool JW_ModbusRTU::asyncMotor() const
{
    return _motor == JW_MODBUS_ASYNC;
}

void JW_ModbusRTU::setFrameGapMs(uint16_t gapMs)
{
    _frameGapMs = gapMs;
    _frameGapUs = (uint32_t)gapMs * 1000UL;
}

uint16_t JW_ModbusRTU::frameGapMs() const
{
    return _frameGapMs;
}

void JW_ModbusRTU::setFrameGapUs(uint32_t gapUs)
{
    _frameGapUs = gapUs;

    uint32_t roundedMs = gapUs / 1000UL;

    if ((gapUs % 1000UL) != 0UL &&
        roundedMs < 0xFFFFUL)
    {
        ++roundedMs;
    }

    if (roundedMs > 0xFFFFUL)
    {
        roundedMs = 0xFFFFUL;
    }

    _frameGapMs = (uint16_t)roundedMs;
}

uint32_t JW_ModbusRTU::frameGapUs() const
{
    return _frameGapUs;
}

void JW_ModbusRTU::setQueuedTxEnabled(bool enabled)
{
    _queuedTxEnabled = enabled;
}

bool JW_ModbusRTU::queuedTxEnabled() const
{
    return _queuedTxEnabled;
}

bool JW_ModbusRTU::queuedTxActive() const
{
    return _queuedTxEnabled &&
           _port->queuedWriteSupported();
}

void JW_ModbusRTU::setBulkRxEnabled(bool enabled)
{
    _bulkRxEnabled = enabled;
}

bool JW_ModbusRTU::bulkRxEnabled() const
{
    return _bulkRxEnabled;
}

void JW_ModbusRTU::setEarlyServerDispatchEnabled(bool enabled)
{
    _earlyServerDispatchEnabled = enabled;
}

bool JW_ModbusRTU::earlyServerDispatchEnabled() const
{
    return _earlyServerDispatchEnabled;
}

void JW_ModbusRTU::setCoils(uint8_t *bits, uint16_t count)
{
    _coils = bits;
    _coilCount = bits == nullptr ? 0 : count;
}

uint16_t JW_ModbusRTU::coilCount() const
{
    return _coilCount;
}

bool JW_ModbusRTU::getCoil(uint16_t address, bool &value) const
{
    if (_coils == nullptr || address >= _coilCount)
    {
        return false;
    }

    value = packedBit(_coils, address);
    return true;
}

bool JW_ModbusRTU::setCoil(uint16_t address, bool value)
{
    if (_coils == nullptr || address >= _coilCount)
    {
        return false;
    }

    setPackedBit(_coils, address, value);
    return true;
}

void JW_ModbusRTU::setDiscreteInputs(
    const uint8_t *bits,
    uint16_t count)
{
    _discreteInputs = bits;
    _discreteInputCount = bits == nullptr ? 0 : count;
}

uint16_t JW_ModbusRTU::discreteInputCount() const
{
    return _discreteInputCount;
}

bool JW_ModbusRTU::getDiscreteInput(
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

void JW_ModbusRTU::setHoldingRegisters(
    uint16_t *registers,
    uint16_t count)
{
    _holdingRegisters = registers;
    _holdingCount = registers == nullptr ? 0 : count;
}

uint16_t JW_ModbusRTU::holdingRegisterCount() const
{
    return _holdingCount;
}

bool JW_ModbusRTU::getHoldingRegister(
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

bool JW_ModbusRTU::setHoldingRegister(
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

void JW_ModbusRTU::setInputRegisters(
    const uint16_t *registers,
    uint16_t count)
{
    _inputRegisters = registers;
    _inputCount = registers == nullptr ? 0 : count;
}

uint16_t JW_ModbusRTU::inputRegisterCount() const
{
    return _inputCount;
}

bool JW_ModbusRTU::getInputRegister(
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

bool JW_ModbusRTU::hasValidRequest() const
{
    return _validRequestSeen;
}

uint32_t JW_ModbusRTU::lastValidRequestMs() const
{
    return _lastValidRequestMs;
}

bool JW_ModbusRTU::hasCoilWrite() const
{
    return _coilWriteSeen;
}

uint32_t JW_ModbusRTU::lastCoilWriteMs() const
{
    return _lastCoilWriteMs;
}

void JW_ModbusRTU::task()
{
    poll();
}

void JW_ModbusRTU::poll()
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

void JW_ModbusRTU::pollServer()
{
    if (_bulkRxEnabled)
    {
        if (_rxLength >= JW_MODBUS_RTU_MAX_FRAME)
        {
            clearRxBuffer();
            setError(JW_MODBUS_BUFFER_OVERFLOW);
            return;
        }

        const size_t room =
            JW_MODBUS_RTU_MAX_FRAME - _rxLength;
        const size_t readBytes =
            _port->readAvailable(
                &_rxBuffer[_rxLength],
                room);

        if (readBytes > 0)
        {
            _stats.rxBytes += (uint64_t)readBytes;
            _rxLength += (uint16_t)readBytes;
            _lastByteUs = micros();
        }

        if (_rxLength >= JW_MODBUS_RTU_MAX_FRAME &&
            _port->available() > 0)
        {
            clearRxBuffer();
            setError(JW_MODBUS_BUFFER_OVERFLOW);
            return;
        }
    }
    else
    {
        while (_port->available() > 0)
        {
            int value = _port->read();

            if (value < 0)
            {
                break;
            }

            // Contar cada byte consumido, incluso el que desborda el buffer.
            _stats.rxBytes++;
            if (_rxLength >= JW_MODBUS_RTU_MAX_FRAME)
            {
                clearRxBuffer();
                setError(JW_MODBUS_BUFFER_OVERFLOW);
                return;
            }

            _rxBuffer[_rxLength++] = (uint8_t)value;
            _lastByteUs = micros();
        }
    }

    if (_rxLength == 0)
    {
        return;
    }

    const uint32_t frameAgeUs =
        (uint32_t)(micros() - _lastByteUs);

    bool parseNow =
        frameAgeUs >= _frameGapUs;

    if (_earlyServerDispatchEnabled)
    {
        if (_rxLength == 1 &&
            (_rxBuffer[0] == _slaveId || _rxBuffer[0] == 0))
        {
            if (frameAgeUs < JW_MODBUS_FAST_PARTIAL_HOLD_US)
            {
                return;
            }
        }
        else if (_rxLength >= 2 &&
                 (_rxBuffer[0] == _slaveId || _rxBuffer[0] == 0))
        {
            uint16_t expectedLength = 0;
            bool knownLocalShape = true;

            switch (_rxBuffer[1])
            {
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x06:
                expectedLength = 8;
                break;

            case 0x0F:
            case 0x10:
                if (_rxLength < 7)
                {
                    if (frameAgeUs <
                        JW_MODBUS_FAST_PARTIAL_HOLD_US)
                    {
                        return;
                    }
                }
                else
                {
                    const uint16_t candidateLength =
                        (uint16_t)9 + _rxBuffer[6];

                    if (candidateLength <=
                        JW_MODBUS_RTU_MAX_FRAME)
                    {
                        expectedLength = candidateLength;
                    }
                    else
                    {
                        knownLocalShape = false;
                    }
                }
                break;

            default:
                knownLocalShape = false;
                break;
            }

            if (knownLocalShape && expectedLength > 0)
            {
                if (_rxLength >= expectedLength)
                {
                    parseNow = true;
                }
                else if (frameAgeUs <
                         JW_MODBUS_FAST_PARTIAL_HOLD_US)
                {
                    return;
                }
            }
        }
    }

    if (!parseNow)
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
                            candidateLength <= JW_MODBUS_RTU_MAX_FRAME)
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

            if (_earlyServerDispatchEnabled &&
                frameAgeUs < JW_MODBUS_FAST_PARTIAL_HOLD_US &&
                remaining > 0 &&
                (frame[0] == _slaveId || frame[0] == 0))
            {
                bool preservePartial = remaining == 1;

                if (remaining >= 2)
                {
                    switch (frame[1])
                    {
                    case 0x01:
                    case 0x02:
                    case 0x03:
                    case 0x04:
                    case 0x05:
                    case 0x06:
                        preservePartial = remaining < 8;
                        break;

                    case 0x0F:
                    case 0x10:
                        if (remaining < 7)
                        {
                            preservePartial = true;
                        }
                        else
                        {
                            const uint16_t expectedLength =
                                (uint16_t)9 + frame[6];

                            preservePartial =
                                expectedLength <=
                                    JW_MODBUS_RTU_MAX_FRAME &&
                                remaining < expectedLength;
                        }
                        break;

                    default:
                        preservePartial = false;
                        break;
                    }
                }

                if (preservePartial)
                {
                    if (offset > 0)
                    {
                        memmove(
                            _rxBuffer,
                            frame,
                            remaining);
                    }

                    _rxLength = remaining;
                    return;
                }
            }

            // Trafico ajeno o tail ambiguo: no contaminar CRC del Slave local.
            // H3B.3 contabiliza el descarte para distinguir perdida fisica
            // de bytes recibidos que el parser no pudo clasificar.
            _stats.serverDiscardedTails++;
            _stats.serverDiscardedBytes += (uint64_t)remaining;
            _stats.serverDiscardedLastLength = remaining;
            _stats.serverDiscardedLastAgeUs = frameAgeUs;

            if (frameAgeUs > _stats.serverDiscardedMaxAgeUs)
            {
                _stats.serverDiscardedMaxAgeUs = frameAgeUs;
            }

            switch (remaining)
            {
            case 1: _stats.serverDiscardedLen1++; break;
            case 2: _stats.serverDiscardedLen2++; break;
            case 3: _stats.serverDiscardedLen3++; break;
            case 4: _stats.serverDiscardedLen4++; break;
            case 5: _stats.serverDiscardedLen5++; break;
            case 6: _stats.serverDiscardedLen6++; break;
            case 7: _stats.serverDiscardedLen7++; break;
            case 8: _stats.serverDiscardedLen8++; break;
            default: _stats.serverDiscardedLenGt8++; break;
            }

            break;
        }

        processServerFrame(frame, frameLength);
        offset += frameLength;
    }

    clearRxBuffer();
}

void JW_ModbusRTU::pollMaster()
{
    if (_bulkRxEnabled)
    {
        if (_rxLength >= JW_MODBUS_RTU_MAX_FRAME)
        {
            clearRxBuffer();
            completeMasterTransaction(JW_MODBUS_BUFFER_OVERFLOW);
            return;
        }

        const size_t room =
            JW_MODBUS_RTU_MAX_FRAME - _rxLength;
        const size_t readBytes =
            _port->readAvailable(
                &_rxBuffer[_rxLength],
                room);

        if (readBytes > 0)
        {
            _stats.rxBytes += (uint64_t)readBytes;
            _rxLength += (uint16_t)readBytes;
            _lastByteUs = micros();
        }

        if (_rxLength >= JW_MODBUS_RTU_MAX_FRAME &&
            _port->available() > 0)
        {
            clearRxBuffer();
            completeMasterTransaction(JW_MODBUS_BUFFER_OVERFLOW);
            return;
        }
    }
    else
    {
        while (_port->available() > 0)
        {
            int value = _port->read();

            if (value < 0)
            {
                break;
            }

            // Contar cada byte consumido, incluso el que desborda el buffer.
            _stats.rxBytes++;
            if (_rxLength >= JW_MODBUS_RTU_MAX_FRAME)
            {
                clearRxBuffer();
                completeMasterTransaction(JW_MODBUS_BUFFER_OVERFLOW);
                return;
            }

            _rxBuffer[_rxLength++] = (uint8_t)value;
            _lastByteUs = micros();
        }
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
        case JW_MODBUS_MASTER_OP_READ_COILS:
        case JW_MODBUS_MASTER_OP_READ_DISCRETE_INPUTS:
            expectedLength =
                (uint16_t)(5 + ((_masterQuantity + 7U) / 8U));
            break;

        case JW_MODBUS_MASTER_OP_READ_HOLDING_REGISTERS:
        case JW_MODBUS_MASTER_OP_READ_INPUT_REGISTERS:
            expectedLength =
                (uint16_t)(5 + _masterQuantity * 2U);
            break;

        case JW_MODBUS_MASTER_OP_WRITE_SINGLE_COIL:
        case JW_MODBUS_MASTER_OP_WRITE_SINGLE_REGISTER:
        case JW_MODBUS_MASTER_OP_WRITE_MULTIPLE_COILS:
        case JW_MODBUS_MASTER_OP_WRITE_MULTIPLE_REGISTERS:
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
             (uint32_t)(micros() - _lastByteUs) >= _frameGapUs)
    {
        processMasterFrame(_rxBuffer, _rxLength);
        clearRxBuffer();
        return;
    }

    if ((uint32_t)(millis() - _masterStartMs) >= _masterTimeoutMs)
    {
        _stats.masterTimeouts++;
        clearRxBuffer();
        completeMasterTransaction(JW_MODBUS_TIMEOUT);
    }
}

bool JW_ModbusRTU::requestReadCoils(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t *destinationPacked,
    uint32_t timeoutMs)
{
    return startReadBitsRequest(
        0x01,
        JW_MODBUS_MASTER_OP_READ_COILS,
        targetSlaveId,
        startAddress,
        quantity,
        destinationPacked,
        timeoutMs);
}

bool JW_ModbusRTU::requestReadDiscreteInputs(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t *destinationPacked,
    uint32_t timeoutMs)
{
    return startReadBitsRequest(
        0x02,
        JW_MODBUS_MASTER_OP_READ_DISCRETE_INPUTS,
        targetSlaveId,
        startAddress,
        quantity,
        destinationPacked,
        timeoutMs);
}

bool JW_ModbusRTU::requestReadHoldingRegisters(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    return startReadRegistersRequest(
        0x03,
        JW_MODBUS_MASTER_OP_READ_HOLDING_REGISTERS,
        targetSlaveId,
        startAddress,
        quantity,
        destination,
        timeoutMs);
}

bool JW_ModbusRTU::requestReadInputRegisters(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    return startReadRegistersRequest(
        0x04,
        JW_MODBUS_MASTER_OP_READ_INPUT_REGISTERS,
        targetSlaveId,
        startAddress,
        quantity,
        destination,
        timeoutMs);
}

bool JW_ModbusRTU::requestWriteSingleCoil(
    uint8_t targetSlaveId,
    uint16_t address,
    bool value,
    uint32_t timeoutMs)
{
    return startWriteSingleRequest(
        0x05,
        JW_MODBUS_MASTER_OP_WRITE_SINGLE_COIL,
        targetSlaveId,
        address,
        value ? 0xFF00 : 0x0000,
        timeoutMs);
}

bool JW_ModbusRTU::requestWriteSingleRegister(
    uint8_t targetSlaveId,
    uint16_t address,
    uint16_t value,
    uint32_t timeoutMs)
{
    return startWriteSingleRequest(
        0x06,
        JW_MODBUS_MASTER_OP_WRITE_SINGLE_REGISTER,
        targetSlaveId,
        address,
        value,
        timeoutMs);
}

bool JW_ModbusRTU::requestWriteMultipleCoils(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    const uint8_t *sourcePacked,
    uint32_t timeoutMs)
{
    if (masterBusy())
    {
        setError(JW_MODBUS_BUSY);
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
        requestLength > JW_MODBUS_RTU_MAX_FRAME ||
        timeoutMs == 0)
    {
        setError(JW_MODBUS_INVALID_RESPONSE);
        return false;
    }

    if (masterDone())
    {
        clearMasterResult();
    }

    drainRs485();
    clearRxBuffer();

    _masterOperation = JW_MODBUS_MASTER_OP_WRITE_MULTIPLE_COILS;
    _masterTargetSlaveId = targetSlaveId;
    _masterExpectedFunction = 0x0F;
    _masterStartAddress = startAddress;
    _masterQuantity = quantity;
    _masterRegisterDestination = nullptr;
    _masterBitDestination = nullptr;
    _masterWriteValue = 0;
    _masterTimeoutMs = timeoutMs;
    _masterResult = JW_MODBUS_OK;
    _masterState = JW_MODBUS_MASTER_WAIT_RESPONSE;

    uint8_t request[JW_MODBUS_RTU_MAX_FRAME];
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

    const size_t written = writeTransport(request, requestLength);

    if (written != requestLength)
    {
        completeMasterTransaction(JW_MODBUS_TRANSPORT_ERROR);
        return false;
    }

    _stats.txFrames++;
    _masterStartMs = millis();
    clearError();
    return true;
}

bool JW_ModbusRTU::requestWriteMultipleRegisters(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    const uint16_t *source,
    uint32_t timeoutMs)
{
    if (masterBusy())
    {
        setError(JW_MODBUS_BUSY);
        return false;
    }

    const uint16_t byteCount = (uint16_t)(quantity * 2U);
    const uint16_t requestLength = (uint16_t)(9U + byteCount);

    if (!isReady() ||
        source == nullptr ||
        targetSlaveId == 0 ||
        targetSlaveId > 247 ||
        quantity == 0 ||
        quantity > 123 ||
        requestLength > JW_MODBUS_RTU_MAX_FRAME ||
        timeoutMs == 0)
    {
        setError(JW_MODBUS_INVALID_RESPONSE);
        return false;
    }

    if (masterDone())
    {
        clearMasterResult();
    }

    drainRs485();
    clearRxBuffer();

    _masterOperation = JW_MODBUS_MASTER_OP_WRITE_MULTIPLE_REGISTERS;
    _masterTargetSlaveId = targetSlaveId;
    _masterExpectedFunction = 0x10;
    _masterStartAddress = startAddress;
    _masterQuantity = quantity;
    _masterRegisterDestination = nullptr;
    _masterBitDestination = nullptr;
    _masterWriteValue = 0;
    _masterTimeoutMs = timeoutMs;
    _masterResult = JW_MODBUS_OK;
    _masterState = JW_MODBUS_MASTER_WAIT_RESPONSE;

    uint8_t request[JW_MODBUS_RTU_MAX_FRAME];
    request[0] = targetSlaveId;
    request[1] = 0x10;
    request[2] = highByte(startAddress);
    request[3] = lowByte(startAddress);
    request[4] = highByte(quantity);
    request[5] = lowByte(quantity);
    request[6] = (uint8_t)byteCount;

    for (uint16_t i = 0; i < quantity; i++)
    {
        request[7 + i * 2U] = highByte(source[i]);
        request[8 + i * 2U] = lowByte(source[i]);
    }

    appendCRC(request, (size_t)(7U + byteCount));

    const size_t written = writeTransport(request, requestLength);

    if (written != requestLength)
    {
        completeMasterTransaction(JW_MODBUS_TRANSPORT_ERROR);
        return false;
    }

    _stats.txFrames++;
    _masterStartMs = millis();
    clearError();
    return true;
}

bool JW_ModbusRTU::startReadBitsRequest(
    uint8_t functionCode,
    JWModbusMasterOperation operation,
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t *destinationPacked,
    uint32_t timeoutMs)
{
    if (masterBusy())
    {
        setError(JW_MODBUS_BUSY);
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
        setError(JW_MODBUS_INVALID_RESPONSE);
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
    _masterResult = JW_MODBUS_OK;
    _masterState = JW_MODBUS_MASTER_WAIT_RESPONSE;

    uint8_t request[8];
    request[0] = targetSlaveId;
    request[1] = functionCode;
    request[2] = highByte(startAddress);
    request[3] = lowByte(startAddress);
    request[4] = highByte(quantity);
    request[5] = lowByte(quantity);
    appendCRC(request, 6);

    const size_t written = writeTransport(request, sizeof(request));

    if (written != sizeof(request))
    {
        completeMasterTransaction(JW_MODBUS_TRANSPORT_ERROR);
        return false;
    }

    _stats.txFrames++;
    _masterStartMs = millis();
    clearError();
    return true;
}

bool JW_ModbusRTU::startReadRegistersRequest(
    uint8_t functionCode,
    JWModbusMasterOperation operation,
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    if (masterBusy())
    {
        setError(JW_MODBUS_BUSY);
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
        setError(JW_MODBUS_INVALID_RESPONSE);
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
    _masterResult = JW_MODBUS_OK;
    _masterState = JW_MODBUS_MASTER_WAIT_RESPONSE;

    uint8_t request[8];
    request[0] = targetSlaveId;
    request[1] = functionCode;
    request[2] = highByte(startAddress);
    request[3] = lowByte(startAddress);
    request[4] = highByte(quantity);
    request[5] = lowByte(quantity);
    appendCRC(request, 6);

    const size_t written = writeTransport(request, sizeof(request));

    if (written != sizeof(request))
    {
        completeMasterTransaction(JW_MODBUS_TRANSPORT_ERROR);
        return false;
    }

    _stats.txFrames++;
    _masterStartMs = millis();
    clearError();
    return true;
}

bool JW_ModbusRTU::startWriteSingleRequest(
    uint8_t functionCode,
    JWModbusMasterOperation operation,
    uint8_t targetSlaveId,
    uint16_t address,
    uint16_t value,
    uint32_t timeoutMs)
{
    if (masterBusy())
    {
        setError(JW_MODBUS_BUSY);
        return false;
    }

    if (!isReady() ||
        targetSlaveId == 0 ||
        targetSlaveId > 247 ||
        timeoutMs == 0)
    {
        setError(JW_MODBUS_INVALID_RESPONSE);
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
    _masterResult = JW_MODBUS_OK;
    _masterState = JW_MODBUS_MASTER_WAIT_RESPONSE;

    uint8_t request[8];
    request[0] = targetSlaveId;
    request[1] = functionCode;
    request[2] = highByte(address);
    request[3] = lowByte(address);
    request[4] = highByte(value);
    request[5] = lowByte(value);
    appendCRC(request, 6);

    const size_t written = writeTransport(request, sizeof(request));

    if (written != sizeof(request))
    {
        completeMasterTransaction(JW_MODBUS_TRANSPORT_ERROR);
        return false;
    }

    _stats.txFrames++;
    _masterStartMs = millis();
    clearError();
    return true;
}

bool JW_ModbusRTU::masterBusy() const
{
    return _masterState == JW_MODBUS_MASTER_WAIT_RESPONSE;
}

bool JW_ModbusRTU::masterDone() const
{
    return _masterState == JW_MODBUS_MASTER_DONE ||
           _masterState == JW_MODBUS_MASTER_ERROR;
}

bool JW_ModbusRTU::masterSucceeded() const
{
    return _masterState == JW_MODBUS_MASTER_DONE &&
           _masterResult == JW_MODBUS_OK;
}

JWModbusMasterState JW_ModbusRTU::masterState() const
{
    return _masterState;
}

JWModbusRTUError JW_ModbusRTU::masterResult() const
{
    return _masterResult;
}

void JW_ModbusRTU::clearMasterResult()
{
    if (!masterBusy())
    {
        resetMasterContext();
    }
}

bool JW_ModbusRTU::finishSyncRequest(bool started)
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

bool JW_ModbusRTU::readCoilsSync(
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

bool JW_ModbusRTU::readDiscreteInputsSync(
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

bool JW_ModbusRTU::readHoldingRegistersSync(
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

bool JW_ModbusRTU::readInputRegistersSync(
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

bool JW_ModbusRTU::writeSingleCoilSync(
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

bool JW_ModbusRTU::writeSingleRegisterSync(
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

bool JW_ModbusRTU::writeMultipleCoilsSync(
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

bool JW_ModbusRTU::writeMultipleRegistersSync(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    const uint16_t *source,
    uint32_t timeoutMs)
{
    return finishSyncRequest(requestWriteMultipleRegisters(
        targetSlaveId,
        startAddress,
        quantity,
        source,
        timeoutMs));
}

bool JW_ModbusRTU::readCoils(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t *destinationPacked,
    uint32_t timeoutMs)
{
    if (_motor == JW_MODBUS_SYNC)
    {
        return readCoilsSync(
            targetSlaveId,
            startAddress,
            quantity,
            destinationPacked,
            timeoutMs);
    }

    return requestReadCoils(
        targetSlaveId,
        startAddress,
        quantity,
        destinationPacked,
        timeoutMs);
}

bool JW_ModbusRTU::readDiscreteInputs(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint8_t *destinationPacked,
    uint32_t timeoutMs)
{
    if (_motor == JW_MODBUS_SYNC)
    {
        return readDiscreteInputsSync(
            targetSlaveId,
            startAddress,
            quantity,
            destinationPacked,
            timeoutMs);
    }

    return requestReadDiscreteInputs(
        targetSlaveId,
        startAddress,
        quantity,
        destinationPacked,
        timeoutMs);
}

bool JW_ModbusRTU::readHoldingRegisters(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    if (_motor == JW_MODBUS_SYNC)
    {
        return readHoldingRegistersSync(
            targetSlaveId,
            startAddress,
            quantity,
            destination,
            timeoutMs);
    }

    return requestReadHoldingRegisters(
        targetSlaveId,
        startAddress,
        quantity,
        destination,
        timeoutMs);
}

bool JW_ModbusRTU::readInputRegisters(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    if (_motor == JW_MODBUS_SYNC)
    {
        return readInputRegistersSync(
            targetSlaveId,
            startAddress,
            quantity,
            destination,
            timeoutMs);
    }

    return requestReadInputRegisters(
        targetSlaveId,
        startAddress,
        quantity,
        destination,
        timeoutMs);
}

bool JW_ModbusRTU::writeSingleCoil(
    uint8_t targetSlaveId,
    uint16_t address,
    bool value,
    uint32_t timeoutMs)
{
    if (_motor == JW_MODBUS_SYNC)
    {
        return writeSingleCoilSync(
            targetSlaveId,
            address,
            value,
            timeoutMs);
    }

    return requestWriteSingleCoil(
        targetSlaveId,
        address,
        value,
        timeoutMs);
}

bool JW_ModbusRTU::writeSingleRegister(
    uint8_t targetSlaveId,
    uint16_t address,
    uint16_t value,
    uint32_t timeoutMs)
{
    if (_motor == JW_MODBUS_SYNC)
    {
        return writeSingleRegisterSync(
            targetSlaveId,
            address,
            value,
            timeoutMs);
    }

    return requestWriteSingleRegister(
        targetSlaveId,
        address,
        value,
        timeoutMs);
}

bool JW_ModbusRTU::writeMultipleCoils(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    const uint8_t *sourcePacked,
    uint32_t timeoutMs)
{
    if (_motor == JW_MODBUS_SYNC)
    {
        return writeMultipleCoilsSync(
            targetSlaveId,
            startAddress,
            quantity,
            sourcePacked,
            timeoutMs);
    }

    return requestWriteMultipleCoils(
        targetSlaveId,
        startAddress,
        quantity,
        sourcePacked,
        timeoutMs);
}

bool JW_ModbusRTU::writeMultipleRegisters(
    uint8_t targetSlaveId,
    uint16_t startAddress,
    uint16_t quantity,
    const uint16_t *source,
    uint32_t timeoutMs)
{
    if (_motor == JW_MODBUS_SYNC)
    {
        return writeMultipleRegistersSync(
            targetSlaveId,
            startAddress,
            quantity,
            source,
            timeoutMs);
    }

    return requestWriteMultipleRegisters(
        targetSlaveId,
        startAddress,
        quantity,
        source,
        timeoutMs);
}

void JW_ModbusRTU::resetMasterContext()
{
    _masterState = JW_MODBUS_MASTER_IDLE;
    _masterOperation = JW_MODBUS_MASTER_OP_NONE;
    _masterResult = JW_MODBUS_OK;
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

void JW_ModbusRTU::completeMasterTransaction(
    JWModbusRTUError result)
{
    _masterResult = result;

    if (result == JW_MODBUS_OK)
    {
        _masterState = JW_MODBUS_MASTER_DONE;
        _stats.requestsOk++;
        clearError();
    }
    else
    {
        _masterState = JW_MODBUS_MASTER_ERROR;
        setError(result);
    }
}

bool JW_ModbusRTU::processMasterFrame(
    const uint8_t *frame,
    uint16_t length)
{
    _stats.rxFrames++;

    if (frame == nullptr || length < 5)
    {
        completeMasterTransaction(JW_MODBUS_INVALID_RESPONSE);
        return false;
    }

    if (!checkCRC(frame, length))
    {
        _stats.crcErrors++;
        completeMasterTransaction(JW_MODBUS_CRC_ERROR);
        return false;
    }

    if (frame[0] != _masterTargetSlaveId)
    {
        completeMasterTransaction(JW_MODBUS_INVALID_RESPONSE);
        return false;
    }

    if (frame[1] == (uint8_t)(_masterExpectedFunction | 0x80))
    {
        completeMasterTransaction(JW_MODBUS_EXCEPTION);
        return false;
    }

    if (frame[1] != _masterExpectedFunction)
    {
        completeMasterTransaction(JW_MODBUS_INVALID_RESPONSE);
        return false;
    }

    switch (_masterOperation)
    {
    case JW_MODBUS_MASTER_OP_READ_COILS:
    case JW_MODBUS_MASTER_OP_READ_DISCRETE_INPUTS:
    {
        if (_masterBitDestination == nullptr ||
            _masterQuantity == 0 ||
            _masterQuantity > 2000)
        {
            completeMasterTransaction(JW_MODBUS_INVALID_RESPONSE);
            return false;
        }

        const uint16_t expectedByteCount =
            (uint16_t)((_masterQuantity + 7U) / 8U);
        const uint16_t expectedLength =
            (uint16_t)(5U + expectedByteCount);

        if (frame[2] != expectedByteCount || length != expectedLength)
        {
            completeMasterTransaction(JW_MODBUS_INVALID_RESPONSE);
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

        completeMasterTransaction(JW_MODBUS_OK);
        return true;
    }

    case JW_MODBUS_MASTER_OP_READ_HOLDING_REGISTERS:
    case JW_MODBUS_MASTER_OP_READ_INPUT_REGISTERS:
    {
        if (_masterRegisterDestination == nullptr ||
            _masterQuantity == 0 ||
            _masterQuantity > 125)
        {
            completeMasterTransaction(JW_MODBUS_INVALID_RESPONSE);
            return false;
        }

        const uint16_t expectedByteCount = _masterQuantity * 2U;
        const uint16_t expectedLength =
            (uint16_t)(5U + expectedByteCount);

        if (frame[2] != expectedByteCount || length != expectedLength)
        {
            completeMasterTransaction(JW_MODBUS_INVALID_RESPONSE);
            return false;
        }

        for (uint16_t i = 0; i < _masterQuantity; i++)
        {
            _masterRegisterDestination[i] =
                ((uint16_t)frame[3 + i * 2U] << 8) |
                frame[4 + i * 2U];
        }

        completeMasterTransaction(JW_MODBUS_OK);
        return true;
    }

    case JW_MODBUS_MASTER_OP_WRITE_SINGLE_COIL:
    case JW_MODBUS_MASTER_OP_WRITE_SINGLE_REGISTER:
        if (length != 8 ||
            frame[2] != highByte(_masterStartAddress) ||
            frame[3] != lowByte(_masterStartAddress) ||
            frame[4] != highByte(_masterWriteValue) ||
            frame[5] != lowByte(_masterWriteValue))
        {
            completeMasterTransaction(JW_MODBUS_INVALID_RESPONSE);
            return false;
        }

        completeMasterTransaction(JW_MODBUS_OK);
        return true;

    case JW_MODBUS_MASTER_OP_WRITE_MULTIPLE_COILS:
    case JW_MODBUS_MASTER_OP_WRITE_MULTIPLE_REGISTERS:
        if (length != 8 ||
            frame[2] != highByte(_masterStartAddress) ||
            frame[3] != lowByte(_masterStartAddress) ||
            frame[4] != highByte(_masterQuantity) ||
            frame[5] != lowByte(_masterQuantity))
        {
            completeMasterTransaction(JW_MODBUS_INVALID_RESPONSE);
            return false;
        }

        completeMasterTransaction(JW_MODBUS_OK);
        return true;

    default:
        completeMasterTransaction(JW_MODBUS_INVALID_RESPONSE);
        return false;
    }
}

void JW_ModbusRTU::drainRs485()
{
    while (_port->available() > 0)
    {
        _port->read();
    }
}

void JW_ModbusRTU::setCrcLookupEnabled(bool enabled)
{
    _crcLookupEnabled = enabled;
}

bool JW_ModbusRTU::crcLookupEnabled()
{
    return _crcLookupEnabled;
}

uint16_t JW_ModbusRTU::crc16Bitwise(
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

uint16_t JW_ModbusRTU::crc16Lookup(
    const uint8_t *data,
    size_t length)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++)
    {
        const uint8_t index =
            (uint8_t)((crc ^ data[i]) & 0x00FFU);

        crc =
            (uint16_t)(
                (crc >> 8) ^
                JW_MODBUS_CRC16_TABLE[index]);
    }

    return crc;
}

uint16_t JW_ModbusRTU::crc16(
    const uint8_t *data,
    size_t length)
{
    return
        _crcLookupEnabled
            ? crc16Lookup(data, length)
            : crc16Bitwise(data, length);
}

bool JW_ModbusRTU::checkCRC(
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

void JW_ModbusRTU::appendCRC(
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

JWModbusRTUError JW_ModbusRTU::lastError() const
{
    return _lastError;
}

const char *JW_ModbusRTU::lastErrorString() const
{
    switch (_lastError)
    {
    case JW_MODBUS_OK:
        return "OK";
    case JW_MODBUS_DISABLED:
        return "Modbus disabled";
    case JW_MODBUS_NOT_STARTED:
        return "Modbus not started";
    case JW_MODBUS_INVALID_SLAVE_ID:
        return "Invalid slave ID";
    case JW_MODBUS_INVALID_REGISTER_MAP:
        return "Invalid register map";
    case JW_MODBUS_TIMEOUT:
        return "Timeout";
    case JW_MODBUS_CRC_ERROR:
        return "CRC error";
    case JW_MODBUS_EXCEPTION:
        return "Modbus exception";
    case JW_MODBUS_INVALID_RESPONSE:
        return "Invalid response";
    case JW_MODBUS_BUFFER_OVERFLOW:
        return "Buffer overflow";
    case JW_MODBUS_UNSUPPORTED_FUNCTION:
        return "Unsupported function";
    case JW_MODBUS_BUSY:
        return "Master busy";
    case JW_MODBUS_TRANSPORT_ERROR:
        return "Transport error";
    default:
        return "Unknown Modbus error";
    }
}

const char *JW_ModbusRTU::configString() const
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

const JWModbusRTUStats &JW_ModbusRTU::stats() const
{
    return _stats;
}

void JW_ModbusRTU::resetStats()
{
    _stats = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}

void JW_ModbusRTU::printStatus(Print &out) const
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

    out.print("Frame gap us: ");
    out.println(_frameGapUs);

    out.print("Motor: ");
    out.println(_motor == JW_MODBUS_ASYNC ? "ASYNC" : "SYNC");

    out.print("Queued TX requested: ");
    out.println(_queuedTxEnabled ? "yes" : "no");

    out.print("Queued TX active: ");
    out.println(queuedTxActive() ? "yes" : "no");

    out.print("RS485 TX buffer bytes: ");
    out.println((unsigned long)_port->txBufferSize());

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

size_t JW_ModbusRTU::writeTransport(
    const uint8_t *buffer,
    size_t size)
{
    const size_t written =
        queuedTxActive()
            ? _port->writeQueued(buffer, size)
            : _port->write(buffer, size);

    _stats.txBytes += (uint64_t)written;
    return written;
}

void JW_ModbusRTU::clearRxBuffer()
{
    _rxLength = 0;
}

void JW_ModbusRTU::setError(JWModbusRTUError error)
{
    _lastError = error;
}

void JW_ModbusRTU::clearError()
{
    _lastError = JW_MODBUS_OK;
}

bool JW_ModbusRTU::processServerFrame(
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
        setError(JW_MODBUS_CRC_ERROR);
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
            sendException(functionCode, JW_MODBUS_EX_ILLEGAL_FUNCTION);
        }
        setError(JW_MODBUS_UNSUPPORTED_FUNCTION);
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

void JW_ModbusRTU::sendException(
    uint8_t functionCode,
    uint8_t exceptionCode)
{
    uint8_t response[5];
    response[0] = _slaveId;
    response[1] = functionCode | 0x80;
    response[2] = exceptionCode;
    appendCRC(response, 3);

    writeTransport(response, sizeof(response));
    _stats.exceptionsSent++;
    _stats.txFrames++;
}

void JW_ModbusRTU::sendFrame(
    uint8_t *frame,
    uint16_t payloadLength)
{
    appendCRC(frame, payloadLength);
    writeTransport(frame, payloadLength + 2);
    _stats.txFrames++;
}

bool JW_ModbusRTU::handleReadBits(
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
        sendException(functionCode, JW_MODBUS_EX_ILLEGAL_DATA_VALUE);
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    if (map == nullptr || count == 0)
    {
        sendException(functionCode, JW_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    const uint16_t start = ((uint16_t)frame[2] << 8) | frame[3];
    const uint16_t quantity = ((uint16_t)frame[4] << 8) | frame[5];

    if (quantity == 0 || quantity > 2000)
    {
        sendException(functionCode, JW_MODBUS_EX_ILLEGAL_DATA_VALUE);
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    if (start >= count || (uint32_t)start + quantity > count)
    {
        sendException(functionCode, JW_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    const uint16_t byteCount = (uint16_t)((quantity + 7U) / 8U);
    uint8_t response[JW_MODBUS_RTU_MAX_FRAME] = {};
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

bool JW_ModbusRTU::handleReadRegisters(
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
        sendException(functionCode, JW_MODBUS_EX_ILLEGAL_DATA_VALUE);
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    if (map == nullptr || count == 0)
    {
        sendException(functionCode, JW_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    const uint16_t start = ((uint16_t)frame[2] << 8) | frame[3];
    const uint16_t quantity = ((uint16_t)frame[4] << 8) | frame[5];

    if (quantity == 0 || quantity > 125)
    {
        sendException(functionCode, JW_MODBUS_EX_ILLEGAL_DATA_VALUE);
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    if (start >= count || (uint32_t)start + quantity > count)
    {
        sendException(functionCode, JW_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    uint8_t response[JW_MODBUS_RTU_MAX_FRAME];
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

bool JW_ModbusRTU::handleWriteSingleCoil(
    const uint8_t *frame,
    uint16_t length,
    bool broadcast)
{
    if (length != 8)
    {
        if (!broadcast)
        {
            sendException(0x05, JW_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    if (_coils == nullptr || _coilCount == 0)
    {
        if (!broadcast)
        {
            sendException(0x05, JW_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        }
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    const uint16_t address = ((uint16_t)frame[2] << 8) | frame[3];
    const uint16_t value = ((uint16_t)frame[4] << 8) | frame[5];

    if (address >= _coilCount)
    {
        if (!broadcast)
        {
            sendException(0x05, JW_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        }
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    if (value != 0xFF00 && value != 0x0000)
    {
        if (!broadcast)
        {
            sendException(0x05, JW_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        setError(JW_MODBUS_EXCEPTION);
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

bool JW_ModbusRTU::handleWriteSingleRegister(
    const uint8_t *frame,
    uint16_t length,
    bool broadcast)
{
    if (length != 8)
    {
        if (!broadcast)
        {
            sendException(0x06, JW_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    if (_holdingRegisters == nullptr || _holdingCount == 0)
    {
        if (!broadcast)
        {
            sendException(0x06, JW_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        }
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    const uint16_t address = ((uint16_t)frame[2] << 8) | frame[3];
    const uint16_t value = ((uint16_t)frame[4] << 8) | frame[5];

    if (address >= _holdingCount)
    {
        if (!broadcast)
        {
            sendException(0x06, JW_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        }
        setError(JW_MODBUS_EXCEPTION);
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

bool JW_ModbusRTU::handleWriteMultipleCoils(
    const uint8_t *frame,
    uint16_t length,
    bool broadcast)
{
    if (length < 10)
    {
        if (!broadcast)
        {
            sendException(0x0F, JW_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    if (_coils == nullptr || _coilCount == 0)
    {
        if (!broadcast)
        {
            sendException(0x0F, JW_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        }
        setError(JW_MODBUS_EXCEPTION);
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
            sendException(0x0F, JW_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    if (start >= _coilCount ||
        (uint32_t)start + quantity > _coilCount)
    {
        if (!broadcast)
        {
            sendException(0x0F, JW_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        }
        setError(JW_MODBUS_EXCEPTION);
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

bool JW_ModbusRTU::handleWriteMultipleRegisters(
    const uint8_t *frame,
    uint16_t length,
    bool broadcast)
{
    if (length < 9)
    {
        if (!broadcast)
        {
            sendException(0x10, JW_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    if (_holdingRegisters == nullptr || _holdingCount == 0)
    {
        if (!broadcast)
        {
            sendException(0x10, JW_MODBUS_EX_SLAVE_DEVICE_FAILURE);
        }
        setError(JW_MODBUS_EXCEPTION);
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
            sendException(0x10, JW_MODBUS_EX_ILLEGAL_DATA_VALUE);
        }
        setError(JW_MODBUS_EXCEPTION);
        return false;
    }

    if (start >= _holdingCount ||
        (uint32_t)start + quantity > _holdingCount)
    {
        if (!broadcast)
        {
            sendException(0x10, JW_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        }
        setError(JW_MODBUS_EXCEPTION);
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

bool JW_ModbusRTU::packedBit(
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

void JW_ModbusRTU::setPackedBit(
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
