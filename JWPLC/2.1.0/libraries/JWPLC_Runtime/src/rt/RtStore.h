// Persistencia del Runtime: FRAM (programa A/B, configuración, retentivos y
// eventos) y microSD (biblioteca de programas y exportación de eventos).
//
// Todas las funciones usan el bus SPI: se llaman solo desde la tarea del
// Runtime, nunca desde los callbacks de pantalla.
//
// Mapa de FRAM en modo Runtime (8 KiB; el último KiB queda para el sistema):
//
//   0x0000  configuración, copia 1 (64 B)
//   0x0040  configuración, copia 2 (64 B)
//   0x0100  programa, ranura A (1536 B)
//   0x0700  programa, ranura B (1536 B)
//   0x0D00  retentivos (64 x 4 B + CRC)
//   0x0E80  eventos (32 x 16 B, anillo) y su índice en 0x1080
#pragma once

#include "RtProgram.h"

namespace jwrt
{

static const uint8_t START_STOP = 0;
static const uint8_t START_RUN = 1;
static const uint8_t START_LAST = 2;

struct Config
{
    uint32_t magic;
    uint32_t seq;        // aumenta en cada escritura (la copia más nueva gana)
    uint8_t slot;        // ranura del programa activo (0 = A, 1 = B)
    uint8_t startMode;   // START_*
    uint8_t lastRun;     // 1 si estaba en RUN al apagar
    uint8_t pinEnabled;  // 1 = pide PIN para Admin
    uint16_t pin;        // 0000..9999
    uint8_t idleMinutes; // vuelta a la pantalla RUN sin teclas (0 = nunca)
    uint8_t reserved[21];
    uint32_t crc;
};

// Eventos (16 bytes).
enum EventCode : uint8_t
{
    EV_NONE = 0,
    EV_POWER_ON,
    EV_RUN,
    EV_STOP,
    EV_PROGRAM_SAVED,
    EV_ALARM_ON,
    EV_ALARM_OFF,
    EV_SD_LOADED,
    EV_SD_SAVED,
    EV_PIN_FAIL,
    EV_PARAM,
    EV_RETAIN_RESTORED,
    EV_PROGRAM_ERASED,
    EV_CLOCK_SET,
};

struct Event
{
    uint8_t code;
    uint8_t arg;      // bloque, salida o severidad
    uint8_t year;     // desde 2000 (0 = sin hora)
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint32_t uptimeS; // segundos desde el arranque
    uint32_t reserved;
};

static const uint8_t EVENT_SLOTS = 32;

const char *eventText(uint8_t code);

class Store
{
public:
    bool begin(); // lee configuración; false si la FRAM no responde
    bool framOk() const { return _framOk; }

    Config &config() { return _cfg; }
    bool saveConfig();

    bool loadProgram(Program &p);         // ranura activa
    bool saveProgram(const Program &p);   // escribe la otra ranura y la activa

    bool loadRetained(uint32_t values[MAX_BLOCKS]);
    bool saveRetained(const uint32_t values[MAX_BLOCKS]);
    void clearRetained();

    void addEvent(const Event &e);
    uint8_t eventCount() const { return _evCount; }
    // i = 0 es el más reciente.
    bool event(uint8_t i, Event &out) const;
    void clearEvents();

    // microSD
    bool sdReady();
    bool sdSave(const Program &p, char path[], size_t size);
    uint8_t sdList(char names[][NAME_LEN + 4], uint8_t max);
    bool sdLoad(const char file[], Program &p);
    bool sdExportEvents(char path[], size_t size);

private:
    bool readConfigCopy(uint32_t addr, Config &c);
    void loadEvents();

    Config _cfg;
    bool _framOk = false;
    Event _ev[EVENT_SLOTS];
    uint8_t _evHead = 0;  // próxima posición a escribir
    uint8_t _evCount = 0;
};

} // namespace jwrt
