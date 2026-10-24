// JWPLC_mBlock_RS485.h
//
// Texto por el puerto RS485 del JWPLC Basic para los bloques de mBlock.
// Aparte de JWPLC_mBlock.h: sólo lo incluyen los bloques RS485.
//
//   JWPLC_RS485.begin(115200, SERIAL_8N1);    // API del package
//   JWPLC_Line485.send("hola");               // envía "hola\n"
//   if (JWPLC_Line485.available()) ...        // llegó una línea completa
//   JWPLC_Line485.line();                     // última línea recibida
//
// El transceptor del JWPLC (MAX13487E) gestiona solo la dirección DE/RE.
// No usar a la vez que Modbus RTU: comparten el mismo puerto.

#ifndef JWPLC_MBLOCK_RS485_H
#define JWPLC_MBLOCK_RS485_H

#include "JWPLC_mBlock.h"
#include <JWPLC_RS485.h>

class JWPLC_Line485Helper
{
public:
    static const uint16_t MAX_LINE = 128;

    template <typename T>
    void send(T value)
    {
        JWPLC_RS485.println(jwplc_mblock::text(value));
    }

    // true una vez por cada línea completa recibida (terminada en \n).
    bool available()
    {
        poll();
        if (!_ready)
            return false;
        _ready = false;
        return true;
    }

    String line() const { return _line; }

private:
    String _buffer;
    String _line;
    bool _ready = false;

    void poll()
    {
        while (JWPLC_RS485.available() > 0)
        {
            const int c = JWPLC_RS485.read();
            if (c < 0)
                break;
            if (c == '\n')
            {
                _buffer.trim(); // quita el \r de "\r\n"
                _line = _buffer;
                _buffer = "";
                _ready = true;
            }
            else if (_buffer.length() < MAX_LINE)
            {
                _buffer += (char)c;
            }
        }
    }
};

inline JWPLC_Line485Helper JWPLC_Line485;

#endif // JWPLC_MBLOCK_RS485_H
