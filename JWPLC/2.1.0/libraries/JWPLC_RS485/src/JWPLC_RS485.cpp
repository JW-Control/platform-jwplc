#include "JWPLC_RS485.h"

// Puerto generico subyacente. Static local: se construye en el primer uso,
// sin depender del orden de inicializacion entre unidades de compilacion
// (JWPLC_ModbusRTU lo toma desde su propio constructor global).
JW_RS485 &jwplcRs485Port()
{
    static JW_RS485 port(Serial2, JWPLC_RS485_RX_PIN, JWPLC_RS485_TX_PIN);
    return port;
}

JWPLC_RS485Class JWPLC_RS485;

extern "C" void __attribute__((weak)) jwplcRs485PreTransmitCallback(void)
{
    // No-op por defecto.
    // JWPLC Basic usa MAX13487EESA+ con auto-dirección.
}

extern "C" void __attribute__((weak)) jwplcRs485PostTransmitCallback(void)
{
    // No-op por defecto.
    // Hardware futuro con DE/RE puede sobreescribir este hook desde el sketch/core.
}

extern "C" void __attribute__((weak)) jwplcRs485ActivityCallback(void)
{
    // No-op por defecto.
    // JWPLC_Display puede sobreescribir este hook para reflejar actividad BUS.
}

static void jwplcRs485PreTransmit(void *)
{
    jwplcRs485PreTransmitCallback();
}

static void jwplcRs485PostTransmit(void *)
{
    jwplcRs485PostTransmitCallback();
}

static void jwplcRs485Activity(void *)
{
    jwplcRs485ActivityCallback();
}

JWPLC_RS485Class::JWPLC_RS485Class()
    : _port(jwplcRs485Port()),
      _baud(JWPLC_RS485_DEFAULT_BAUD),
      _config(JWPLC_RS485_DEFAULT_CONFIG),
      _disabledError(JWPLC_RS485_NOT_STARTED)
{
#if JWPLC_RS485_AUTO_DIRECTION
    _port.setAutoDirection(JWPLC_RS485_TX_BUFFER_SIZE);
#else
    _port.setCallbackDirection();
#endif

    _port.setForceApbClock(JWPLC_RS485_FORCE_APB_CLOCK != 0);
    _port.setPreTransmitCallback(jwplcRs485PreTransmit);
    _port.setPostTransmitCallback(jwplcRs485PostTransmit);
    _port.setActivityCallback(jwplcRs485Activity);
}

bool JWPLC_RS485Class::begin()
{
    return begin(JWPLC_RS485_DEFAULT_BAUD, JWPLC_RS485_DEFAULT_CONFIG);
}

bool JWPLC_RS485Class::begin(uint32_t baud)
{
    return begin(baud, JWPLC_RS485_DEFAULT_CONFIG);
}

bool JWPLC_RS485Class::begin(uint32_t baud, uint32_t config)
{
    _baud = baud;
    _config = config;

#if !JWPLC_HAS_RS485
    _disabledError = JWPLC_RS485_DISABLED;
    return false;
#else
    return _port.begin(baud, config);
#endif
}

void JWPLC_RS485Class::end()
{
#if JWPLC_HAS_RS485
    _port.end();
#else
    _disabledError = JWPLC_RS485_NOT_STARTED;
#endif
}

bool JWPLC_RS485Class::isEnabled() const
{
#if JWPLC_HAS_RS485
    return true;
#else
    return false;
#endif
}

bool JWPLC_RS485Class::isReady() const
{
#if JWPLC_HAS_RS485
    return _port.isReady();
#else
    return false;
#endif
}

uint32_t JWPLC_RS485Class::baudRate() const
{
    return _baud;
}

uint32_t JWPLC_RS485Class::effectiveBaudRate() const
{
#if JWPLC_HAS_RS485
    return _port.effectiveBaudRate();
#else
    return 0;
#endif
}

uint32_t JWPLC_RS485Class::config() const
{
    return _config;
}

bool JWPLC_RS485Class::autoDirection() const
{
#if JWPLC_HAS_RS485 && JWPLC_RS485_AUTO_DIRECTION
    return true;
#else
    return false;
#endif
}

bool JWPLC_RS485Class::apbClockForced() const
{
#if JWPLC_RS485_FORCE_APB_CLOCK
    return true;
#else
    return false;
#endif
}

const char *JWPLC_RS485Class::clockSourceString() const
{
    return apbClockForced()
        ? "APB_FORCED"
        : "AUTO";
}

size_t JWPLC_RS485Class::txBufferSize() const
{
    return _port.txBufferSize();
}

bool JWPLC_RS485Class::queuedWriteSupported() const
{
#if JWPLC_HAS_RS485 && JWPLC_RS485_AUTO_DIRECTION
    return _port.queuedWriteSupported();
#else
    return false;
#endif
}

int8_t JWPLC_RS485Class::rxPin() const
{
    return _port.rxPin();
}

int8_t JWPLC_RS485Class::txPin() const
{
    return _port.txPin();
}

int JWPLC_RS485Class::available()
{
#if !JWPLC_HAS_RS485
    return 0;
#else
    return _port.available();
#endif
}

