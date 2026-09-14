#include "JWPLC_ModbusTCP.h"

#include <string.h>

#include "jwplc_spi_bus.h"

JWPLC_ModbusTCPClass JWPLC_ModbusTCP;

JWPLC_ModbusTCPClass::JWPLC_ModbusTCPClass()
    : _serverEnabled(false),
      _serverListening(false),
      _clientActive(false),
      _unitId(JWPLC_MODBUS_TCP_DEFAULT_UNIT_ID),
      _port(JWPLC_MODBUS_TCP_DEFAULT_PORT),
      _frameTimeoutMs(JWPLC_MODBUS_TCP_FRAME_TIMEOUT_MS),
      _lastRxMs(0),
      _lastListenAttemptMs(0),
      _server(JWPLC_MODBUS_TCP_DEFAULT_PORT),
      _client(),
      _coils(nullptr),
      _coilCount(0),
      _discreteInputs(nullptr),
      _discreteInputCount(0),
      _holdingRegisters(nullptr),
      _holdingCount(0),
      _inputRegisters(nullptr),
      _inputCount(0),
      _rxLength(0),
      _expectedLength(0),
      _serverState(JWPLC_MODBUS_TCP_SERVER_STOPPED),
      _lastError(JWPLC_MODBUS_TCP_OK)
{
    memset(_rxBuffer, 0, sizeof(_rxBuffer));
    memset(_txBuffer, 0, sizeof(_txBuffer));
    memset(&_stats, 0, sizeof(_stats));
}

bool JWPLC_ModbusTCPClass::beginServer(uint8_t unitId, uint16_t port)
{
    if (port == 0)
    {
        setError(JWPLC_MODBUS_TCP_TRANSPORT_ERROR);
        return false;
    }

    // A14.1 mantiene configuración de una sola vez para no dejar sockets de
    // escucha huérfanos: el backend Ethernet actual todavía no expone stop()
    // de servidor. A14.2 cerrará ese contrato junto al Client cooperativo.
    if (_serverEnabled)
    {
        if (_unitId == unitId && _port == port)
        {
            return true;
        }

        setError(JWPLC_MODBUS_TCP_BUSY);
        return false;
    }

    _unitId = unitId;
    _port = port;
    _server = EthernetServer(_port);
    _serverEnabled = true;
    _serverListening = false;
    _clientActive = false;
    _client = EthernetClient();
    _lastListenAttemptMs = 0;
    resetRx();

    if (JWPLC_Ethernet.isReady())
    {
        ensureServerListening();
    }
    else
    {
        _serverState = JWPLC_MODBUS_TCP_SERVER_WAIT_ETHERNET;
        setError(JWPLC_MODBUS_TCP_ETHERNET_NOT_READY);
    }

    // true significa configuración aceptada. La red puede seguir en DHCP y
    // serverReady() permite saber cuándo el socket 502 ya quedó operativo.
    return true;
}

void JWPLC_ModbusTCPClass::task()
{
    if (!_serverEnabled)
    {
        return;
    }

    if (!JWPLC_Ethernet.isReady())
    {
        // Un reset/reinicio del W5500 invalida los handles de sockets previos.
        // No se llama stop() aquí porque el propio backend Ethernet puede estar
        // reconstruyendo el chip y el mutex SPI pertenece al runtime común.
        _serverListening = false;
        _clientActive = false;
        _client = EthernetClient();
        resetRx();
        _serverState = JWPLC_MODBUS_TCP_SERVER_WAIT_ETHERNET;
        setError(JWPLC_MODBUS_TCP_ETHERNET_NOT_READY);
        return;
    }

    ensureServerListening();

    if (!_serverListening)
    {
        return;
    }

    serviceServer();
}

void JWPLC_ModbusTCPClass::poll()
{
    task();
}

bool JWPLC_ModbusTCPClass::serverEnabled() const
{
    return _serverEnabled;
}

bool JWPLC_ModbusTCPClass::serverReady() const
{
    return _serverEnabled && _serverListening && JWPLC_Ethernet.isReady();
}

bool JWPLC_ModbusTCPClass::clientConnected() const
{
    return _clientActive;
}

uint8_t JWPLC_ModbusTCPClass::unitId() const
{
    return _unitId;
}

uint16_t JWPLC_ModbusTCPClass::port() const
{
    return _port;
}

