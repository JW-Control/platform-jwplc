#include "JW_RS485.h"

// ESP32 clasico con core 3.x: la fuente de reloj de la UART es configurable.
#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32) && \
    defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
#define JW_RS485_HAS_CLOCK_SOURCE 1
#else
#define JW_RS485_HAS_CLOCK_SOURCE 0
#endif

JW_RS485::JW_RS485(HardwareSerial &serial, int8_t rxPin, int8_t txPin)
    : _serial(&serial),
      _baud(JW_RS485_DEFAULT_BAUD),
      _config(JW_RS485_DEFAULT_CONFIG),
      _rxPin(rxPin),
      _txPin(txPin),
      _direction(JW_RS485_DIRECTION_CALLBACKS),
      _dePin(-1),
      _deActiveHigh(true),
      _requestedTxBufferSize(0),
      _txBufferSize(0),
      _forceApbClock(false),
      _preTransmit(nullptr),
      _preTransmitData(nullptr),
      _postTransmit(nullptr),
      _postTransmitData(nullptr),
      _activity(nullptr),
      _activityData(nullptr),
      _ready(false),
      _lastError(JW_RS485_NOT_STARTED),
      _lastActivityMs(0),
      _lastRxActivityMs(0),
      _lastTxActivityMs(0)
{
}

void JW_RS485::setAutoDirection(size_t txBufferSize)
{
    _direction = JW_RS485_DIRECTION_AUTO;
    _dePin = -1;
    _requestedTxBufferSize = txBufferSize;
}

void JW_RS485::setDirectionPin(int8_t dePin, bool activeHigh)
{
    _direction = (dePin >= 0)
        ? JW_RS485_DIRECTION_PIN
        : JW_RS485_DIRECTION_CALLBACKS;
    _dePin = dePin;
    _deActiveHigh = activeHigh;
    _requestedTxBufferSize = 0;
}

void JW_RS485::setCallbackDirection()
{
    _direction = JW_RS485_DIRECTION_CALLBACKS;
    _dePin = -1;
    _requestedTxBufferSize = 0;
}

void JW_RS485::setPreTransmitCallback(JW_RS485_Callback callback, void *userData)
{
    _preTransmit = callback;
    _preTransmitData = userData;
}

void JW_RS485::setPostTransmitCallback(JW_RS485_Callback callback, void *userData)
{
    _postTransmit = callback;
    _postTransmitData = userData;
}

void JW_RS485::setActivityCallback(JW_RS485_Callback callback, void *userData)
{
    _activity = callback;
    _activityData = userData;
}

void JW_RS485::setForceApbClock(bool enabled)
{
    _forceApbClock = enabled;
}

bool JW_RS485::begin()
{
    return begin(JW_RS485_DEFAULT_BAUD, JW_RS485_DEFAULT_CONFIG);
}

bool JW_RS485::begin(uint32_t baud)
{
    return begin(baud, JW_RS485_DEFAULT_CONFIG);
}

bool JW_RS485::begin(uint32_t baud, uint32_t config)
{
    if (_serial == nullptr)
    {
        _ready = false;
        setError(JW_RS485_INVALID_SERIAL);
        return false;
    }

    _baud = baud;
    _config = config;

    if (_direction == JW_RS485_DIRECTION_PIN && _dePin >= 0)
    {
        pinMode(_dePin, OUTPUT);
        digitalWrite(_dePin, _deActiveHigh ? LOW : HIGH);  // Recepcion
    }

    // El buffer TX solo puede cambiarse antes del primer begin().
    if (_direction == JW_RS485_DIRECTION_AUTO &&
        _requestedTxBufferSize > 0 &&
        !_ready)
    {
        const size_t configured =
            _serial->setTxBufferSize(_requestedTxBufferSize);

        if (configured > 0)
        {
            _txBufferSize = configured;
        }
    }

    if (_forceApbClock)
    {
#if JW_RS485_HAS_CLOCK_SOURCE
        if (!_serial->setClockSource(UART_CLK_SRC_APB))
        {
            _ready = false;
            setError(JW_RS485_CLOCK_SOURCE_FAILED);
            return false;
        }
#endif
    }

    _serial->begin(_baud, _config, _rxPin, _txPin);
    // HardwareSerial::begin() retorna void. Su operator bool() comprueba
    // uartIsDriverInstalled(); no declarar listo un UART que no inició.
    if (!static_cast<bool>(*_serial))
    {
        _ready = false;
        setError(JW_RS485_UNKNOWN_ERROR);
        return false;
    }

    _ready = true;
    clearError();
    _lastActivityMs = 0;
    _lastRxActivityMs = 0;
    _lastTxActivityMs = 0;

    return true;
}

