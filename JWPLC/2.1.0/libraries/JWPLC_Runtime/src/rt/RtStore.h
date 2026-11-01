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
//   0x0E80  libre (antes eventos; ahora van al historial de JWPLC_Display, 0x1E00)
//   0x1100  módulos de expansión vinculados (7 x 16 B + CRC)
//   0x1180  configuración de esclavo (JWPLC_Expansion), si el equipo es módulo
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
    uint8_t busBaud;     // bus de expansión: 0 = 115200 (fábrica), 1..5 = código 0..4
    uint8_t busFrame;    // 0 = 8N1, 1 = 8E1, 2 = 8O1
    uint8_t reserved[19];
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
    EV_X_ADDED,     // arg = módulo
    EV_X_REMOVED,
    EV_X_OFFLINE,
    EV_X_ONLINE,
    EV_X_WRONG,     // otro equipo en el ID del módulo
    EV_COUNT,
    EV_OTHER = 0xFF, // evento del sketch o del HMI (código propio en Event.other)
};

// Los eventos van al historial común de JWPLC_Display (JWPLC_UI_EventLog):
// RAM 64, FRAM 16 en 0x1E00 y copia a /EVENTOS.CSV. El Runtime anota con
// códigos RT_EVENT_BASE + EventCode; el encendido es el 100 del sistema.
static const uint16_t RT_EVENT_BASE = 400;

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
    uint32_t uptimeS; // segundos desde el arranque (si no hay hora)
    uint16_t other;   // código del historial si code == EV_OTHER
    uint16_t reserved;
};

const char *eventText(uint8_t code);
// Texto de un evento leído del historial (también los del sketch/HMI).
const char *eventText(const Event &e);

// Módulo de expansión vinculado (16 bytes). Módulo n = posición n - 1.
static const uint8_t XL_USED = 0x01;
static const uint8_t XL_LOCKED = 0x02; // configuración fija desde el sketch del esclavo

struct XLink
{
    uint8_t flags;      // XL_*
    uint8_t id;         // ID Modbus 1..246
    uint8_t product;
    uint8_t io;         // DI en el nibble alto, DO en el bajo (8/8 = 0x88)
    uint8_t safeValue;  // estado seguro: bit n = Q(n+1) ON
    uint8_t safeHold;   // bit n = Q(n+1) mantiene
    uint8_t watchdog;   // x 100 ms
    uint8_t firmware;   // mayor en el nibble alto, menor en el bajo
    uint32_t uid;       // UID del esclavo (CRC de su MAC)
    uint32_t reserved;
};

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

    bool loadLinks(XLink links[NUM_X]);
    bool saveLinks(const XLink links[NUM_X]);

    void addEvent(uint8_t code, uint8_t arg);
    uint8_t eventCount() const;
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
    void startEvents();

    Config _cfg;
    bool _framOk = false;
};

} // namespace jwrt