JWPLCModbusTCPServerState JWPLC_ModbusTCPClass::serverState() const
{
    return _serverState;
}

void JWPLC_ModbusTCPClass::setFrameTimeoutMs(uint32_t timeoutMs)
{
    _frameTimeoutMs = timeoutMs == 0 ? 1 : timeoutMs;
}

uint32_t JWPLC_ModbusTCPClass::frameTimeoutMs() const
{
    return _frameTimeoutMs;
}

void JWPLC_ModbusTCPClass::setCoils(uint8_t *bits, uint16_t count)
{
    _coils = bits;
    _coilCount = bits != nullptr ? count : 0;
}

uint16_t JWPLC_ModbusTCPClass::coilCount() const
{
    return _coilCount;
}

bool JWPLC_ModbusTCPClass::getCoil(uint16_t address, bool &value) const
{
    if (_coils == nullptr || address >= _coilCount)
    {
        return false;
    }

    value = getPackedBit(_coils, address);
    return true;
}

bool JWPLC_ModbusTCPClass::setCoil(uint16_t address, bool value)
{
    if (_coils == nullptr || address >= _coilCount)
    {
        return false;
    }

    setPackedBit(_coils, address, value);
    return true;
}

void JWPLC_ModbusTCPClass::setDiscreteInputs(const uint8_t *bits, uint16_t count)
{
    _discreteInputs = bits;
    _discreteInputCount = bits != nullptr ? count : 0;
}

uint16_t JWPLC_ModbusTCPClass::discreteInputCount() const
{
    return _discreteInputCount;
}

bool JWPLC_ModbusTCPClass::getDiscreteInput(uint16_t address, bool &value) const
{
    if (_discreteInputs == nullptr || address >= _discreteInputCount)
    {
        return false;
    }

    value = getPackedBit(_discreteInputs, address);
    return true;
}

void JWPLC_ModbusTCPClass::setHoldingRegisters(uint16_t *registers, uint16_t count)
{
    _holdingRegisters = registers;
    _holdingCount = registers != nullptr ? count : 0;
}

uint16_t JWPLC_ModbusTCPClass::holdingRegisterCount() const
{
    return _holdingCount;
}

bool JWPLC_ModbusTCPClass::getHoldingRegister(uint16_t address, uint16_t &value) const
{
    if (_holdingRegisters == nullptr || address >= _holdingCount)
    {
        return false;
    }

    value = _holdingRegisters[address];
    return true;
}

bool JWPLC_ModbusTCPClass::setHoldingRegister(uint16_t address, uint16_t value)
{
    if (_holdingRegisters == nullptr || address >= _holdingCount)
    {
        return false;
    }

    _holdingRegisters[address] = value;
    return true;
}

void JWPLC_ModbusTCPClass::setInputRegisters(const uint16_t *registers, uint16_t count)
{
    _inputRegisters = registers;
    _inputCount = registers != nullptr ? count : 0;
}

uint16_t JWPLC_ModbusTCPClass::inputRegisterCount() const
{
    return _inputCount;
}

bool JWPLC_ModbusTCPClass::getInputRegister(uint16_t address, uint16_t &value) const
{
    if (_inputRegisters == nullptr || address >= _inputCount)
    {
        return false;
    }

    value = _inputRegisters[address];
    return true;
}

JWPLCModbusTCPError JWPLC_ModbusTCPClass::lastError() const
{
    return _lastError;
}

const char *JWPLC_ModbusTCPClass::lastErrorString() const
{
    switch (_lastError)
    {
    case JWPLC_MODBUS_TCP_OK:
        return "OK";
    case JWPLC_MODBUS_TCP_NOT_STARTED:
        return "Not started";
    case JWPLC_MODBUS_TCP_ETHERNET_NOT_READY:
        return "Ethernet not ready";
    case JWPLC_MODBUS_TCP_BUS_LOCK_TIMEOUT:
        return "SPI bus lock timeout";
    case JWPLC_MODBUS_TCP_INVALID_MBAP:
        return "Invalid MBAP";
    case JWPLC_MODBUS_TCP_INVALID_LENGTH:
        return "Invalid length";
    case JWPLC_MODBUS_TCP_UNIT_ID_MISMATCH:
        return "Unit ID mismatch";
    case JWPLC_MODBUS_TCP_INVALID_REGISTER_MAP:
        return "Invalid register map";
    case JWPLC_MODBUS_TCP_EXCEPTION:
        return "Modbus exception";
    case JWPLC_MODBUS_TCP_TIMEOUT:
        return "Frame timeout";
    case JWPLC_MODBUS_TCP_TRANSPORT_ERROR:
        return "Transport error";
    case JWPLC_MODBUS_TCP_BUSY:
        return "Busy";
    default:
        return "Unknown error";
    }
}

