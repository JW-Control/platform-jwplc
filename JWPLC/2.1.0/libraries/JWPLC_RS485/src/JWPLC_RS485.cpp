#include "JWPLC_RS485.h"

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

JWPLC_RS485Class::JWPLC_RS485Class()
    : _serial(&Serial2),
      _baud(JWPLC_RS485_DEFAULT_BAUD),
      _config(JWPLC_RS485_DEFAULT_CONFIG),
      _rxPin(JWPLC_RS485_RX_PIN),
      _txPin(JWPLC_RS485_TX_PIN),
      _ready(false),
      _lastError(JWPLC_RS485_NOT_STARTED)
{
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
#if !JWPLC_HAS_RS485
    _ready = false;
    _baud = baud;
    _config = config;
    setError(JWPLC_RS485_DISABLED);
    return false;
#else
    if (_serial == nullptr)
    {
        _ready = false;
        setError(JWPLC_RS485_INVALID_SERIAL);
        return false;
    }

    _baud = baud;
    _config = config;

    _serial->begin(_baud, _config, _rxPin, _txPin);
    _ready = true;
    clearError();

    return true;
#endif
}

void JWPLC_RS485Class::end()
{
#if JWPLC_HAS_RS485
    if (_serial != nullptr)
    {
        _serial->end();
    }
#endif

    _ready = false;
    setError(JWPLC_RS485_NOT_STARTED);
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
    return _ready;
#else
    return false;
#endif
}

uint32_t JWPLC_RS485Class::baudRate() const
{
    return _baud;
}

uint32_t JWPLC_RS485Class::config() const
{
    return _config;
}

int8_t JWPLC_RS485Class::rxPin() const
{
    return _rxPin;
}

int8_t JWPLC_RS485Class::txPin() const
{
    return _txPin;
}

int JWPLC_RS485Class::available()
{
#if !JWPLC_HAS_RS485
    return 0;
#else
    if (!_ready || _serial == nullptr)
    {
        return 0;
    }

    return _serial->available();
#endif
}

int JWPLC_RS485Class::peek()
{
#if !JWPLC_HAS_RS485
    return -1;
#else
    if (!_ready || _serial == nullptr)
    {
        return -1;
    }

    return _serial->peek();
#endif
}

int JWPLC_RS485Class::read()
{
#if !JWPLC_HAS_RS485
    return -1;
#else
    if (!_ready || _serial == nullptr)
    {
        return -1;
    }

    return _serial->read();
#endif
}

size_t JWPLC_RS485Class::write(uint8_t data)
{
    return write(&data, 1);
}

size_t JWPLC_RS485Class::write(const uint8_t *buffer, size_t size)
{
#if !JWPLC_HAS_RS485
    setError(JWPLC_RS485_DISABLED);
    return 0;
#else
    if (!_ready || _serial == nullptr || buffer == nullptr || size == 0)
    {
        if (!_ready)
        {
            setError(JWPLC_RS485_NOT_STARTED);
        }
        return 0;
    }

    jwplcRs485PreTransmitCallback();

    size_t written = _serial->write(buffer, size);
    _serial->flush();

    jwplcRs485PostTransmitCallback();

    return written;
#endif
}

void JWPLC_RS485Class::flush()
{
#if JWPLC_HAS_RS485
    if (_ready && _serial != nullptr)
    {
        _serial->flush();
    }
#endif
}

Stream &JWPLC_RS485Class::stream()
{
    return *_serial;
}

HardwareSerial &JWPLC_RS485Class::serial()
{
    return *_serial;
}

JWPLCRS485Error JWPLC_RS485Class::lastError() const
{
    return _lastError;
}

const char *JWPLC_RS485Class::lastErrorString() const
{
    switch (_lastError)
    {
    case JWPLC_RS485_OK:
        return "OK";
    case JWPLC_RS485_DISABLED:
        return "RS485 disabled";
    case JWPLC_RS485_NOT_STARTED:
        return "RS485 not started";
    case JWPLC_RS485_INVALID_SERIAL:
        return "Invalid Serial2";
    default:
        return "Unknown RS485 error";
    }
}

const char *JWPLC_RS485Class::statusString() const
{
#if !JWPLC_HAS_RS485
    return "RS485 disabled";
#else
    if (!_ready)
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
    out.println(_rxPin);

    out.print("TX pin: ");
    out.println(_txPin);

    out.print("Last error: ");
    out.println(lastErrorString());
}

void JWPLC_RS485Class::setError(JWPLCRS485Error error)
{
    _lastError = error;
}

void JWPLC_RS485Class::clearError()
{
    _lastError = JWPLC_RS485_OK;
}
