#include "RtStore.h"

#include <JWPLC_GlobalPeripherals.h>
#include <string.h>

namespace jwrt
{

// JW_FRAM::write() es de bajo nivel: no activa el "write enable" (WREN) de la
// FRAM, que además se borra solo al terminar cada escritura. Sin esto la
// escritura no tiene efecto en la memoria real.
static bool framWrite(uint32_t addr, const uint8_t *data, size_t len)
{
    if (!JWPLC_FRAM.writeEnable(true))
    {
        return false;
    }
    const bool ok = JWPLC_FRAM.write(addr, data, len);
    JWPLC_FRAM.writeEnable(false);
    return ok;
}

static const uint32_t CFG_MAGIC = 0x5452574AUL; // "JWRT"
static const uint32_t IMG_MAGIC = 0x4750574AUL; // "JWPG"
static const uint16_t IMG_VERSION = 1;

static const uint32_t ADDR_CFG1 = 0x0000;
static const uint32_t ADDR_CFG2 = 0x0040;
static const uint32_t ADDR_SLOT[2] = {0x0100, 0x0700};
static const uint32_t ADDR_RETAIN = 0x0D00;
static const uint32_t ADDR_EVENTS = 0x0E80;
static const uint32_t ADDR_EVHEAD = 0x1080;

struct ImageHeader
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t crc;
    uint32_t reserved;
};

static_assert(sizeof(ImageHeader) + sizeof(Program) <= 0x600, "el programa no entra en la ranura");
static_assert(sizeof(Config) == 40, "Config debe ocupar 40 bytes");
static_assert(sizeof(Event) == 16, "Event debe ocupar 16 bytes");

const char *eventText(uint8_t code)
{
    switch (code)
    {
    case EV_POWER_ON:
        return "Encendido";
    case EV_RUN:
        return "RUN";
    case EV_STOP:
        return "STOP";
    case EV_PROGRAM_SAVED:
        return "Programa guardado";
    case EV_ALARM_ON:
        return "Aviso activo";
    case EV_ALARM_OFF:
        return "Aviso resuelto";
    case EV_SD_LOADED:
        return "Cargado de SD";
    case EV_SD_SAVED:
        return "Guardado en SD";
    case EV_PIN_FAIL:
        return "PIN incorrecto";
    case EV_PARAM:
        return "Parametro cambiado";
    case EV_RETAIN_RESTORED:
        return "Retentivos leidos";
    case EV_PROGRAM_ERASED:
        return "Programa borrado";
    case EV_CLOCK_SET:
        return "Reloj ajustado";
    default:
        return "?";
    }
}

static uint32_t cfgCrc(const Config &c)
{
    return crc32((const uint8_t *)&c, offsetof(Config, crc));
}

bool Store::readConfigCopy(uint32_t addr, Config &c)
{
    if (!JWPLC_FRAM.read(addr, (uint8_t *)&c, sizeof(c)))
    {
        return false;
    }
    return c.magic == CFG_MAGIC && c.crc == cfgCrc(c);
}

bool Store::begin()
{
    memset(&_cfg, 0, sizeof(_cfg));
    _cfg.magic = CFG_MAGIC;
    _cfg.startMode = START_LAST;
    _cfg.idleMinutes = 5;

    uint8_t probe = 0;
    _framOk = JWPLC_FRAM.read(ADDR_CFG1, &probe, 1);
    if (!_framOk)
    {
        return false;
    }

    Config a, b;
    const bool okA = readConfigCopy(ADDR_CFG1, a);
    const bool okB = readConfigCopy(ADDR_CFG2, b);
    if (okA && (!okB || a.seq >= b.seq))
    {
        _cfg = a;
    }
    else if (okB)
    {
        _cfg = b;
    }
    loadEvents();
    return true;
}

bool Store::saveConfig()
{
    if (!_framOk)
    {
        return false;
    }
    _cfg.magic = CFG_MAGIC;
    _cfg.seq++;
    _cfg.crc = cfgCrc(_cfg);
    // Dos copias: si se corta la energía a mitad de una, queda la otra.
    const uint32_t addr = (_cfg.seq & 1) ? ADDR_CFG1 : ADDR_CFG2;
    return framWrite(addr, (const uint8_t *)&_cfg, sizeof(_cfg));
}

static bool readImage(uint32_t addr, Program &p)
{
    ImageHeader h;
    if (!JWPLC_FRAM.read(addr, (uint8_t *)&h, sizeof(h)))
    {
        return false;
    }
    if (h.magic != IMG_MAGIC || h.version != IMG_VERSION || h.size != sizeof(Program))
    {
        return false;
    }
    if (!JWPLC_FRAM.read(addr + sizeof(h), (uint8_t *)&p, sizeof(p)))
    {
        return false;
    }
    return crc32((const uint8_t *)&p, sizeof(p)) == h.crc && p.count <= MAX_BLOCKS;
}