const JWPLCModbusTCPStats &JWPLC_ModbusTCPClass::stats() const
{
    return _stats;
}

void JWPLC_ModbusTCPClass::resetStats()
{
    memset(&_stats, 0, sizeof(_stats));
}

void JWPLC_ModbusTCPClass::printStatus(Print &out) const
{
    out.print("ModbusTCP server: ");
    out.println(serverReady() ? "READY" : (serverEnabled() ? "WAIT" : "STOPPED"));
    out.print("Port: ");
    out.println(_port);
    out.print("Unit ID: ");
    out.println(_unitId);
    out.print("Client: ");
    out.println(_clientActive ? "ACTIVE" : "NONE");
    out.print("Last error: ");
    out.println(lastErrorString());
    out.print("RX/TX: ");
    out.print(_stats.rxFrames);
    out.print('/');
    out.println(_stats.txFrames);
    out.print("OK/EX: ");
    out.print(_stats.requestsOk);
    out.print('/');
    out.println(_stats.exceptionsSent);
}

bool JWPLC_ModbusTCPClass::acquireBus(uint32_t timeoutMs)
{
    if (!jwplcSPI_acquire(timeoutMs))
    {
        _stats.busLockTimeouts++;
        setError(JWPLC_MODBUS_TCP_BUS_LOCK_TIMEOUT);
        return false;
    }

    jwplcSPI_deselectAll();
    return true;
}

void JWPLC_ModbusTCPClass::releaseBus()
{
    jwplcSPI_release();
}

void JWPLC_ModbusTCPClass::setError(JWPLCModbusTCPError error)
{
    _lastError = error;
}

void JWPLC_ModbusTCPClass::clearError()
{
    _lastError = JWPLC_MODBUS_TCP_OK;
}

void JWPLC_ModbusTCPClass::resetRx()
{
    _rxLength = 0;
    _expectedLength = 0;
    _lastRxMs = 0;
}

void JWPLC_ModbusTCPClass::dropClient()
{
    if (_clientActive && acquireBus(20))
    {
        // timeout 0 mantiene el cierre forzado acotado. stop() puede ejecutar
        // una única cesión de scheduler, pero sólo se usa para tramas inválidas
        // o timeout, no en el camino normal de cada request.
        _client.setConnectionTimeout(0);
        _client.stop();
        releaseBus();
    }

    _client = EthernetClient();
    _clientActive = false;
    resetRx();

    if (_serverEnabled && JWPLC_Ethernet.isReady())
    {
        _serverState = JWPLC_MODBUS_TCP_SERVER_LISTENING;
    }
    else if (_serverEnabled)
    {
        _serverState = JWPLC_MODBUS_TCP_SERVER_WAIT_ETHERNET;
    }
    else
    {
        _serverState = JWPLC_MODBUS_TCP_SERVER_STOPPED;
    }
}

void JWPLC_ModbusTCPClass::ensureServerListening()
{
    if (_serverListening)
    {
        return;
    }

    const uint32_t now = millis();
    if (_lastListenAttemptMs != 0 &&
        (uint32_t)(now - _lastListenAttemptMs) < 500UL)
    {
        return;
    }

    _lastListenAttemptMs = now;

    if (!acquireBus(20))
    {
        return;
    }

    // Si el socket LISTEN sobrevivió a una reentrada, no crear otro.
    bool listening = (bool)_server;
    if (!listening)
    {
        _server.begin();
        listening = (bool)_server;
    }

    releaseBus();

    if (!listening)
    {
        _serverListening = false;
        _serverState = JWPLC_MODBUS_TCP_SERVER_ERROR;
        setError(JWPLC_MODBUS_TCP_TRANSPORT_ERROR);
        return;
    }

    _serverListening = true;
    _serverState = _clientActive ? JWPLC_MODBUS_TCP_SERVER_CLIENT_ACTIVE
                                 : JWPLC_MODBUS_TCP_SERVER_LISTENING;
    clearError();
}

