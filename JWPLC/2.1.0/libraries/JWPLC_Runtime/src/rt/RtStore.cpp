#include "RtStore.h"

#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_UI_EventLog.h>
#include <JW_RTC.h>
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
static const uint16_t IMG_VERSION = 2; // v2: salidas remotas; la v1 se sigue leyendo

static const uint32_t ADDR_CFG1 = 0x0000;
static const uint32_t ADDR_CFG2 = 0x0040;
static const uint32_t ADDR_SLOT[2] = {0x0100, 0x0700};
static const uint32_t ADDR_RETAIN = 0x0D00;
static const uint32_t ADDR_LINKS = 0x1100;

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
static_assert(sizeof(XLink) == 16, "XLink debe ocupar 16 bytes");
static_assert(ADDR_LINKS + NUM_X * sizeof(XLink) + 4 <= 0x1180, "la tabla de módulos pisa al esclavo");

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
    case EV_X_ADDED:
        return "Modulo agregado";
    case EV_X_REMOVED:
        return "Modulo quitado";
    case EV_X_OFFLINE:
        return "Modulo sin respuesta";
    case EV_X_ONLINE:
        return "Modulo en linea";
    case EV_X_WRONG:
        return "Modulo distinto";
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
    startEvents();
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

// Cabecera válida: v2 completa o v1 (sin salidas remotas, que quedan abiertas).
static bool headerOk(const ImageHeader &h)
{
    return h.magic == IMG_MAGIC && ((h.version == IMG_VERSION && h.size == sizeof(Program)) ||
                                    (h.version == 1 && h.size == PROGRAM_V1_SIZE));
}

static bool bodyOk(const ImageHeader &h, Program &p)
{
    if (h.size < sizeof(Program))
    {
        memset((uint8_t *)&p + h.size, 0, sizeof(Program) - h.size);
    }
    return crc32((const uint8_t *)&p, h.size) == h.crc && p.count <= MAX_BLOCKS;
}

