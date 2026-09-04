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
    JWPLC_MODBUS_UNSUPPORTED_FUNCTION
};

enum JWPLCModbusExceptionCode : uint8_t
{
    JWPLC_MODBUS_EX_ILLEGAL_FUNCTION = 0x01,
    JWPLC_MODBUS_EX_ILLEGAL_DATA_ADDRESS = 0x02,
    JWPLC_MODBUS_EX_ILLEGAL_DATA_VALUE = 0x03,
    JWPLC_MODBUS_EX_SLAVE_DEVICE_FAILURE = 0x04
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

    void setHoldingRegisters(uint16_t *registers, uint16_t count);
    uint16_t holdingRegisterCount() const;

    bool getHoldingRegister(uint16_t address, uint16_t &value) const;
    bool setHoldingRegister(uint16_t address, uint16_t value);

    void task();
    void poll();

    bool readHoldingRegisters(uint8_t targetSlaveId,
                              uint16_t startAddress,
                              uint16_t quantity,
                              uint16_t *destination,
                              uint32_t timeoutMs = 1000);

    bool writeSingleRegister(uint8_t targetSlaveId,
                             uint16_t address,
                             uint16_t value,
                             uint32_t timeoutMs = 1000);

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
    bool _ready;
    uint8_t _slaveId;
    uint32_t _baud;
    uint32_t _config;
    uint16_t _frameGapMs;

    uint16_t *_holdingRegisters;
    uint16_t _holdingCount;

    uint8_t _rxBuffer[JWPLC_MODBUS_RTU_MAX_FRAME];
    uint16_t _rxLength;
    uint32_t _lastByteMs;

    JWPLCModbusRTUError _lastError;
    JWPLCModbusRTUStats _stats;

    void clearRxBuffer();
    void setError(JWPLCModbusRTUError error);
    void clearError();

    bool processServerFrame(const uint8_t *frame, uint16_t length);
    void sendException(uint8_t functionCode, uint8_t exceptionCode);
    void sendFrame(uint8_t *frame, uint16_t payloadLength);

    void handleReadHoldingRegisters(const uint8_t *frame, uint16_t length, bool broadcast);
    void handleWriteSingleRegister(const uint8_t *frame, uint16_t length, bool broadcast);
    void handleWriteMultipleRegisters(const uint8_t *frame, uint16_t length, bool broadcast);

    bool waitMasterResponse(uint8_t *buffer,
                            uint16_t maxLength,
                            uint16_t &length,
                            uint32_t timeoutMs);
};

extern JWPLC_ModbusRTUClass JWPLC_ModbusRTU;

#endif // JWPLC_MODBUS_RTU_H