void JWPLC_ModbusTCPClass::serviceServer()
{
    const uint32_t now = millis();

    if (_clientActive && _rxLength > 0 &&
        (uint32_t)(now - _lastRxMs) >= _frameTimeoutMs)
    {
        _stats.frameTimeouts++;
        setError(JWPLC_MODBUS_TCP_TIMEOUT);
        dropClient();
        return;
    }

    if (!_clientActive)
    {
        if (!acquireBus(20))
        {
            return;
        }

        EthernetClient candidate = _server.available();
        releaseBus();

        if (candidate)
        {
            _client = candidate;
            _clientActive = true;
            _stats.clientConnections++;
            _serverState = JWPLC_MODBUS_TCP_SERVER_CLIENT_ACTIVE;
            resetRx();
        }
        else
        {
            _serverState = JWPLC_MODBUS_TCP_SERVER_LISTENING;
            return;
        }
    }

    if (!acquireBus(20))
    {
        return;
    }

    bool fatalFrame = false;
    JWPLCModbusTCPError fatalError = JWPLC_MODBUS_TCP_OK;

    int availableBytes = _client.available();
    const bool connected = _client.connected() != 0;

    if (availableBytes <= 0)
    {
        releaseBus();

        if (!connected)
        {
            _client = EthernetClient();
            _clientActive = false;
            resetRx();
            _serverState = JWPLC_MODBUS_TCP_SERVER_LISTENING;
        }
        return;
    }

    uint16_t budget = JWPLC_MODBUS_TCP_RX_BUDGET;

    while (budget > 0 && availableBytes > 0)
    {
        uint16_t target = _expectedLength != 0 ? _expectedLength : 6;

        if (_rxLength >= target)
        {
            if (_expectedLength == 0 && _rxLength == 6)
            {
                const uint16_t protocolId = readU16BE(&_rxBuffer[2]);
                const uint16_t declaredLength = readU16BE(&_rxBuffer[4]);

                if (protocolId != 0)
                {
                    fatalFrame = true;
                    fatalError = JWPLC_MODBUS_TCP_INVALID_MBAP;
                    break;
                }

                // Length incluye Unit ID + PDU. Mínimo: Unit ID + Function.
                if (declaredLength < 2 || declaredLength > 254)
                {
                    fatalFrame = true;
                    fatalError = JWPLC_MODBUS_TCP_INVALID_LENGTH;
                    break;
                }

                _expectedLength = (uint16_t)(6 + declaredLength);
                if (_expectedLength > JWPLC_MODBUS_TCP_MAX_ADU)
                {
                    fatalFrame = true;
                    fatalError = JWPLC_MODBUS_TCP_INVALID_LENGTH;
                    break;
                }

                target = _expectedLength;
            }
            else
            {
                break;
            }
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

        const int received = _client.read(&_rxBuffer[_rxLength], chunk);
        if (received <= 0)
        {
            break;
        }

        _rxLength = (uint16_t)(_rxLength + received);
        budget = (uint16_t)(budget - received);
        _lastRxMs = now;

        if (_expectedLength == 0 && _rxLength == 6)
        {
            const uint16_t protocolId = readU16BE(&_rxBuffer[2]);
            const uint16_t declaredLength = readU16BE(&_rxBuffer[4]);

            if (protocolId != 0)
            {
                fatalFrame = true;
                fatalError = JWPLC_MODBUS_TCP_INVALID_MBAP;
                break;
            }

            if (declaredLength < 2 || declaredLength > 254)
            {
                fatalFrame = true;
                fatalError = JWPLC_MODBUS_TCP_INVALID_LENGTH;
                break;
            }

            _expectedLength = (uint16_t)(6 + declaredLength);
            if (_expectedLength > JWPLC_MODBUS_TCP_MAX_ADU)
            {
                fatalFrame = true;
                fatalError = JWPLC_MODBUS_TCP_INVALID_LENGTH;
                break;
            }
        }

        if (_expectedLength != 0 && _rxLength >= _expectedLength)
        {
            break;
        }

        availableBytes = _client.available();
    }

    releaseBus();

    if (fatalFrame)
    {
        _stats.protocolErrors++;
        setError(fatalError);
        dropClient();
        return;
    }

    if (_expectedLength == 0 || _rxLength < _expectedLength)
    {
        return;
    }

    _stats.rxFrames++;

    uint16_t responseLength = 0;
    const bool responseReady = processRequest(_expectedLength, responseLength);
    resetRx();

    if (!responseReady)
    {
        return;
    }

    (void)sendResponse(responseLength);
}

bool JWPLC_ModbusTCPClass::processRequest(uint16_t aduLength, uint16_t &responseLength)
{
    responseLength = 0;

    if (aduLength < 8 || aduLength > JWPLC_MODBUS_TCP_MAX_ADU)
    {
        _stats.protocolErrors++;
        setError(JWPLC_MODBUS_TCP_INVALID_LENGTH);
        return false;
    }

    const uint16_t protocolId = readU16BE(&_rxBuffer[2]);
    const uint16_t declaredLength = readU16BE(&_rxBuffer[4]);

    if (protocolId != 0 || (uint16_t)(6 + declaredLength) != aduLength)
    {
        _stats.protocolErrors++;
        setError(JWPLC_MODBUS_TCP_INVALID_MBAP);
        return false;
    }

    const uint8_t requestUnitId = _rxBuffer[6];
    if (requestUnitId != _unitId)
    {
        setError(JWPLC_MODBUS_TCP_UNIT_ID_MISMATCH);
        return false;
    }

    // MBAP de respuesta: Transaction ID se conserva, Protocol ID = 0 y
    // Unit ID se ecoa. Length se completa al terminar de construir el PDU.
    _txBuffer[0] = _rxBuffer[0];
    _txBuffer[1] = _rxBuffer[1];
    _txBuffer[2] = 0;
    _txBuffer[3] = 0;
    _txBuffer[4] = 0;
    _txBuffer[5] = 0;
    _txBuffer[6] = requestUnitId;

    const uint8_t *pdu = &_rxBuffer[7];
    const uint16_t pduLength = (uint16_t)(aduLength - 7);
    const uint8_t functionCode = pdu[0];

    bool ok = false;

    switch (functionCode)
    {
    case 0x01:
        ok = processReadBits(functionCode,
                             _coils,
                             _coilCount,
                             pdu,
                             pduLength,
                             responseLength);
        break;

    case 0x02:
        ok = processReadBits(functionCode,
                             _discreteInputs,
                             _discreteInputCount,
                             pdu,
                             pduLength,
                             responseLength);
        break;

    case 0x03:
        ok = processReadRegisters(functionCode,
                                  _holdingRegisters,
                                  _holdingCount,
                                  pdu,
                                  pduLength,
                                  responseLength);
        break;

    case 0x04:
        ok = processReadRegisters(functionCode,
                                  _inputRegisters,
                                  _inputCount,
                                  pdu,
                                  pduLength,
                                  responseLength);
        break;

    case 0x05:
        ok = processWriteSingleCoil(pdu, pduLength, responseLength);
        break;

    case 0x06:
        ok = processWriteSingleRegister(pdu, pduLength, responseLength);
        break;

    case 0x0F:
        ok = processWriteMultipleCoils(pdu, pduLength, responseLength);
        break;

    case 0x10:
        ok = processWriteMultipleRegisters(pdu, pduLength, responseLength);
        break;

    default:
        ok = buildException(functionCode,
                            JWPLC_MODBUS_TCP_EX_ILLEGAL_FUNCTION,
                            responseLength);
        break;
    }

    if (!ok)
    {
        return false;
    }

    if (responseLength < 8 || responseLength > JWPLC_MODBUS_TCP_MAX_ADU)
    {
        _stats.protocolErrors++;
        setError(JWPLC_MODBUS_TCP_INVALID_LENGTH);
        return false;
    }

    writeU16BE(&_txBuffer[4], (uint16_t)(responseLength - 6));

    if ((_txBuffer[7] & 0x80U) == 0)
    {
        _stats.requestsOk++;
        clearError();
    }

    return true;
}

bool JWPLC_ModbusTCPClass::buildException(
    uint8_t functionCode,
    JWPLCModbusTCPExceptionCode exceptionCode,
    uint16_t &responseLength)
{
    _txBuffer[7] = (uint8_t)(functionCode | 0x80U);
    _txBuffer[8] = (uint8_t)exceptionCode;
    responseLength = 9;
    writeU16BE(&_txBuffer[4], 3); // Unit ID + 2-byte exception PDU.
    _stats.exceptionsSent++;
    setError(JWPLC_MODBUS_TCP_EXCEPTION);
    return true;
}

bool JWPLC_ModbusTCPClass::sendResponse(uint16_t responseLength)
{
    if (!_clientActive || responseLength == 0 ||
        responseLength > JWPLC_MODBUS_TCP_MAX_ADU)
    {
        setError(JWPLC_MODBUS_TCP_TRANSPORT_ERROR);
        return false;
    }

    if (!acquireBus(20))
    {
        return false;
    }

    const size_t written = _client.write(_txBuffer, responseLength);
    releaseBus();

    if (written != responseLength)
    {
        setError(JWPLC_MODBUS_TCP_TRANSPORT_ERROR);
        return false;
    }

    _stats.txFrames++;
    return true;
}

bool JWPLC_ModbusTCPClass::processReadBits(
    uint8_t functionCode,
    const uint8_t *map,
    uint16_t mapCount,
    const uint8_t *pdu,
    uint16_t pduLength,
    uint16_t &responseLength)
{
    if (pduLength != 5)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_VALUE,
                              responseLength);
    }

    const uint16_t start = readU16BE(&pdu[1]);
    const uint16_t quantity = readU16BE(&pdu[3]);

    if (quantity < 1 || quantity > 2000)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_VALUE,
                              responseLength);
    }

    if (map == nullptr || !rangeValid(start, quantity, mapCount))
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_ADDRESS,
                              responseLength);
    }

    const uint8_t byteCount = (uint8_t)((quantity + 7U) / 8U);
    const uint16_t pduResponseLength = (uint16_t)(2 + byteCount);
    responseLength = (uint16_t)(7 + pduResponseLength);

    if (responseLength > JWPLC_MODBUS_TCP_MAX_ADU)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_SERVER_DEVICE_FAILURE,
                              responseLength);
    }

    _txBuffer[7] = functionCode;
    _txBuffer[8] = byteCount;
    memset(&_txBuffer[9], 0, byteCount);

    for (uint16_t i = 0; i < quantity; ++i)
    {
        if (getPackedBit(map, (uint16_t)(start + i)))
        {
            _txBuffer[9 + (i >> 3)] |= (uint8_t)(1U << (i & 0x07U));
        }
    }

    return true;
}

