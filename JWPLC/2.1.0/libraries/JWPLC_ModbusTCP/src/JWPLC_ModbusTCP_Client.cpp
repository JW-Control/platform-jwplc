#include "JWPLC_ModbusTCP_Client.h"

#include <string.h>

#include "jwplc_spi_bus.h"

JWPLC_ModbusTCPClientClass JWPLC_ModbusTCPClient;

JWPLC_ModbusTCPClientClass::JWPLC_ModbusTCPClientClass()
    : _configured(false),
      _serverIP(0, 0, 0, 0),
      _serverPort(JWPLC_MODBUS_TCP_CLIENT_DEFAULT_PORT),
      _unitId(JWPLC_MODBUS_TCP_CLIENT_DEFAULT_UNIT_ID),
      _tcpClient(),
      _asyncTx(),
      _sessionConnected(false),
      _connectStarted(false),
      _sendStarted(false),
      _state(JWPLC_MODBUS_TCP_CLIENT_IDLE),
      _result(JWPLC_MODBUS_TCP_CLIENT_NOT_CONFIGURED),
      _operation(OP_NONE),
      _exceptionCode(0),
      _nextTransactionId(1),
      _activeTransactionId(0),
      _expectedFunction(0),
      _startAddress(0),
      _quantity(0),
      _writeValue(0),
      _registerDestination(nullptr),
      _requestStartMs(0),
      _timeoutMs(0),
      _txLength(0),
      _rxLength(0),
      _expectedRxLength(0)
{
    memset(_txBuffer, 0, sizeof(_txBuffer));
    memset(_rxBuffer, 0, sizeof(_rxBuffer));
    memset(&_stats, 0, sizeof(_stats));
}

bool JWPLC_ModbusTCPClientClass::begin(
    IPAddress serverIP,
    uint8_t unitId,
    uint16_t port)
{
    if (busy())
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_BUSY;
        return false;
    }

#if defined(ESP8266) || defined(ESP32)
    const bool invalidIP =
        serverIP == IPAddress((uint32_t)0) ||
        serverIP == IPAddress(0xFFFFFFFFul);
#else
    const bool invalidIP =
        serverIP == IPAddress(0ul) ||
        serverIP == IPAddress(0xFFFFFFFFul);
#endif

    if (invalidIP || port == 0 || unitId == 0 || unitId > 247)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        return false;
    }

    if (_configured &&
        _serverIP == serverIP &&
        _serverPort == port &&
        _unitId == unitId)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_OK;
        return true;
    }

    closeSession();

    _serverIP = serverIP;
    _serverPort = port;
    _unitId = unitId;
    _configured = true;
    _state = JWPLC_MODBUS_TCP_CLIENT_IDLE;
    _result = JWPLC_MODBUS_TCP_CLIENT_OK;
    _exceptionCode = 0;
    resetTransactionContext();
    return true;
}

void JWPLC_ModbusTCPClientClass::end()
{
    closeSession();
    _configured = false;
    _serverIP = IPAddress(0, 0, 0, 0);
    _serverPort = JWPLC_MODBUS_TCP_CLIENT_DEFAULT_PORT;
    _unitId = JWPLC_MODBUS_TCP_CLIENT_DEFAULT_UNIT_ID;
    resetTransactionContext();
    _state = JWPLC_MODBUS_TCP_CLIENT_IDLE;
    _result = JWPLC_MODBUS_TCP_CLIENT_NOT_CONFIGURED;
}

void JWPLC_ModbusTCPClientClass::task()
{
    if (!busy())
    {
        return;
    }

    const uint32_t now = millis();
    if (_timeoutMs > 0 &&
        (uint32_t)(now - _requestStartMs) >= _timeoutMs)
    {
        fail(JWPLC_MODBUS_TCP_CLIENT_TIMEOUT, true);
        return;
    }

    if (!JWPLC_Ethernet.isReady())
    {
        // Un reset del W5500 invalida cualquier socket previo. No se toca el
        // hardware mientras el runtime Ethernet todavía no volvió a READY.
        _tcpClient = EthernetClient();
        _asyncTx.reset();
        _sessionConnected = false;
        _connectStarted = false;
        _sendStarted = false;
        _state = JWPLC_MODBUS_TCP_CLIENT_WAIT_ETHERNET;
        return;
    }

    switch (_state)
    {
    case JWPLC_MODBUS_TCP_CLIENT_WAIT_ETHERNET:
    case JWPLC_MODBUS_TCP_CLIENT_CONNECTING:
        serviceConnection();
        break;

    case JWPLC_MODBUS_TCP_CLIENT_SENDING:
        serviceSend();
        break;

    case JWPLC_MODBUS_TCP_CLIENT_WAIT_RESPONSE:
        serviceResponse();
        break;

    default:
        break;
    }
}

