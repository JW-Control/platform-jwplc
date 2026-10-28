// N1 de JW_DWIN en host con un transporte simulado.
#include "JW_DWIN.h"
#include <map>
#include <vector>

uint32_t g_mockMs = 1000;
Stream Serial;

struct Op { char type; uint16_t vp; std::vector<uint16_t> data; };

class MockTransport : public JW_DWIN_Transport
{
public:
    std::map<uint16_t, uint16_t> vp;
    std::vector<Op> ops;
    bool online = true;
    bool beginOk = true;

    bool begin() override { return beginOk; }
    bool readRegisters(uint16_t a, uint16_t n, uint16_t *d) override
    {
        ops.push_back({'R', a, {n}});
        if (!online) return false;
        for (uint16_t i = 0; i < n; i++) d[i] = vp[a + i];
        return true;
    }
    bool writeRegister(uint16_t a, uint16_t v) override
    {
        ops.push_back({'W', a, {v}});
        if (!online) return false;
        vp[a] = v;
        return true;
    }
    bool writeRegisters(uint16_t a, uint16_t n, const uint16_t *s) override
    {
        ops.push_back({'M', a, std::vector<uint16_t>(s, s + n)});
        if (!online) return false;
        for (uint16_t i = 0; i < n; i++) vp[a + i] = s[i];
        return true;
    }
    const char *lastErrorString() const override { return online ? "OK" : "Timeout"; }
};

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main()
{
    // Antes de begin(): no se usa el transporte.
    {
        MockTransport t;
        JW_DWIN dwin(t);
        DWIN_Value v = {0x10, 0x00, 0, 0, 0};
        dwin.sendValue(v, 5);
        CHECK(t.ops.empty());
        t.beginOk = false;
        CHECK(!dwin.begin());
    }

    MockTransport t;
    JW_DWIN dwin(t);
    CHECK(dwin.begin());

    // sendValue: primera llamada escribe aunque el valor sea 0; luego cache.
    DWIN_Value prog = {0x10, 0x00, 0, 0, 0};
    dwin.sendValue(prog, 0);
    CHECK(t.ops.size() == 1 && t.ops[0].type == 'W' && t.ops[0].vp == 0x1000);
    dwin.sendValue(prog, 0);
    CHECK(t.ops.size() == 1);
    dwin.sendValue(prog, 3);
    CHECK(t.ops.size() == 2 && t.vp[0x1000] == 3);
    dwin.sendValue(prog, 3, true);
    CHECK(t.ops.size() == 3);

    // Escritura fallida: val refleja lo pedido (lógica de la aplicación) y
    // se reintenta en la siguiente llamada con el mismo valor.
    t.online = false;
    dwin.sendValue(prog, 7);
    CHECK(prog.val == 7 && t.vp[0x1000] == 3);
    t.online = true;
    t.ops.clear();
    dwin.sendValue(prog, 7);
    CHECK(t.ops.size() == 1 && t.vp[0x1000] == 7);

    // readValue
    DWIN_Value temp = {0x10, 0x01, 22, 0, 1};
    t.vp[0x1001] = 555;
    CHECK(dwin.readValue(temp) && temp.val == 555 && temp.fval > 55.4f && temp.fval < 55.6f);
    CHECK(!dwin.readValue(temp));
    t.online = false;
    t.vp[0x1001] = 1;
    CHECK(!dwin.readValue(temp) && temp.val == 555);
    t.online = true;

    // sendText: relleno con espacios, empaquetado big-endian y recorte.
    DWIN_String name = {0x10, 0xA0, "", 5};
    t.ops.clear();
    CHECK(dwin.sendText(name, "AB"));
    CHECK(t.ops.size() == 1 && t.ops[0].type == 'M' && t.ops[0].vp == 0x10A0);
    CHECK(t.ops[0].data.size() == 3 && t.ops[0].data[0] == 0x4142 && t.ops[0].data[1] == 0x2020 && t.ops[0].data[2] == 0x2000);
    CHECK(dwin.sendText(name, "AB") && t.ops.size() == 1);
    CHECK(dwin.sendText(name, "ABCDEFG") && t.ops.back().data[2] == 0x4500);
    DWIN_String tooLong = {0x10, 0xB0, "", JW_DWIN_MAX_TEXT + 1};
    CHECK(!dwin.sendText(tooLong, "x"));

    // checkKey: VP distinto de 0 activa Enable y se limpia en la DWIN.
    DWIN_Key start = {0x10, 0x31, 4, 5};
    t.vp[0x1031] = 0;
    dwin.checkKey(start);
    CHECK(!start.Enable);
    t.vp[0x1031] = 1;
    dwin.checkKey(start);
    CHECK(start.Enable && t.vp[0x1031] == 0);
    start.Enable = 0;

    // checkKeys: una lectura por tramo y escritura de 0 solo en las pulsadas.
    {
        DWIN_Key jump = {0x10, 0x23, 1, 5};
        DWIN_Key stop = {0x10, 0x32, 5, 5};
        DWIN_Key far = {0x10, 0x60, 6, 5};    // hueco > JW_DWIN_READ_MAX_GAP: otro tramo
        DWIN_Key other = {0x12, 0x20, 5, 5};  // otra página
        DWIN_Key *const keys[] = {&stop, &far, nullptr, &start, &other, &jump};
        t.vp[0x1031] = 0;
        t.vp[0x1032] = 1;
        t.vp[0x1060] = 0;
        t.vp[0x1220] = 3;
        t.ops.clear();
        CHECK(dwin.checkKeys(keys, 6));
        CHECK(stop.Enable && other.Enable && !start.Enable && !jump.Enable && !far.Enable);
        CHECK(t.vp[0x1032] == 0 && t.vp[0x1220] == 0);
        // Tramos en orden de VP, cada uno con su limpieza.
        CHECK(t.ops.size() == 5);
        CHECK(t.ops[0].type == 'R' && t.ops[0].vp == 0x1023 && t.ops[0].data[0] == 16);
        CHECK(t.ops[1].type == 'W' && t.ops[1].vp == 0x1032 && t.ops[1].data[0] == 0);
        CHECK(t.ops[2].type == 'R' && t.ops[2].vp == 0x1060 && t.ops[2].data[0] == 1);
        CHECK(t.ops[3].type == 'R' && t.ops[3].vp == 0x1220 && t.ops[3].data[0] == 1);
        CHECK(t.ops[4].type == 'W' && t.ops[4].vp == 0x1220);

        // Sin pulsaciones: solo lecturas, y Enable sigue hasta que se consume.
        t.ops.clear();
        CHECK(dwin.checkKeys(keys, 6));
        CHECK(t.ops.size() == 3 && stop.Enable);
        stop.Enable = other.Enable = 0;

        // Dos teclas en el mismo VP: ambas se activan, una sola limpieza.
        DWIN_Key twin = {0x10, 0x32, 9, 5};
        DWIN_Key *const pair[] = {&stop, &twin};
        t.vp[0x1032] = 1;
        t.ops.clear();
        CHECK(dwin.checkKeys(pair, 2));
        CHECK(stop.Enable && twin.Enable && t.ops.size() == 2);
        stop.Enable = 0;

        // Lectura fallida: false y la tecla no cambia; se lee en la próxima.
        DWIN_Key *const one[] = {&jump};
        t.online = false;
        t.vp[0x1023] = 1;
        CHECK(!dwin.checkKeys(one, 1));
        CHECK(!jump.Enable);
        t.online = true;
        CHECK(dwin.checkKeys(one, 1) && jump.Enable && t.vp[0x1023] == 0);

        // Más de JW_DWIN_MAX_READ registros: el tramo se parte.
        DWIN_Key row[12];
        DWIN_Key *rowPtr[12];
        for (int i = 0; i < 12; i++)
        {
            row[i] = {0x30, (uint8_t)(i * 15), 0, 5};
            rowPtr[i] = &row[i];
        }
        t.vp[0x30A5] = 1;  // i = 11
        t.ops.clear();
        CHECK(dwin.checkKeys(rowPtr, 12));
        CHECK(row[11].Enable && !row[0].Enable);
        CHECK(t.ops.size() == 3);
        CHECK(t.ops[0].vp == 0x3000 && t.ops[0].data[0] == 121);
        CHECK(t.ops[1].vp == 0x3087 && t.ops[1].data[0] == 31);
    }

    // readValues: una lectura por tramo; cada objeto como con readValue.
    {
        DWIN_Value row[3] = {{0x2A, 0x05, 0, 0, 0}, {0x2A, 0x06, 1, 0, 0}, {0x2A, 0x09, 4, 0, 1}};
        DWIN_Value farv = {0x2A, 0x40, 0, 0, 0};  // hueco > JW_DWIN_READ_MAX_GAP
        DWIN_Value *const vals[] = {&row[2], nullptr, &farv, &row[0], &row[1]};
        bool changed[5];
        for (int i = 0; i < 3; i++) dwin.sendValue(row[i], 10);
        dwin.sendValue(farv, 10);
        t.vp[0x2A06] = 12;
        t.vp[0x2A09] = 125;
        t.ops.clear();
        CHECK(dwin.readValues(vals, 5, changed));
        CHECK(t.ops.size() == 2);
        CHECK(t.ops[0].type == 'R' && t.ops[0].vp == 0x2A05 && t.ops[0].data[0] == 5);
        CHECK(t.ops[1].type == 'R' && t.ops[1].vp == 0x2A40 && t.ops[1].data[0] == 1);
        CHECK(changed[0] && !changed[1] && !changed[2] && !changed[3] && changed[4]);
        CHECK(row[1].val == 12 && row[2].val == 125 && row[2].fval > 12.4f && row[2].fval < 12.6f);
        CHECK(dwin.isConfirmed(row[0]) && dwin.isConfirmed(farv));

        // Sin cambios; changed es opcional.
        CHECK(dwin.readValues(vals, 5, changed) && !changed[0] && !changed[4]);
        CHECK(dwin.readValues(vals, 5));

        // Lectura fallida: false y los objetos no cambian.
        DWIN_Value *const one[] = {&row[0]};
        t.online = false;
        t.vp[0x2A05] = 77;
        CHECK(!dwin.readValues(one, 1, changed) && !changed[0] && row[0].val == 10);
        t.online = true;
        CHECK(dwin.readValues(one, 1, changed) && changed[0] && row[0].val == 77);
    }

    // keyEncoder con vuelta circular.
    DWIN_Key up = {0x12, 0x20, 5, 5};
    DWIN_Key down = {0x12, 0x21, 6, 5};
    uint32_t count = 5;
    t.vp[0x1220] = 1;
    dwin.keyEncoder(up, down, count, 5, 9);
    CHECK(count == 9);
    t.vp[0x1221] = 1;
    dwin.keyEncoder(up, down, count, 5, 9);
    CHECK(count == 5);

    // enableTouch: trama 0x5AA5 en 0x00B0, cache por estado, espera 25 ms.
    t.ops.clear();
    uint32_t before = millis();
    dwin.enableTouch(start, 0);
    CHECK(t.ops.size() == 1 && t.ops[0].vp == 0x00B0);
    CHECK(t.ops[0].data[0] == 0x5AA5 && t.ops[0].data[1] == 0x10 && t.ops[0].data[2] == 0x0405 && t.ops[0].data[3] == 0);
    CHECK(millis() - before == 25);
    dwin.enableTouch(start, 0);
    CHECK(t.ops.size() == 1);

    // setPage
    t.ops.clear();
    CHECK(dwin.setPage(0x12) && t.ops[0].vp == 0x0084 && t.ops[0].data[0] == 0x5A01 && t.ops[0].data[1] == 0x12);
    CHECK(dwin.currentPage() == 0x12);

    // Sin enlace: tras 3 fallos solo un intento cada 500 ms.
    t.online = false;
    for (int i = 0; i < 3; i++) dwin.sendValue(prog, 100 + i);
    CHECK(!dwin.isOnline());
    t.ops.clear();
    for (int i = 0; i < 10; i++) dwin.sendValue(prog, 200 + i);
    CHECK(t.ops.empty());
    g_mockMs += 500;
    dwin.sendValue(prog, 300);
    CHECK(t.ops.size() == 1);

    // Reconexión: invalida la cache y avisa una sola vez.
    t.online = true;
    g_mockMs += 500;
    dwin.sendValue(prog, 300);
    CHECK(dwin.isOnline() && dwin.reconnected() && !dwin.reconnected());
    t.ops.clear();
    dwin.sendText(name, "ABCDEFG");  // mismo texto: se reenvía tras reconexión
    dwin.enableTouch(start, 0);      // mismo estado: se reenvía tras reconexión
    CHECK(t.ops.size() == 2);
    dwin.sendText(name, "ABCDEFG");
    dwin.enableTouch(start, 0);
    CHECK(t.ops.size() == 2);

    // isConfirmed: un valor enviado y confirmado queda confirmado; un envío
    // fallido no.
    DWIN_Value edit = {0x10, 0x30, 9, 0, 0};
    dwin.sendValue(edit, 40);
    CHECK(dwin.isConfirmed(edit));
    t.online = false;
    dwin.sendValue(edit, 41);
    CHECK(!dwin.isConfirmed(edit) && edit.val == 41);
    t.online = true;
    dwin.sendValue(edit, 41);
    CHECK(dwin.isConfirmed(edit));

    // Pantalla reiniciada: la lectura que restablece el enlace no cuenta
    // como edición aunque el VP traiga otro valor (el inicial de la DWIN).
    t.online = false;
    for (int i = 0; i < 3; i++) dwin.readValue(edit);
    CHECK(!dwin.isOnline());
    t.vp[0x1030] = 0;  // valor inicial tras el reinicio
    t.online = true;
    g_mockMs += 500;
    CHECK(!dwin.readValue(edit));
    CHECK(dwin.isOnline() && dwin.reconnected());
    CHECK(edit.val == 41 && !dwin.isConfirmed(edit));
    dwin.sendValue(edit, 41);  // la aplicación reenvía el valor vigente
    CHECK(dwin.isConfirmed(edit) && t.vp[0x1030] == 41);
    t.vp[0x1030] = 45;         // edición real del operador
    CHECK(dwin.readValue(edit) && edit.val == 45 && dwin.isConfirmed(edit));

    // readValues igual: la lectura que restablece el enlace no cuenta.
    {
        DWIN_Value *const vals[] = {&edit};
        bool changed[1];
        t.online = false;
        for (int i = 0; i < 3; i++) dwin.readValues(vals, 1);
        CHECK(!dwin.isOnline());
        t.vp[0x1030] = 0;
        t.online = true;
        g_mockMs += 500;
        CHECK(dwin.readValues(vals, 1, changed) && !changed[0]);
        CHECK(dwin.reconnected() && edit.val == 45 && !dwin.isConfirmed(edit));
        dwin.sendValue(edit, 45);
        t.vp[0x1030] = 46;
        CHECK(dwin.readValues(vals, 1, changed) && changed[0] && edit.val == 46);
    }

    // Tras una reconexión cualquier objeto confirmado antes deja de estarlo.
    t.online = false;
    for (int i = 0; i < 3; i++) dwin.sendValue(prog, 500 + i);
    t.online = true;
    g_mockMs += 500;
    dwin.sendValue(prog, 600);
    CHECK(dwin.reconnected() && !dwin.isConfirmed(edit));

    // Sin debug configurado no se imprime nada.
    CHECK(Serial.out.empty());

    if (failures == 0) printf("JW_DWIN N1 PASS\n");
    return failures == 0 ? 0 : 1;
}
