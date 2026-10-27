#ifndef JW_DWIN_MODBUS_USB_H
#define JW_DWIN_MODBUS_USB_H

#include "JW_DWIN_Transport.h"

// =====================================================
// JW_DWIN_ModbusUSB - transporte DWIN por el USB de la tarjeta
// =====================================================
//
// Modbus RTU dentro de un sobre, por el mismo puerto que el texto de
// depuracion (normalmente Serial). Pensado para el modo Simular -> USB de
// JW DWIN, que emula la pantalla en la PC.
//
// Sobre, igual en los dos sentidos:
//
//   1B 4A 57 LEN <trama RTU completa con su CRC>      (ESC 'J' 'W', LEN = 4..255)
//
// Todo byte fuera de un sobre es texto. El texto que llega de la PC (por
// ejemplo, comandos del Digital Twin) queda en text(), que es un Stream:
// el firmware debe leer sus comandos de ahi y no del puerto directo, porque
// el transporte consume los bytes del puerto al esperar cada respuesta.
//
// Cada sobre sale en una sola llamada a write(): en el core ESP32 eso toma
// el mutex del UART, asi que el texto de depuracion de otras tareas no se
// mete en medio. Un sobre con largo o CRC invalido se descarta.

#ifndef JW_DWIN_USB_DEFAULT_TIMEOUT_MS
#define JW_DWIN_USB_DEFAULT_TIMEOUT_MS 150
#endif

// Bytes de texto recibido que se guardan hasta que el firmware los lee.
#ifndef JW_DWIN_USB_TEXT_BUFFER
#define JW_DWIN_USB_TEXT_BUFFER 512
#endif

// Un sobre a medias sin bytes nuevos en este tiempo se abandona: su
// encabezado vuelve a ser texto, o el sobre se descarta si ya traia largo.
#ifndef JW_DWIN_USB_STALE_MS
#define JW_DWIN_USB_STALE_MS 100
#endif

enum JW_DWIN_USBError : uint8_t
{
    JW_DWIN_USB_OK = 0,
    JW_DWIN_USB_TIMEOUT,
    JW_DWIN_USB_CRC_ERROR,
    JW_DWIN_USB_INVALID_RESPONSE,
    JW_DWIN_USB_EXCEPTION,
    JW_DWIN_USB_INVALID_ARGUMENT,
    JW_DWIN_USB_NOT_STARTED
};

class JW_DWIN_ModbusUSB : public JW_DWIN_Transport
{
public:
    // El puerto ya debe estar iniciado (Serial.begin) por el sketch.
    explicit JW_DWIN_ModbusUSB(Stream &port, uint8_t slaveId = 1);

    void setTimeout(uint32_t timeoutMs) { _timeoutMs = timeoutMs; }
    uint32_t timeout() const { return _timeoutMs; }
    uint8_t slaveId() const { return _slaveId; }

    bool begin() override;

    bool readRegisters(uint16_t vp, uint16_t count, uint16_t *destination) override;
    bool writeRegister(uint16_t vp, uint16_t value) override;
    bool writeRegisters(uint16_t vp, uint16_t count, const uint16_t *source) override;

    const char *lastErrorString() const override;
    JW_DWIN_USBError lastError() const { return _lastError; }
    uint8_t lastException() const { return _lastException; }

    // Texto recibido fuera de los sobres. Leerlo seguido: available() y read()
    // tambien procesan lo que haya llegado al puerto. Lo que se escribe en
    // text() sale tal cual por el puerto.
    Stream &text() { return _text; }

    // Procesa lo recibido sin esperar (lo llama text().available()).
    void poll();

    // Contadores desde el arranque.
    uint32_t badEnvelopes() const { return _badEnvelopes; }  // largo o CRC invalido
    uint32_t strayFrames() const { return _strayFrames; }    // tramas sin pedido pendiente
    uint32_t textOverflows() const { return _textOverflows; } // bytes de texto perdidos

private:
    class TextStream : public Stream
    {
    public:
        explicit TextStream(JW_DWIN_ModbusUSB &owner) : _owner(owner) {}
        int available() override;
        int read() override;
        int peek() override;
        size_t write(uint8_t c) override;
        size_t write(const uint8_t *buffer, size_t size) override;
        void flush() override;

    private:
        JW_DWIN_ModbusUSB &_owner;
    };

    enum RxState : uint8_t
    {
        RX_TEXT,
        RX_MAGIC1, // llego 1B
        RX_MAGIC2, // llego 1B 4A
        RX_LEN,    // llego 1B 4A 57
        RX_BODY
    };

    Stream &_port;
    TextStream _text;
    uint8_t _slaveId;
    uint32_t _timeoutMs;
    bool _started;

    JW_DWIN_USBError _lastError;
    uint8_t _lastException;

    RxState _rxState;
    uint8_t _rxLen;
    uint8_t _rxCount;
    uint8_t _rxFrame[255];
    uint32_t _rxLastMs;

    bool _waiting;   // hay un pedido esperando respuesta
    bool _gotFrame;  // la respuesta quedo en _rxFrame (_rxLen bytes)
    bool _sawBadCrc; // llego un sobre con CRC malo mientras se esperaba

    uint8_t _tx[4 + 255]; // sobre del pedido; la trama empieza en _tx[4]

    uint8_t _textBuf[JW_DWIN_USB_TEXT_BUFFER];
    uint16_t _textHead;
    uint16_t _textCount;

    uint32_t _badEnvelopes;
    uint32_t _strayFrames;
    uint32_t _textOverflows;

    bool transact(uint8_t length);
    bool expectFunction(uint8_t function);
    void feed(uint8_t b);
    void checkStale();
    void frameDone();
    void pushText(uint8_t b);
    int popText();
    int peekText() const;
    bool fail(JW_DWIN_USBError error);

    static uint16_t crc16(const uint8_t *data, uint16_t length);
};

#endif // JW_DWIN_MODBUS_USB_H
