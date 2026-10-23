#ifndef JW_MODBUS_RTU_H
#define JW_MODBUS_RTU_H

#include <Arduino.h>
#include <JW_RS485.h>

// Selector de motor del Master.
// ASYNC: request*() inicia la transaccion y retorna; task() la completa.
// SYNC: las funciones unificadas esperan la respuesta (bloqueante con timeout).
enum JWModbusMotor : uint8_t
{
    JW_MODBUS_SYNC = 0,
    JW_MODBUS_ASYNC = 1
};

#ifndef JW_MODBUS_RTU_DEFAULT_BAUD
#define JW_MODBUS_RTU_DEFAULT_BAUD 19200UL
#endif

#ifndef JW_MODBUS_RTU_DEFAULT_CONFIG
#define JW_MODBUS_RTU_DEFAULT_CONFIG SERIAL_8E1
#endif

#ifndef JW_MODBUS_RTU_DEFAULT_SLAVE_ID
#define JW_MODBUS_RTU_DEFAULT_SLAVE_ID 1
#endif

#ifndef JW_MODBUS_RTU_MAX_FRAME
#define JW_MODBUS_RTU_MAX_FRAME 256
#endif

enum JWModbusRTUError : uint8_t
{
    JW_MODBUS_OK = 0,
    JW_MODBUS_DISABLED,
    JW_MODBUS_NOT_STARTED,
    JW_MODBUS_INVALID_SLAVE_ID,
    JW_MODBUS_INVALID_REGISTER_MAP,
    JW_MODBUS_TIMEOUT,
    JW_MODBUS_CRC_ERROR,
    JW_MODBUS_EXCEPTION,
    JW_MODBUS_INVALID_RESPONSE,
    JW_MODBUS_BUFFER_OVERFLOW,
    JW_MODBUS_UNSUPPORTED_FUNCTION,
    JW_MODBUS_BUSY,
    JW_MODBUS_TRANSPORT_ERROR
};

enum JWModbusExceptionCode : uint8_t
{
    JW_MODBUS_EX_ILLEGAL_FUNCTION = 0x01,
    JW_MODBUS_EX_ILLEGAL_DATA_ADDRESS = 0x02,
    JW_MODBUS_EX_ILLEGAL_DATA_VALUE = 0x03,
    JW_MODBUS_EX_SLAVE_DEVICE_FAILURE = 0x04
};

enum JWModbusMasterState : uint8_t
{
    JW_MODBUS_MASTER_IDLE = 0,
    JW_MODBUS_MASTER_WAIT_RESPONSE,
    JW_MODBUS_MASTER_DONE,
    JW_MODBUS_MASTER_ERROR
};

struct JWModbusRTUStats
{
    uint32_t rxFrames;
    uint32_t txFrames;
    uint32_t requestsOk;
    uint32_t crcErrors;
    uint32_t exceptionsSent;
    uint32_t masterTimeouts;

    // H3B.3: contabilidad de transporte/framing para diagnostico.
    // Aditivo: no altera la semantica de los contadores historicos.
    uint64_t rxBytes;
    uint64_t txBytes;
    uint32_t serverDiscardedTails;
    uint64_t serverDiscardedBytes;

    // H3C.1: forma temporal de los tails descartados.
    uint32_t serverDiscardedLastLength;
    uint32_t serverDiscardedLastAgeUs;
    uint32_t serverDiscardedMaxAgeUs;
    uint32_t serverDiscardedLen1;
    uint32_t serverDiscardedLen2;
    uint32_t serverDiscardedLen3;
    uint32_t serverDiscardedLen4;
    uint32_t serverDiscardedLen5;
    uint32_t serverDiscardedLen6;
    uint32_t serverDiscardedLen7;
    uint32_t serverDiscardedLen8;
    uint32_t serverDiscardedLenGt8;
};

class JW_ModbusRTU
{
public:
    explicit JW_ModbusRTU(JW_RS485 &port);

