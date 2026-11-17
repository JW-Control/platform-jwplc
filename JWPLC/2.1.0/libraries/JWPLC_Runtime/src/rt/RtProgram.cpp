#include "RtProgram.h"

#include <string.h>

namespace jwrt
{

// Formas de onda de la ayuda: 16 pasos, '_' = 0 y '-' = 1.
static const TypeInfo TYPES[] = {
    // type, cat, símbolo, nombre, descripción, pins, min, nombres de pin,
    // p1, p2, retentivo, ayuda (2 líneas), ondas
    {BT_AND, CAT_LOGIC, "&", "AND", "Y: todas en 1", 4, 2, {nullptr, nullptr, nullptr, nullptr},
     PK_NONE, nullptr, PK_NONE, nullptr, false,
     "Q es 1 solo si todas", "las entradas son 1.",
     "__------____----", "____------__--__", "____----____--__", "I1", "I2"},
    {BT_OR, CAT_LOGIC, ">=1", "OR", "O: alguna en 1", 4, 2, {nullptr, nullptr, nullptr, nullptr},
     PK_NONE, nullptr, PK_NONE, nullptr, false,
     "Q es 1 si al menos una", "entrada es 1.",
     "__------____----", "____------__--__", "__--------__----", "I1", "I2"},
    {BT_NAND, CAT_LOGIC, "&", "NAND", "Y negada", 4, 2, {nullptr, nullptr, nullptr, nullptr},
     PK_NONE, nullptr, PK_NONE, nullptr, false,
     "Q es 0 solo si todas", "las entradas son 1.",
     "__------____----", "____------__--__", "----____----__--", "I1", "I2"},
    {BT_NOR, CAT_LOGIC, ">=1", "NOR", "O negada", 4, 2, {nullptr, nullptr, nullptr, nullptr},
     PK_NONE, nullptr, PK_NONE, nullptr, false,
     "Q es 1 solo si todas", "las entradas son 0.",
     "__------____----", "____------__--__", "--________--____", "I1", "I2"},
    {BT_XOR, CAT_LOGIC, "=1", "XOR", "O exclusiva", 2, 2, {nullptr, nullptr, nullptr, nullptr},
     PK_NONE, nullptr, PK_NONE, nullptr, false,
     "Q es 1 si las dos", "entradas son distintas.",
     "__------____----", "____------__--__", "__--____--____--", "I1", "I2"},
    {BT_NOT, CAT_LOGIC, "1", "NOT", "Inversor", 1, 1, {nullptr, nullptr, nullptr, nullptr},
     PK_NONE, nullptr, PK_NONE, nullptr, false,
     "Q es lo contrario", "de la entrada.",
     "__----____----__", nullptr, "--____----____--", "In", nullptr},
    {BT_EDGE_UP, CAT_LOGIC, "/", "FLANCO+", "Pulso al subir", 1, 1, {nullptr, nullptr, nullptr, nullptr},
     PK_NONE, nullptr, PK_NONE, nullptr, false,
     "Q es 1 durante un scan", "cuando la entrada sube.",
     "__----____----__", nullptr, "__-_______-_____", "In", nullptr},
    {BT_EDGE_DN, CAT_LOGIC, "\\", "FLANCO-", "Pulso al bajar", 1, 1, {nullptr, nullptr, nullptr, nullptr},
     PK_NONE, nullptr, PK_NONE, nullptr, false,
     "Q es 1 durante un scan", "cuando la entrada baja.",
     "__----____----__", nullptr, "______-_______-_", "In", nullptr},
    {BT_SR, CAT_LOGIC, "SR", "SR", "Rele autoenclavador", 2, 2, {"S", "R", nullptr, nullptr},
     PK_NONE, nullptr, PK_NONE, nullptr, true,
     "S enciende y R apaga Q.", "R tiene prioridad.",
     "__-_________-___", "_______-________", "__-----_____----", "S", "R"},
    {BT_IMP, CAT_LOGIC, "IMP", "IMPULSO", "Rele de impulsos", 3, 3, {"Trg", "S", "R"},
     PK_NONE, nullptr, PK_NONE, nullptr, true,
     "Cada pulso en Trg cambia", "Q. S enciende, R apaga.",
     "__-_____-_____-_", nullptr, "__------______--", "Trg", nullptr},
    {BT_TON, CAT_TIME, "TON", "TON", "Retardo conexion", 1, 1, {"Trg", nullptr, nullptr, nullptr},
     PK_TIME, "T", PK_NONE, nullptr, false,
     "Q sube T despues de que", "Trg sube; baja con Trg.",
     "__--------___-__", nullptr, "_____-----______", "Trg", nullptr},
    {BT_TOF, CAT_TIME, "TOF", "TOF", "Retardo desconex.", 2, 2, {"Trg", "R", nullptr, nullptr},
     PK_TIME, "T", PK_NONE, nullptr, false,
     "Q sigue a Trg y baja T", "despues de que Trg cae.",
     "__---_______----", nullptr, "__-------___----", "Trg", nullptr},
    {BT_TP, CAT_TIME, "TP", "TP", "Impulso de tiempo T", 2, 2, {"Trg", "R", nullptr, nullptr},
     PK_TIME, "T", PK_NONE, nullptr, false,
     "Al subir Trg, Q da un", "impulso de duracion T.",
     "__-_______------", nullptr, "__----____----__", "Trg", nullptr},
    {BT_TONR, CAT_TIME, "TONR", "TONR", "Retardo con memoria", 2, 2, {"Trg", "R", nullptr, nullptr},
     PK_TIME, "T", PK_NONE, nullptr, true,
     "Suma el tiempo con Trg", "en 1; R lo pone en cero.",
     "__---___--______", "_____________-__", "_________----___", "Trg", "R"},
    {BT_GEN, CAT_TIME, "GEN", "GEN", "Gen. de impulsos", 1, 1, {"En", nullptr, nullptr, nullptr},
     PK_TIME, "TH", PK_TIME, "TL", false,
     "Con En en 1, Q oscila:", "TH en 1 y TL en 0.",
     "__------------__", nullptr, "__--__--__--____", "En", nullptr},
    {BT_CTUD, CAT_COUNT, "CTUD", "CONTADOR", "Contador +/-", 3, 3, {"R", "Cnt", "Dir"},
     PK_COUNT, "On", PK_NONE, nullptr, true,
     "Cuenta los flancos de Cnt", "(Dir=1 resta). Q: cuenta>=On.",
     "_-_-_-_-_-______", "_____________-__", "_____--------___", "Cnt", "R"},
    {BT_WEEK, CAT_CLOCK, "WEEK", "SEMANAL", "Temporizador semanal", 0, 0, {nullptr, nullptr, nullptr, nullptr},
     PK_CLOCK, "On", PK_CLOCK, "Off", false,
     "Q es 1 entre On y Off", "los dias marcados (RTC).",
     nullptr, nullptr, "____------______", nullptr, nullptr},
    {BT_ALARM, CAT_SYSTEM, "!", "AVISO", "Texto de aviso", 1, 1, {"En", nullptr, nullptr, nullptr},
     PK_NONE, nullptr, PK_NONE, nullptr, false,
     "Con En en 1 muestra el", "texto y lo registra.",
     "__------______--", nullptr, "__------______--", "En", nullptr},
};

static const uint8_t TYPE_COUNT = sizeof(TYPES) / sizeof(TYPES[0]);

const TypeInfo *typeInfo(uint8_t type)
{
    for (uint8_t i = 0; i < TYPE_COUNT; i++)
    {
        if (TYPES[i].type == type)
        {
            return &TYPES[i];
        }
    }
    return nullptr;
}

uint8_t typesInCategory(uint8_t category, uint8_t out[], uint8_t max)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < TYPE_COUNT && n < max; i++)
    {
        if (TYPES[i].category == category)
        {
            out[n++] = TYPES[i].type;
        }
    }
    return n;
}

