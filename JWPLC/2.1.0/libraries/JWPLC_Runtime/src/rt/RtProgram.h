// Modelo de programa del Runtime de panel (motor v2: entradas variables y
// negación por pin, al estilo de las funciones del LOGO!).
//
// Uso interno de la librería: el sketch no ve estos tipos.
#pragma once

#include <Arduino.h>
#include <stddef.h>

namespace jwrt
{

// ---------------------------------------------------------------- límites
static const uint8_t MAX_BLOCKS = 64;  // B01..B64
static const uint8_t MAX_PINS = 4;     // entradas por bloque
static const uint8_t NUM_I = 8;        // I1..I8 (I0_0..I0_7)
static const uint8_t NUM_Q = 8;        // Q1..Q8 (Q0_0..Q0_7)
static const uint8_t NUM_M = 8;        // marcas M1..M8
static const uint8_t NUM_TEXTS = 8;    // textos de aviso
static const uint8_t TEXT_LEN = 20;    // caracteres por texto (con el 0 final)
static const uint8_t NAME_LEN = 16;    // nombre del programa (con el 0 final)
static const uint8_t NUM_X = 7;        // módulos de expansión (1..7)
static const uint8_t X_IO = 8;         // entradas y salidas por módulo
static const uint8_t NUM_OUT = NUM_Q + NUM_X * X_IO; // salidas del programa: Q1..Q8 y 1.Q1..7.Q8

// ---------------------------------------------------------------- fuentes
// Una entrada de bloque, Q o M lee una "fuente" de un byte.
static const uint8_t SRC_OPEN = 0x00; // x: entrada sin conectar
static const uint8_t SRC_HI = 0x01;   // constante 1
static const uint8_t SRC_LO = 0x02;   // constante 0
static const uint8_t SRC_I = 0x10;    // 0x10..0x17 = I1..I8
static const uint8_t SRC_Q = 0x20;    // 0x20..0x27 = Q1..Q8 (valor del scan anterior)
static const uint8_t SRC_M = 0x30;    // 0x30..0x37 = M1..M8 (valor del scan anterior)
static const uint8_t SRC_XI = 0x40;   // 0x40..0x77 = 1.I1..7.I8 (módulo = (s - 0x40) / 8 + 1)
static const uint8_t SRC_B = 0x80;    // 0x80..0xBF = salida del bloque B01..B64

inline bool srcIsBlock(uint8_t s) { return s >= SRC_B && s < SRC_B + MAX_BLOCKS; }
inline uint8_t srcBlock(uint8_t s) { return (uint8_t)(s - SRC_B); }
inline bool srcIsRemote(uint8_t s) { return s >= SRC_XI && s < SRC_XI + NUM_X * X_IO; }
inline uint8_t srcModule(uint8_t s) { return (uint8_t)((s - SRC_XI) / X_IO); } // 0..6

// ---------------------------------------------------------------- tipos
enum BlockType : uint8_t
{
    BT_NONE = 0,
    // Lógica (GF)
    BT_AND = 1,
    BT_OR,
    BT_NAND,
    BT_NOR,
    BT_XOR,
    BT_NOT,
    BT_EDGE_UP,
    BT_EDGE_DN,
    // Memoria
    BT_SR = 10,
    BT_IMP,
    // Tiempo
    BT_TON = 20,
    BT_TOF,
    BT_TP,
    BT_TONR,
    BT_GEN,
    // Contador
    BT_CTUD = 30,
    // Reloj
    BT_WEEK = 40,
    // Sistema
    BT_ALARM = 50,
};

// Banderas del bloque.
static const uint8_t BF_RETAIN = 0x01;    // conserva el estado al apagar (FRAM)
static const uint8_t BF_PARAM = 0x02;     // aparece en el menú Parámetros ("+")
static const uint8_t BF_BASE_MASK = 0x0C; // base de tiempo: 0 = s, 1 = m, 2 = h
static const uint8_t BF_BASE_SHIFT = 2;

// Un bloque ocupa 16 bytes en RAM y en la imagen de FRAM.
struct Block
{
    uint8_t type;          // BlockType
    uint8_t flags;         // BF_*
    uint8_t in[MAX_PINS];  // fuentes de las entradas
    uint8_t neg;           // bit i = entrada i negada
    uint8_t aux;           // WEEK: días (bit0 = lunes); ALARM: texto y severidad
    int32_t p1;            // parámetro 1 (tiempo, umbral, hora de conexión)
    int32_t p2;            // parámetro 2 (tiempo de pausa, hora de desconexión)
};
static_assert(sizeof(Block) == 16, "Block debe ocupar 16 bytes");

// Programa completo. Se guarda tal cual en FRAM y en la SD.
struct Program
{
    char name[NAME_LEN];
    uint8_t count;            // bloques usados (B01..Bcount)
    uint8_t reserved[3];
    uint8_t q[NUM_Q];         // fuente de cada salida
    uint8_t m[NUM_M];         // fuente de cada marca
    uint8_t qNeg;             // bit i = Q(i+1) negada
    uint8_t mNeg;
    uint8_t reserved2[2];
    Block blocks[MAX_BLOCKS];
    char texts[NUM_TEXTS][TEXT_LEN];
    // Imagen v2: salidas de los módulos de expansión (1.Q1..7.Q8).
    uint8_t xq[NUM_X][X_IO];  // fuente de cada salida remota
    uint8_t xqNeg[NUM_X];     // bit i = salida i+1 del módulo negada
    uint8_t reserved3;
};

// Tamaño de la imagen v1 (sin las salidas remotas): se sigue leyendo.
static const uint16_t PROGRAM_V1_SIZE = (uint16_t)offsetof(Program, xq);

// Salida o del programa: 0..7 = Q1..Q8, 8.. = 1.Q1..7.Q8.
inline uint8_t outSource(const Program &p, uint8_t o)
{
    return o < NUM_Q ? p.q[o] : p.xq[(o - NUM_Q) / X_IO][(o - NUM_Q) % X_IO];
}
inline bool outNeg(const Program &p, uint8_t o)
{
    return o < NUM_Q ? ((p.qNeg >> o) & 1) : ((p.xqNeg[(o - NUM_Q) / X_IO] >> ((o - NUM_Q) % X_IO)) & 1);
}
void setOutSource(Program &p, uint8_t o, uint8_t src);
void setOutNeg(Program &p, uint8_t o, bool neg);
// "Q3" o "1.Q3"
void outName(uint8_t o, char out[], size_t size);

// Severidad de ALARM (bits 4..5 de aux); bits 0..2 = índice del texto.
static const uint8_t SEV_INFO = 0;
static const uint8_t SEV_WARN = 1;
static const uint8_t SEV_CRIT = 2;

// ---------------------------------------------------------------- catálogo
enum Category : uint8_t
{
    CAT_LOGIC = 0,
    CAT_TIME,
    CAT_COUNT,
    CAT_CLOCK,
    CAT_SYSTEM,
    CAT_ANALOG,
    CAT_COUNT_ALL
};

// Clase de parámetro, para el editor.
enum ParamKind : uint8_t
{
    PK_NONE = 0,
    PK_TIME,     // tiempo con base s/m/h (valor 0..9999 = "99:99")
    PK_COUNT,    // entero 0..999999
    PK_CLOCK,    // hora del día en minutos (0..1439) -> "HH:MM"
};

struct TypeInfo
{
    uint8_t type;
    uint8_t category;
    const char *symbol;  // lo que se dibuja en el bloque: "&", ">=1", "TON"...
    const char *name;    // nombre corto de la paleta
    const char *desc;    // una línea de descripción
    uint8_t pins;        // entradas por defecto
    uint8_t minPins;     // las compuertas admiten 2..4
    const char *pinNames[MAX_PINS]; // nullptr = sin nombre (compuertas)
    uint8_t p1Kind;
    const char *p1Name;
    uint8_t p2Kind;
    const char *p2Name;
    bool canRetain;
    // Ayuda: dos líneas de texto y formas de onda (16 pasos, '_' = 0, '-' = 1)
    const char *help1;
    const char *help2;
    const char *waveIn;   // entrada principal
    const char *waveIn2;  // segunda entrada (o nullptr)
    const char *waveQ;    // salida
    const char *waveInName;
    const char *waveIn2Name;
};

const TypeInfo *typeInfo(uint8_t type);
uint8_t typesInCategory(uint8_t category, uint8_t out[], uint8_t max);
const char *categoryName(uint8_t category);

// Texto corto de una fuente: "I1", "Q3", "M2", "B05", "hi", "lo", "x".
void sourceName(uint8_t src, char out[], size_t size);

// Tiempo en ms a partir del valor y la base del bloque.
uint32_t timeMs(int32_t value, uint8_t base);
// "12:50s", "05:30m", "01:15h"
void formatTime(int32_t value, uint8_t base, char out[], size_t size);
// "07:30"
void formatClock(int32_t minutes, char out[], size_t size);

// Programa vacío, con nombre.
void clearProgram(Program &p, const char name[]);
// Programa de ejemplo (portón con temporizador, alumbrado semanal, contador).
// Programas de ejemplo: 0 = «Demo feria», 1 = «Intermitente», 2 = «Escalonado».
static const uint8_t DEMO_FERIA = 0;
static const uint8_t DEMO_INTERMITENTE = 1;
static const uint8_t DEMO_ESCALONADO = 2;
void demoProgram(Program &p, uint8_t which = DEMO_FERIA);

// Bloque nuevo con sus valores por defecto.
void initBlock(Block &b, uint8_t type);

// Borra el bloque idx y renumera las referencias (las que apuntaban a él
// quedan abiertas). Devuelve false si idx no existe.
bool deleteBlock(Program &p, uint8_t idx);

// Agrega un bloque al final. Devuelve su índice o 0xFF si no hay lugar.
uint8_t addBlock(Program &p, uint8_t type);

// CRC32 (para FRAM y SD).
uint32_t crc32(const uint8_t *data, size_t len, uint32_t crc = 0);

} // namespace jwrt