bool Store::loadProgram(Program &p)
{
    if (!_framOk)
    {
        return false;
    }
    const uint8_t slot = _cfg.slot & 1;
    if (readImage(ADDR_SLOT[slot], p))
    {
        return true;
    }
    // La activa está dañada: se intenta la anterior (rollback).
    return readImage(ADDR_SLOT[slot ^ 1], p);
}

bool Store::saveProgram(const Program &p)
{
    if (!_framOk)
    {
        return false;
    }
    const uint8_t slot = (uint8_t)((_cfg.slot & 1) ^ 1);
    ImageHeader h = {IMG_MAGIC, IMG_VERSION, (uint16_t)sizeof(Program),
                     crc32((const uint8_t *)&p, sizeof(p)), 0};
    if (!framWrite(ADDR_SLOT[slot] + sizeof(h), (const uint8_t *)&p, sizeof(p)) ||
        !framWrite(ADDR_SLOT[slot], (const uint8_t *)&h, sizeof(h)))
    {
        return false;
    }
    Program check;
    if (!readImage(ADDR_SLOT[slot], check))
    {
        return false;
    }
    // Recién ahora se cambia de ranura: la anterior queda como respaldo.
    _cfg.slot = slot;
    return saveConfig();
}

bool Store::loadRetained(uint32_t values[MAX_BLOCKS])
{
    if (!_framOk)
    {
        return false;
    }
    uint32_t buf[MAX_BLOCKS + 1];
    if (!JWPLC_FRAM.read(ADDR_RETAIN, (uint8_t *)buf, sizeof(buf)))
    {
        return false;
    }
    if (crc32((const uint8_t *)buf, sizeof(uint32_t) * MAX_BLOCKS) != buf[MAX_BLOCKS])
    {
        return false;
    }
    memcpy(values, buf, sizeof(uint32_t) * MAX_BLOCKS);
    return true;
}

bool Store::saveRetained(const uint32_t values[MAX_BLOCKS])
{
    if (!_framOk)
    {
        return false;
    }
    uint32_t buf[MAX_BLOCKS + 1];
    memcpy(buf, values, sizeof(uint32_t) * MAX_BLOCKS);
    buf[MAX_BLOCKS] = crc32((const uint8_t *)buf, sizeof(uint32_t) * MAX_BLOCKS);
    return framWrite(ADDR_RETAIN, (const uint8_t *)buf, sizeof(buf));
}

void Store::clearRetained()
{
    uint32_t zero[MAX_BLOCKS];
    memset(zero, 0, sizeof(zero));
    saveRetained(zero);
}

void Store::loadEvents()
{
    _evHead = 0;
    _evCount = 0;
    memset(_ev, 0, sizeof(_ev));
    uint8_t meta[2] = {0, 0};
    if (!JWPLC_FRAM.read(ADDR_EVHEAD, meta, 2) || meta[0] >= EVENT_SLOTS || meta[1] > EVENT_SLOTS)
    {
        return;
    }
    if (!JWPLC_FRAM.read(ADDR_EVENTS, (uint8_t *)_ev, sizeof(_ev)))
    {
        memset(_ev, 0, sizeof(_ev));
        return;
    }
    _evHead = meta[0];
    _evCount = meta[1];
}

void Store::addEvent(const Event &e)
{
    _ev[_evHead] = e;
    const uint8_t pos = _evHead;
    _evHead = (uint8_t)((_evHead + 1) % EVENT_SLOTS);
    if (_evCount < EVENT_SLOTS)
    {
        _evCount++;
    }
    if (_framOk)
    {
        framWrite(ADDR_EVENTS + pos * sizeof(Event), (const uint8_t *)&e, sizeof(e));
        const uint8_t meta[2] = {_evHead, _evCount};
        framWrite(ADDR_EVHEAD, meta, 2);
    }
}

bool Store::event(uint8_t i, Event &out) const
{
    if (i >= _evCount)
    {
        return false;
    }
    out = _ev[(_evHead + EVENT_SLOTS - 1 - i) % EVENT_SLOTS];
    return true;
}

void Store::clearEvents()
{
    _evHead = 0;
    _evCount = 0;
    memset(_ev, 0, sizeof(_ev));
    if (_framOk)
    {
        const uint8_t meta[2] = {0, 0};
        framWrite(ADDR_EVHEAD, meta, 2);
    }
}

// ---------------------------------------------------------------- microSD

static const char SD_DIR[] = "/jwplc";

bool Store::sdReady()
{
    return JWPLC_SD.isCardPresent() && JWPLC_SD.isReady();
}

