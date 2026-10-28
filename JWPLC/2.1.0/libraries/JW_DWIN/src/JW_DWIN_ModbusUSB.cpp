#include "JW_DWIN_ModbusUSB.h"
#include <string.h>

// Mayor cantidad de registros que caben en una trama de 255 bytes.
static const uint16_t MAX_READ_REGISTERS = 125;  // 5 + 2n
static const uint16_t MAX_WRITE_REGISTERS = 123; // 9 + 2n

JW_DWIN_ModbusUSB::JW_DWIN_ModbusUSB(Stream &port, uint8_t slaveId)
    : _port(port),
      _text(*this),
      _slaveId(slaveId),
      _timeoutMs(JW_DWIN_USB_DEFAULT_TIMEOUT_MS),
      _started(false),
      _lastError(JW_DWIN_USB_OK),
      _lastException(0),
      _rxState(RX_TEXT),
      _rxLen(0),
      _rxCount(0),
      _rxLastMs(0),
      _waiting(false),
      _gotFrame(false),
      _sawBadCrc(false),
      _textHead(0),
      _textCount(0),
      _badEnvelopes(0),
      _strayFrames(0),
      _textOverflows(0)
{
}

bool JW_DWIN_ModbusUSB::begin()
{
    _rxState = RX_TEXT;
    _waiting = false;
    _gotFrame = false;
    _lastError = JW_DWIN_USB_OK;
    _started = true;
    return true;
}

// =======================
// PEDIDOS
// =======================

bool JW_DWIN_ModbusUSB::readRegisters(uint16_t vp, uint16_t count, uint16_t *destination)
{
    if (count == 0 || count > MAX_READ_REGISTERS || !destination)
        return fail(JW_DWIN_USB_INVALID_ARGUMENT);

    uint8_t *f = _tx + 4;
    f[0] = _slaveId;
    f[1] = 0x03;
    f[2] = vp >> 8;
    f[3] = vp & 0xFF;
    f[4] = count >> 8;
    f[5] = count & 0xFF;

    if (!transact(6) || !expectFunction(0x03))
        return false;

    if (_rxLen != 5 + count * 2 || _rxFrame[2] != count * 2)
        return fail(JW_DWIN_USB_INVALID_RESPONSE);

    for (uint16_t i = 0; i < count; i++)
        destination[i] = (_rxFrame[3 + i * 2] << 8) | _rxFrame[4 + i * 2];

    return true;
}

bool JW_DWIN_ModbusUSB::writeRegister(uint16_t vp, uint16_t value)
{
    uint8_t *f = _tx + 4;
    f[0] = _slaveId;
    f[1] = 0x06;
    f[2] = vp >> 8;
    f[3] = vp & 0xFF;
    f[4] = value >> 8;
    f[5] = value & 0xFF;

    if (!transact(6) || !expectFunction(0x06))
        return false;

    // La respuesta repite el pedido.
    if (_rxLen != 8 || memcmp(_rxFrame, f, 6) != 0)
        return fail(JW_DWIN_USB_INVALID_RESPONSE);

    return true;
}

bool JW_DWIN_ModbusUSB::writeRegisters(uint16_t vp, uint16_t count, const uint16_t *source)
{
    if (count == 0 || count > MAX_WRITE_REGISTERS || !source)
        return fail(JW_DWIN_USB_INVALID_ARGUMENT);

    uint8_t *f = _tx + 4;
    f[0] = _slaveId;
    f[1] = 0x10;
    f[2] = vp >> 8;
    f[3] = vp & 0xFF;
    f[4] = count >> 8;
    f[5] = count & 0xFF;
    f[6] = count * 2;
    for (uint16_t i = 0; i < count; i++)
    {
        f[7 + i * 2] = source[i] >> 8;
        f[8 + i * 2] = source[i] & 0xFF;
    }

    if (!transact(7 + count * 2) || !expectFunction(0x10))
        return false;

    // La respuesta repite direccion y cantidad.
    if (_rxLen != 8 || memcmp(_rxFrame, f, 6) != 0)
        return fail(JW_DWIN_USB_INVALID_RESPONSE);

    return true;
}

// Envia la trama de _tx[4] (length bytes, sin CRC) y espera la respuesta,
// que queda en _rxFrame.
bool JW_DWIN_ModbusUSB::transact(uint8_t length)
{
    if (!_started)
        return fail(JW_DWIN_USB_NOT_STARTED);

    // Texto pendiente y respuestas viejas, antes de enviar.
    poll();

    uint8_t *f = _tx + 4;
    uint16_t crc = crc16(f, length);
    f[length] = crc & 0xFF;
    f[length + 1] = crc >> 8;

    _tx[0] = 0x1B;
    _tx[1] = 0x4A;
    _tx[2] = 0x57;
    _tx[3] = length + 2;

    _waiting = true;
    _gotFrame = false;
    _sawBadCrc = false;

    // Una sola llamada: el sobre sale entero.
    _port.write(_tx, 4 + length + 2);

    // Un sobre con CRC malo era la respuesta: no se espera otra.
    uint32_t t0 = millis();
    while (!_gotFrame && !_sawBadCrc)
    {
        if (_port.available() > 0)
        {
            int b = _port.read();
            if (b >= 0)
                feed((uint8_t)b);
            continue;
        }
        if (millis() - t0 >= _timeoutMs)
            break;
        delay(1);
    }
    _waiting = false;

    if (!_gotFrame)
        return fail(_sawBadCrc ? JW_DWIN_USB_CRC_ERROR : JW_DWIN_USB_TIMEOUT);

    _lastError = JW_DWIN_USB_OK;
    return true;
}