void JWPLC_ModbusTCPClientClass::poll()
{
    task();
}

bool JWPLC_ModbusTCPClientClass::requestReadHoldingRegisters(
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    if (destination == nullptr || quantity == 0 || quantity > 125)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        return false;
    }

    return startRequest(
        OP_READ_HOLDING_REGISTERS,
        0x03,
        startAddress,
        quantity,
        0,
        destination,
        timeoutMs);
}

bool JWPLC_ModbusTCPClientClass::requestWriteSingleRegister(
    uint16_t address,
    uint16_t value,
    uint32_t timeoutMs)
{
    return startRequest(
        OP_WRITE_SINGLE_REGISTER,
        0x06,
        address,
        1,
        value,
        nullptr,
        timeoutMs);
}

bool JWPLC_ModbusTCPClientClass::configured() const
{
    return _configured;
}

bool JWPLC_ModbusTCPClientClass::sessionConnected() const
{
    return _sessionConnected;
}

bool JWPLC_ModbusTCPClientClass::busy() const
{
    return _state == JWPLC_MODBUS_TCP_CLIENT_WAIT_ETHERNET ||
           _state == JWPLC_MODBUS_TCP_CLIENT_CONNECTING ||
           _state == JWPLC_MODBUS_TCP_CLIENT_SENDING ||
           _state == JWPLC_MODBUS_TCP_CLIENT_WAIT_RESPONSE;
}

bool JWPLC_ModbusTCPClientClass::done() const
{
    return _state == JWPLC_MODBUS_TCP_CLIENT_DONE ||
           _state == JWPLC_MODBUS_TCP_CLIENT_ERROR;
}

bool JWPLC_ModbusTCPClientClass::succeeded() const
{
    return _state == JWPLC_MODBUS_TCP_CLIENT_DONE &&
           _result == JWPLC_MODBUS_TCP_CLIENT_OK;
}

JWPLCModbusTCPClientState JWPLC_ModbusTCPClientClass::state() const
{
    return _state;
}

JWPLCModbusTCPClientError JWPLC_ModbusTCPClientClass::result() const
{
    return _result;
}

uint8_t JWPLC_ModbusTCPClientClass::exceptionCode() const
{
    return _exceptionCode;
}

uint16_t JWPLC_ModbusTCPClientClass::transactionId() const
{
    return _activeTransactionId;
}

IPAddress JWPLC_ModbusTCPClientClass::serverIP() const
{
    return _serverIP;
}

uint16_t JWPLC_ModbusTCPClientClass::serverPort() const
{
    return _serverPort;
}

uint8_t JWPLC_ModbusTCPClientClass::unitId() const
{
    return _unitId;
}

void JWPLC_ModbusTCPClientClass::clearResult()
{
    if (busy())
    {
        return;
    }

    resetTransactionContext();
    _state = JWPLC_MODBUS_TCP_CLIENT_IDLE;
    _result = _configured ? JWPLC_MODBUS_TCP_CLIENT_OK
                          : JWPLC_MODBUS_TCP_CLIENT_NOT_CONFIGURED;
}

const JWPLCModbusTCPClientStats &JWPLC_ModbusTCPClientClass::stats() const
{
    return _stats;
}

void JWPLC_ModbusTCPClientClass::resetStats()
{
    memset(&_stats, 0, sizeof(_stats));
}

