#include "JW_SD.h"

#if defined(ESP32)
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

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

size_t JWPLCFile::read(uint8_t *buffer, size_t size)
{
    if (!_file || buffer == nullptr || size == 0)
    {
        return 0;
    }

    if (!lock())
    {
        return 0;
    }

    int value = _file.read(buffer, size);
    unlock();

    return (value > 0)
               ? static_cast<size_t>(value)
               : 0;
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
    case JW_SD_ERR_DATALOG_INVALID_CONFIG:
        return "DataLog invalid config";
    case JW_SD_ERR_DATALOG_ALLOC_FAILED:
        return "DataLog RAM allocation failed";
    case JW_SD_ERR_DATALOG_NOT_ACTIVE:
        return "DataLog not active";
    case JW_SD_ERR_DATALOG_BUFFER_FULL:
        return "DataLog buffer full";
    case JW_SD_ERR_DATALOG_COMMIT_FAILED:
        return "DataLog commit failed";
    case JW_SD_ERR_DATALOG_NO_SLOT:
        return "No free DataLog slot";
    case JW_SD_ERR_DATALOG_BUSY:
        return "DataLog busy";
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


// =====================================================
// JW_SD - manager de DataLogs
// =====================================================

bool JW_SD::ensureDataLogRegistryMutex()
{
#if defined(ESP32)
    if (_dataLogRegistryMutex == nullptr)
    {
        SemaphoreHandle_t mutex =
            xSemaphoreCreateMutex();

        if (mutex == nullptr)
        {
            return false;
        }

        _dataLogRegistryMutex =
            static_cast<void *>(mutex);
    }
#endif

    return true;
}

void JW_SD::lockDataLogRegistry() const
{
#if defined(ESP32)
    if (_dataLogRegistryMutex != nullptr)
    {
        xSemaphoreTake(
            static_cast<SemaphoreHandle_t>(
                _dataLogRegistryMutex),
            portMAX_DELAY);
    }
#endif
}

void JW_SD::unlockDataLogRegistry() const
{
#if defined(ESP32)
    if (_dataLogRegistryMutex != nullptr)
    {
        xSemaphoreGive(
            static_cast<SemaphoreHandle_t>(
                _dataLogRegistryMutex));
    }
#endif
}

bool JW_SD::registerDataLog(
    JWPLCDataLog *dataLog)
{
    if (dataLog == nullptr)
    {
        return false;
    }

    if (!ensureDataLogRegistryMutex())
    {
        return false;
    }

    lockDataLogRegistry();

    for (
        uint8_t i = 0;
        i < MAX_DATALOGS;
        ++i)
    {
        if (_dataLogs[i] == dataLog)
        {
            unlockDataLogRegistry();
            return true;
        }
    }

    for (
        uint8_t i = 0;
        i < MAX_DATALOGS;
        ++i)
    {
        if (_dataLogs[i] == nullptr)
        {
            _dataLogs[i] =
                dataLog;

            unlockDataLogRegistry();
            return true;
        }
    }

    unlockDataLogRegistry();

    return false;
}

void JW_SD::unregisterDataLog(
    JWPLCDataLog *dataLog)
{
    if (
        dataLog == nullptr ||
        _dataLogRegistryMutex == nullptr)
    {
        return;
    }

    lockDataLogRegistry();

    for (
        uint8_t i = 0;
        i < MAX_DATALOGS;
        ++i)
    {
        if (_dataLogs[i] == dataLog)
        {
            _dataLogs[i] =
                nullptr;

            break;
        }
    }

    unlockDataLogRegistry();
}

void JW_SD::serviceDataLogs()
{
    if (_dataLogRegistryMutex == nullptr)
    {
        return;
    }

    lockDataLogRegistry();

    for (
        uint8_t attempt = 0;
        attempt < MAX_DATALOGS;
        ++attempt)
    {
        const uint8_t index =
            _dataLogServiceCursor;

        _dataLogServiceCursor =
            static_cast<uint8_t>(
                (_dataLogServiceCursor + 1) %
                MAX_DATALOGS);

        JWPLCDataLog *dataLog =
            _dataLogs[index];

        if (dataLog != nullptr)
        {
            // El registry lock se conserva mientras service()
            // usa el puntero. close()/destructor esperan.
            dataLog->service();

            unlockDataLogRegistry();
            return;
        }
    }

    unlockDataLogRegistry();
}

uint8_t JW_SD::activeDataLogs() const
{
    if (_dataLogRegistryMutex == nullptr)
    {
        return 0;
    }

    lockDataLogRegistry();

    uint8_t active = 0;

    for (
        uint8_t i = 0;
        i < MAX_DATALOGS;
        ++i)
    {
        if (_dataLogs[i] != nullptr)
        {
            ++active;
        }
    }

    unlockDataLogRegistry();

    return active;
}


// =====================================================
// JWPLCDataLog
// =====================================================

JWPLCDataLog::JWPLCDataLog()
{
}

JWPLCDataLog::~JWPLCDataLog()
{
    JW_SD *storage =
        _storage;

    if (storage != nullptr)
    {
        storage->unregisterDataLog(
            this);
    }

    if (_file)
    {
        _file.close();
    }

    resetState(true);

#if defined(ESP32)
    if (_stateMutex != nullptr)
    {
        vSemaphoreDelete(
            static_cast<SemaphoreHandle_t>(
                _stateMutex));

        _stateMutex = nullptr;
    }
#endif
}

bool JWPLCDataLog::ensureStateMutex()
{
#if defined(ESP32)
    if (_stateMutex == nullptr)
    {
        SemaphoreHandle_t mutex =
            xSemaphoreCreateMutex();

        if (mutex == nullptr)
        {
            return false;
        }

        _stateMutex =
            static_cast<void *>(mutex);
    }
#endif

    return true;
}

void JWPLCDataLog::lockState() const
{
#if defined(ESP32)
    if (_stateMutex != nullptr)
    {
        xSemaphoreTake(
            static_cast<SemaphoreHandle_t>(
                _stateMutex),
            portMAX_DELAY);
    }
#endif
}

void JWPLCDataLog::unlockState() const
{
#if defined(ESP32)
    if (_stateMutex != nullptr)
    {
        xSemaphoreGive(
            static_cast<SemaphoreHandle_t>(
                _stateMutex));
    }
#endif
}

bool JWPLCDataLog::begin(
    JW_SD &storage,
    const char *path)
{
    JW_SDDataLogConfig config;

    return begin(
        storage,
        path,
        config);
}

bool JWPLCDataLog::begin(
    JW_SD &storage,
    const char *path,
    size_t bufferSize,
    size_t commitThresholdBytes,
    uint32_t commitTimeoutMs)
{
    JW_SDDataLogConfig config(
        bufferSize,
        commitThresholdBytes,
        commitTimeoutMs);

    return begin(
        storage,
        path,
        config);
}

bool JWPLCDataLog::begin(
    JW_SD &storage,
    const char *path,
    const JW_SDDataLogConfig &config)
{
    if (!ensureStateMutex())
    {
        setError(
            JW_SD_ERR_DATALOG_ALLOC_FAILED);

        return false;
    }

    if (isActive())
    {
        if (!close(true))
        {
            return false;
        }
    }

    lockState();

    resetState(true);

    _storage =
        &storage;

    if (!_storage->isReady())
    {
        _storage = nullptr;

        setError(
            JW_SD_ERR_NOT_READY);

        unlockState();

        return false;
    }

    if (
        path == nullptr ||
        path[0] == '\0' ||
        strlen(path) >= MAX_PATH ||
        config.bufferSize == 0 ||
        config.commitThresholdBytes == 0 ||
        config.commitThresholdBytes >
            config.bufferSize ||
        config.commitTimeoutMs == 0)
    {
        _storage = nullptr;

        setError(
            JW_SD_ERR_DATALOG_INVALID_CONFIG);

        unlockState();

        return false;
    }

    _buffer =
        static_cast<uint8_t *>(
            malloc(config.bufferSize));

    if (_buffer == nullptr)
    {
        _storage = nullptr;

        setError(
            JW_SD_ERR_DATALOG_ALLOC_FAILED);

        unlockState();

        return false;
    }

    _bufferSize =
        config.bufferSize;

    _commitThresholdBytes =
        config.commitThresholdBytes;

    _commitTimeoutMs =
        config.commitTimeoutMs;

    strncpy(
        _path,
        path,
        MAX_PATH - 1);

    _path[MAX_PATH - 1] =
        '\0';

    unlockState();

    if (!openFile())
    {
        JW_SDError error =
            storage.lastError();

        if (error == JW_SD_OK)
        {
            error =
                JW_SD_ERR_OPEN_FAILED;
        }

        lockState();

        resetState(true);
        setError(error);

        unlockState();

        return false;
    }

    lockState();

    _active = true;

    unlockState();

    if (!storage.registerDataLog(this))
    {
        _file.close();

        lockState();

        resetState(true);

        setError(
            JW_SD_ERR_DATALOG_NO_SLOT);

        unlockState();

        return false;
    }

    lockState();

    setError(JW_SD_OK);

    unlockState();

    return true;
}

size_t JWPLCDataLog::freeBytesUnsafe() const
{
    if (
        _buffer == nullptr ||
        _bufferSize < _count)
    {
        return 0;
    }

    return (
        _bufferSize -
        _count);
}

bool JWPLCDataLog::enqueueUnsafe(
    const uint8_t *data,
    size_t size)
{
    if (
        _buffer == nullptr ||
        data == nullptr ||
        size == 0 ||
        size > freeBytesUnsafe())
    {
        return false;
    }

    const size_t remainingToEnd =
        _bufferSize -
        _head;

    const size_t first =
        (size < remainingToEnd)
            ? size
            : remainingToEnd;

    memcpy(
        _buffer + _head,
        data,
        first);

    const size_t second =
        size -
        first;

    if (second > 0)
    {
        memcpy(
            _buffer,
            data + first,
            second);
    }

    _head =
        (_head + size) %
        _bufferSize;

    _count +=
        size;

    return true;
}

size_t JWPLCDataLog::write(
    const uint8_t *data,
    size_t size)
{
    if (!ensureStateMutex())
    {
        return 0;
    }

    lockState();

    if (!_active)
    {
        setError(
            JW_SD_ERR_DATALOG_NOT_ACTIVE);

        unlockState();

        return 0;
    }

    if (_closing)
    {
        setError(
            JW_SD_ERR_DATALOG_BUSY);

        unlockState();

        return 0;
    }

    if (data == nullptr)
    {
        setError(
            JW_SD_ERR_DATALOG_INVALID_CONFIG);

        unlockState();

        return 0;
    }

    if (size == 0)
    {
        setError(JW_SD_OK);

        unlockState();

        return 0;
    }

    // Registro atomico: entra completo o no entra.
    if (size > freeBytesUnsafe())
    {
        setError(
            JW_SD_ERR_DATALOG_BUFFER_FULL);

        unlockState();

        return 0;
    }

    const bool wasEmpty =
        (_count == 0);

    if (!enqueueUnsafe(
            data,
            size))
    {
        setError(
            JW_SD_ERR_DATALOG_BUFFER_FULL);

        unlockState();

        return 0;
    }

    if (wasEmpty)
    {
        _pendingSinceMs =
            millis();
    }

    ++_acceptedWrites;

    _acceptedBytes +=
        size;

    setError(JW_SD_OK);

    unlockState();

    return size;
}

size_t JWPLCDataLog::write(
    const char *text)
{
    if (text == nullptr)
    {
        setError(
            JW_SD_ERR_DATALOG_INVALID_CONFIG);

        return 0;
    }

    return write(
        reinterpret_cast<const uint8_t *>(
            text),
        strlen(text));
}

size_t JWPLCDataLog::writeLine(
    const char *text)
{
    if (
        text == nullptr ||
        !ensureStateMutex())
    {
        return 0;
    }

    lockState();

    if (!_active)
    {
        setError(
            JW_SD_ERR_DATALOG_NOT_ACTIVE);

        unlockState();

        return 0;
    }

    if (_closing)
    {
        setError(
            JW_SD_ERR_DATALOG_BUSY);

        unlockState();

        return 0;
    }

    const size_t textSize =
        strlen(text);

    const size_t totalSize =
        textSize + 1;

    if (totalSize > freeBytesUnsafe())
    {
        setError(
            JW_SD_ERR_DATALOG_BUFFER_FULL);

        unlockState();

        return 0;
    }

    const bool wasEmpty =
        (_count == 0);

    if (
        textSize > 0 &&
        !enqueueUnsafe(
            reinterpret_cast<const uint8_t *>(
                text),
            textSize))
    {
        setError(
            JW_SD_ERR_DATALOG_BUFFER_FULL);

        unlockState();

        return 0;
    }

    static const uint8_t newline =
        '\n';

    if (!enqueueUnsafe(
            &newline,
            1))
    {
        setError(
            JW_SD_ERR_DATALOG_BUFFER_FULL);

        unlockState();

        return 0;
    }

    if (wasEmpty)
    {
        _pendingSinceMs =
            millis();
    }

    ++_acceptedWrites;

    _acceptedBytes +=
        totalSize;

    setError(JW_SD_OK);

    unlockState();

    return totalSize;
}

bool JWPLCDataLog::openFile()
{
    if (
        _storage == nullptr ||
        !_storage->isReady() ||
        _path[0] == '\0')
    {
        return false;
    }

#if defined(ESP32)
    _file =
        _storage->open(
            _path,
            FILE_APPEND);
#else
    _file =
        _storage->open(
            _path,
            FILE_WRITE);
#endif

    return static_cast<bool>(
        _file);
}

bool JWPLCDataLog::shouldCommitUnsafe(
    uint32_t now) const
{
    if (
        !_active ||
        _closing ||
        _commitInProgress ||
        _count == 0)
    {
        return false;
    }

    if (
        _count >=
        _commitThresholdBytes)
    {
        return true;
    }

    return (
        (uint32_t)(
            now -
            _pendingSinceMs) >=
        _commitTimeoutMs);
}

void JWPLCDataLog::service()
{
    if (!ensureStateMutex())
    {
        return;
    }

    lockState();

    const bool shouldCommit =
        shouldCommitUnsafe(
            millis());

    unlockState();

    if (shouldCommit)
    {
        (void)commit();
    }
}

bool JWPLCDataLog::commit()
{
    return commitInternal(false);
}

bool JWPLCDataLog::commitInternal(
    bool allowClosing)
{
    if (!ensureStateMutex())
    {
        return false;
    }

    lockState();

    if (!_active)
    {
        setError(
            JW_SD_ERR_DATALOG_NOT_ACTIVE);

        unlockState();

        return false;
    }

    if (
        _closing &&
        !allowClosing)
    {
        setError(
            JW_SD_ERR_DATALOG_BUSY);

        unlockState();

        return false;
    }

    if (_commitInProgress)
    {
        setError(
            JW_SD_ERR_DATALOG_BUSY);

        unlockState();

        return false;
    }

    if (_storage == nullptr)
    {
        setError(
            JW_SD_ERR_NOT_READY);

        unlockState();

        return false;
    }

    if (_count == 0)
    {
        setError(JW_SD_OK);

        unlockState();

        return true;
    }

    _commitInProgress =
        true;

    JW_SD *storage =
        _storage;

    // Snapshot del ring al iniciar el commit.
    //
    // Regla Alpha14:
    // NO se avanza tail ni se reduce count mientras el
    // almacenamiento fisico no haya terminado y la tarjeta
    // siga confirmada como presente.
    //
    // Esto preserva los datos en RAM ante extraccion durante
    // write()/flush().
    const size_t snapshotTail =
        _tail;

    const size_t snapshotCount =
        _count;

    unlockState();

    auto finishFailure =
        [this](JW_SDError error) -> bool
    {
        lockState();

        ++_failedCommits;

        _commitInProgress =
            false;

        setError(error);

        unlockState();

        return false;
    };

    if (!storage->isReady())
    {
        return finishFailure(
            JW_SD_ERR_NOT_READY);
    }

    // Primera barrera de presencia.
    if (!storage->isCardPresent())
    {
        return finishFailure(
            JW_SD_ERR_NO_CARD);
    }

    if (!_file)
    {
        if (!openFile())
        {
            JW_SDError error =
                storage->lastError();

            if (error == JW_SD_OK)
            {
                error =
                    JW_SD_ERR_DATALOG_COMMIT_FAILED;
            }

            return finishFailure(
                error);
        }
    }

    size_t remaining =
        snapshotCount;

    size_t readIndex =
        snapshotTail;

    while (remaining > 0)
    {
        // La tarjeta puede desaparecer despues de la
        // comprobacion inicial. Revalidar antes de cada
        // transaccion fisica.
        if (!storage->isCardPresent())
        {
            return finishFailure(
                JW_SD_ERR_NO_CARD);
        }

        const size_t remainingToEnd =
            _bufferSize -
            readIndex;

        const size_t contiguous =
            (remaining < remainingToEnd)
                ? remaining
                : remainingToEnd;

        const size_t written =
            _file.write(
                _buffer + readIndex,
                contiguous);

        if (written != contiguous)
        {
            JW_SDError error =
                storage->isCardPresent()
                    ? storage->lastError()
                    : JW_SD_ERR_NO_CARD;

            if (error == JW_SD_OK)
            {
                error =
                    JW_SD_ERR_DATALOG_COMMIT_FAILED;
            }

            // El ring NO se consume.
            //
            // Una escritura fisica interrumpida puede haber
            // alcanzado parcialmente al filesystem, por lo que
            // la politica de recuperacion posterior se valida
            // fisicamente en G3b-R.
            return finishFailure(
                error);
        }

        readIndex =
            (readIndex + written) %
            _bufferSize;

        remaining -=
            written;
    }

    // flush() no devuelve estado en JWPLCFile.
    // Por eso no se considera durable hasta comprobar ademas
    // que la tarjeta continua fisicamente presente.
    _file.flush();

    // Segunda barrera: post-write / post-flush.
    if (!storage->isCardPresent())
    {
        return finishFailure(
            JW_SD_ERR_NO_CARD);
    }

    lockState();

    // Ningun otro commit puede mover tail mientras
    // _commitInProgress=true. Los productores solo pueden
    // agregar por head, por lo que el snapshot original sigue
    // retenido hasta este punto.
    const bool snapshotStillValid =
        _tail == snapshotTail &&
        _count >= snapshotCount;

    if (!snapshotStillValid)
    {
        ++_failedCommits;

        _commitInProgress =
            false;

        setError(
            JW_SD_ERR_DATALOG_BUSY);

        unlockState();

        return false;
    }

    // SOLO AHORA el snapshot pasa de PENDING a COMMITTED.
    _tail =
        (_tail + snapshotCount) %
        _bufferSize;

    _count -=
        snapshotCount;

    _committedBytes +=
        snapshotCount;

    ++_commitCount;

    if (_count == 0)
    {
        _pendingSinceMs = 0;
    }
    else
    {
        // Los registros añadidos durante el commit forman
        // la siguiente ventana pendiente.
        _pendingSinceMs =
            millis();
    }

    _commitInProgress =
        false;

    setError(JW_SD_OK);

    unlockState();

    return true;
}

bool JWPLCDataLog::close(
    bool commitPending)
{
    if (!ensureStateMutex())
    {
        return false;
    }

    lockState();

    if (!_active)
    {
        setError(
            JW_SD_ERR_DATALOG_NOT_ACTIVE);

        unlockState();

        return false;
    }

    // Nunca cerrar el File mientras otro commit fisico
    // sigue usando el mismo handle.
    if (_commitInProgress)
    {
        setError(
            JW_SD_ERR_DATALOG_BUSY);

        unlockState();

        return false;
    }

    _closing =
        true;

    const bool hasPending =
        (_count > 0);

    JW_SD *storage =
        _storage;

    unlockState();

    // Al desregistrarse, el runtime ya no puede iniciar
    // otro service() sobre este objeto.
    if (storage != nullptr)
    {
        storage->unregisterDataLog(
            this);
    }

    if (
        commitPending &&
        hasPending)
    {
        if (!commitInternal(true))
        {
            lockState();

            _closing = false;

            unlockState();

            if (storage != nullptr)
            {
                (void)storage->registerDataLog(
                    this);
            }

            return false;
        }
    }

    if (_file)
    {
        _file.close();
    }

    lockState();

    resetState(true);

    setError(JW_SD_OK);

    unlockState();

    return true;
}

bool JWPLCDataLog::isActive() const
{
    if (_stateMutex == nullptr)
    {
        return false;
    }

    lockState();

    const bool value =
        _active;

    unlockState();

    return value;
}

const char *JWPLCDataLog::path() const
{
    return _path;
}

size_t JWPLCDataLog::bufferSize() const
{
    if (_stateMutex == nullptr)
    {
        return 0;
    }

    lockState();

    const size_t value =
        _bufferSize;

    unlockState();

    return value;
}

size_t JWPLCDataLog::pendingBytes() const
{
    if (_stateMutex == nullptr)
    {
        return 0;
    }

    lockState();

    const size_t value =
        _count;

    unlockState();

    return value;
}

size_t JWPLCDataLog::freeBytes() const
{
    if (_stateMutex == nullptr)
    {
        return 0;
    }

    lockState();

    const size_t value =
        freeBytesUnsafe();

    unlockState();

    return value;
}

size_t JWPLCDataLog::commitThreshold() const
{
    if (_stateMutex == nullptr)
    {
        return 0;
    }

    lockState();

    const size_t value =
        _commitThresholdBytes;

    unlockState();

    return value;
}

uint32_t JWPLCDataLog::commitTimeout() const
{
    if (_stateMutex == nullptr)
    {
        return 0;
    }

    lockState();

    const uint32_t value =
        _commitTimeoutMs;

    unlockState();

    return value;
}

uint32_t JWPLCDataLog::acceptedWrites() const
{
    if (_stateMutex == nullptr)
    {
        return 0;
    }

    lockState();

    const uint32_t value =
        _acceptedWrites;

    unlockState();

    return value;
}

uint64_t JWPLCDataLog::acceptedBytes() const
{
    if (_stateMutex == nullptr)
    {
        return 0;
    }

    lockState();

    const uint64_t value =
        _acceptedBytes;

    unlockState();

    return value;
}

uint64_t JWPLCDataLog::committedBytes() const
{
    if (_stateMutex == nullptr)
    {
        return 0;
    }

    lockState();

    const uint64_t value =
        _committedBytes;

    unlockState();

    return value;
}

uint32_t JWPLCDataLog::commitCount() const
{
    if (_stateMutex == nullptr)
    {
        return 0;
    }

    lockState();

    const uint32_t value =
        _commitCount;

    unlockState();

    return value;
}

uint32_t JWPLCDataLog::failedCommits() const
{
    if (_stateMutex == nullptr)
    {
        return 0;
    }

    lockState();

    const uint32_t value =
        _failedCommits;

    unlockState();

    return value;
}

JW_SDError JWPLCDataLog::lastError() const
{
    if (_stateMutex == nullptr)
    {
        return _lastError;
    }

    lockState();

    const JW_SDError value =
        _lastError;

    unlockState();

    return value;
}

const char *JWPLCDataLog::lastErrorString() const
{
    switch (lastError())
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

    case JW_SD_ERR_DATALOG_INVALID_CONFIG:
        return "DataLog invalid config";

    case JW_SD_ERR_DATALOG_ALLOC_FAILED:
        return "DataLog RAM allocation failed";

    case JW_SD_ERR_DATALOG_NOT_ACTIVE:
        return "DataLog not active";

    case JW_SD_ERR_DATALOG_BUFFER_FULL:
        return "DataLog buffer full";

    case JW_SD_ERR_DATALOG_COMMIT_FAILED:
        return "DataLog commit failed";

    case JW_SD_ERR_DATALOG_NO_SLOT:
        return "No free DataLog slot";

    case JW_SD_ERR_DATALOG_BUSY:
        return "DataLog busy";

    default:
        return "Unknown error";
    }
}

