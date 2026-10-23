#include "JWPLC_ModbusRTU.h"

JWPLC_ModbusRTUClass JWPLC_ModbusRTU;

JWPLC_ModbusRTUClass::JWPLC_ModbusRTUClass()
    : _impl(jwplcRs485Port())
{
}

JW_ModbusRTU &JWPLC_ModbusRTUClass::engine()
{
    return _impl;
}

bool JWPLC_ModbusRTUClass::begin()
{
    return _impl.begin(JWPLC_MODBUS_RTU_DEFAULT_SLAVE_ID, JWPLC_MODBUS_RTU_DEFAULT_BAUD, JWPLC_MODBUS_RTU_DEFAULT_CONFIG);
}

bool JWPLC_ModbusRTUClass::begin(uint8_t slaveId)
{
    return _impl.begin(slaveId, JWPLC_MODBUS_RTU_DEFAULT_BAUD, JWPLC_MODBUS_RTU_DEFAULT_CONFIG);
}

bool JWPLC_ModbusRTUClass::begin(uint8_t slaveId, uint32_t baud, uint32_t config)
{
    return _impl.begin(slaveId, baud, config);
}

void JWPLC_ModbusRTUClass::end()
{
    _impl.end();
}

bool JWPLC_ModbusRTUClass::isReady() const
{
    return _impl.isReady();
}

uint8_t JWPLC_ModbusRTUClass::slaveId() const
{
    return _impl.slaveId();
}

uint32_t JWPLC_ModbusRTUClass::baudRate() const
{
    return _impl.baudRate();
}

uint32_t JWPLC_ModbusRTUClass::effectiveBaudRate() const
{
    return _impl.effectiveBaudRate();
}

uint32_t JWPLC_ModbusRTUClass::config() const
{
    return _impl.config();
}

bool JWPLC_ModbusRTUClass::motor(JWPLCModbusMotor mode)
{
    return _impl.motor(static_cast<JWModbusMotor>(mode));
}

JWPLCModbusMotor JWPLC_ModbusRTUClass::motor() const
{
    return static_cast<JWPLCModbusMotor>(_impl.motor());
}

bool JWPLC_ModbusRTUClass::asyncMotor() const
{
    return _impl.asyncMotor();
}

void JWPLC_ModbusRTUClass::setFrameGapMs(uint16_t gapMs)
{
    _impl.setFrameGapMs(gapMs);
}

uint16_t JWPLC_ModbusRTUClass::frameGapMs() const
{
    return _impl.frameGapMs();
}

void JWPLC_ModbusRTUClass::setFrameGapUs(uint32_t gapUs)
{
    _impl.setFrameGapUs(gapUs);
}

uint32_t JWPLC_ModbusRTUClass::frameGapUs() const
{
    return _impl.frameGapUs();
}

void JWPLC_ModbusRTUClass::setQueuedTxEnabled(bool enabled)
{
    _impl.setQueuedTxEnabled(enabled);
}

bool JWPLC_ModbusRTUClass::queuedTxEnabled() const
{
    return _impl.queuedTxEnabled();
}

bool JWPLC_ModbusRTUClass::queuedTxActive() const
{
    return _impl.queuedTxActive();
}

void JWPLC_ModbusRTUClass::setBulkRxEnabled(bool enabled)
{
    _impl.setBulkRxEnabled(enabled);
}

bool JWPLC_ModbusRTUClass::bulkRxEnabled() const
{
    return _impl.bulkRxEnabled();
}

void JWPLC_ModbusRTUClass::setEarlyServerDispatchEnabled(bool enabled)
{
    _impl.setEarlyServerDispatchEnabled(enabled);
}

bool JWPLC_ModbusRTUClass::earlyServerDispatchEnabled() const
{
    return _impl.earlyServerDispatchEnabled();
}

void JWPLC_ModbusRTUClass::setCoils(uint8_t *bits, uint16_t count)
{
    _impl.setCoils(bits, count);
}

uint16_t JWPLC_ModbusRTUClass::coilCount() const
{
    return _impl.coilCount();
}

bool JWPLC_ModbusRTUClass::getCoil(uint16_t address, bool &value) const
{
    return _impl.getCoil(address, value);
}

bool JWPLC_ModbusRTUClass::setCoil(uint16_t address, bool value)
{
    return _impl.setCoil(address, value);
}

void JWPLC_ModbusRTUClass::setDiscreteInputs(const uint8_t *bits, uint16_t count)
{
    _impl.setDiscreteInputs(bits, count);
}

uint16_t JWPLC_ModbusRTUClass::discreteInputCount() const
{
    return _impl.discreteInputCount();
}

bool JWPLC_ModbusRTUClass::getDiscreteInput(uint16_t address, bool &value) const
{
    return _impl.getDiscreteInput(address, value);
}

void JWPLC_ModbusRTUClass::setHoldingRegisters(uint16_t *registers, uint16_t count)
{
    _impl.setHoldingRegisters(registers, count);
}