const char *JWPLC_ModbusTCPClientClass::resultString() const
{
    switch (_result)
    {
    case JWPLC_MODBUS_TCP_CLIENT_OK:
        return "OK";
    case JWPLC_MODBUS_TCP_CLIENT_NOT_CONFIGURED:
        return "Not configured";
    case JWPLC_MODBUS_TCP_CLIENT_ETHERNET_NOT_READY:
        return "Ethernet not ready";
    case JWPLC_MODBUS_TCP_CLIENT_BUS_LOCK_TIMEOUT:
        return "SPI bus lock timeout";
    case JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT:
        return "Invalid argument";
    case JWPLC_MODBUS_TCP_CLIENT_BUSY:
        return "Busy";
    case JWPLC_MODBUS_TCP_CLIENT_TIMEOUT:
        return "Timeout";
    case JWPLC_MODBUS_TCP_CLIENT_TRANSPORT_ERROR:
        return "Transport error";
    case JWPLC_MODBUS_TCP_CLIENT_INVALID_MBAP:
        return "Invalid MBAP";
    case JWPLC_MODBUS_TCP_CLIENT_TRANSACTION_MISMATCH:
        return "Transaction mismatch";
    case JWPLC_MODBUS_TCP_CLIENT_UNIT_ID_MISMATCH:
        return "Unit ID mismatch";
    case JWPLC_MODBUS_TCP_CLIENT_FUNCTION_MISMATCH:
        return "Function mismatch";
    case JWPLC_MODBUS_TCP_CLIENT_EXCEPTION:
        return "Modbus exception";
    case JWPLC_MODBUS_TCP_CLIENT_INVALID_RESPONSE:
        return "Invalid response";
    default:
        return "Unknown error";
    }
}

void JWPLC_ModbusTCPClientClass::printStatus(Print &out) const
{
    out.print("ModbusTCP client: ");
    out.println(_configured ? "CONFIGURED" : "DISABLED");
    out.print("Endpoint: ");
    out.print(_serverIP);
    out.print(':');
    out.println(_serverPort);
    out.print("Unit ID: ");
    out.println(_unitId);
    out.print("Session: ");
    out.println(_sessionConnected ? "CONNECTED" : "DISCONNECTED");
    out.print("State: ");
    out.println((int)_state);
    out.print("Result: ");
    out.println(resultString());
    out.print("TID: ");
    out.println(_activeTransactionId);
    out.print("RX/TX/OK: ");
    out.print(_stats.rxFrames);
    out.print('/');
    out.print(_stats.txFrames);
    out.print('/');
    out.println(_stats.requestsOk);
}

bool JWPLC_ModbusTCPClientClass::acquireBus(uint32_t timeoutMs)
{
    if (!jwplcSPI_acquire(timeoutMs))
    {
        _stats.busLockTimeouts++;
        _result = JWPLC_MODBUS_TCP_CLIENT_BUS_LOCK_TIMEOUT;
        return false;
    }

    jwplcSPI_deselectAll();
    return true;
}

void JWPLC_ModbusTCPClientClass::releaseBus()
{
    jwplcSPI_release();
}

void JWPLC_ModbusTCPClientClass::resetRx()
{
    _rxLength = 0;
    _expectedRxLength = 0;
}

void JWPLC_ModbusTCPClientClass::resetTransactionContext()
{
    _asyncTx.reset();
    _connectStarted = false;
    _sendStarted = false;
    _operation = OP_NONE;
    _exceptionCode = 0;
    _activeTransactionId = 0;
    _expectedFunction = 0;
    _startAddress = 0;
    _quantity = 0;
    _writeValue = 0;
    _registerDestination = nullptr;
    _requestStartMs = 0;
    _timeoutMs = 0;
    _txLength = 0;
    resetRx();
}

void JWPLC_ModbusTCPClientClass::closeSession()
{
    _asyncTx.reset();

    if ((bool)_tcpClient && acquireBus(20))
    {
        _tcpClient.cancelConnectAsync();
        releaseBus();
    }

    _tcpClient = EthernetClient();
    _sessionConnected = false;
    _connectStarted = false;
    _sendStarted = false;
    resetRx();
}

