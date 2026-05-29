#include "JW_SD.h"

// =====================================================
// JWPLCFile
// =====================================================

JWPLCFile::JWPLCFile()
    : _file(), _owner(nullptr)
{
}

JWPLCFile::JWPLCFile(File file, JW_SD *owner)
    : _file(file), _owner(owner)
{
}

JWPLCFile::operator bool() const
{
    return (bool)_file;
}

bool JWPLCFile::lock()
{
    if (_owner == nullptr)
    {
        return true;
    }

    return _owner->lockForOperation();
}

void JWPLCFile::unlock()
{
    if (_owner != nullptr)
    {
        _owner->unlockForOperation();
    }
}

size_t JWPLCFile::write(uint8_t value)
{
    if (!_file)
    {
        return 0;
    }

    if (!lock())
    {
        return 0;
    }

    size_t written = _file.write(value);
    unlock();

    return written;
}

size_t JWPLCFile::write(const uint8_t *buffer, size_t size)
{
    if (!_file || buffer == nullptr || size == 0)
    {
        return 0;
    }

    if (!lock())
    {
        return 0;
    }

    size_t written = _file.write(buffer, size);
    unlock();

    return written;
}

int JWPLCFile::available()
{
    if (!_file)
    {
        return 0;
    }

    if (!lock())
    {
        return 0;
    }

    int value = _file.available();
    unlock();

    return value;
}

int JWPLCFile::read()
{
    if (!_file)
    {
        return -1;
    }

    if (!lock())
    {
        return -1;
    }

    int value = _file.read();
    unlock();

    return value;
}

int JWPLCFile::peek()
{
    if (!_file)
    {
        return -1;
    }

    if (!lock())
    {
        return -1;
    }

    int value = _file.peek();
    unlock();

    return value;
}

void JWPLCFile::flush()
{
    if (!_file)
    {
        return;
    }

    if (!lock())
    {
        return;
    }

    _file.flush();
    unlock();
}

void JWPLCFile::close()
{
    if (!_file)
    {
        return;
    }

    if (!lock())
    {
        return;
    }

    _file.close();
    unlock();
}

bool JWPLCFile::seek(uint32_t pos)
{
    if (!_file)
    {
        return false;
    }

    if (!lock())
    {
        return false;
    }

    bool ok = _file.seek(pos);
    unlock();

    return ok;
}

uint32_t JWPLCFile::position()
{
    if (!_file)
    {
        return 0;
    }

    if (!lock())
    {
        return 0;
    }

    uint32_t value = _file.position();
    unlock();

    return value;
}

uint32_t JWPLCFile::size()
{
    if (!_file)
    {
        return 0;
    }

    if (!lock())
    {
        return 0;
    }

    uint32_t value = _file.size();
    unlock();

    return value;
}

bool JWPLCFile::isDirectory()
{
    if (!_file)
    {
        return false;
    }

    if (!lock())
    {
        return false;
    }

    bool value = _file.isDirectory();
    unlock();

    return value;
}

const char *JWPLCFile::name()
{
    if (!_file)
    {
        return "";
    }

    if (!lock())
    {
        return "";
    }

    const char *value = _file.name();
    unlock();

    return value;
}

JWPLCFile JWPLCFile::openNextFile()
{
#if defined(ESP32)
    return openNextFile(FILE_READ);
#else
    return openNextFile(FILE_READ);
#endif
}

#if defined(ESP32)
JWPLCFile JWPLCFile::openNextFile(const char *mode)
#else
JWPLCFile JWPLCFile::openNextFile(uint8_t mode)
#endif
{
    if (!_file)
    {
        return JWPLCFile();
    }

    if (!lock())
    {
        return JWPLCFile();
    }

    File next = _file.openNextFile(mode);
    unlock();

    return JWPLCFile(next, _owner);
}

void JWPLCFile::rewindDirectory()
{
    if (!_file)
    {
        return;
    }

    if (!lock())
    {
        return;
    }

    _file.rewindDirectory();
    unlock();
}

File &JWPLCFile::native()
{
    return _file;
}

// =====================================================
// JW_SD
// =====================================================

JW_SD::JW_SD()
    : _csPin(SS),
      _spi(&SPI),
      _frequency(4000000UL),
      _detectPin(-1),
      _detectActiveLow(true),
      _detectUsePullup(false),
      _enabled(true),
      _ready(false),
      _beginAttempted(false),
      _lockCallback(nullptr),
      _unlockCallback(nullptr),
      _lockUserData(nullptr),
      _callbackTimeoutMs(100),
      _operationTimeoutMs(100),
      _lastError(JW_SD_OK)
{
}

