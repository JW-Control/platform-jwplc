// Núcleo del Runtime: estado compartido entre la tarea del motor y la UI.
//
// La UI corre dentro de los callbacks de JWPLC_Display (con el bus SPI
// tomado), así que nunca toca FRAM, SD, Ethernet ni el RTC: pide la operación
// con post() y la tarea del Runtime la hace fuera del batch de pantalla.
#pragma once

#include "RtEngine.h"
#include "RtExpansion.h"
#include "RtStore.h"

namespace jwrt
{

enum Mode : uint8_t
{
    MODE_STOP = 0,
    MODE_RUN = 1,
};

enum Command : uint8_t
{
    CMD_NONE = 0,
    CMD_RUN,
    CMD_STOP,
    CMD_APPLY_DRAFT,    // valida, guarda en FRAM y carga el borrador
    CMD_SET_PARAM,      // cambia un parámetro del programa activo (en RUN)
    CMD_ERASE_PROGRAM,
    CMD_LOAD_DEMO,
    CMD_SD_SAVE,
    CMD_SD_LIST,
    CMD_SD_LOAD,        // a.text = archivo
    CMD_SD_EXPORT,
    CMD_SET_CLOCK,      // a.p1 = AAAAMMDD, a.p2 = HHMMSS
    CMD_SAVE_CONFIG,
    CMD_CLEAR_EVENTS,
    CMD_CLEAR_RETAIN,
    CMD_EVENT,          // a.block = código, a.aux = dato
    CMD_TEST_Q,         // a.aux = mapa de Q en modo prueba (solo en STOP)
    CMD_TEST_END,
    CMD_NET_REFRESH,
    // Expansión (solo en STOP salvo X_PROBE de un módulo vinculado).
    CMD_X_PROBE,        // a.aux = ID, a.flags = 1 trama de fábrica -> core.expansion.lastProbe
    CMD_X_ADD,          // a.aux = ID nuevo, a.p1 = estado seguro (valor | mantener << 8 | vigilancia << 16)
    CMD_X_SCAN,         // -> core.xFound
    CMD_X_LINK,         // a.aux = ID
    CMD_X_SAFE,         // a.block = módulo, a.p1 como CMD_X_ADD
    CMD_X_REMOVE,       // a.block = módulo
    CMD_X_BUS,          // a.p1 = código de baudrate, a.p2 = trama
};

struct CommandArgs
{
    uint8_t block;
    uint8_t aux;
    uint8_t flags;
    int32_t p1;
    int32_t p2;
    char text[NAME_LEN + 4];
};

static const uint8_t RESULT_OK = 1;
static const uint8_t RESULT_FAIL = 2;

struct Core
{
    Engine engine;
    Store store;
    Expansion expansion;
    XFound xFound[X_FOUND_MAX];
    volatile uint8_t xFoundCount = 0;
    volatile int8_t xLastModule = -1; // módulo agregado o vinculado por la última orden
    Program draft;          // copia de trabajo del editor (solo la UI)

    volatile uint8_t mode = MODE_STOP;
    volatile bool framOk = false;
    volatile bool sdOk = false;
    volatile bool testMode = false;
    volatile uint8_t testQ = 0;
    volatile uint32_t scanCount = 0;
    volatile uint32_t scanUsLast = 0;
    volatile uint32_t scanUsMax = 0;
    volatile uint32_t scanUsAvg = 0; // media móvil

    // Resultado de la última orden.
    volatile uint32_t doneSeq = 0;
    volatile uint8_t result = 0;
    char resultText[48] = {0};

    // Lista de la SD (CMD_SD_LIST).
    char sdFiles[12][NAME_LEN + 4];
    volatile uint8_t sdFileCount = 0;

    // Red (CMD_NET_REFRESH).
    char ip[20] = "-";
    char mask[20] = "-";
    char gateway[20] = "-";
    volatile bool linkUp = false;

    // Sesión de Admin (la UI la abre con el PIN).
    bool admin = false;
};

extern Core core;

// Envía una orden a la tarea. Devuelve el número de secuencia; la orden
// terminó cuando core.doneSeq >= ese número.
uint32_t post(Command cmd, const CommandArgs &args);
uint32_t post(Command cmd);
bool done(uint32_t seq);

// Hora del RTC (snapshot del sistema).
ClockNow clockNow();
bool clockText(char out[], size_t size);      // "12:41"
bool dateText(char out[], size_t size);       // "Sa 01/11/26"

} // namespace jwrt