void JWPLC_ModbusTCPClientClass::fail(
    JWPLCModbusTCPClientError error,
    bool closeTransport)
{
    switch (error)
    {
    case JWPLC_MODBUS_TCP_CLIENT_TIMEOUT:
        _stats.timeouts++;
        break;
    case JWPLC_MODBUS_TCP_CLIENT_TRANSPORT_ERROR:
        _stats.transportErrors++;
        break;
    case JWPLC_MODBUS_TCP_CLIENT_INVALID_MBAP:
    case JWPLC_MODBUS_TCP_CLIENT_TRANSACTION_MISMATCH:
    case JWPLC_MODBUS_TCP_CLIENT_UNIT_ID_MISMATCH:
    case JWPLC_MODBUS_TCP_CLIENT_FUNCTION_MISMATCH:
    case JWPLC_MODBUS_TCP_CLIENT_INVALID_RESPONSE:
        _stats.protocolErrors++;
        break;
    default:
        break;
    }

    if (closeTransport)
    {
        closeSession();
    }
    else
    {
        _asyncTx.reset();
        _sendStarted = false;
        resetRx();
    }

    _result = error;
    _state = JWPLC_MODBUS_TCP_CLIENT_ERROR;
}

void JWPLC_ModbusTCPClientClass::completeSuccess()
{
    _stats.requestsOk++;
    _result = JWPLC_MODBUS_TCP_CLIENT_OK;
    _state = JWPLC_MODBUS_TCP_CLIENT_DONE;
    _asyncTx.reset();
    _sendStarted = false;
    resetRx();
}

bool JWPLC_ModbusTCPClientClass::startRequest(
    Operation operation,
    uint8_t functionCode,
    uint16_t startAddress,
    uint16_t quantity,
    uint16_t writeValue,
    uint16_t *destination,
    uint32_t timeoutMs)
{
    if (busy())
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_BUSY;
        return false;
    }

    if (!_configured)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_NOT_CONFIGURED;
        return false;
    }

    if (timeoutMs == 0)
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        return false;
    }

    if (done())
    {
        clearResult();
    }

    _operation = operation;
    _expectedFunction = functionCode;
    _startAddress = startAddress;
    _quantity = quantity;
    _writeValue = writeValue;
    _registerDestination = destination;
    _exceptionCode = 0;
    _activeTransactionId = _nextTransactionId++;
    _requestStartMs = millis();
    _timeoutMs = timeoutMs;
    _connectStarted = false;
    _sendStarted = false;
    resetRx();

    if (operation == OP_READ_HOLDING_REGISTERS)
    {
        buildReadHoldingRequest();
    }
    else if (operation == OP_WRITE_SINGLE_REGISTER)
    {
        buildWriteSingleRegisterRequest();
    }
    else
    {
        _result = JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT;
        _state = JWPLC_MODBUS_TCP_CLIENT_ERROR;
        return false;
    }

    _result = JWPLC_MODBUS_TCP_CLIENT_OK;
    _state = JWPLC_Ethernet.isReady()
                 ? JWPLC_MODBUS_TCP_CLIENT_CONNECTING
                 : JWPLC_MODBUS_TCP_CLIENT_WAIT_ETHERNET;
    return true;
}

void JWPLC_ModbusTCPClientClass::serviceConnection()
{
    if (_sessionConnected)
    {
        if (!acquireBus(20))
        {
            return;
        }

        const bool stillConnected = _tcpClient.connected() != 0;
        releaseBus();

        if (stillConnected)
        {
            _state = JWPLC_MODBUS_TCP_CLIENT_SENDING;
            return;
        }

        _tcpClient = EthernetClient();
        _sessionConnected = false;
        _connectStarted = false;
    }

    if (!_connectStarted)
    {
        if (!acquireBus(20))
        {
            return;
        }

        const int result =
            _tcpClient.beginConnectAsync(_serverIP, _serverPort);
        releaseBus();

        if (result < 0)
        {
            fail(JWPLC_MODBUS_TCP_CLIENT_TRANSPORT_ERROR, true);
            return;
        }

        if (result > 0)
        {
            _sessionConnected = true;
            _stats.connections++;
            _state = JWPLC_MODBUS_TCP_CLIENT_SENDING;
            return;
        }

        _connectStarted = true;
        _state = JWPLC_MODBUS_TCP_CLIENT_CONNECTING;
        return;
    }

    if (!acquireBus(20))
    {
        return;
    }

    const int result = _tcpClient.pollConnectAsync();
    releaseBus();

    if (result < 0)
    {
        fail(JWPLC_MODBUS_TCP_CLIENT_TRANSPORT_ERROR, true);
        return;
    }

    if (result > 0)
    {
        _sessionConnected = true;
        _connectStarted = false;
        _stats.connections++;
        _state = JWPLC_MODBUS_TCP_CLIENT_SENDING;
    }
}