    // Puerto RS-485 usado como transporte.
    JW_RS485 &port();

    bool begin();
    bool begin(uint8_t slaveId);
    bool begin(uint8_t slaveId, uint32_t baud, uint32_t config);
    void end();

    bool isReady() const;
    uint8_t slaveId() const;
    uint32_t baudRate() const;
    uint32_t effectiveBaudRate() const;
    uint32_t config() const;

    // Selector publico de motor. ASYNC es el default del package.
    // Retorna false si se intenta cambiar durante una transaccion activa.
    bool motor(JWModbusMotor mode);
    JWModbusMotor motor() const;
    bool asyncMotor() const;

    // Compatibilidad historica en milisegundos.
    void setFrameGapMs(uint16_t gapMs);
    uint16_t frameGapMs() const;

    // Timing fino; el motor interno de framing usa microsegundos.
    void setFrameGapUs(uint32_t gapUs);
    uint32_t frameGapUs() const;

    // F3: seleccion de transporte TX. El default sigue siendo bloqueante
    // para conservar compatibilidad hasta cerrar la qualification.
    void setQueuedTxEnabled(bool enabled);
    bool queuedTxEnabled() const;
    bool queuedTxActive() const;

    // H3B: ruta RX por bloques. Se mantiene desactivada por defecto hasta
    // cerrar qualification; la ruta byte-a-byte histórica sigue disponible.
    void setBulkRxEnabled(bool enabled);
    bool bulkRxEnabled() const;

    // H3C: framing estructural del Slave. Permite despachar requests locales
    // completas sin esperar frameGap y conservar prefijos incompletos durante
    // una ventana de recuperacion. Desactivado por defecto hasta qualification.
    void setEarlyServerDispatchEnabled(bool enabled);
    bool earlyServerDispatchEnabled() const;

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

    bool requestWriteMultipleRegisters(uint8_t targetSlaveId,
                                       uint16_t startAddress,
                                       uint16_t quantity,
                                       const uint16_t *source,
                                       uint32_t timeoutMs = 1000);

    bool masterBusy() const;
    bool masterDone() const;
    bool masterSucceeded() const;
    JWModbusMasterState masterState() const;
    JWModbusRTUError masterResult() const;
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

    bool writeMultipleRegistersSync(uint8_t targetSlaveId,
                                    uint16_t startAddress,
                                    uint16_t quantity,
                                    const uint16_t *source,
                                    uint32_t timeoutMs = 1000);

    // API unificada por funcion. El nombre no cambia entre motores:
    // ASYNC -> inicia la transaccion y retorna si fue aceptada.
    // SYNC  -> retorna al completar la transaccion.
    bool readCoils(uint8_t targetSlaveId,
                   uint16_t startAddress,
                   uint16_t quantity,
                   uint8_t *destinationPacked,
                   uint32_t timeoutMs = 1000);

    bool readDiscreteInputs(uint8_t targetSlaveId,
                            uint16_t startAddress,
                            uint16_t quantity,
                            uint8_t *destinationPacked,
                            uint32_t timeoutMs = 1000);

    bool readHoldingRegisters(uint8_t targetSlaveId,
                              uint16_t startAddress,
                              uint16_t quantity,
                              uint16_t *destination,
                              uint32_t timeoutMs = 1000);

    bool readInputRegisters(uint8_t targetSlaveId,
                            uint16_t startAddress,
                            uint16_t quantity,
                            uint16_t *destination,
                            uint32_t timeoutMs = 1000);

    bool writeSingleCoil(uint8_t targetSlaveId,
                         uint16_t address,
                         bool value,
                         uint32_t timeoutMs = 1000);

    bool writeSingleRegister(uint8_t targetSlaveId,
                             uint16_t address,
                             uint16_t value,
                             uint32_t timeoutMs = 1000);

