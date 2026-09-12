#ifndef JWPLC_MODBUS_TCP_CLIENT_H
#define JWPLC_MODBUS_TCP_CLIENT_H

#include <Arduino.h>
#include <IPAddress.h>
#include <JWPLC_Ethernet.h>
#include <jwplc_ethernet_async_tx.h>

#ifndef JWPLC_MODBUS_TCP_CLIENT_DEFAULT_PORT
#define JWPLC_MODBUS_TCP_CLIENT_DEFAULT_PORT 502
#endif

#ifndef JWPLC_MODBUS_TCP_CLIENT_DEFAULT_UNIT_ID
#define JWPLC_MODBUS_TCP_CLIENT_DEFAULT_UNIT_ID 1
#endif

#ifndef JWPLC_MODBUS_TCP_CLIENT_MAX_ADU
#define JWPLC_MODBUS_TCP_CLIENT_MAX_ADU 260
#endif

#ifndef JWPLC_MODBUS_TCP_CLIENT_RX_BUDGET
#define JWPLC_MODBUS_TCP_CLIENT_RX_BUDGET 64
#endif

enum JWPLCModbusTCPClientError : uint8_t
{
    JWPLC_MODBUS_TCP_CLIENT_OK = 0,
    JWPLC_MODBUS_TCP_CLIENT_NOT_CONFIGURED,
    JWPLC_MODBUS_TCP_CLIENT_ETHERNET_NOT_READY,
    JWPLC_MODBUS_TCP_CLIENT_BUS_LOCK_TIMEOUT,
    JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT,
    JWPLC_MODBUS_TCP_CLIENT_BUSY,
    JWPLC_MODBUS_TCP_CLIENT_TIMEOUT,
    JWPLC_MODBUS_TCP_CLIENT_TRANSPORT_ERROR,
    JWPLC_MODBUS_TCP_CLIENT_INVALID_MBAP,
    JWPLC_MODBUS_TCP_CLIENT_TRANSACTION_MISMATCH,
    JWPLC_MODBUS_TCP_CLIENT_UNIT_ID_MISMATCH,
    JWPLC_MODBUS_TCP_CLIENT_FUNCTION_MISMATCH,
    JWPLC_MODBUS_TCP_CLIENT_EXCEPTION,
    JWPLC_MODBUS_TCP_CLIENT_INVALID_RESPONSE
};

enum JWPLCModbusTCPClientState : uint8_t
{
    JWPLC_MODBUS_TCP_CLIENT_IDLE = 0,
    JWPLC_MODBUS_TCP_CLIENT_WAIT_ETHERNET,
    JWPLC_MODBUS_TCP_CLIENT_CONNECTING,
    JWPLC_MODBUS_TCP_CLIENT_SENDING,
    JWPLC_MODBUS_TCP_CLIENT_WAIT_RESPONSE,
    JWPLC_MODBUS_TCP_CLIENT_DONE,
    JWPLC_MODBUS_TCP_CLIENT_ERROR
};

struct JWPLCModbusTCPClientStats
{
    uint32_t connections;
    uint32_t txFrames;
    uint32_t rxFrames;
    uint32_t requestsOk;
    uint32_t exceptions;
    uint32_t timeouts;
    uint32_t transportErrors;
    uint32_t protocolErrors;
    uint32_t busLockTimeouts;
};

class JWPLC_ModbusTCPClientClass
{
public:
    JWPLC_ModbusTCPClientClass();

    // Configura el endpoint remoto. La conexión se abre de forma lazy al
    // iniciar la primera request y luego se conserva para requests sucesivas.
    bool begin(IPAddress serverIP,
               uint8_t unitId = JWPLC_MODBUS_TCP_CLIENT_DEFAULT_UNIT_ID,
               uint16_t port = JWPLC_MODBUS_TCP_CLIENT_DEFAULT_PORT);
    void end();

    // Motor cooperativo. Debe llamarse con alta frecuencia mientras busy().
    void task();
    void poll();

    // Lecturas de registros. FC03/FC04 comparten forma de respuesta.
    bool requestReadHoldingRegisters(uint16_t startAddress,
                                     uint16_t quantity,
                                     uint16_t *destination,
                                     uint32_t timeoutMs = 1000);

    bool requestReadInputRegisters(uint16_t startAddress,
                                   uint16_t quantity,
                                   uint16_t *destination,
                                   uint32_t timeoutMs = 1000);

    // Writes single. FC05/FC06 comparten respuesta echo address + value.
    bool requestWriteSingleCoil(uint16_t address,
                                bool value,
                                uint32_t timeoutMs = 1000);

    bool requestWriteSingleRegister(uint16_t address,
                                    uint16_t value,
                                    uint32_t timeoutMs = 1000);

    bool configured() const;
    bool sessionConnected() const;
    bool busy() const;
    bool done() const;
    bool succeeded() const;
    JWPLCModbusTCPClientState state() const;
    JWPLCModbusTCPClientError result() const;
    uint8_t exceptionCode() const;
    uint16_t transactionId() const;
    IPAddress serverIP() const;
    uint16_t serverPort() const;
    uint8_t unitId() const;
    void clearResult();

    const JWPLCModbusTCPClientStats &stats() const;
    void resetStats();
    const char *resultString() const;
    void printStatus(Print &out) const;

private:
    enum Operation : uint8_t
    {
        OP_NONE = 0,
        OP_READ_HOLDING_REGISTERS,
        OP_WRITE_SINGLE_REGISTER
    };

    bool _configured;
    IPAddress _serverIP;
    uint16_t _serverPort;
    uint8_t _unitId;

    EthernetClient _tcpClient;
    JWPLC_EthernetAsyncTx _asyncTx;
    bool _sessionConnected;
    bool _connectStarted;
    bool _sendStarted;

    JWPLCModbusTCPClientState _state;
    JWPLCModbusTCPClientError _result;
    Operation _operation;
    uint8_t _exceptionCode;

    uint16_t _nextTransactionId;
    uint16_t _activeTransactionId;
    uint8_t _expectedFunction;
    uint16_t _startAddress;
    uint16_t _quantity;
    uint16_t _writeValue;
    uint16_t *_registerDestination;

    uint32_t _requestStartMs;
    uint32_t _timeoutMs;

    uint8_t _txBuffer[JWPLC_MODBUS_TCP_CLIENT_MAX_ADU];
    uint16_t _txLength;
    uint8_t _rxBuffer[JWPLC_MODBUS_TCP_CLIENT_MAX_ADU];
    uint16_t _rxLength;
    uint16_t _expectedRxLength;

    JWPLCModbusTCPClientStats _stats;

    bool acquireBus(uint32_t timeoutMs);
    void releaseBus();
    void resetRx();
    void resetTransactionContext();
    void closeSession();
    void fail(JWPLCModbusTCPClientError error, bool closeTransport);
    void completeSuccess();

    bool startRequest(Operation operation,
                      uint8_t functionCode,
                      uint16_t startAddress,
                      uint16_t quantity,
                      uint16_t writeValue,
                      uint16_t *destination,
                      uint32_t timeoutMs);

    void serviceConnection();
    void serviceSend();
    void serviceResponse();
    bool processResponse();

    void buildReadHoldingRequest();
    void buildWriteSingleRegisterRequest();

    static uint16_t readU16BE(const uint8_t *p);
    static void writeU16BE(uint8_t *p, uint16_t value);
};

extern JWPLC_ModbusTCPClientClass JWPLC_ModbusTCPClient;

#endif // JWPLC_MODBUS_TCP_CLIENT_H