void JWPLC_ModbusTCPClientClass::serviceSend()
{
    if (!_sessionConnected)
    {
        _state = JWPLC_MODBUS_TCP_CLIENT_CONNECTING;
        return;
    }

    if (!_sendStarted)
    {
        if (!acquireBus(20))
        {
            return;
        }

        const int result =
            _asyncTx.begin(_tcpClient, _txBuffer, _txLength);
        const bool pending = _asyncTx.inProgress();
        releaseBus();

        if (result < 0)
        {
            fail(JWPLC_MODBUS_TCP_CLIENT_TRANSPORT_ERROR, true);
            return;
        }

        if (result > 0)
        {
            _stats.txFrames++;
            resetRx();
            _state = JWPLC_MODBUS_TCP_CLIENT_WAIT_RESPONSE;
            return;
        }

        if (!pending)
        {
            // TX buffer W5500 temporalmente sin espacio suficiente.
            return;
        }

        _sendStarted = true;
        return;
    }

    if (!acquireBus(20))
    {
        return;
    }

    const int result = _asyncTx.poll(_tcpClient);
    releaseBus();

    if (result < 0)
    {
        fail(JWPLC_MODBUS_TCP_CLIENT_TRANSPORT_ERROR, true);
        return;
    }

    if (result > 0)
    {
        _sendStarted = false;
        _stats.txFrames++;
        resetRx();
        _state = JWPLC_MODBUS_TCP_CLIENT_WAIT_RESPONSE;
    }
}

void JWPLC_ModbusTCPClientClass::serviceResponse()
{
    if (!acquireBus(20))
    {
        return;
    }

    int availableBytes = _tcpClient.available();
    const bool connectedNow = _tcpClient.connected() != 0;

    if (availableBytes <= 0)
    {
        releaseBus();

        if (!connectedNow)
        {
            fail(JWPLC_MODBUS_TCP_CLIENT_TRANSPORT_ERROR, true);
        }
        return;
    }

    bool invalidFrame = false;
    uint16_t budget = JWPLC_MODBUS_TCP_CLIENT_RX_BUDGET;

    while (budget > 0 && availableBytes > 0)
    {
        uint16_t target = _expectedRxLength != 0
                              ? _expectedRxLength
                              : 6;

        if (_rxLength >= target)
        {
            break;
        }

        uint16_t room = (uint16_t)(target - _rxLength);
        uint16_t chunk = (uint16_t)availableBytes;

        if (chunk > room)
            chunk = room;
        if (chunk > budget)
            chunk = budget;

        if (chunk == 0)
        {
            break;
        }

        const int received =
            _tcpClient.read(&_rxBuffer[_rxLength], chunk);

        if (received <= 0)
        {
            break;
        }

        _rxLength = (uint16_t)(_rxLength + received);
        budget = (uint16_t)(budget - received);

        if (_expectedRxLength == 0 && _rxLength == 6)
        {
            const uint16_t protocolId = readU16BE(&_rxBuffer[2]);
            const uint16_t declaredLength = readU16BE(&_rxBuffer[4]);

            if (protocolId != 0 ||
                declaredLength < 2 ||
                declaredLength > 254)
            {
                invalidFrame = true;
                break;
            }

            _expectedRxLength = (uint16_t)(6 + declaredLength);
            if (_expectedRxLength > JWPLC_MODBUS_TCP_CLIENT_MAX_ADU)
            {
                invalidFrame = true;
                break;
            }
        }

        if (_expectedRxLength != 0 &&
            _rxLength >= _expectedRxLength)
        {
            break;
        }

        availableBytes = _tcpClient.available();
    }

    releaseBus();

    if (invalidFrame)
    {
        fail(JWPLC_MODBUS_TCP_CLIENT_INVALID_MBAP, true);
        return;
    }

    if (_expectedRxLength == 0 ||
        _rxLength < _expectedRxLength)
    {
        return;
    }

    _stats.rxFrames++;
    (void)processResponse();
}

