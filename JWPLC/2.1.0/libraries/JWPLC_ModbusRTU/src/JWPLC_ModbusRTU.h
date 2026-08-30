#ifndef JWPLC_MODBUS_RTU_H
#define JWPLC_MODBUS_RTU_H

#include <Arduino.h>
#include <JWPLC_RS485.h>

#ifndef JWPLC_MODBUS_RTU_DEFAULT_BAUD
#define JWPLC_MODBUS_RTU_DEFAULT_BAUD 19200UL
#endif

#ifndef JWPLC_MODBUS_RTU_DEFAULT_CONFIG
#define JWPLC_MODBUS_RTU_DEFAULT_CONFIG SERIAL_8E1
#endif

#ifndef JWPLC_MODBUS_RTU_DEFAULT_SLAVE_ID
#define JWPLC_MODBUS_RTU_DEFAULT_SLAVE_ID 1
#endif

#ifndef JWPLC_MODBUS_RTU_MAX_FRAME
#define JWPLC_MODBUS_RTU_MAX_FRAME 256
#endif

enum JWPLCModbusRTUError : uint8_t
{
    JWPLC_MODBUS_OK = 0,
    JWPLC_MODBUS_DISABLED,
    JWPLC_MODBUS_NOT_STARTED,
    JWPLC_MODBUS_INVALID_SLAVE_ID,
    JWPLC_MODBUS_INVALID_REGISTER_MAP,
    JWPLC_MODBUS_TIMEOUT,
    JWPLC_MODBUS_CRC_ERROR,
    JWPLC_MODBUS_EXCEPTION,
    JWPLC_MODBUS_INVALID_RESPONSE,
    JWPLC_MODBUS_BUFFER_OVERFLOW,
    JWPLC_MODBUS_UNSUPPORTED_FUNCTION,
    JWPLC_MODBUS_BUSY,
    JWPLC_MODBUS_TRANSPORT_ERROR
};

enum JWPLCModbusExceptionCode : uint8_t
{
    JWPLC_MODBUS_EX_ILLEGAL_FUNCTION = 0x01,
    JWPLC_MODBUS_EX_ILLEGAL_DATA_ADDRESS = 0x02,
    JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE = 0x03,
    JWPLC_MODBUS_EX_SLAVE_DEVICE_FAILURE = 0x04
};

enum JWPLCModbusMasterState : uint8_t
{
    JWPLC_MODBUS_MASTER_IDLE = 0,
    JWPLC_MODBUS_MASTER_WAIT_RESPONSE,
    JWPLC_MODBUS_MASTER_DONE,
    JWPLC_MODBUS_MASTER_ERROR
};

struct JWPLCModbusRTUStats
{
    uint32_t rxFrames;
    uint32_t txFrames;
    uint32_t requestsOk;
    uint32_t crcErrors;
    uint32_t exceptionsSent;
    uint32_t masterTimeouts;
};

class JWPLC_ModbusRTUClass
{
public:
    JWPLC_ModbusRTUClass();

    bool begin();
    bool begin(uint8_t slaveId);
    bool begin(uint8_t slaveId, uint32_t baud, uint32_t config);
    void end();

    bool isReady() const;
    uint8_t slaveId() const;
    uint32_t baudRate() const;
    uint32_t config() const;

    void setFrameGapMs(uint16_t gapMs);
    uint16_t frameGapMs() const;

    // Mapas Slave. Coils y Discrete Inputs usan bits empaquetados LSB-first:
    // bit 0 del byte 0 = direccion 0, bit 1 = direccion 1, etc.
    void setCoils(uint8_t *bits, uint16_t count);
    uint16_t coilCount() const;
    bool getCoil(uint16_t address, bool &value) const;
    bool setCoil(uint16_t address, bool value);

    void setDiscreteInputs(const uint8_t *bits, uint16_t count);
    uint16_t discreteInputCount() const;
    bool getDiscreteInput(uint16_t address, bool &value) const;

    void setHoldingRegisters(uint16_t *registers, uint16_t count);
    uint16_t holdingRegisterCount() const;
    bool getHoldingRegister(uint16_t address, uint16_t &value) const;
    bool setHoldingRegister(uint16_t address, uint16_t value);

    void setInputRegisters(const uint16_t *registers, uint16_t count);
    uint16_t inputRegisterCount() const;
    bool getInputRegister(uint16_t address, uint16_t &value) const;

    // Actividad Slave util para Remote I/O / fail-safe.
    bool hasValidRequest() const;
    uint32_t lastValidRequestMs() const;
    bool hasCoilWrite() const;
    uint32_t lastCoilWriteMs() const;

    // Motor cooperativo. En modo Master debe llamarse con alta frecuencia
    // mientras haya una transaccion pendiente.
    void task();
    void poll();