uint16_t JWPLC_ModbusRTUClass::holdingRegisterCount() const
{
    return _impl.holdingRegisterCount();
}

bool JWPLC_ModbusRTUClass::getHoldingRegister(uint16_t address, uint16_t &value) const
{
    return _impl.getHoldingRegister(address, value);
}

bool JWPLC_ModbusRTUClass::setHoldingRegister(uint16_t address, uint16_t value)
{
    return _impl.setHoldingRegister(address, value);
}

void JWPLC_ModbusRTUClass::setInputRegisters(const uint16_t *registers, uint16_t count)
{
    _impl.setInputRegisters(registers, count);
}

uint16_t JWPLC_ModbusRTUClass::inputRegisterCount() const
{
    return _impl.inputRegisterCount();
}

bool JWPLC_ModbusRTUClass::getInputRegister(uint16_t address, uint16_t &value) const
{
    return _impl.getInputRegister(address, value);
}

bool JWPLC_ModbusRTUClass::hasValidRequest() const
{
    return _impl.hasValidRequest();
}

uint32_t JWPLC_ModbusRTUClass::lastValidRequestMs() const
{
    return _impl.lastValidRequestMs();
}

bool JWPLC_ModbusRTUClass::hasCoilWrite() const
{
    return _impl.hasCoilWrite();
}

uint32_t JWPLC_ModbusRTUClass::lastCoilWriteMs() const
{
    return _impl.lastCoilWriteMs();
}

void JWPLC_ModbusRTUClass::task()
{
    _impl.task();
}

void JWPLC_ModbusRTUClass::poll()
{
    _impl.poll();
}