void JW_RS485::end()
{
    if (_serial != nullptr)
    {
        _serial->end();
    }

    _ready = false;
    setError(JW_RS485_NOT_STARTED);
    _lastActivityMs = 0;
    _lastRxActivityMs = 0;
    _lastTxActivityMs = 0;
}

bool JW_RS485::isEnabled() const
{
    return true;
}

bool JW_RS485::isReady() const
{
    return _ready;
}

uint32_t JW_RS485::baudRate() const
{
    return _baud;
}

uint32_t JW_RS485::effectiveBaudRate() const
{
    if (_ready && _serial != nullptr)
    {
        return _serial->baudRate();
    }

    return 0;
}

uint32_t JW_RS485::config() const
{
    return _config;
}

JWRS485Direction JW_RS485::direction() const
{
    return _direction;
}

bool JW_RS485::autoDirection() const
{
    return _direction == JW_RS485_DIRECTION_AUTO;
}

int8_t JW_RS485::directionPin() const
{
    return _dePin;
}

bool JW_RS485::apbClockForced() const
{
#if JW_RS485_HAS_CLOCK_SOURCE
    return _forceApbClock;
#else
    return false;
#endif
}

const char *JW_RS485::clockSourceString() const
{
    return apbClockForced()
        ? "APB_FORCED"
        : "AUTO";
}

size_t JW_RS485::txBufferSize() const
{
    return _txBufferSize;
}

bool JW_RS485::queuedWriteSupported() const
{
    return _ready &&
           _serial != nullptr &&
           _direction == JW_RS485_DIRECTION_AUTO &&
           _txBufferSize > 0;
}

int8_t JW_RS485::rxPin() const
{
    return _rxPin;
}

int8_t JW_RS485::txPin() const
{
    return _txPin;
}

int JW_RS485::available()
{
    if (!_ready || _serial == nullptr)
    {
        return 0;
    }

    int count = _serial->available();

    if (count > 0)
    {
        markRxActivity();
    }

    return count;
}

int JW_RS485::peek()
{
    if (!_ready || _serial == nullptr)
    {
        return -1;
    }

    return _serial->peek();
}

int JW_RS485::read()
{
    if (!_ready || _serial == nullptr)
    {
        return -1;
    }

    int value = _serial->read();

    if (value >= 0)
    {
        markRxActivity();
    }

    return value;
}

size_t JW_RS485::readAvailable(
    uint8_t *buffer,
    size_t maxSize)
{
    if (!_ready ||
        _serial == nullptr ||
        buffer == nullptr ||
        maxSize == 0)
    {
        return 0;
    }

    const int availableBytes =
        _serial->available();

    if (availableBytes <= 0)
    {
        return 0;
    }

    const size_t toRead =
        (size_t)availableBytes < maxSize
            ? (size_t)availableBytes
            : maxSize;

    const size_t readBytes =
        _serial->read(buffer, toRead);

    if (readBytes > 0)
    {
        markRxActivity();
    }

    return readBytes;
}

size_t JW_RS485::write(uint8_t data)
{
    return write(&data, 1);
}

size_t JW_RS485::write(const uint8_t *buffer, size_t size)
{
    if (!_ready || _serial == nullptr || buffer == nullptr || size == 0)
    {
        if (!_ready)
        {
            setError(JW_RS485_NOT_STARTED);
        }
        return 0;
    }

    beginTransmit();

    size_t written = _serial->write(buffer, size);
    _serial->flush();

    if (written > 0)
    {
        markTxActivity();
    }

    endTransmit();

    return written;
}