const char *categoryName(uint8_t category)
{
    static const char *const NAMES[CAT_COUNT_ALL] = {
        "LOGICA", "TIEMPO", "CONTADOR", "RELOJ", "SISTEMA", "ANALOG."};
    return category < CAT_COUNT_ALL ? NAMES[category] : "?";
}

void sourceName(uint8_t src, char out[], size_t size)
{
    if (src == SRC_OPEN)
    {
        snprintf(out, size, "x");
    }
    else if (src == SRC_HI)
    {
        snprintf(out, size, "hi");
    }
    else if (src == SRC_LO)
    {
        snprintf(out, size, "lo");
    }
    else if (src >= SRC_I && src < SRC_I + NUM_I)
    {
        snprintf(out, size, "I%u", (unsigned)(src - SRC_I + 1));
    }
    else if (src >= SRC_Q && src < SRC_Q + NUM_Q)
    {
        snprintf(out, size, "Q%u", (unsigned)(src - SRC_Q + 1));
    }
    else if (src >= SRC_M && src < SRC_M + NUM_M)
    {
        snprintf(out, size, "M%u", (unsigned)(src - SRC_M + 1));
    }
    else if (srcIsRemote(src))
    {
        snprintf(out, size, "%u.I%u", (unsigned)(srcModule(src) + 1), (unsigned)((src - SRC_XI) % X_IO + 1));
    }
    else if (srcIsBlock(src))
    {
        snprintf(out, size, "B%02u", (unsigned)(srcBlock(src) + 1));
    }
    else
    {
        snprintf(out, size, "?");
    }
}

