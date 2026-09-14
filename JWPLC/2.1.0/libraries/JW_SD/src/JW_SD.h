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
    // Se asigna solamente al crear el DataLog.
    size_t bufferSize = 4096;

    // Commit por ocupacion. El timeout actua como segunda condicion.
    size_t commitThresholdBytes = 512;

    // Evita que muestras poco frecuentes permanezcan indefinidamente en RAM.
    uint32_t commitTimeoutMs = 5000;
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

    // =================================================
    // DataLog RAM -> microSD
    // =================================================
    //
    // dataLogWrite() acepta datos en RAM y retorna sin forzar una
    // escritura fisica a SD. serviceDataLog() decide el commit por
    // threshold o timeout.
    //
    // Durante Alpha14.6 G1 el servicio se invoca manualmente para
    // validar la libreria de forma aislada. El runtime JWPLC lo
    // llamara automaticamente en el siguiente gate.

    bool dataLogCreate(const char *path);
    bool dataLogCreate(
        const char *path,
        const JW_SDDataLogConfig &config);

    size_t dataLogWrite(
        const uint8_t *data,
        size_t size);

    size_t dataLogWrite(const char *text);
    size_t dataLogWriteLine(const char *text);

    void serviceDataLog();

    bool dataLogCommit();

    // commitPending=true intenta persistir todo antes de cerrar.
    // false descarta lo que permanezca solamente en RAM.
    bool dataLogClose(bool commitPending = true);

    bool dataLogActive() const;
    size_t dataLogPendingBytes() const;
    size_t dataLogFreeBytes() const;
    JW_SDDataLogStatus dataLogStatus() const;

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

    // =================================================
    // DataLog interno
    // =================================================

    static constexpr size_t DATALOG_MAX_PATH = 96;

    uint8_t *_dataLogBuffer = nullptr;
    size_t _dataLogBufferSize = 0;
    size_t _dataLogHead = 0;
    size_t _dataLogTail = 0;
    size_t _dataLogCount = 0;

    size_t _dataLogCommitThresholdBytes = 0;
    uint32_t _dataLogCommitTimeoutMs = 0;
    uint32_t _dataLogPendingSinceMs = 0;

    bool _dataLogActive = false;
    JWPLCFile _dataLogFile;
    char _dataLogPath[DATALOG_MAX_PATH] = {0};

    uint32_t _dataLogAcceptedWrites = 0;
    uint64_t _dataLogAcceptedBytes = 0;
    uint64_t _dataLogCommittedBytes = 0;
    uint32_t _dataLogCommitCount = 0;
    uint32_t _dataLogFailedCommits = 0;

    bool enqueueDataLogBytes(
        const uint8_t *data,
        size_t size);

    bool openDataLogFile();

    bool shouldCommitDataLog(
        uint32_t now) const;

    void resetDataLogState(
        bool releaseBuffer);

    void setError(JW_SDError error);
    bool lockForOperation();
    void unlockForOperation();
    void configureDetectPinIfNeeded();
};

#endif // JW_SD_H