    // API Master principal: inicia la transaccion y retorna de inmediato.
    // true = solicitud aceptada/iniciada, no significa que ya termino.
    // Destinos/sources de bits son buffers empaquetados LSB-first.
    bool requestReadCoils(uint8_t targetSlaveId,
                          uint16_t startAddress,
                          uint16_t quantity,
                          uint8_t *destinationPacked,
                          uint32_t timeoutMs = 1000);

    bool requestReadDiscreteInputs(uint8_t targetSlaveId,
                                   uint16_t startAddress,
                                   uint16_t quantity,
                                   uint8_t *destinationPacked,
                                   uint32_t timeoutMs = 1000);

    bool requestReadHoldingRegisters(uint8_t targetSlaveId,
                                     uint16_t startAddress,
                                     uint16_t quantity,
                                     uint16_t *destination,
                                     uint32_t timeoutMs = 1000);

    bool requestReadInputRegisters(uint8_t targetSlaveId,
                                   uint16_t startAddress,
                                   uint16_t quantity,
                                   uint16_t *destination,
                                   uint32_t timeoutMs = 1000);

    bool requestWriteSingleCoil(uint8_t targetSlaveId,
                                uint16_t address,
                                bool value,
                                uint32_t timeoutMs = 1000);

    bool requestWriteSingleRegister(uint8_t targetSlaveId,
                                    uint16_t address,
                                    uint16_t value,
                                    uint32_t timeoutMs = 1000);

    bool requestWriteMultipleCoils(uint8_t targetSlaveId,
                                   uint16_t startAddress,
                                   uint16_t quantity,
                                   const uint8_t *sourcePacked,
                                   uint32_t timeoutMs = 1000);

    bool masterBusy() const;
    bool masterDone() const;
    bool masterSucceeded() const;
    JWPLCModbusMasterState masterState() const;
    JWPLCModbusRTUError masterResult() const;
    void clearMasterResult();

    // API Master bloqueante explicita. Implementada sobre el mismo motor
    // cooperativo para evitar dos parsers/semanticas internas divergentes.
    bool readCoilsSync(uint8_t targetSlaveId,
                       uint16_t startAddress,
                       uint16_t quantity,
                       uint8_t *destinationPacked,
                       uint32_t timeoutMs = 1000);

    bool readDiscreteInputsSync(uint8_t targetSlaveId,
                                uint16_t startAddress,
                                uint16_t quantity,
                                uint8_t *destinationPacked,
                                uint32_t timeoutMs = 1000);

    bool readHoldingRegistersSync(uint8_t targetSlaveId,
                                  uint16_t startAddress,
                                  uint16_t quantity,
                                  uint16_t *destination,
                                  uint32_t timeoutMs = 1000);

    bool readInputRegistersSync(uint8_t targetSlaveId,
                                uint16_t startAddress,
                                uint16_t quantity,
                                uint16_t *destination,
                                uint32_t timeoutMs = 1000);

    bool writeSingleCoilSync(uint8_t targetSlaveId,
                             uint16_t address,
                             bool value,
                             uint32_t timeoutMs = 1000);

    bool writeSingleRegisterSync(uint8_t targetSlaveId,
                                 uint16_t address,
                                 uint16_t value,
                                 uint32_t timeoutMs = 1000);

    bool writeMultipleCoilsSync(uint8_t targetSlaveId,
                                uint16_t startAddress,
                                uint16_t quantity,
                                const uint8_t *sourcePacked,
                                uint32_t timeoutMs = 1000);

    // Compatibilidad temporal Alpha7 con sketches previos.
    bool readHoldingRegisters(uint8_t targetSlaveId,
                              uint16_t startAddress,
                              uint16_t quantity,
                              uint16_t *destination,
                              uint32_t timeoutMs = 1000)
    {
        return readHoldingRegistersSync(
            targetSlaveId,
            startAddress,
            quantity,
            destination,
            timeoutMs);
    }

    bool writeSingleRegister(uint8_t targetSlaveId,
                             uint16_t address,
                             uint16_t value,
                             uint32_t timeoutMs = 1000)
    {
        return writeSingleRegisterSync(
            targetSlaveId,
            address,
            value,
            timeoutMs);
    }

    static uint16_t crc16(const uint8_t *data, size_t length);
    static bool checkCRC(const uint8_t *frame, size_t length);
    static void appendCRC(uint8_t *frame, size_t payloadLength);