uint32_t timeMs(int32_t value, uint8_t base)
{
    if (value < 0)
    {
        value = 0;
    }
    const uint32_t hi = (uint32_t)value / 100;
    const uint32_t lo = (uint32_t)value % 100;
    switch (base)
    {
    case 1: // m: MM:SS
        return (hi * 60UL + lo) * 1000UL;
    case 2: // h: HH:MM
        return (hi * 60UL + lo) * 60000UL;
    default: // s: SS:cc
        return hi * 1000UL + lo * 10UL;
    }
}

void formatTime(int32_t value, uint8_t base, char out[], size_t size)
{
    if (value < 0)
    {
        value = 0;
    }
    static const char UNITS[3] = {'s', 'm', 'h'};
    snprintf(out, size, "%02ld:%02ld%c", (long)(value / 100), (long)(value % 100),
             UNITS[base < 3 ? base : 0]);
}

void formatClock(int32_t minutes, char out[], size_t size)
{
    if (minutes < 0)
    {
        minutes = 0;
    }
    snprintf(out, size, "%02ld:%02ld", (long)(minutes / 60) % 24, (long)(minutes % 60));
}

void clearProgram(Program &p, const char name[])
{
    memset(&p, 0, sizeof(p));
    strncpy(p.name, name, NAME_LEN - 1);
}

void initBlock(Block &b, uint8_t type)
{
    memset(&b, 0, sizeof(b));
    b.type = type;
    const TypeInfo *ti = typeInfo(type);
    if (!ti)
    {
        return;
    }
    for (uint8_t i = 0; i < MAX_PINS; i++)
    {
        b.in[i] = SRC_OPEN;
    }
    switch (type)
    {
    case BT_TON:
    case BT_TOF:
    case BT_TP:
    case BT_TONR:
        b.p1 = 300; // 03:00s
        b.flags |= BF_PARAM;
        break;
    case BT_GEN:
        b.p1 = 50; // 00:50s
        b.p2 = 50;
        b.flags |= BF_PARAM;
        break;
    case BT_CTUD:
        b.p1 = 10;
        b.flags |= BF_PARAM;
        break;
    case BT_WEEK:
        b.aux = 0x1F;   // lunes a viernes
        b.p1 = 7 * 60;  // 07:00
        b.p2 = 18 * 60; // 18:00
        b.flags |= BF_PARAM;
        break;
    case BT_ALARM:
        b.aux = (uint8_t)(SEV_WARN << 4);
        break;
    default:
        break;
    }
}

uint8_t addBlock(Program &p, uint8_t type)
{
    if (p.count >= MAX_BLOCKS || !typeInfo(type))
    {
        return 0xFF;
    }
    const uint8_t idx = p.count++;
    initBlock(p.blocks[idx], type);
    return idx;
}

static uint8_t remapSource(uint8_t s, uint8_t removed)
{
    if (!srcIsBlock(s))
    {
        return s;
    }
    const uint8_t b = srcBlock(s);
    if (b == removed)
    {
        return SRC_OPEN;
    }
    return b > removed ? (uint8_t)(s - 1) : s;
}

bool deleteBlock(Program &p, uint8_t idx)
{
    if (idx >= p.count)
    {
        return false;
    }
    for (uint8_t i = idx; i + 1 < p.count; i++)
    {
        p.blocks[i] = p.blocks[i + 1];
    }
    p.count--;
    memset(&p.blocks[p.count], 0, sizeof(Block));
    for (uint8_t i = 0; i < p.count; i++)
    {
        for (uint8_t k = 0; k < MAX_PINS; k++)
        {
            p.blocks[i].in[k] = remapSource(p.blocks[i].in[k], idx);
        }
    }
    for (uint8_t i = 0; i < NUM_Q; i++)
    {
        p.q[i] = remapSource(p.q[i], idx);
    }
    for (uint8_t i = 0; i < NUM_M; i++)
    {
        p.m[i] = remapSource(p.m[i], idx);
    }
    for (uint8_t x = 0; x < NUM_X; x++)
    {
        for (uint8_t i = 0; i < X_IO; i++)
        {
            p.xq[x][i] = remapSource(p.xq[x][i], idx);
        }
    }
    return true;
}

void setOutSource(Program &p, uint8_t o, uint8_t src)
{
    if (o < NUM_Q)
    {
        p.q[o] = src;
    }
    else if (o < NUM_OUT)
    {
        p.xq[(o - NUM_Q) / X_IO][(o - NUM_Q) % X_IO] = src;
    }
}

void setOutNeg(Program &p, uint8_t o, bool neg)
{
    uint8_t *bits = o < NUM_Q ? &p.qNeg : &p.xqNeg[(o - NUM_Q) / X_IO];
    const uint8_t bit = (uint8_t)(1u << (o < NUM_Q ? o : (o - NUM_Q) % X_IO));
    *bits = neg ? (uint8_t)(*bits | bit) : (uint8_t)(*bits & ~bit);
}

