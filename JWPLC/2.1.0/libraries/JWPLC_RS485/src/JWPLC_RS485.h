#ifndef JWPLC_RS485_H
#define JWPLC_RS485_H

#include <Arduino.h>
#include "jwplc_hardware_config.h"

#ifndef JWPLC_RS485_DEFAULT_BAUD
#define JWPLC_RS485_DEFAULT_BAUD 115200UL
#endif

#ifndef JWPLC_RS485_DEFAULT_CONFIG
#define JWPLC_RS485_DEFAULT_CONFIG SERIAL_8N1
#endif

// ESP32 clasico: REF_TICK produce un error medido de +0.6441 % a
// 230400 baud. La policy del puerto RS-485 JWPLC fuerza APB antes de begin().
// Otros SoC conservan la seleccion del core hasta qualification propia.
#ifndef JWPLC_RS485_FORCE_APB_CLOCK
#if defined(CONFIG_IDF_TARGET_ESP32)
#define JWPLC_RS485_FORCE_APB_CLOCK 1
#else
#define JWPLC_RS485_FORCE_APB_CLOCK 0
#endif
#endif

enum JWPLCRS485Error : uint8_t
{
    JWPLC_RS485_OK = 0,
    JWPLC_RS485_DISABLED,
    JWPLC_RS485_NOT_STARTED,
    JWPLC_RS485_INVALID_SERIAL,
    JWPLC_RS485_UNKNOWN_ERROR,
    JWPLC_RS485_CLOCK_SOURCE_FAILED
};

class JWPLC_RS485Class : public Stream
{
public:
    JWPLC_RS485Class();

    bool begin();
    bool begin(uint32_t baud);
    bool begin(uint32_t baud, uint32_t config);
    void end();

    bool isEnabled() const;
    bool isReady() const;

    uint32_t baudRate() const;
    uint32_t effectiveBaudRate() const;
    uint32_t config() const;
    bool autoDirection() const;
    bool apbClockForced() const;
    const char *clockSourceString() const;
    size_t txBufferSize() const;
    bool queuedWriteSupported() const;

    uint32_t lastActivityMs() const;
    uint32_t lastRxActivityMs() const;
    uint32_t lastTxActivityMs() const;
    bool hasRecentActivity(uint32_t windowMs) const;

    int8_t rxPin() const;
    int8_t txPin() const;

    int available();
    int peek();
    int read();

    size_t write(uint8_t data) override;
    size_t write(const uint8_t *buffer, size_t size) override;

    // Ruta aditiva para hardware AutoDirection. Encola y retorna sin flush.
    // Si el hardware no la soporta, cae de forma segura al write bloqueante.
    size_t writeQueued(uint8_t data);
    size_t writeQueued(const uint8_t *buffer, size_t size);

    using Print::write;

    void flush();

    Stream &stream();
    HardwareSerial &serial();

    JWPLCRS485Error lastError() const;
    const char *lastErrorString() const;
    const char *statusString() const;
    const char *configString() const;

    void printStatus(Print &out);

private:
    HardwareSerial *_serial;
    uint32_t _baud;
    uint32_t _config;
    int8_t _rxPin;
    int8_t _txPin;
    size_t _txBufferSize;
    bool _ready;
    JWPLCRS485Error _lastError;

    uint32_t _lastActivityMs;
    uint32_t _lastRxActivityMs;
    uint32_t _lastTxActivityMs;

    void markRxActivity();
    void markTxActivity();
    void markActivity();

    void setError(JWPLCRS485Error error);
    void clearError();
};

extern JWPLC_RS485Class JWPLC_RS485;

// Hooks weak para transceptores futuros con DE/RE manual.
// En JWPLC Basic actual, MAX13487EESA+ usa auto-dirección y estos hooks no hacen nada.
extern "C" void jwplcRs485PreTransmitCallback(void);
extern "C" void jwplcRs485PostTransmitCallback(void);
extern "C" void jwplcRs485ActivityCallback(void);

#endif // JWPLC_RS485_H