bool JWPLC_ModbusTCPClass::processReadRegisters(
    uint8_t functionCode,
    const uint16_t *map,
    uint16_t mapCount,
    const uint8_t *pdu,
    uint16_t pduLength,
    uint16_t &responseLength)
{
    if (pduLength != 5)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_VALUE,
                              responseLength);
    }

    const uint16_t start = readU16BE(&pdu[1]);
    const uint16_t quantity = readU16BE(&pdu[3]);

    if (quantity < 1 || quantity > 125)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_VALUE,
                              responseLength);
    }

    if (map == nullptr || !rangeValid(start, quantity, mapCount))
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_ADDRESS,
                              responseLength);
    }

    const uint16_t byteCount = (uint16_t)(quantity * 2U);
    responseLength = (uint16_t)(9 + byteCount);

    if (responseLength > JWPLC_MODBUS_TCP_MAX_ADU)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_SERVER_DEVICE_FAILURE,
                              responseLength);
    }

    _txBuffer[7] = functionCode;
    _txBuffer[8] = (uint8_t)byteCount;

    for (uint16_t i = 0; i < quantity; ++i)
    {
        writeU16BE(&_txBuffer[9 + (i * 2U)], map[start + i]);
    }

    return true;
}

bool JWPLC_ModbusTCPClass::processWriteSingleCoil(
    const uint8_t *pdu,
    uint16_t pduLength,
    uint16_t &responseLength)
{
    const uint8_t functionCode = 0x05;

    if (pduLength != 5)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_VALUE,
                              responseLength);
    }

    const uint16_t address = readU16BE(&pdu[1]);
    const uint16_t rawValue = readU16BE(&pdu[3]);

    if (rawValue != 0xFF00U && rawValue != 0x0000U)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_VALUE,
                              responseLength);
    }

    if (_coils == nullptr || address >= _coilCount)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_ADDRESS,
                              responseLength);
    }

    setPackedBit(_coils, address, rawValue == 0xFF00U);
    memcpy(&_txBuffer[7], pdu, 5);
    responseLength = 12;
    return true;
}