    JWPLCModbusRTUError lastError() const;
    const char *lastErrorString() const;
    const char *configString() const;
    const JWPLCModbusRTUStats &stats() const;
    void resetStats();
    void printStatus(Print &out) const;

private:
    enum JWPLCModbusMasterOperation : uint8_t
    {
        JWPLC_MODBUS_MASTER_OP_NONE = 0,
        JWPLC_MODBUS_MASTER_OP_READ_COILS,
        JWPLC_MODBUS_MASTER_OP_READ_DISCRETE_INPUTS,
        JWPLC_MODBUS_MASTER_OP_READ_HOLDING_REGISTERS,
        JWPLC_MODBUS_MASTER_OP_READ_INPUT_REGISTERS,
        JWPLC_MODBUS_MASTER_OP_WRITE_SINGLE_COIL,
        JWPLC_MODBUS_MASTER_OP_WRITE_SINGLE_REGISTER,
        JWPLC_MODBUS_MASTER_OP_WRITE_MULTIPLE_COILS
    };

    bool _ready;
    uint8_t _slaveId;
    uint32_t _baud;
    uint32_t _config;
    uint16_t _frameGapMs;

    uint8_t *_coils;
    uint16_t _coilCount;
    const uint8_t *_discreteInputs;
    uint16_t _discreteInputCount;
    uint16_t *_holdingRegisters;
    uint16_t _holdingCount;
    const uint16_t *_inputRegisters;
    uint16_t _inputCount;

    bool _validRequestSeen;
    uint32_t _lastValidRequestMs;
    bool _coilWriteSeen;
    uint32_t _lastCoilWriteMs;

    uint8_t _rxBuffer[JWPLC_MODBUS_RTU_MAX_FRAME];
    uint16_t _rxLength;
    uint32_t _lastByteMs;

    JWPLCModbusRTUError _lastError;
    JWPLCModbusRTUStats _stats;

    JWPLCModbusMasterState _masterState;
    JWPLCModbusMasterOperation _masterOperation;
    JWPLCModbusRTUError _masterResult;
    uint8_t _masterTargetSlaveId;
    uint8_t _masterExpectedFunction;
    uint16_t _masterStartAddress;
    uint16_t _masterQuantity;
    uint16_t *_masterRegisterDestination;
    uint8_t *_masterBitDestination;
    uint16_t _masterWriteValue;
    uint32_t _masterStartMs;
    uint32_t _masterTimeoutMs;

    void clearRxBuffer();
    void setError(JWPLCModbusRTUError error);
    void clearError();

    void pollServer();
    void pollMaster();
    void resetMasterContext();
    void completeMasterTransaction(JWPLCModbusRTUError result);
    bool processMasterFrame(const uint8_t *frame, uint16_t length);
    void drainRs485();
    bool finishSyncRequest(bool started);

    bool startReadBitsRequest(uint8_t functionCode,
                              JWPLCModbusMasterOperation operation,
                              uint8_t targetSlaveId,
                              uint16_t startAddress,
                              uint16_t quantity,
                              uint8_t *destinationPacked,
                              uint32_t timeoutMs);

    bool startReadRegistersRequest(uint8_t functionCode,
                                   JWPLCModbusMasterOperation operation,
                                   uint8_t targetSlaveId,
                                   uint16_t startAddress,
                                   uint16_t quantity,
                                   uint16_t *destination,
                                   uint32_t timeoutMs);

    bool startWriteSingleRequest(uint8_t functionCode,
                                 JWPLCModbusMasterOperation operation,
                                 uint8_t targetSlaveId,
                                 uint16_t address,
                                 uint16_t value,
                                 uint32_t timeoutMs);

    bool processServerFrame(const uint8_t *frame, uint16_t length);
    void sendException(uint8_t functionCode, uint8_t exceptionCode);
    void sendFrame(uint8_t *frame, uint16_t payloadLength);

    bool handleReadBits(const uint8_t *frame,
                        uint16_t length,
                        bool broadcast,
                        uint8_t functionCode,
                        const uint8_t *map,
                        uint16_t count);

    bool handleReadRegisters(const uint8_t *frame,
                             uint16_t length,
                             bool broadcast,
                             uint8_t functionCode,
                             const uint16_t *map,
                             uint16_t count);

    bool handleWriteSingleCoil(const uint8_t *frame,
                               uint16_t length,
                               bool broadcast);
    bool handleWriteSingleRegister(const uint8_t *frame,
                                   uint16_t length,
                                   bool broadcast);
    bool handleWriteMultipleCoils(const uint8_t *frame,
                                  uint16_t length,
                                  bool broadcast);
    bool handleWriteMultipleRegisters(const uint8_t *frame,
                                      uint16_t length,
                                      bool broadcast);

    static bool packedBit(const uint8_t *map, uint16_t address);
    static void setPackedBit(uint8_t *map, uint16_t address, bool value);
};

extern JWPLC_ModbusRTUClass JWPLC_ModbusRTU;

#endif // JWPLC_MODBUS_RTU_H
