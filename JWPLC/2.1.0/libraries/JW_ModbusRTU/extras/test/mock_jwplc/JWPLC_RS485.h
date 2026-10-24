// Transporte minimo con la API de JWPLC_RS485 (AutoDirection, TX en cola)
// para compilar en host el JWPLC_ModbusRTU.cpp original del package.
#ifndef JWPLC_RS485_H
#define JWPLC_RS485_H
#include "Arduino.h"

extern HardwareSerial g_uart;

class JWPLC_RS485Class
{
public:
    bool ready = false;
    bool begin(uint32_t baud, uint32_t config)
    {
        g_uart.setTxBufferSize(512);
        g_uart.begin(baud, config, 16, 17);
        ready = true;
        return true;
    }
    void end() { g_uart.end(); ready = false; }
    bool isReady() const { return ready; }
    uint32_t effectiveBaudRate() const { return ready ? g_uart.baudRate() : 0; }
    bool queuedWriteSupported() const { return ready; }
    size_t txBufferSize() const { return 512; }
    int available() { return ready ? g_uart.available() : 0; }
    int read() { return ready ? g_uart.read() : -1; }
    size_t readAvailable(uint8_t *b, size_t n) { return ready ? g_uart.read(b, n) : 0; }
    size_t write(const uint8_t *b, size_t n) { size_t w = g_uart.write(b, n); g_uart.flush(); return w; }
    size_t writeQueued(const uint8_t *b, size_t n) { return g_uart.write(b, n); }
};
extern JWPLC_RS485Class JWPLC_RS485;
#endif