JW_SDDataLogStatus JWPLCDataLog::status() const
{
    JW_SDDataLogStatus result{};

    if (_stateMutex == nullptr)
    {
        result.lastError =
            _lastError;

        return result;
    }

    lockState();

    result.active =
        _active;

    result.capacityBytes =
        _bufferSize;

    result.pendingBytes =
        _count;

    result.freeBytes =
        freeBytesUnsafe();

    result.commitThresholdBytes =
        _commitThresholdBytes;

    result.commitTimeoutMs =
        _commitTimeoutMs;

    result.acceptedWrites =
        _acceptedWrites;

    result.acceptedBytes =
        _acceptedBytes;

    result.committedBytes =
        _committedBytes;

    result.commitCount =
        _commitCount;

    result.failedCommits =
        _failedCommits;

    result.lastError =
        _lastError;

    unlockState();

    return result;
}

void JWPLCDataLog::resetState(
    bool releaseBuffer)
{
    if (
        releaseBuffer &&
        _buffer != nullptr)
    {
        free(_buffer);

        _buffer =
            nullptr;
    }

    _storage = nullptr;

    _bufferSize = 0;

    _head = 0;
    _tail = 0;
    _count = 0;

    _commitThresholdBytes = 0;
    _commitTimeoutMs = 0;
    _pendingSinceMs = 0;

    _active = false;
    _closing = false;
    _commitInProgress = false;

    _file =
        JWPLCFile();

    _path[0] =
        '\0';

    _acceptedWrites = 0;
    _acceptedBytes = 0;
    _committedBytes = 0;

    _commitCount = 0;
    _failedCommits = 0;
}

void JWPLCDataLog::setError(
    JW_SDError error)
{
    _lastError =
        error;
}