bool JWPLC_ModbusTCPClass::processWriteSingleRegister(
    const uint8_t *pdu,
    uint16_t pduLength,
    uint16_t &responseLength)
{
    const uint8_t functionCode = 0x06;

    if (pduLength != 5)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_VALUE,
                              responseLength);
    }

    const uint16_t address = readU16BE(&pdu[1]);
    const uint16_t value = readU16BE(&pdu[3]);

    if (_holdingRegisters == nullptr || address >= _holdingCount)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_ADDRESS,
                              responseLength);
    }

    _holdingRegisters[address] = value;
    memcpy(&_txBuffer[7], pdu, 5);
    responseLength = 12;
    return true;
}

bool JWPLC_ModbusTCPClass::processWriteMultipleCoils(
    const uint8_t *pdu,
    uint16_t pduLength,
    uint16_t &responseLength)
{
    const uint8_t functionCode = 0x0F;

    if (pduLength < 6)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_VALUE,
                              responseLength);
    }

    const uint16_t start = readU16BE(&pdu[1]);
    const uint16_t quantity = readU16BE(&pdu[3]);
    const uint8_t byteCount = pdu[5];
    const uint16_t expectedBytes = (uint16_t)((quantity + 7U) / 8U);

    if (quantity < 1 || quantity > 1968 ||
        byteCount != expectedBytes ||
        pduLength != (uint16_t)(6 + byteCount))
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_VALUE,
                              responseLength);
    }

    if (_coils == nullptr || !rangeValid(start, quantity, _coilCount))
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_ADDRESS,
                              responseLength);
    }

    for (uint16_t i = 0; i < quantity; ++i)
    {
        const bool value = (pdu[6 + (i >> 3)] &
                            (uint8_t)(1U << (i & 0x07U))) != 0;
        setPackedBit(_coils, (uint16_t)(start + i), value);
    }

    _txBuffer[7] = functionCode;
    writeU16BE(&_txBuffer[8], start);
    writeU16BE(&_txBuffer[10], quantity);
    responseLength = 12;
    return true;
}