int JWPLC_RS485Class::peek()
{
#if !JWPLC_HAS_RS485
    return -1;
#else
    return _port.peek();
#endif
}

int JWPLC_RS485Class::read()
{
#if !JWPLC_HAS_RS485
    return -1;
#else
    return _port.read();
#endif
}

size_t JWPLC_RS485Class::readAvailable(
    uint8_t *buffer,
    size_t maxSize)
{
#if !JWPLC_HAS_RS485
    return 0;
#else
    return _port.readAvailable(buffer, maxSize);
#endif
}

size_t JWPLC_RS485Class::write(uint8_t data)
{
    return write(&data, 1);
}

size_t JWPLC_RS485Class::write(const uint8_t *buffer, size_t size)
{
#if !JWPLC_HAS_RS485
    _disabledError = JWPLC_RS485_DISABLED;
    return 0;
#else
    return _port.write(buffer, size);
#endif
}

size_t JWPLC_RS485Class::writeQueued(uint8_t data)
{
    return writeQueued(&data, 1);
}

size_t JWPLC_RS485Class::writeQueued(
    const uint8_t *buffer,
    size_t size)
{
#if !JWPLC_HAS_RS485
    _disabledError = JWPLC_RS485_DISABLED;
    return 0;
#else
    return _port.writeQueued(buffer, size);
#endif
}

void JWPLC_RS485Class::flush()
{
#if JWPLC_HAS_RS485
    _port.flush();
#endif
}

Stream &JWPLC_RS485Class::stream()
{
    return _port.stream();
}

HardwareSerial &JWPLC_RS485Class::serial()
{
    return _port.serial();
}

JW_RS485 &JWPLC_RS485Class::port()
{
    return _port;
}

JWPLCRS485Error JWPLC_RS485Class::lastError() const
{
#if !JWPLC_HAS_RS485
    return _disabledError;
#else
    // JWRS485Error conserva el mismo orden numerico que JWPLCRS485Error.
    return static_cast<JWPLCRS485Error>(_port.lastError());
#endif
}

const char *JWPLC_RS485Class::lastErrorString() const
{
    switch (lastError())
    {
    case JWPLC_RS485_OK:
        return "OK";
    case JWPLC_RS485_DISABLED:
        return "RS485 disabled";
    case JWPLC_RS485_NOT_STARTED:
        return "RS485 not started";
    case JWPLC_RS485_INVALID_SERIAL:
        return "Invalid Serial2";
    case JWPLC_RS485_CLOCK_SOURCE_FAILED:
        return "UART clock source setup failed";
    default:
        return "Unknown RS485 error";
    }
}

const char *JWPLC_RS485Class::statusString() const
{
#if !JWPLC_HAS_RS485
    return "RS485 disabled";
#else
    if (!isReady())
    {
        return "RS485 not started";
    }

    return "OK";
#endif
}

const char *JWPLC_RS485Class::configString() const
{
    if (_config == SERIAL_8N1)
    {
        return "SERIAL_8N1";
    }
    if (_config == SERIAL_8E1)
    {
        return "SERIAL_8E1";
    }
    if (_config == SERIAL_8O1)
    {
        return "SERIAL_8O1";
    }
    if (_config == SERIAL_8N2)
    {
        return "SERIAL_8N2";
    }
    if (_config == SERIAL_8E2)
    {
        return "SERIAL_8E2";
    }
    if (_config == SERIAL_8O2)
    {
        return "SERIAL_8O2";
    }

    return "CUSTOM";
}

void JWPLC_RS485Class::printStatus(Print &out)
{
    out.print("RS485 enabled: ");
    out.println(isEnabled() ? "yes" : "no");

    out.print("Ready: ");
    out.println(isReady() ? "yes" : "no");

    out.print("Status: ");
    out.println(statusString());

    out.print("Baud: ");
    out.println(_baud);

    out.print("Config: ");
    out.println(configString());

    out.print("RX pin: ");
    out.println(rxPin());

    out.print("TX pin: ");
    out.println(txPin());

    out.print("AutoDirection: ");
    out.println(autoDirection() ? "yes" : "no");

    out.print("UART clock source: ");
    out.println(clockSourceString());

    out.print("TX buffer bytes: ");
    out.println((unsigned long)txBufferSize());

    out.print("Queued TX supported: ");
    out.println(queuedWriteSupported() ? "yes" : "no");

    out.print("Last error: ");
    out.println(lastErrorString());
}

uint32_t JWPLC_RS485Class::lastActivityMs() const
{
    return _port.lastActivityMs();
}

uint32_t JWPLC_RS485Class::lastRxActivityMs() const
{
    return _port.lastRxActivityMs();
}

uint32_t JWPLC_RS485Class::lastTxActivityMs() const
{
    return _port.lastTxActivityMs();
}

bool JWPLC_RS485Class::hasRecentActivity(uint32_t windowMs) const
{
#if !JWPLC_HAS_RS485
    return false;
#else
    return _port.hasRecentActivity(windowMs);
#endif
}