    bool writeMultipleCoils(uint8_t targetSlaveId,
                            uint16_t startAddress,
                            uint16_t quantity,
                            const uint8_t *sourcePacked,
                            uint32_t timeoutMs = 1000);

    bool writeMultipleRegisters(uint8_t targetSlaveId,
                                uint16_t startAddress,
                                uint16_t quantity,
                                const uint16_t *source,
                                uint32_t timeoutMs = 1000);

    // H3D: selector CRC para qualification. BITWISE sigue siendo el
    // default hasta cerrar el A/B de rendimiento.
    static void setCrcLookupEnabled(bool enabled);
    static bool crcLookupEnabled();

    static uint16_t crc16(const uint8_t *data, size_t length);
    static bool checkCRC(const uint8_t *frame, size_t length);
    static void appendCRC(uint8_t *frame, size_t payloadLength);

    JWModbusRTUError lastError() const;
    const char *lastErrorString() const;
    const char *configString() const;
    const JWModbusRTUStats &stats() const;
    void resetStats();
    void printStatus(Print &out) const;

private:
    enum JWModbusMasterOperation : uint8_t
    {
        JW_MODBUS_MASTER_OP_NONE = 0,
        JW_MODBUS_MASTER_OP_READ_COILS,
        JW_MODBUS_MASTER_OP_READ_DISCRETE_INPUTS,
        JW_MODBUS_MASTER_OP_READ_HOLDING_REGISTERS,
        JW_MODBUS_MASTER_OP_READ_INPUT_REGISTERS,
        JW_MODBUS_MASTER_OP_WRITE_SINGLE_COIL,
        JW_MODBUS_MASTER_OP_WRITE_SINGLE_REGISTER,
        JW_MODBUS_MASTER_OP_WRITE_MULTIPLE_COILS,
        JW_MODBUS_MASTER_OP_WRITE_MULTIPLE_REGISTERS
    };

    JW_RS485 *_port;
    bool _ready;
    uint8_t _slaveId;
    uint32_t _baud;
    uint32_t _config;
    uint16_t _frameGapMs;
    uint32_t _frameGapUs;
    JWModbusMotor _motor;
    bool _queuedTxEnabled;
    bool _bulkRxEnabled;
    bool _earlyServerDispatchEnabled;

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

    uint8_t _rxBuffer[JW_MODBUS_RTU_MAX_FRAME];
    uint16_t _rxLength;
    uint32_t _lastByteUs;

    JWModbusRTUError _lastError;
    JWModbusRTUStats _stats;

    JWModbusMasterState _masterState;
    JWModbusMasterOperation _masterOperation;
    JWModbusRTUError _masterResult;
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
    size_t writeTransport(const uint8_t *buffer, size_t size);
    void setError(JWModbusRTUError error);
    void clearError();

    void pollServer();
    void pollMaster();
    void resetMasterContext();
    void completeMasterTransaction(JWModbusRTUError result);
    bool processMasterFrame(const uint8_t *frame, uint16_t length);
    void drainRs485();
    bool finishSyncRequest(bool started);

    bool startReadBitsRequest(uint8_t functionCode,
                              JWModbusMasterOperation operation,
                              uint8_t targetSlaveId,
                              uint16_t startAddress,
                              uint16_t quantity,
                              uint8_t *destinationPacked,
                              uint32_t timeoutMs);

    bool startReadRegistersRequest(uint8_t functionCode,
                                   JWModbusMasterOperation operation,
                                   uint8_t targetSlaveId,
                                   uint16_t startAddress,
                                   uint16_t quantity,
                                   uint16_t *destination,
                                   uint32_t timeoutMs);

    bool startWriteSingleRequest(uint8_t functionCode,
                                 JWModbusMasterOperation operation,
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

    static bool _crcLookupEnabled;
    static uint16_t crc16Bitwise(const uint8_t *data, size_t length);
    static uint16_t crc16Lookup(const uint8_t *data, size_t length);
};


#endif // JW_MODBUS_RTU_H