bool JWPLC_ModbusTCPClass::processWriteMultipleRegisters(
    const uint8_t *pdu,
    uint16_t pduLength,
    uint16_t &responseLength)
{
    const uint8_t functionCode = 0x10;

    if (pduLength < 6)
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_VALUE,
                              responseLength);
    }

    const uint16_t start = readU16BE(&pdu[1]);
    const uint16_t quantity = readU16BE(&pdu[3]);
    const uint8_t byteCount = pdu[5];
    const uint16_t expectedBytes = (uint16_t)(quantity * 2U);

    if (quantity < 1 || quantity > 123 ||
        byteCount != expectedBytes ||
        pduLength != (uint16_t)(6 + byteCount))
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_VALUE,
                              responseLength);
    }

    if (_holdingRegisters == nullptr ||
        !rangeValid(start, quantity, _holdingCount))
    {
        return buildException(functionCode,
                              JWPLC_MODBUS_TCP_EX_ILLEGAL_DATA_ADDRESS,
                              responseLength);
    }

    for (uint16_t i = 0; i < quantity; ++i)
    {
        _holdingRegisters[start + i] = readU16BE(&pdu[6 + (i * 2U)]);
    }

    _txBuffer[7] = functionCode;
    writeU16BE(&_txBuffer[8], start);
    writeU16BE(&_txBuffer[10], quantity);
    responseLength = 12;
    return true;
}

uint16_t JWPLC_ModbusTCPClass::readU16BE(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

void JWPLC_ModbusTCPClass::writeU16BE(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)(value & 0xFFU);
}

bool JWPLC_ModbusTCPClass::rangeValid(
    uint16_t start,
    uint16_t quantity,
    uint16_t count)
{
    if (quantity == 0 || start >= count)
    {
        return false;
    }

    return quantity <= (uint16_t)(count - start);
}

bool JWPLC_ModbusTCPClass::getPackedBit(const uint8_t *bits, uint16_t address)
{
    return (bits[address >> 3] &
            (uint8_t)(1U << (address & 0x07U))) != 0;
}

void JWPLC_ModbusTCPClass::setPackedBit(
    uint8_t *bits,
    uint16_t address,
    bool value)
{
    const uint8_t mask = (uint8_t)(1U << (address & 0x07U));
    uint8_t &target = bits[address >> 3];

    if (value)
    {
        target |= mask;
    }
    else
    {
        target &= (uint8_t)~mask;
    }
}