bool JWPLC_ModbusRTUClass::requestReadCoils(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, uint8_t *destinationPacked, uint32_t timeoutMs)
{
    return _impl.requestReadCoils(targetSlaveId, startAddress, quantity, destinationPacked, timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestReadDiscreteInputs(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, uint8_t *destinationPacked, uint32_t timeoutMs)
{
    return _impl.requestReadDiscreteInputs(targetSlaveId, startAddress, quantity, destinationPacked, timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestReadHoldingRegisters(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, uint16_t *destination, uint32_t timeoutMs)
{
    return _impl.requestReadHoldingRegisters(targetSlaveId, startAddress, quantity, destination, timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestReadInputRegisters(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, uint16_t *destination, uint32_t timeoutMs)
{
    return _impl.requestReadInputRegisters(targetSlaveId, startAddress, quantity, destination, timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestWriteSingleCoil(uint8_t targetSlaveId, uint16_t address, bool value, uint32_t timeoutMs)
{
    return _impl.requestWriteSingleCoil(targetSlaveId, address, value, timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestWriteSingleRegister(uint8_t targetSlaveId, uint16_t address, uint16_t value, uint32_t timeoutMs)
{
    return _impl.requestWriteSingleRegister(targetSlaveId, address, value, timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestWriteMultipleCoils(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, const uint8_t *sourcePacked, uint32_t timeoutMs)
{
    return _impl.requestWriteMultipleCoils(targetSlaveId, startAddress, quantity, sourcePacked, timeoutMs);
}

bool JWPLC_ModbusRTUClass::requestWriteMultipleRegisters(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, const uint16_t *source, uint32_t timeoutMs)
{
    return _impl.requestWriteMultipleRegisters(targetSlaveId, startAddress, quantity, source, timeoutMs);
}

bool JWPLC_ModbusRTUClass::masterBusy() const
{
    return _impl.masterBusy();
}

bool JWPLC_ModbusRTUClass::masterDone() const
{
    return _impl.masterDone();
}

bool JWPLC_ModbusRTUClass::masterSucceeded() const
{
    return _impl.masterSucceeded();
}

JWPLCModbusMasterState JWPLC_ModbusRTUClass::masterState() const
{
    return static_cast<JWPLCModbusMasterState>(_impl.masterState());
}

JWPLCModbusRTUError JWPLC_ModbusRTUClass::masterResult() const
{
    return static_cast<JWPLCModbusRTUError>(_impl.masterResult());
}

void JWPLC_ModbusRTUClass::clearMasterResult()
{
    _impl.clearMasterResult();
}

bool JWPLC_ModbusRTUClass::readCoilsSync(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, uint8_t *destinationPacked, uint32_t timeoutMs)
{
    return _impl.readCoilsSync(targetSlaveId, startAddress, quantity, destinationPacked, timeoutMs);
}

bool JWPLC_ModbusRTUClass::readDiscreteInputsSync(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, uint8_t *destinationPacked, uint32_t timeoutMs)
{
    return _impl.readDiscreteInputsSync(targetSlaveId, startAddress, quantity, destinationPacked, timeoutMs);
}

bool JWPLC_ModbusRTUClass::readHoldingRegistersSync(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, uint16_t *destination, uint32_t timeoutMs)
{
    return _impl.readHoldingRegistersSync(targetSlaveId, startAddress, quantity, destination, timeoutMs);
}

bool JWPLC_ModbusRTUClass::readInputRegistersSync(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, uint16_t *destination, uint32_t timeoutMs)
{
    return _impl.readInputRegistersSync(targetSlaveId, startAddress, quantity, destination, timeoutMs);
}

bool JWPLC_ModbusRTUClass::writeSingleCoilSync(uint8_t targetSlaveId, uint16_t address, bool value, uint32_t timeoutMs)
{
    return _impl.writeSingleCoilSync(targetSlaveId, address, value, timeoutMs);
}

bool JWPLC_ModbusRTUClass::writeSingleRegisterSync(uint8_t targetSlaveId, uint16_t address, uint16_t value, uint32_t timeoutMs)
{
    return _impl.writeSingleRegisterSync(targetSlaveId, address, value, timeoutMs);
}

bool JWPLC_ModbusRTUClass::writeMultipleCoilsSync(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, const uint8_t *sourcePacked, uint32_t timeoutMs)
{
    return _impl.writeMultipleCoilsSync(targetSlaveId, startAddress, quantity, sourcePacked, timeoutMs);
}

bool JWPLC_ModbusRTUClass::writeMultipleRegistersSync(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, const uint16_t *source, uint32_t timeoutMs)
{
    return _impl.writeMultipleRegistersSync(targetSlaveId, startAddress, quantity, source, timeoutMs);
}

bool JWPLC_ModbusRTUClass::readCoils(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, uint8_t *destinationPacked, uint32_t timeoutMs)
{
    return _impl.readCoils(targetSlaveId, startAddress, quantity, destinationPacked, timeoutMs);
}

bool JWPLC_ModbusRTUClass::readDiscreteInputs(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, uint8_t *destinationPacked, uint32_t timeoutMs)
{
    return _impl.readDiscreteInputs(targetSlaveId, startAddress, quantity, destinationPacked, timeoutMs);
}

bool JWPLC_ModbusRTUClass::readHoldingRegisters(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, uint16_t *destination, uint32_t timeoutMs)
{
    return _impl.readHoldingRegisters(targetSlaveId, startAddress, quantity, destination, timeoutMs);
}

bool JWPLC_ModbusRTUClass::readInputRegisters(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, uint16_t *destination, uint32_t timeoutMs)
{
    return _impl.readInputRegisters(targetSlaveId, startAddress, quantity, destination, timeoutMs);
}

bool JWPLC_ModbusRTUClass::writeSingleCoil(uint8_t targetSlaveId, uint16_t address, bool value, uint32_t timeoutMs)
{
    return _impl.writeSingleCoil(targetSlaveId, address, value, timeoutMs);
}

bool JWPLC_ModbusRTUClass::writeSingleRegister(uint8_t targetSlaveId, uint16_t address, uint16_t value, uint32_t timeoutMs)
{
    return _impl.writeSingleRegister(targetSlaveId, address, value, timeoutMs);
}

bool JWPLC_ModbusRTUClass::writeMultipleCoils(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, const uint8_t *sourcePacked, uint32_t timeoutMs)
{
    return _impl.writeMultipleCoils(targetSlaveId, startAddress, quantity, sourcePacked, timeoutMs);
}

bool JWPLC_ModbusRTUClass::writeMultipleRegisters(uint8_t targetSlaveId, uint16_t startAddress, uint16_t quantity, const uint16_t *source, uint32_t timeoutMs)
{
    return _impl.writeMultipleRegisters(targetSlaveId, startAddress, quantity, source, timeoutMs);
}

void JWPLC_ModbusRTUClass::setCrcLookupEnabled(bool enabled)
{
    JW_ModbusRTU::setCrcLookupEnabled(enabled);
}

bool JWPLC_ModbusRTUClass::crcLookupEnabled()
{
    return JW_ModbusRTU::crcLookupEnabled();
}

uint16_t JWPLC_ModbusRTUClass::crc16(const uint8_t *data, size_t length)
{
    return JW_ModbusRTU::crc16(data, length);
}

bool JWPLC_ModbusRTUClass::checkCRC(const uint8_t *frame, size_t length)
{
    return JW_ModbusRTU::checkCRC(frame, length);
}

void JWPLC_ModbusRTUClass::appendCRC(uint8_t *frame, size_t payloadLength)
{
    JW_ModbusRTU::appendCRC(frame, payloadLength);
}

JWPLCModbusRTUError JWPLC_ModbusRTUClass::lastError() const
{
    return static_cast<JWPLCModbusRTUError>(_impl.lastError());
}

const char * JWPLC_ModbusRTUClass::lastErrorString() const
{
    return _impl.lastErrorString();
}

const char * JWPLC_ModbusRTUClass::configString() const
{
    return _impl.configString();
}

const JWPLCModbusRTUStats & JWPLC_ModbusRTUClass::stats() const
{
    return _impl.stats();
}

void JWPLC_ModbusRTUClass::resetStats()
{
    _impl.resetStats();
}

void JWPLC_ModbusRTUClass::printStatus(Print &out) const
{
    _impl.printStatus(out);
}