static bool readImage(uint32_t addr, Program &p)
{
    ImageHeader h;
    if (!JWPLC_FRAM.read(addr, (uint8_t *)&h, sizeof(h)) || !headerOk(h))
    {
        return false;
    }
    if (!JWPLC_FRAM.read(addr + sizeof(h), (uint8_t *)&p, h.size))
    {
        return false;
    }
    return bodyOk(h, p);
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

bool Store::loadLinks(XLink links[NUM_X])
{
    memset(links, 0, sizeof(XLink) * NUM_X);
    if (!_framOk)
    {
        return false;
    }
    uint8_t buf[NUM_X * sizeof(XLink) + 4];
    if (!JWPLC_FRAM.read(ADDR_LINKS, buf, sizeof(buf)))
    {
        return false;
    }
    uint32_t crc;
    memcpy(&crc, buf + NUM_X * sizeof(XLink), 4);
    if (crc != crc32(buf, NUM_X * sizeof(XLink)))
    {
        return false; // FRAM nueva o dañada: sin módulos
    }
    memcpy(links, buf, NUM_X * sizeof(XLink));
    return true;
}

bool Store::saveLinks(const XLink links[NUM_X])
{
    if (!_framOk)
    {
        return false;
    }
    uint8_t buf[NUM_X * sizeof(XLink) + 4];
    memcpy(buf, links, NUM_X * sizeof(XLink));
    const uint32_t crc = crc32(buf, NUM_X * sizeof(XLink));
    memcpy(buf + NUM_X * sizeof(XLink), &crc, 4);
    return framWrite(ADDR_LINKS, buf, sizeof(buf));
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

// --------------------------------------------------------------- eventos
static JWPLC_UIEventDescriptor g_rtDesc[EV_COUNT];

static uint8_t rtSeverity(uint8_t code)
{
    switch (code)
    {
    case EV_ALARM_ON:
        return JWPLC_EVENT_SEV_CRIT;
    case EV_PIN_FAIL:
    case EV_X_OFFLINE:
    case EV_X_WRONG:
        return JWPLC_EVENT_SEV_WARN;
    default:
        return JWPLC_EVENT_SEV_INFO;
    }
}

static uint8_t rtCategory(uint8_t code)
{
    switch (code)
    {
    case EV_ALARM_ON:
    case EV_ALARM_OFF:
        return JWPLC_EVENT_CAT_ALARM;
    case EV_RUN:
    case EV_STOP:
    case EV_PARAM:
        return JWPLC_EVENT_CAT_PROCESS;
    default:
        return JWPLC_EVENT_CAT_SYSTEM;
    }
}

void Store::startEvents()
{
    // Descriptores del Runtime (texto, severidad y categoría) para el historial común.
    for (uint8_t c = 1; c < EV_COUNT; c++)
    {
        g_rtDesc[c].code = (uint16_t)(RT_EVENT_BASE + c);
        g_rtDesc[c].label = eventText(c);
        g_rtDesc[c].severity = rtSeverity(c);
        g_rtDesc[c].category = rtCategory(c);
    }
    JWPLCUIEventLog::setExtraDescriptors(&g_rtDesc[1], EV_COUNT - 1);
    // Restaura los últimos 16 de la FRAM y anota el encendido (código 100).
    JWPLCUIEventLog::startStore();
}

void Store::addEvent(uint8_t code, uint8_t arg)
{
    if (code == EV_POWER_ON)
    {
        return; // lo anota el historial al arrancar (código 100)
    }
    const uint8_t transition = (code == EV_ALARM_OFF || code == EV_X_ONLINE) ? JWPLC_EVENT_TRANS_NORMAL
                                                                             : JWPLC_EVENT_TRANS_ACTIVE;
    JWPLCUIEventLog::logEventArg((uint16_t)(RT_EVENT_BASE + code), transition, rtCategory(code), rtSeverity(code), arg);
}

uint8_t Store::eventCount() const
{
    const size_t n = JWPLCUIEventLog::count();
    return n > 255 ? 255 : (uint8_t)n;
}

bool Store::event(uint8_t i, Event &out) const
{
    JWPLC_UIEventRecord rec;
    if (!JWPLCUIEventLog::getRecord(i, rec))
    {
        return false;
    }
    memset(&out, 0, sizeof(out));
    if (rec.code == JWPLC_EVT_SYS_BOOT)
    {
        out.code = EV_POWER_ON;
    }
    else if (rec.code > RT_EVENT_BASE && rec.code < RT_EVENT_BASE + EV_COUNT)
    {
        out.code = (uint8_t)(rec.code - RT_EVENT_BASE);
    }
    else
    {
        out.code = EV_OTHER;
        out.other = rec.code;
    }
    out.arg = rec.arg;
    JW_RTC::DateTime dt;
    if (!(rec.timestamp & 0x80000000UL) && JW_RTC::fromUnix(rec.timestamp, dt) && dt.year >= 2000)
    {
        out.year = (uint8_t)(dt.year - 2000);
        out.month = dt.month;
        out.day = dt.day;
        out.hour = dt.hour;
        out.minute = dt.minute;
        out.second = dt.second;
    }
    else
    {
        out.uptimeS = rec.timestamp & 0x7FFFFFFFUL;
    }
    return true;
}

void Store::clearEvents()
{
    JWPLCUIEventLog::clear();
}

const char *eventText(const Event &e)
{
    if (e.code != EV_OTHER)
    {
        return eventText(e.code);
    }
    const char *t = JWPLCUIEventLog::label(e.other);
    return t != nullptr ? t : "Evento";
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
    const bool ok = f.read((uint8_t *)&h, sizeof(h)) == sizeof(h) && headerOk(h) &&
                    f.read((uint8_t *)&tmp, h.size) == h.size && bodyOk(h, tmp);
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
    for (uint8_t i = eventCount(); i > 0; i--)
    {
        Event e;
        event((uint8_t)(i - 1), e);
        char line[80];
        snprintf(line, sizeof(line), "%04u-%02u-%02u;%02u:%02u:%02u;%s;%u;%lu",
                 (unsigned)(e.year ? 2000 + e.year : 0), e.month, e.day, e.hour, e.minute, e.second,
                 eventText(e), e.arg, (unsigned long)e.uptimeS);
        f.println(line);
    }
    f.close();
    return true;
}

} // namespace jwrt