void outName(uint8_t o, char out[], size_t size)
{
    if (o < NUM_Q)
    {
        snprintf(out, size, "Q%u", (unsigned)(o + 1));
    }
    else
    {
        snprintf(out, size, "%u.Q%u", (unsigned)((o - NUM_Q) / X_IO + 1), (unsigned)((o - NUM_Q) % X_IO + 1));
    }
}

// «Intermitente»: solo salidas y dos TON; cabe en una pantalla del editor
// y se ve funcionar sin tocar entradas (no hace falta simularlas). El lazo se
// cierra con la marca M1 (valor del ciclo anterior), no con un cable de
// bloque a bloque: así el mapa queda en dos columnas.
static void demoIntermitente(Program &p)
{
    clearProgram(p, "Intermitente");
    // B01 TON: entrada = M1 negada. Con M1 en 0 cuenta 1 s y se activa.
    uint8_t b = addBlock(p, BT_TON);
    p.blocks[b].in[0] = SRC_M + 0;
    p.blocks[b].neg = 0x01;
    p.blocks[b].p1 = 100;
    p.blocks[b].flags |= BF_PARAM;
    // B02 TON: 1 s después de B01 se activa y, por M1, apaga a B01; los dos
    // vuelven a 0 y el ciclo empieza de nuevo (1 s encendida, 1 s apagada).
    b = addBlock(p, BT_TON);
    p.blocks[b].in[0] = SRC_B + 0;
    p.blocks[b].p1 = 100;
    p.blocks[b].flags |= BF_PARAM;
    p.m[0] = SRC_B + 1;
    // Q1 sigue a B01 y Q2 es su contrario: se alternan.
    p.q[0] = SRC_B + 0;
    p.q[1] = SRC_B + 0;
    p.qNeg = 0x02;
}

// «Escalonado»: una entrada y dos TON. Con I1 en 1, Q1 y Q2 se encienden
// en cascada a 1 y 2 s; al soltar I1 se apagan. Cabe en una pantalla.
static void demoEscalonado(Program &p)
{
    clearProgram(p, "Escalonado");
    static const uint16_t TIEMPOS[2] = {100, 200}; // centésimas: 1 y 2 s
    for (uint8_t i = 0; i < 2; i++)
    {
        uint8_t b = addBlock(p, BT_TON);
        p.blocks[b].in[0] = SRC_I + 0;
        p.blocks[b].p1 = TIEMPOS[i];
        p.blocks[b].flags |= BF_PARAM;
        p.q[i] = (uint8_t)(SRC_B + b);
    }
}

void demoProgram(Program &p, uint8_t which)
{
    if (which == DEMO_ESCALONADO)
    {
        demoEscalonado(p);
        return;
    }
    if (which == DEMO_INTERMITENTE)
    {
        demoIntermitente(p);
        return;
    }
    // «Demo feria»: una máquina en tres pasos, pensada para explicarla en vivo.
    clearProgram(p, "Demo feria");
    // B01 marcha/parada del motor: SR con S = I1 (marcha) y R = I2 (parada) -> Q1.
    uint8_t b = addBlock(p, BT_SR);
    p.blocks[b].in[0] = SRC_I + 0;
    p.blocks[b].in[1] = SRC_I + 1;
    p.q[0] = SRC_B + 0;
    // B02 bomba: arranca 3 s después del motor (TON) -> Q2. Tiempo en Parámetros.
    b = addBlock(p, BT_TON);
    p.blocks[b].in[0] = SRC_B + 0;
    p.blocks[b].p1 = 300;
    p.blocks[b].flags |= BF_PARAM;
    p.q[1] = SRC_B + 1;
    // B03 contador de piezas: Cnt = I3, R = I4; Q3 al llegar a 5. Conserva la cuenta al apagar.
    b = addBlock(p, BT_CTUD);
    p.blocks[b].in[0] = SRC_I + 3;
    p.blocks[b].in[1] = SRC_I + 2;
    p.blocks[b].in[2] = SRC_LO;
    p.blocks[b].p1 = 5;
    p.blocks[b].flags |= BF_RETAIN | BF_PARAM;
    p.q[2] = SRC_B + 2;
    // B04 aviso "Lote completo" cuando el contador llega.
    b = addBlock(p, BT_ALARM);
    p.blocks[b].in[0] = SRC_B + 2;
    p.blocks[b].aux = (uint8_t)((SEV_WARN << 4) | 0);
    strncpy(p.texts[0], "Lote completo", TEXT_LEN - 1);
}

uint32_t crc32(const uint8_t *data, size_t len, uint32_t crc)
{
    crc = ~crc;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t k = 0; k < 8; k++)
        {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
        }
    }
    return ~crc;
}

} // namespace jwrt
