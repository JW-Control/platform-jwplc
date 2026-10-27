// N1 de JW_DWIN_ModbusUSB en host: sobres, texto mezclado y errores.
#include "JW_DWIN.h"
#include "JW_DWIN_ModbusUSB.h"
#include <deque>
#include <map>
#include <string>
#include <vector>

uint32_t g_mockMs = 1000;
Stream Serial;

typedef std::vector<uint8_t> Bytes;

static uint16_t crc(const Bytes &d)
{
    uint16_t c = 0xFFFF;
    for (uint8_t b : d)
    {
        c ^= b;
        for (int k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xA001 : c >> 1;
    }
    return c;
}

static Bytes withCrc(Bytes d)
{
    uint16_t c = crc(d);
    d.push_back(c & 0xFF);
    d.push_back(c >> 8);
    return d;
}

static Bytes envelope(const Bytes &frame)
{
    Bytes e = {0x1B, 0x4A, 0x57, (uint8_t)frame.size()};
    e.insert(e.end(), frame.begin(), frame.end());
    return e;
}

static Bytes text(const std::string &s) { return Bytes(s.begin(), s.end()); }

static Bytes cat(Bytes a, const Bytes &b)
{
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

// Puerto de la tarjeta. Los bytes de la PC llegan en un tiempo dado del reloj
// simulado; el esclavo responde a cada sobre que escribe la tarjeta.
class FakePort : public Stream
{
public:
    struct Rx { uint32_t at; uint8_t b; };
    std::deque<Rx> rx;
    std::vector<Bytes> writes; // una entrada por llamada a write()
    std::map<uint16_t, uint16_t> vp;
    bool respond = true;
    uint8_t id = 1;
    uint32_t delayMs = 2;
    uint32_t gapMs = 1;   // entre bytes de la respuesta
    Bytes before, after;  // texto alrededor de la respuesta
    bool corruptCrc = false;
    int exception = 0;

    void push(const Bytes &b, uint32_t at = 0)
    {
        for (uint8_t x : b) rx.push_back({at ? at : g_mockMs, x});
    }

    int available() override
    {
        int n = 0;
        for (const Rx &r : rx) if (r.at <= g_mockMs) n++; else break;
        return n;
    }
    int read() override
    {
        if (!available()) return -1;
        uint8_t b = rx.front().b;
        rx.pop_front();
        return b;
    }
    int peek() override { return available() ? rx.front().b : -1; }
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t *b, size_t n) override
    {
        Bytes w(b, b + n);
        writes.push_back(w);
        if (respond && n > 4 && b[0] == 0x1B) answer(Bytes(w.begin() + 4, w.end()));
        return n;
    }

    void answer(const Bytes &f)
    {
        uint8_t fn = f[1];
        uint16_t a = (f[2] << 8) | f[3];
        Bytes r;
        if (exception)
            r = {id, (uint8_t)(fn | 0x80), (uint8_t)exception};
        else if (fn == 3)
        {
            uint16_t n = (f[4] << 8) | f[5];
            r = {id, 3, (uint8_t)(n * 2)};
            for (uint16_t i = 0; i < n; i++) { r.push_back(vp[a + i] >> 8); r.push_back(vp[a + i] & 0xFF); }
        }
        else if (fn == 6)
        {
            vp[a] = (f[4] << 8) | f[5];
            r = Bytes(f.begin(), f.begin() + 6);
            r[0] = id;
        }
        else if (fn == 16)
        {
            uint16_t n = (f[4] << 8) | f[5];
            for (uint16_t i = 0; i < n; i++) vp[a + i] = (f[7 + i * 2] << 8) | f[8 + i * 2];
            r = Bytes(f.begin(), f.begin() + 6);
            r[0] = id;
        }
        Bytes e = envelope(withCrc(r));
        if (corruptCrc) e.back() ^= 0xFF;
        // Por defecto un byte por milisegundo: la respuesta llega partida.
        Bytes all = cat(cat(before, e), after);
        for (size_t i = 0; i < all.size(); i++) rx.push_back({g_mockMs + delayMs + (uint32_t)i * gapMs, all[i]});
    }
};

static std::string drain(Stream &s)
{
    std::string out;
    while (s.available()) out += (char)s.read();
    return out;
}

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main()
{
    // Sobre exacto del pedido, en una sola escritura.
    {
        FakePort p;
        JW_DWIN_ModbusUSB t(p);
        uint16_t v[10];
        CHECK(!t.readRegisters(0, 10, v));
        CHECK(std::string(t.lastErrorString()) == "Not started");
        CHECK(p.writes.empty());

        CHECK(t.begin());
        CHECK(t.readRegisters(0, 10, v));
        CHECK(p.writes.size() == 1);
        // 01 03 00 00 00 0A C5 CD: ejemplo clasico de Modbus.
        CHECK(p.writes[0] == Bytes({0x1B, 0x4A, 0x57, 0x08, 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD}));
    }

    // Escritura y lectura con texto antes y despues de cada respuesta.
    {
        FakePort p;
        p.before = text("hola\n");
        p.after = text("chau\n");
        JW_DWIN_ModbusUSB t(p);
        t.begin();
        uint16_t src[3] = {0x1234, 0xABCD, 7};
        CHECK(t.writeRegisters(0x1000, 3, src));
        CHECK(t.writeRegister(0x0084, 0x5A01));
        uint16_t dst[3] = {0, 0, 0};
        CHECK(t.readRegisters(0x1000, 3, dst));
        CHECK(dst[0] == 0x1234 && dst[1] == 0xABCD && dst[2] == 7);
        CHECK(p.vp[0x0084] == 0x5A01);
        g_mockMs += 50;
        CHECK(drain(t.text()) == "hola\nchau\nhola\nchau\nhola\nchau\n");
        CHECK(t.badEnvelopes() == 0 && t.strayFrames() == 0 && t.textOverflows() == 0);
    }

    // ESC suelto en el texto, y ESC justo antes de un sobre.
    {
        FakePort p;
        p.before = text("a\x1B" "b\x1BJ" "c\x1B");
        JW_DWIN_ModbusUSB t(p);
        t.begin();
        CHECK(t.writeRegister(1, 2));
        CHECK(drain(t.text()) == "a\x1B" "b\x1BJ" "c\x1B");
    }

    // CRC malo: se descarta, el pedido falla con "CRC error" y el texto sigue.
    {
        FakePort p;
        p.corruptCrc = true;
        p.after = text("ok\n");
        JW_DWIN_ModbusUSB t(p);
        t.begin();
        uint32_t t0 = millis();
        CHECK(!t.writeRegister(1, 2));
        CHECK(std::string(t.lastErrorString()) == "CRC error");
        CHECK(t.badEnvelopes() == 1);
        CHECK(millis() - t0 < 50); // no espera el timeout
        g_mockMs += 50;
        CHECK(drain(t.text()) == "ok\n");
    }

    // Largo invalido: se descartan los 4 bytes del encabezado.
    {
        FakePort p;
        p.respond = false;
        JW_DWIN_ModbusUSB t(p);
        t.begin();
        p.push(cat(Bytes({0x1B, 0x4A, 0x57, 0x02}), text("x\n")));
        CHECK(drain(t.text()) == "x\n");
        CHECK(t.badEnvelopes() == 1);
    }

    // Sin respuesta: timeout de 150 ms; la respuesta tardia se descarta.
    {
        FakePort p;
        p.delayMs = 400;
        JW_DWIN_ModbusUSB t(p);
        t.begin();
        uint32_t t0 = millis();
        CHECK(!t.writeRegister(1, 2));
        CHECK(std::string(t.lastErrorString()) == "Timeout");
        CHECK(millis() - t0 >= 150 && millis() - t0 < 160);
        g_mockMs += 400;
        p.delayMs = 2;
        uint16_t v = 0;
        CHECK(t.readRegisters(1, 1, &v));
        CHECK(v == 2);
        CHECK(t.strayFrames() == 1);
    }

    // Respuesta de otro id: no cuenta.
    {
        FakePort p;
        p.id = 2;
        JW_DWIN_ModbusUSB t(p);
        t.begin();
        CHECK(!t.writeRegister(1, 2));
        CHECK(std::string(t.lastErrorString()) == "Timeout");
        CHECK(t.strayFrames() == 1);
    }

    // Excepcion Modbus.
    {
        FakePort p;
        p.exception = 2;
        JW_DWIN_ModbusUSB t(p);
        t.begin();
        uint16_t v;
        CHECK(!t.readRegisters(0xFFFF, 2, &v));
        CHECK(std::string(t.lastErrorString()) == "Modbus exception");
        CHECK(t.lastException() == 2);
    }

    // Argumentos fuera de rango.
    {
        FakePort p;
        JW_DWIN_ModbusUSB t(p);
        t.begin();
        uint16_t v[126] = {};
        CHECK(!t.readRegisters(0, 0, v));
        CHECK(!t.readRegisters(0, 126, v));
        CHECK(!t.writeRegisters(0, 124, v));
        CHECK(std::string(t.lastErrorString()) == "Invalid argument");
        CHECK(p.writes.empty());
        p.gapMs = 0;
        CHECK(t.readRegisters(0, 125, v));
        CHECK(t.writeRegisters(0, 123, v));
        CHECK(p.writes[1].size() == 4 + 255);
    }

    // Encabezado a medias sin mas bytes: vuelve a ser texto.
    {
        FakePort p;
        JW_DWIN_ModbusUSB t(p);
        t.begin();
        p.push(text("\x1BJ"));
        CHECK(t.text().available() == 0);
        g_mockMs += JW_DWIN_USB_STALE_MS;
        CHECK(drain(t.text()) == "\x1BJ");
    }

    // Texto que no se lee: se guarda hasta JW_DWIN_USB_TEXT_BUFFER.
    {
        FakePort p;
        JW_DWIN_ModbusUSB t(p);
        t.begin();
        p.push(Bytes(JW_DWIN_USB_TEXT_BUFFER + 10, 'z'));
        CHECK(t.writeRegister(1, 2));
        CHECK(t.textOverflows() == 10);
        CHECK(drain(t.text()).size() == JW_DWIN_USB_TEXT_BUFFER);
    }

    // Lo escrito en text() sale tal cual por el puerto.
    {
        FakePort p;
        JW_DWIN_ModbusUSB t(p);
        t.text().write((const uint8_t *)"hi\n", 3);
        CHECK(p.writes.size() == 1 && p.writes[0] == text("hi\n"));
    }

    // JW_DWIN completo sobre el transporte.
    {
        FakePort p;
        JW_DWIN_ModbusUSB t(p);
        JW_DWIN dwin(t);
        CHECK(dwin.begin());
        CHECK(dwin.setPage(0x10));
        DWIN_Value v = {0x10, 0x00, 0, 0, 0};
        dwin.sendValue(v, 42);
        CHECK(p.vp[0x1000] == 42);
        CHECK(dwin.isConfirmed(v));
        p.respond = false;
        for (int i = 0; i < 3; i++) dwin.sendValue(v, 43 + i);
        CHECK(!dwin.isOnline());
    }

    if (failures)
    {
        printf("JW_DWIN_ModbusUSB N1: %d fallos\n", failures);
        return 1;
    }
    printf("JW_DWIN_ModbusUSB N1 PASS\n");
    return 0;
}
