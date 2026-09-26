#ifndef JWPLC_MODBUS_TCP_H
#define JWPLC_MODBUS_TCP_H

#include <Arduino.h>
#include <IPAddress.h>
#include <JWPLC_Ethernet.h>
#include "JWPLC_ModbusTCP_Client.h"

#ifndef JWPLC_MODBUS_TCP_DEFAULT_PORT
#define JWPLC_MODBUS_TCP_DEFAULT_PORT 502
#endif

#ifndef JWPLC_MODBUS_TCP_DEFAULT_UNIT_ID
#define JWPLC_MODBUS_TCP_DEFAULT_UNIT_ID 1
#endif

#ifndef JWPLC_MODBUS_TCP_MAX_ADU
#define JWPLC_MODBUS_TCP_MAX_ADU 260
#endif

#ifndef JWPLC_MODBUS_TCP_FRAME_TIMEOUT_MS
#define JWPLC_MODBUS_TCP_FRAME_TIMEOUT_MS 1000UL
#endif

#ifndef JWPLC_MODBUS_TCP_RX_BUDGET
#define JWPLC_MODBUS_TCP_RX_BUDGET 64
#endif

enum JWPLCModbusTCPError : uint8_t
{
    JWPLC_MODBUS_TCP_OK = 0,
    JWPLC_MODBUS_TCP_NOT_STARTED,
    JWPLC_MODBUS_TCP_ETHERNET_NOT_READY,
    JWPLC_MODBUS_TCP_BUS_LOCK_TIMEOUT,
    JWPLC_MODBUS_TCP_INVALID_MBAP,
    JWPLC_MODBUS_TCP_INVALID_LENGTH,
    JWPLC_MODBUS_TCP_UNIT_ID_MISMATCH,
    JWPLC_MODBUS_TCP_INVALID_REGISTER_MAP,
    JWPLC_MODBUS_TCP_EXCEPTION,
    JWPLC_MODBUS_TCP_TIMEOUT,
    JWPLC_MODBUS_TCP_TRANSPORT_ERROR,
    JWPLC_MODBUS_TCP_BUSY
};

enum JWPLCModbusTCPServerState : uint8_t
{
    JWPLC_MODBUS_TCP_SERVER_STOPPED = 0,
    JWPLC_MODBUS_TCP_SERVER_WAIT_ETHERNET,
    JWPLC_MODBUS_TCP_SERVER_LISTENING,
    JWPLC_MODBUS_TCP_SERVER_CLIENT_ACTIVE,
    JWPLC_MODBUS_TCP_SERVER_ERROR
};

enum JWPLCModbusTCPExceptionCode : uint8_t
{
    JWPLC_MODBUS_TCP_EX_ILLEGAL_FUNCTION = 0x01,
    JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_ADDRESS = 0x02,
    JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_VALUE = 0x03,
    JWPLC_MODBUS_TCP_EX_SERVER_DEVICE_FAILURE = 0x04
};

struct JWPLCModbusTCPStats
{
    uint32_t clientConnections;
    uint32_t rxFrames;
    uint32_t txFrames;
    uint32_t requestsOk;
    uint32_t exceptionsSent;
    uint32_t protocolErrors;
    uint32_t frameTimeouts;
    uint32_t busLockTimeouts;
};

class JWPLC_ModbusTCPClass
{
public:
    JWPLC_ModbusTCPClass();

    // Server Modbus TCP. beginServer() no fuerza una inicialización Ethernet
    // síncrona: si el autoload todavía está obteniendo DHCP, task() espera
    // cooperativamente hasta que JWPLC_Ethernet esté READY.
    // En A14.1 el servidor se configura una vez durante setup(). El cierre
    // explícito de sockets se añadirá junto al backend cooperativo de A14.2.
    bool beginServer(uint8_t unitId = JWPLC_MODBUS_TCP_DEFAULT_UNIT_ID,
                     uint16_t port = JWPLC_MODBUS_TCP_DEFAULT_PORT);

