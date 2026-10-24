// Mock minimo de Arduino para pruebas N1 en host (g++ -std=c++11).
// El tiempo solo avanza con mockAdvanceUs() o yield(): ambos binarios de la
// prueba de equivalencia ven exactamente la misma linea de tiempo.
#ifndef JW_TEST_MOCK_ARDUINO_H
#define JW_TEST_MOCK_ARDUINO_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <deque>
#include <vector>

#define SERIAL_8N1 0x800001cUL
#define SERIAL_8E1 0x800001eUL
#define SERIAL_8O1 0x800001fUL
#define SERIAL_8N2 0x800003cUL
#define SERIAL_8E2 0x800003eUL
#define SERIAL_8O2 0x800003fUL

#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))

#define OUTPUT 0x03
#define HIGH 1
#define LOW 0

extern uint64_t g_mockUs;
inline void mockAdvanceUs(uint32_t us) { g_mockUs += us; }
inline uint32_t micros() { return (uint32_t)g_mockUs; }
inline uint32_t millis() { return (uint32_t)(g_mockUs / 1000ULL); }
void yield();

struct MockPinEvent { int pin; int value; };
extern std::vector<MockPinEvent> g_pinEvents;
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t pin, uint8_t value) { g_pinEvents.push_back({pin, value}); }

class Print
{
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size)
    {
        size_t n = 0;
        while (size--) n += write(*buffer++);
        return n;
    }
    size_t print(const char *s) { return s ? write((const uint8_t *)s, strlen(s)) : 0; }
    size_t print(unsigned long v) { char b[24]; snprintf(b, sizeof(b), "%lu", v); return print(b); }
    size_t print(long v) { char b[24]; snprintf(b, sizeof(b), "%ld", v); return print(b); }
    size_t print(unsigned int v) { return print((unsigned long)v); }
    size_t print(int v) { return print((long)v); }
    size_t println() { return print("\n"); }
    template <typename T> size_t println(T v) { size_t n = print(v); return n + println(); }
};

class Stream : public Print
{
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() {}
};

// UART simulada: TX capturado, RX alimentado por la prueba.
class HardwareSerial : public Stream
{
public:
    std::vector<uint8_t> tx;
    std::deque<uint8_t> rx;
    uint32_t baud = 0;
    uint32_t cfg = 0;
    int8_t rxPin = -1;
    int8_t txPin = -1;
    size_t txBufferSize = 0;
    int flushCount = 0;
    bool begun = false;

    void begin(unsigned long b, uint32_t c, int8_t r, int8_t t)
    {
        baud = (uint32_t)b; cfg = c; rxPin = r; txPin = t; begun = true;
    }
    void end() { begun = false; }
    // Como en ESP32: true si el driver UART quedó instalado.
    explicit operator bool() const { return begun; }
    size_t setTxBufferSize(size_t n) { txBufferSize = n; return n; }
    uint32_t baudRate() { return baud; }

    int available() override { return (int)rx.size(); }
    int peek() override { return rx.empty() ? -1 : rx.front(); }
    int read() override
    {
        if (rx.empty()) return -1;
        int v = rx.front(); rx.pop_front(); return v;
    }
    size_t read(uint8_t *buffer, size_t size)
    {
        size_t n = 0;
        while (n < size && !rx.empty()) { buffer[n++] = rx.front(); rx.pop_front(); }
        return n;
    }
    size_t write(uint8_t v) override { tx.push_back(v); return 1; }
    size_t write(const uint8_t *buffer, size_t size) override
    {
        tx.insert(tx.end(), buffer, buffer + size); return size;
    }
    void flush() override { flushCount++; }
};

#endif