bool JW_DWIN_ModbusUSB::expectFunction(uint8_t function)
{
    if (_rxFrame[1] == (function | 0x80))
    {
        _lastException = _rxLen == 5 ? _rxFrame[2] : 0;
        return fail(JW_DWIN_USB_EXCEPTION);
    }
    if (_rxFrame[1] != function)
        return fail(JW_DWIN_USB_INVALID_RESPONSE);
    return true;
}

bool JW_DWIN_ModbusUSB::fail(JW_DWIN_USBError error)
{
    _lastError = error;
    return false;
}

const char *JW_DWIN_ModbusUSB::lastErrorString() const
{
    switch (_lastError)
    {
    case JW_DWIN_USB_OK:
        return "OK";
    case JW_DWIN_USB_TIMEOUT:
        return "Timeout";
    case JW_DWIN_USB_CRC_ERROR:
        return "CRC error";
    case JW_DWIN_USB_INVALID_RESPONSE:
        return "Invalid response";
    case JW_DWIN_USB_EXCEPTION:
        return "Modbus exception";
    case JW_DWIN_USB_INVALID_ARGUMENT:
        return "Invalid argument";
    case JW_DWIN_USB_NOT_STARTED:
        return "Not started";
    default:
        return "Unknown error";
    }
}

// =======================
// RECEPCION
// =======================

void JW_DWIN_ModbusUSB::poll()
{
    while (_port.available() > 0)
    {
        int b = _port.read();
        if (b < 0)
            break;
        feed((uint8_t)b);
    }
    checkStale();
}

void JW_DWIN_ModbusUSB::feed(uint8_t b)
{
    _rxLastMs = millis();

    switch (_rxState)
    {
    case RX_TEXT:
        if (b == 0x1B)
            _rxState = RX_MAGIC1;
        else
            pushText(b);
        return;

    case RX_MAGIC1:
        if (b == 0x4A)
        {
            _rxState = RX_MAGIC2;
            return;
        }
        pushText(0x1B);
        break;

    case RX_MAGIC2:
        if (b == 0x57)
        {
            _rxState = RX_LEN;
            return;
        }
        pushText(0x1B);
        pushText(0x4A);
        break;

    case RX_LEN:
        if (b < 4)
        {
            // Largo invalido: se descartan los 4 bytes del encabezado.
            _badEnvelopes++;
            _rxState = RX_TEXT;
            return;
        }
        _rxLen = b;
        _rxCount = 0;
        _rxState = RX_BODY;
        return;

    case RX_BODY:
        _rxFrame[_rxCount++] = b;
        if (_rxCount == _rxLen)
        {
            _rxState = RX_TEXT;
            frameDone();
        }
        return;
    }

    // El byte no siguio el encabezado: era texto y se vuelve a leer como tal
    // (puede ser otro 1B).
    _rxState = RX_TEXT;
    feed(b);
}

void JW_DWIN_ModbusUSB::checkStale()
{
    if (_rxState == RX_TEXT || millis() - _rxLastMs < JW_DWIN_USB_STALE_MS)
        return;

    if (_rxState == RX_MAGIC1)
    {
        pushText(0x1B);
    }
    else if (_rxState == RX_MAGIC2)
    {
        pushText(0x1B);
        pushText(0x4A);
    }
    else
    {
        _badEnvelopes++;
    }
    _rxState = RX_TEXT;
}

void JW_DWIN_ModbusUSB::frameDone()
{
    uint16_t crc = crc16(_rxFrame, _rxLen - 2);
    if (_rxFrame[_rxLen - 2] != (crc & 0xFF) || _rxFrame[_rxLen - 1] != (crc >> 8))
    {
        _badEnvelopes++;
        if (_waiting)
            _sawBadCrc = true;
        return;
    }

    if (_waiting && _rxFrame[0] == _slaveId)
    {
        _gotFrame = true;
        return;
    }

    // Respuesta que llego tarde, o de otro id.
    _strayFrames++;
}

// =======================
// TEXTO
// =======================

void JW_DWIN_ModbusUSB::pushText(uint8_t b)
{
    if (_textCount >= JW_DWIN_USB_TEXT_BUFFER)
    {
        _textOverflows++;
        return;
    }
    _textBuf[(_textHead + _textCount) % JW_DWIN_USB_TEXT_BUFFER] = b;
    _textCount++;
}

int JW_DWIN_ModbusUSB::popText()
{
    if (_textCount == 0)
        return -1;
    uint8_t b = _textBuf[_textHead];
    _textHead = (_textHead + 1) % JW_DWIN_USB_TEXT_BUFFER;
    _textCount--;
    return b;
}

int JW_DWIN_ModbusUSB::peekText() const
{
    return _textCount ? _textBuf[_textHead] : -1;
}

int JW_DWIN_ModbusUSB::TextStream::available()
{
    _owner.poll();
    return _owner._textCount;
}

int JW_DWIN_ModbusUSB::TextStream::read()
{
    if (_owner._textCount == 0)
        _owner.poll();
    return _owner.popText();
}

int JW_DWIN_ModbusUSB::TextStream::peek()
{
    if (_owner._textCount == 0)
        _owner.poll();
    return _owner.peekText();
}

size_t JW_DWIN_ModbusUSB::TextStream::write(uint8_t c)
{
    return _owner._port.write(c);
}

size_t JW_DWIN_ModbusUSB::TextStream::write(const uint8_t *buffer, size_t size)
{
    return _owner._port.write(buffer, size);
}

void JW_DWIN_ModbusUSB::TextStream::flush()
{
    _owner._port.flush();
}

// CRC-16 Modbus (polinomio 0xA001 reflejado, inicio 0xFFFF).
uint16_t JW_DWIN_ModbusUSB::crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (uint8_t k = 0; k < 8; k++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}
