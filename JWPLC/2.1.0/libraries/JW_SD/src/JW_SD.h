#ifndef JW_SD_H
#define JW_SD_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

class JW_SD;

// =====================================================
// Resultado / error interno
// =====================================================

enum JW_SDError : uint8_t
{
    JW_SD_OK = 0,
    JW_SD_ERR_DISABLED,
    JW_SD_ERR_NO_CARD,
    JW_SD_ERR_LOCK_TIMEOUT,
    JW_SD_ERR_BEGIN_FAILED,
    JW_SD_ERR_NOT_READY,
    JW_SD_ERR_OPEN_FAILED,
    JW_SD_ERR_OPERATION_FAILED,
    JW_SD_ERR_DATALOG_INVALID_CONFIG,
    JW_SD_ERR_DATALOG_ALLOC_FAILED,
    JW_SD_ERR_DATALOG_NOT_ACTIVE,
    JW_SD_ERR_DATALOG_BUFFER_FULL,
    JW_SD_ERR_DATALOG_COMMIT_FAILED
};

// =====================================================
// DataLog de alto nivel
// =====================================================

struct JW_SDDataLogConfig
{
    static constexpr size_t DEFAULT_BUFFER_SIZE = 4096;
    static constexpr size_t DEFAULT_COMMIT_THRESHOLD_BYTES = 512;
    static constexpr uint32_t DEFAULT_COMMIT_TIMEOUT_MS = 5000;

    size_t bufferSize;
    size_t commitThresholdBytes;
    uint32_t commitTimeoutMs;

    JW_SDDataLogConfig(
        size_t buffer = DEFAULT_BUFFER_SIZE,
        size_t threshold = DEFAULT_COMMIT_THRESHOLD_BYTES,
        uint32_t timeoutMs = DEFAULT_COMMIT_TIMEOUT_MS)
        : bufferSize(buffer),
          commitThresholdBytes(threshold),
          commitTimeoutMs(timeoutMs)
    {
    }
};

struct JW_SDDataLogStatus
{
    bool active;
    size_t capacityBytes;
    size_t pendingBytes;
    size_t freeBytes;
    size_t commitThresholdBytes;
    uint32_t commitTimeoutMs;
    uint32_t acceptedWrites;
    uint64_t acceptedBytes;
    uint64_t committedBytes;
    uint32_t commitCount;
    uint32_t failedCommits;
    JW_SDError lastError;
};

// =====================================================
// Wrapper de archivo protegido
// =====================================================
//
// JWPLCFile envuelve un File nativo y ejecuta lock/unlock del bus
// alrededor de operaciones comunes. Esto permite usar microSD en un
// bus SPI compartido con TFT, FRAM, Ethernet, etc.

class JWPLCFile : public Stream
{
public:
    JWPLCFile();
    JWPLCFile(File file, JW_SD *owner);

    explicit operator bool() const;

    // Print / Stream
    size_t write(uint8_t value) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    int available() override;
    int read() override;

    // Lectura bulk protegida por el mismo lock SPI que read() byte a byte.
    size_t read(uint8_t *buffer, size_t size);

    int peek() override;
    void flush() override;

    using Print::write;

    // File-like helpers
    void close();
    bool seek(uint32_t pos);
    uint32_t position();
    uint32_t size();
    bool isDirectory();
    const char *name();

    JWPLCFile openNextFile();

#if defined(ESP32)
    JWPLCFile openNextFile(const char *mode);
#else
    JWPLCFile openNextFile(uint8_t mode);
#endif

    void rewindDirectory();

    // Acceso avanzado al File nativo.
    // Usar con cuidado: las operaciones posteriores no quedan protegidas
    // automaticamente por JW_SD.
    File &native();

private:
    File _file;
    JW_SD *_owner;

    bool lock();
    void unlock();
};

// =====================================================
// DataLog de alto nivel
// =====================================================
//
// Cada objeto JWPLCDataLog representa un archivo independiente.
// JW_SD sigue siendo responsable de la tarjeta, filesystem y SPI.
//
// El buffer se asigna solamente al ejecutar begin().
// Los objetos no son copiables porque poseen RAM dinamica y un
// handle persistente de archivo.

class JWPLCDataLog
{
public:
    JWPLCDataLog();
    ~JWPLCDataLog();

    JWPLCDataLog(const JWPLCDataLog &) = delete;
    JWPLCDataLog &operator=(const JWPLCDataLog &) = delete;

    // Uso simple: defaults de JW_SDDataLogConfig.
    bool begin(
        JW_SD &storage,
        const char *path);

    // Configuracion directa y comoda.
    bool begin(
        JW_SD &storage,
        const char *path,
        size_t bufferSize,
        size_t commitThresholdBytes,
        uint32_t commitTimeoutMs);