JW_SD::JW_SD(uint8_t csPin)
    : JW_SD()
{
    configure(csPin);
}

JW_SD::JW_SD(uint8_t csPin, SPIClass *spi, uint32_t frequency)
    : JW_SD()
{
    configure(csPin, spi, frequency);
}

void JW_SD::configure(uint8_t csPin)
{
    _csPin = csPin;
}

void JW_SD::configure(uint8_t csPin, SPIClass *spi, uint32_t frequency)
{
    _csPin = csPin;
    _spi = (spi != nullptr) ? spi : &SPI;
    _frequency = frequency;
}

void JW_SD::setDetectPin(int8_t detectPin, bool activeLow, bool usePullup)
{
    _detectPin = detectPin;
    _detectActiveLow = activeLow;
    _detectUsePullup = usePullup;
}

void JW_SD::setBusLockCallbacks(
    LockCallback lockCallback,
    UnlockCallback unlockCallback,
    void *userData,
    uint32_t timeoutMs)
{
    _lockCallback = lockCallback;
    _unlockCallback = unlockCallback;
    _lockUserData = userData;
    _callbackTimeoutMs = timeoutMs;
}

void JW_SD::setOperationTimeout(uint32_t timeoutMs)
{
    _operationTimeoutMs = timeoutMs;
}

uint32_t JW_SD::operationTimeout() const
{
    return _operationTimeoutMs;
}

void JW_SD::configureDetectPinIfNeeded()
{
    if (_detectPin < 0)
    {
        return;
    }

    pinMode((uint8_t)_detectPin, _detectUsePullup ? INPUT_PULLUP : INPUT);
}

bool JW_SD::begin()
{
    _beginAttempted = true;
    _ready = false;

    if (!_enabled)
    {
        setError(JW_SD_ERR_DISABLED);
        return false;
    }

    setError(JW_SD_OK);

    configureDetectPinIfNeeded();

    if (!isCardPresent())
    {
        setError(JW_SD_ERR_NO_CARD);
        return false;
    }

    if (!lock(_callbackTimeoutMs))
    {
        setError(JW_SD_ERR_LOCK_TIMEOUT);
        return false;
    }

    if (_spi == nullptr)
    {
        _spi = &SPI;
    }

#if defined(ESP32)
    _spi->begin();
    _ready = SD.begin(_csPin, *_spi, _frequency);
#else
    _ready = SD.begin(_csPin);
#endif

    unlock();

    if (!_ready)
    {
        setError(JW_SD_ERR_BEGIN_FAILED);
        return false;
    }

    setError(JW_SD_OK);
    return true;
}

bool JW_SD::begin(uint8_t csPin)
{
    configure(csPin);
    return begin();
}

bool JW_SD::begin(uint8_t csPin, SPIClass *spi, uint32_t frequency)
{
    configure(csPin, spi, frequency);
    return begin();
}

bool JW_SD::isReady() const
{
    return _ready;
}

bool JW_SD::isCardPresent() const
{
    if (!_enabled)
    {
        return false;
    }

    if (_detectPin < 0)
    {
        return true;
    }

    int level = digitalRead((uint8_t)_detectPin);
    return _detectActiveLow ? (level == LOW) : (level == HIGH);
}

JW_SDError JW_SD::lastError() const
{
    return _lastError;
}

void JW_SD::setError(JW_SDError error)
{
    _lastError = error;
}

const char *JW_SD::lastErrorString() const
{
    switch (_lastError)
    {
    case JW_SD_OK:
        return "OK";
    case JW_SD_ERR_DISABLED:
        return "SD disabled";
    case JW_SD_ERR_NO_CARD:
        return "No card";
    case JW_SD_ERR_LOCK_TIMEOUT:
        return "SPI lock timeout";
    case JW_SD_ERR_BEGIN_FAILED:
        return "SD begin failed";
    case JW_SD_ERR_NOT_READY:
        return "SD not ready";
    case JW_SD_ERR_OPEN_FAILED:
        return "Open failed";
    case JW_SD_ERR_OPERATION_FAILED:
        return "Operation failed";
    default:
        return "Unknown error";
    }
}

uint8_t JW_SD::cardType()
{
#if defined(ESP32)
    if (!_ready)
    {
        return 0;
    }

    if (!lockForOperation())
    {
        return 0;
    }

    uint8_t value = SD.cardType();
    unlockForOperation();

    return value;
#else
    return 0;
#endif
}

uint64_t JW_SD::cardSize()
{
#if defined(ESP32)
    if (!_ready)
    {
        return 0;
    }

    if (!lockForOperation())
    {
        return 0;
    }

    uint64_t value = SD.cardSize();
    unlockForOperation();

    return value;
#else
    return 0;
#endif
}