bool JWPLC_ModbusTCPClientClass::processResponse()
{
    if (_expectedRxLength < 9 ||
        _expectedRxLength > JWPLC_MODBUS_TCP_CLIENT_MAX_ADU)
    {
        fail(JWPLC_MODBUS_TCP_CLIENT_INVALID_RESPONSE, true);
        return false;
    }

    const uint16_t transaction = readU16BE(&_rxBuffer[0]);
    const uint16_t protocolId = readU16BE(&_rxBuffer[2]);
    const uint16_t declaredLength = readU16BE(&_rxBuffer[4]);

    if (protocolId != 0 ||
        (uint16_t)(6 + declaredLength) != _expectedRxLength)
    {
        fail(JWPLC_MODBUS_TCP_CLIENT_INVALID_MBAP, true);
        return false;
    }

    if (transaction != _activeTransactionId)
    {
        fail(JWPLC_MODBUS_TCP_CLIENT_TRANSACTION_MISMATCH, true);
        return false;
    }

    if (_rxBuffer[6] != _unitId)
    {
        fail(JWPLC_MODBUS_TCP_CLIENT_UNIT_ID_MISMATCH, true);
        return false;
    }

    const uint8_t functionCode = _rxBuffer[7];

    if (functionCode == (uint8_t)(_expectedFunction | 0x80U))
    {
        if (_expectedRxLength != 9)
        {
            fail(JWPLC_MODBUS_TCP_CLIENT_INVALID_RESPONSE, true);
            return false;
        }

        _exceptionCode = _rxBuffer[8];
        _stats.exceptions++;
        fail(JWPLC_MODBUS_TCP_CLIENT_EXCEPTION, false);
        return false;
    }

    if (functionCode != _expectedFunction)
    {
        fail(JWPLC_MODBUS_TCP_CLIENT_FUNCTION_MISMATCH, true);
        return false;
    }

    if (_operation == OP_READ_HOLDING_REGISTERS)
    {
        const uint16_t expectedBytes = (uint16_t)(_quantity * 2U);
        const uint16_t expectedLength = (uint16_t)(9U + expectedBytes);

        if (_expectedRxLength != expectedLength ||
            _rxBuffer[8] != (uint8_t)expectedBytes ||
            _registerDestination == nullptr)
        {
            fail(JWPLC_MODBUS_TCP_CLIENT_INVALID_RESPONSE, true);
            return false;
        }

        for (uint16_t i = 0; i < _quantity; ++i)
        {
            _registerDestination[i] =
                readU16BE(&_rxBuffer[9 + (i * 2U)]);
        }
    }
    else if (_operation == OP_WRITE_SINGLE_REGISTER)
    {
        if (_expectedRxLength != 12 ||
            readU16BE(&_rxBuffer[8]) != _startAddress ||
            readU16BE(&_rxBuffer[10]) != _writeValue)
        {
            fail(JWPLC_MODBUS_TCP_CLIENT_INVALID_RESPONSE, true);
            return false;
        }
    }
    else
    {
        fail(JWPLC_MODBUS_TCP_CLIENT_INVALID_RESPONSE, true);
        return false;
    }

    completeSuccess();
    return true;
}

void JWPLC_ModbusTCPClientClass::buildReadHoldingRequest()
{
    writeU16BE(&_txBuffer[0], _activeTransactionId);
    writeU16BE(&_txBuffer[2], 0);
    writeU16BE(&_txBuffer[4], 6);
    _txBuffer[6] = _unitId;
    _txBuffer[7] = 0x03;
    writeU16BE(&_txBuffer[8], _startAddress);
    writeU16BE(&_txBuffer[10], _quantity);
    _txLength = 12;
}

void JWPLC_ModbusTCPClientClass::buildWriteSingleRegisterRequest()
{
    writeU16BE(&_txBuffer[0], _activeTransactionId);
    writeU16BE(&_txBuffer[2], 0);
    writeU16BE(&_txBuffer[4], 6);
    _txBuffer[6] = _unitId;
    _txBuffer[7] = 0x06;
    writeU16BE(&_txBuffer[8], _startAddress);
    writeU16BE(&_txBuffer[10], _writeValue);
    _txLength = 12;
}

uint16_t JWPLC_ModbusTCPClientClass::readU16BE(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

void JWPLC_ModbusTCPClientClass::writeU16BE(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)(value & 0xFFU);
}