    // Configuracion reutilizable/avanzada.
    bool begin(
        JW_SD &storage,
        const char *path,
        const JW_SDDataLogConfig &config);

    size_t write(
        const uint8_t *data,
        size_t size);

    size_t write(const char *text);
    size_t writeLine(const char *text);

    // En G1b se invoca manualmente.
    // En G2 el runtime JWPLC registrara y servira los DataLogs.
    void service();

    bool commit();

    // true  = persiste pendientes antes de cerrar.
    // false = descarta lo que solo exista en RAM.
    bool close(bool commitPending = true);

    bool isActive() const;

    const char *path() const;

    size_t bufferSize() const;
    size_t pendingBytes() const;
    size_t freeBytes() const;

    size_t commitThreshold() const;
    uint32_t commitTimeout() const;

    uint32_t acceptedWrites() const;
    uint64_t acceptedBytes() const;
    uint64_t committedBytes() const;

    uint32_t commitCount() const;
    uint32_t failedCommits() const;

    JW_SDError lastError() const;
    const char *lastErrorString() const;

    JW_SDDataLogStatus status() const;

private:
    static constexpr size_t MAX_PATH = 96;

    JW_SD *_storage = nullptr;

    uint8_t *_buffer = nullptr;
    size_t _bufferSize = 0;
    size_t _head = 0;
    size_t _tail = 0;
    size_t _count = 0;

    size_t _commitThresholdBytes = 0;
    uint32_t _commitTimeoutMs = 0;
    uint32_t _pendingSinceMs = 0;

    bool _active = false;

    JWPLCFile _file;
    char _path[MAX_PATH] = {0};

    uint32_t _acceptedWrites = 0;
    uint64_t _acceptedBytes = 0;
    uint64_t _committedBytes = 0;

    uint32_t _commitCount = 0;
    uint32_t _failedCommits = 0;

    JW_SDError _lastError = JW_SD_OK;

    bool enqueue(
        const uint8_t *data,
        size_t size);

    bool openFile();

    bool shouldCommit(
        uint32_t now) const;

    void resetState(
        bool releaseBuffer);

    void setError(
        JW_SDError error);
};

// =====================================================
// Clase principal JW_SD
// =====================================================

class JW_SD
{
public:
    typedef bool (*LockCallback)(uint32_t timeoutMs, void *userData);
    typedef void (*UnlockCallback)(void *userData);

    JW_SD();
    JW_SD(uint8_t csPin);
    JW_SD(uint8_t csPin, SPIClass *spi, uint32_t frequency);

    void configure(uint8_t csPin);
    void configure(uint8_t csPin, SPIClass *spi, uint32_t frequency);

    void setDetectPin(int8_t detectPin, bool activeLow = true, bool usePullup = false);
    void setBusLockCallbacks(
        LockCallback lockCallback,
        UnlockCallback unlockCallback,
        void *userData = nullptr,
        uint32_t timeoutMs = 100);

    void setOperationTimeout(uint32_t timeoutMs);
    uint32_t operationTimeout() const;

    bool begin();
    bool begin(uint8_t csPin);
    bool begin(uint8_t csPin, SPIClass *spi, uint32_t frequency);

    bool isReady() const;
    bool isCardPresent() const;

    JW_SDError lastError() const;
    const char *lastErrorString() const;

    uint8_t cardType();
    uint64_t cardSize();

    bool exists(const char *path);
    bool mkdir(const char *path);
    bool rmdir(const char *path);
    bool remove(const char *path);
    bool rename(const char *pathFrom, const char *pathTo);

#if defined(ESP32)
    JWPLCFile open(const char *path, const char *mode = FILE_READ);
    File openNative(const char *path, const char *mode = FILE_READ);
#else
    JWPLCFile open(const char *path, uint8_t mode = FILE_READ);
    File openNative(const char *path, uint8_t mode = FILE_READ);
#endif
    // Herramientas avanzadas de bloqueo manual.
    // Normalmente no son necesarias si se usa JWPLCFile.
    bool lock(uint32_t timeoutMs);
    void unlock();

    void setEnabled(bool enabled);
    bool isEnabled() const;

private:
    friend class JWPLCFile;

    uint8_t _csPin;
    SPIClass *_spi;
    uint32_t _frequency;

    int8_t _detectPin;
    bool _detectActiveLow;
    bool _detectUsePullup;

    bool _enabled;
    bool _ready;
    bool _beginAttempted;

    LockCallback _lockCallback;
    UnlockCallback _unlockCallback;
    void *_lockUserData;
    uint32_t _callbackTimeoutMs;
    uint32_t _operationTimeoutMs;

    JW_SDError _lastError;
    void setError(JW_SDError error);
    bool lockForOperation();
    void unlockForOperation();
    void configureDetectPinIfNeeded();
};

#endif // JW_SD_H