    // Debe llamarse con alta frecuencia desde loop() mientras el servicio esté
    // habilitado. Cada pasada limita el trabajo de RX para no monopolizar SPI.
    void task();
    void poll();

    bool serverEnabled() const;
    bool serverReady() const;
    bool clientConnected() const;
    uint8_t unitId() const;
    uint16_t port() const;
    JWPLCModbusTCPServerState serverState() const;

    void setFrameTimeoutMs(uint32_t timeoutMs);
    uint32_t frameTimeoutMs() const;

    // Mapas Server. Coils y Discrete Inputs usan bits empaquetados LSB-first:
    // bit 0 del byte 0 = dirección 0, bit 1 = dirección 1, etc.
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

    JWPLCModbusTCPError lastError() const;
    const char *lastErrorString() const;
    const JWPLCModbusTCPStats &stats() const;
    void resetStats();
    void printStatus(Print &out) const;

private:
    bool _serverEnabled;
    bool _serverListening;
    bool _clientActive;
    uint8_t _unitId;
    uint16_t _port;
    uint32_t _frameTimeoutMs;
    uint32_t _lastRxMs;
    uint32_t _lastListenAttemptMs;

    EthernetServer _server;
    EthernetClient _client;

    uint8_t *_coils;
    uint16_t _coilCount;
    const uint8_t *_discreteInputs;
    uint16_t _discreteInputCount;
    uint16_t *_holdingRegisters;
    uint16_t _holdingCount;
    const uint16_t *_inputRegisters;
    uint16_t _inputCount;

    uint8_t _rxBuffer[JWPLC_MODBUS_TCP_MAX_ADU];
    uint16_t _rxLength;
    uint16_t _expectedLength;
    uint8_t _txBuffer[JWPLC_MODBUS_TCP_MAX_ADU];

    JWPLCModbusTCPServerState _serverState;
    JWPLCModbusTCPError _lastError;
    JWPLCModbusTCPStats _stats;

    bool acquireBus(uint32_t timeoutMs);
    void releaseBus();
    void setError(JWPLCModbusTCPError error);
    void clearError();
    void resetRx();
    void dropClient();
    void ensureServerListening();
    void serviceServer();

    bool processRequest(uint16_t aduLength, uint16_t &responseLength);
    bool buildException(uint8_t functionCode,
                        JWPLCModbusTCPExceptionCode exceptionCode,
                        uint16_t &responseLength);
    bool sendResponse(uint16_t responseLength);

    bool processReadBits(uint8_t functionCode,
                         const uint8_t *map,
                         uint16_t mapCount,
                         const uint8_t *pdu,
                         uint16_t pduLength,
                         uint16_t &responseLength);
    bool processReadRegisters(uint8_t functionCode,
                              const uint16_t *map,
                              uint16_t mapCount,
                              const uint8_t *pdu,
                              uint16_t pduLength,
                              uint16_t &responseLength);
    bool processWriteSingleCoil(const uint8_t *pdu,
                                uint16_t pduLength,
                                uint16_t &responseLength);
    bool processWriteSingleRegister(const uint8_t *pdu,
                                    uint16_t pduLength,
                                    uint16_t &responseLength);
    bool processWriteMultipleCoils(const uint8_t *pdu,
                                   uint16_t pduLength,
                                   uint16_t &responseLength);
    bool processWriteMultipleRegisters(const uint8_t *pdu,
                                       uint16_t pduLength,
                                       uint16_t &responseLength);

    static uint16_t readU16BE(const uint8_t *p);
    static void writeU16BE(uint8_t *p, uint16_t value);
    static bool rangeValid(uint16_t start, uint16_t quantity, uint16_t count);
    static bool getPackedBit(const uint8_t *bits, uint16_t address);
    static void setPackedBit(uint8_t *bits, uint16_t address, bool value);
};

extern JWPLC_ModbusTCPClass JWPLC_ModbusTCP;

#endif // JWPLC_MODBUS_TCP_H
