#ifndef JW_RS485_H
#define JW_RS485_H

#include <Arduino.h>
#include <HardwareSerial.h>

// =====================================================
// JW_RS485 - puerto RS-485 de proposito general
// =====================================================
//
// Envuelve un HardwareSerial y agrega lo propio de un bus RS-485:
// - control de direccion por pin DE/RE, por AutoDirection del
//   transceptor o por callbacks del usuario;
// - escritura en cola (writeQueued) cuando el hardware tiene
//   AutoDirection, para motores cooperativos como Modbus RTU ASYNC;
// - telemetria de actividad RX/TX y diagnostico de baud efectivo.
//
// Se pueden crear varias instancias, una por UART.
// JWPLC_RS485 del package JWPLC es una instancia preconfigurada de esta clase.

#ifndef JW_RS485_DEFAULT_BAUD
#define JW_RS485_DEFAULT_BAUD 115200UL
#endif

#ifndef JW_RS485_DEFAULT_CONFIG
#define JW_RS485_DEFAULT_CONFIG SERIAL_8N1
#endif

// Buffer sugerido para encolar una ADU Modbus RTU maxima.
#ifndef JW_RS485_DEFAULT_TX_BUFFER_SIZE
#define JW_RS485_DEFAULT_TX_BUFFER_SIZE 512U
#endif

// Mismo orden numerico que JWPLCRS485Error.
enum JWRS485Error : uint8_t
{
    JW_RS485_OK = 0,
    JW_RS485_DISABLED,
    JW_RS485_NOT_STARTED,
    JW_RS485_INVALID_SERIAL,
    JW_RS485_UNKNOWN_ERROR,
    JW_RS485_CLOCK_SOURCE_FAILED
};

enum JWRS485Direction : uint8_t
{
    // El transceptor conmuta solo (p. ej. MAX13487E). TX en cola permitido.
    JW_RS485_DIRECTION_AUTO = 0,
    // El sketch maneja DE/RE con un pin: alto (o bajo) durante TX.
    JW_RS485_DIRECTION_PIN,
    // Solo callbacks pre/post TX (o nada si no se registran).
    JW_RS485_DIRECTION_CALLBACKS
};

typedef void (*JW_RS485_Callback)(void *userData);

class JW_RS485 : public Stream
{
public:
    // rxPin / txPin = -1 conserva los pines por defecto del core.
    explicit JW_RS485(HardwareSerial &serial,
                      int8_t rxPin = -1,
                      int8_t txPin = -1);

    // ===== Configuracion (antes de begin) =====

    // AutoDirection por hardware. txBufferSize > 0 habilita writeQueued().
    void setAutoDirection(size_t txBufferSize = JW_RS485_DEFAULT_TX_BUFFER_SIZE);

    // Pin DE/RE manejado por la libreria. activeHigh = nivel durante TX.
    void setDirectionPin(int8_t dePin, bool activeHigh = true);

    // Sin pin ni AutoDirection: TX bloqueante con callbacks opcionales.
    void setCallbackDirection();

    // Callbacks opcionales. Se llaman tambien en los modos AUTO y PIN.
    void setPreTransmitCallback(JW_RS485_Callback callback, void *userData = nullptr);
    void setPostTransmitCallback(JW_RS485_Callback callback, void *userData = nullptr);
    void setActivityCallback(JW_RS485_Callback callback, void *userData = nullptr);

    // ESP32 clasico: fuerza la fuente de reloj APB de la UART.
    // REF_TICK produce un error medido de +0.6441 % a 230400 baud.
    void setForceApbClock(bool enabled);

    // ===== Ciclo de vida =====

    bool begin();
    bool begin(uint32_t baud);
    bool begin(uint32_t baud, uint32_t config);
    void end();

    bool isEnabled() const;
    bool isReady() const;

    // ===== Estado =====

    uint32_t baudRate() const;
    uint32_t effectiveBaudRate() const;
    uint32_t config() const;
    JWRS485Direction direction() const;
    bool autoDirection() const;
    int8_t directionPin() const;
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

    // ===== Stream =====

    int available() override;
    int peek() override;
    int read() override;

    // Ruta RX por bloques. Lee hasta maxSize bytes actualmente disponibles
    // y agrupa la telemetria de actividad en una sola actualizacion.
    size_t readAvailable(uint8_t *buffer, size_t maxSize);

    // TX bloqueante: termina cuando el ultimo byte salio de la UART.
    size_t write(uint8_t data) override;
    size_t write(const uint8_t *buffer, size_t size) override;

    // Ruta aditiva para hardware AutoDirection. Encola y retorna sin flush.
    // Si el hardware no la soporta, cae de forma segura al write bloqueante.
    size_t writeQueued(uint8_t data);
    size_t writeQueued(const uint8_t *buffer, size_t size);

    using Print::write;

    void flush() override;

    Stream &stream();
    HardwareSerial &serial();

    JWRS485Error lastError() const;
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

    JWRS485Direction _direction;
    int8_t _dePin;
    bool _deActiveHigh;
    size_t _requestedTxBufferSize;
    size_t _txBufferSize;
    bool _forceApbClock;

    JW_RS485_Callback _preTransmit;
    void *_preTransmitData;
    JW_RS485_Callback _postTransmit;
    void *_postTransmitData;
    JW_RS485_Callback _activity;
    void *_activityData;

    bool _ready;
    JWRS485Error _lastError;

    uint32_t _lastActivityMs;
    uint32_t _lastRxActivityMs;
    uint32_t _lastTxActivityMs;

    void beginTransmit();
    void endTransmit();

    void markRxActivity();
    void markTxActivity();
    void markActivity();

    void setError(JWRS485Error error);
    void clearError();
};

#endif // JW_RS485_H
