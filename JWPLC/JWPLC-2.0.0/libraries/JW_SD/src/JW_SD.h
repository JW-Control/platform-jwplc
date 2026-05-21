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
    JW_SD_ERR_OPERATION_FAILED
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