// Nombre de archivo a partir del nombre del programa: letras, números, "_" y "-".
static void fileName(const char name[], char out[], size_t size)
{
    size_t n = 0;
    for (size_t i = 0; name[i] && n + 1 < size && n < 16; i++)
    {
        char c = name[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '-' || c == '_';
        out[n++] = ok ? c : '_';
    }
    if (n == 0 && size > 4)
    {
        strcpy(out, "prog");
        return;
    }
    out[n] = 0;
}

bool Store::sdSave(const Program &p, char path[], size_t size)
{
    if (!sdReady())
    {
        return false;
    }
    if (!JWPLC_SD.exists(SD_DIR))
    {
        JWPLC_SD.mkdir(SD_DIR);
    }
    char fn[20];
    fileName(p.name, fn, sizeof(fn));
    snprintf(path, size, "%s/%s.jwl", SD_DIR, fn);
    if (JWPLC_SD.exists(path))
    {
        JWPLC_SD.remove(path);
    }
    JWPLCFile f = JWPLC_SD.open(path, FILE_WRITE);
    if (!f)
    {
        return false;
    }
    ImageHeader h = {IMG_MAGIC, IMG_VERSION, (uint16_t)sizeof(Program),
                     crc32((const uint8_t *)&p, sizeof(p)), 0};
    const bool ok = f.write((const uint8_t *)&h, sizeof(h)) == sizeof(h) &&
                    f.write((const uint8_t *)&p, sizeof(p)) == sizeof(p);
    f.close();
    return ok;
}

uint8_t Store::sdList(char names[][NAME_LEN + 4], uint8_t max)
{
    if (!sdReady())
    {
        return 0;
    }
    JWPLCFile dir = JWPLC_SD.open(SD_DIR, FILE_READ);
    if (!dir || !dir.isDirectory())
    {
        return 0;
    }
    uint8_t n = 0;
    while (n < max)
    {
        JWPLCFile f = dir.openNextFile();
        if (!f)
        {
            break;
        }
        const char *nm = f.name();
        const char *slash = strrchr(nm, '/');
        if (slash)
        {
            nm = slash + 1;
        }
        const size_t len = strlen(nm);
        if (!f.isDirectory() && len > 4 && len < NAME_LEN + 4 && strcmp(nm + len - 4, ".jwl") == 0)
        {
            strncpy(names[n], nm, NAME_LEN + 3);
            names[n][NAME_LEN + 3] = 0;
            n++;
        }
        f.close();
    }
    dir.close();
    return n;
}

bool Store::sdLoad(const char file[], Program &p)
{
    if (!sdReady())
    {
        return false;
    }
    char path[48];
    snprintf(path, sizeof(path), "%s/%s", SD_DIR, file);
    JWPLCFile f = JWPLC_SD.open(path, FILE_READ);
    if (!f)
    {
        return false;
    }
    ImageHeader h;
    Program tmp;
    const bool ok = f.read((uint8_t *)&h, sizeof(h)) == sizeof(h) &&
                    h.magic == IMG_MAGIC && h.version == IMG_VERSION &&
                    h.size == sizeof(Program) &&
                    f.read((uint8_t *)&tmp, sizeof(tmp)) == sizeof(tmp) &&
                    crc32((const uint8_t *)&tmp, sizeof(tmp)) == h.crc &&
                    tmp.count <= MAX_BLOCKS;
    f.close();
    if (ok)
    {
        p = tmp;
    }
    return ok;
}

bool Store::sdExportEvents(char path[], size_t size)
{
    if (!sdReady())
    {
        return false;
    }
    if (!JWPLC_SD.exists(SD_DIR))
    {
        JWPLC_SD.mkdir(SD_DIR);
    }
    snprintf(path, size, "%s/eventos.csv", SD_DIR);
    if (JWPLC_SD.exists(path))
    {
        JWPLC_SD.remove(path);
    }
    JWPLCFile f = JWPLC_SD.open(path, FILE_WRITE);
    if (!f)
    {
        return false;
    }
    f.println("fecha;hora;evento;dato;segundos_desde_arranque");
    for (uint8_t i = _evCount; i > 0; i--)
    {
        Event e;
        event((uint8_t)(i - 1), e);
        char line[80];
        snprintf(line, sizeof(line), "%04u-%02u-%02u;%02u:%02u:%02u;%s;%u;%lu",
                 (unsigned)(e.year ? 2000 + e.year : 0), e.month, e.day, e.hour, e.minute, e.second,
                 eventText(e.code), e.arg, (unsigned long)e.uptimeS);
        f.println(line);
    }
    f.close();
    return true;
}

} // namespace jwrt