bool JW_SD::exists(const char *path)
{
    if (!_ready || path == nullptr)
    {
        setError(JW_SD_ERR_NOT_READY);
        return false;
    }

    if (!lockForOperation())
    {
        return false;
    }

    bool ok = SD.exists(path);
    unlockForOperation();

    setError(ok ? JW_SD_OK : JW_SD_ERR_OPERATION_FAILED);
    return ok;
}

bool JW_SD::mkdir(const char *path)
{
    if (!_ready || path == nullptr)
    {
        setError(JW_SD_ERR_NOT_READY);
        return false;
    }

    if (!lockForOperation())
    {
        return false;
    }

    bool ok = SD.mkdir(path);
    unlockForOperation();

    setError(ok ? JW_SD_OK : JW_SD_ERR_OPERATION_FAILED);
    return ok;
}

bool JW_SD::rmdir(const char *path)
{
    if (!_ready || path == nullptr)
    {
        setError(JW_SD_ERR_NOT_READY);
        return false;
    }

    if (!lockForOperation())
    {
        return false;
    }

    bool ok = SD.rmdir(path);
    unlockForOperation();

    setError(ok ? JW_SD_OK : JW_SD_ERR_OPERATION_FAILED);
    return ok;
}

bool JW_SD::remove(const char *path)
{
    if (!_ready || path == nullptr)
    {
        setError(JW_SD_ERR_NOT_READY);
        return false;
    }

    if (!lockForOperation())
    {
        return false;
    }

    bool ok = SD.remove(path);
    unlockForOperation();

    setError(ok ? JW_SD_OK : JW_SD_ERR_OPERATION_FAILED);
    return ok;
}

bool JW_SD::rename(const char *pathFrom, const char *pathTo)
{
    if (!_ready || pathFrom == nullptr || pathTo == nullptr)
    {
        setError(JW_SD_ERR_NOT_READY);
        return false;
    }

    if (!lockForOperation())
    {
        return false;
    }

#if defined(ESP32)
    bool ok = SD.rename(pathFrom, pathTo);
#else
    bool ok = false;
#endif

    unlockForOperation();

    setError(ok ? JW_SD_OK : JW_SD_ERR_OPERATION_FAILED);
    return ok;
}

#if defined(ESP32)
JWPLCFile JW_SD::open(const char *path, const char *mode)
#else
JWPLCFile JW_SD::open(const char *path, uint8_t mode)
#endif
{
    if (!_ready || path == nullptr)
    {
        setError(JW_SD_ERR_NOT_READY);
        return JWPLCFile();
    }

    if (!lockForOperation())
    {
        return JWPLCFile();
    }

    File file = SD.open(path, mode);
    unlockForOperation();

    if (!file)
    {
        setError(JW_SD_ERR_OPEN_FAILED);
        return JWPLCFile();
    }

    setError(JW_SD_OK);
    return JWPLCFile(file, this);
}

#if defined(ESP32)
File JW_SD::openNative(const char *path, const char *mode)
#else
File JW_SD::openNative(const char *path, uint8_t mode)
#endif
{
    if (!_ready || path == nullptr)
    {
        setError(JW_SD_ERR_NOT_READY);
        return File();
    }

    if (!lockForOperation())
    {
        return File();
    }

    File file = SD.open(path, mode);
    unlockForOperation();

    if (!file)
    {
        setError(JW_SD_ERR_OPEN_FAILED);
    }
    else
    {
        setError(JW_SD_OK);
    }

    return file;
}

bool JW_SD::lock(uint32_t timeoutMs)
{
    if (_lockCallback == nullptr)
    {
        return true;
    }

    return _lockCallback(timeoutMs, _lockUserData);
}

void JW_SD::unlock()
{
    if (_unlockCallback != nullptr)
    {
        _unlockCallback(_lockUserData);
    }
}

bool JW_SD::lockForOperation()
{
    if (!lock(_operationTimeoutMs))
    {
        setError(JW_SD_ERR_LOCK_TIMEOUT);
        return false;
    }

    return true;
}

void JW_SD::unlockForOperation()
{
    unlock();
}

void JW_SD::setEnabled(bool enabled)
{
    _enabled = enabled;

    if (!_enabled)
    {
        _ready = false;
        setError(JW_SD_ERR_DISABLED);
        return;
    }

    if (_lastError == JW_SD_ERR_DISABLED)
    {
        setError(JW_SD_OK);
    }
}

bool JW_SD::isEnabled() const
{
    return _enabled;
}