size_t JW_RS485::writeQueued(uint8_t data)
{
    return writeQueued(&data, 1);
}

size_t JW_RS485::writeQueued(
    const uint8_t *buffer,
    size_t size)
{
    if (!queuedWriteSupported())
    {
        // Conserva semantica y DE/RE manual en hardware no AutoDirection.
        return write(buffer, size);
    }

    if (buffer == nullptr || size == 0)
    {
        return 0;
    }

    const size_t written =
        _serial->write(buffer, size);

    if (written > 0)
    {
        markTxActivity();
    }

    return written;
}

void JW_RS485::flush()
{
    if (_ready && _serial != nullptr)
    {
        _serial->flush();
    }
}

Stream &JW_RS485::stream()
{
    return *_serial;
}

HardwareSerial &JW_RS485::serial()
{
    return *_serial;
}

JWRS485Error JW_RS485::lastError() const
{
    return _lastError;
}

const char *JW_RS485::lastErrorString() const
{
    switch (_lastError)
    {
    case JW_RS485_OK:
        return "OK";
    case JW_RS485_DISABLED:
        return "RS485 disabled";
    case JW_RS485_NOT_STARTED:
        return "RS485 not started";
    case JW_RS485_INVALID_SERIAL:
        return "Invalid serial port";
    case JW_RS485_CLOCK_SOURCE_FAILED:
        return "UART clock source setup failed";
    default:
        return "Unknown RS485 error";
    }
}

const char *JW_RS485::statusString() const
{
    if (!_ready)
    {
        return "RS485 not started";
    }

    return "OK";
}

const char *JW_RS485::configString() const
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

void JW_RS485::printStatus(Print &out)
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

    out.print("Direction: ");
    switch (_direction)
    {
    case JW_RS485_DIRECTION_AUTO:
        out.println("AutoDirection");
        break;
    case JW_RS485_DIRECTION_PIN:
        out.print("DE/RE pin ");
        out.println(_dePin);
        break;
    default:
        out.println("callbacks");
        break;
    }

    out.print("UART clock source: ");
    out.println(clockSourceString());

    out.print("TX buffer bytes: ");
    out.println((unsigned long)_txBufferSize);

    out.print("Queued TX supported: ");
    out.println(queuedWriteSupported() ? "yes" : "no");

    out.print("Last error: ");
    out.println(lastErrorString());
}

uint32_t JW_RS485::lastActivityMs() const
{
    return _lastActivityMs;
}

uint32_t JW_RS485::lastRxActivityMs() const
{
    return _lastRxActivityMs;
}

uint32_t JW_RS485::lastTxActivityMs() const
{
    return _lastTxActivityMs;
}

bool JW_RS485::hasRecentActivity(uint32_t windowMs) const
{
    if (!_ready || _lastActivityMs == 0)
    {
        return false;
    }

    return ((uint32_t)(millis() - _lastActivityMs) <= windowMs);
}

void JW_RS485::beginTransmit()
{
    if (_preTransmit != nullptr)
    {
        _preTransmit(_preTransmitData);
    }

    if (_direction == JW_RS485_DIRECTION_PIN && _dePin >= 0)
    {
        digitalWrite(_dePin, _deActiveHigh ? HIGH : LOW);
    }
}

void JW_RS485::endTransmit()
{
    if (_direction == JW_RS485_DIRECTION_PIN && _dePin >= 0)
    {
        digitalWrite(_dePin, _deActiveHigh ? LOW : HIGH);
    }

    if (_postTransmit != nullptr)
    {
        _postTransmit(_postTransmitData);
    }
}

void JW_RS485::markRxActivity()
{
    _lastRxActivityMs = millis();
    markActivity();
}

void JW_RS485::markTxActivity()
{
    _lastTxActivityMs = millis();
    markActivity();
}

void JW_RS485::markActivity()
{
    _lastActivityMs = millis();

    if (_activity != nullptr)
    {
        _activity(_activityData);
    }
}

void JW_RS485::setError(JWRS485Error error)
{
    _lastError = error;
}

void JW_RS485::clearError()
{
    _lastError = JW_RS485_OK;
}
