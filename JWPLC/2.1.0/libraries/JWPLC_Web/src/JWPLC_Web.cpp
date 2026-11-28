// JWPLC_Web.cpp: API del sketch, registro de variables, alarmas, tendencia,
// auditoría y usuarios. El servidor HTTP está en JWPLC_Web_Server.cpp.
#include "JWPLC_Web.h"

#include <stdlib.h>

#include <JWPLC_Red.h>
#include <Preferences.h>
#include <esp_random.h>
#include <math.h>
#include <string.h>

#include "JWPLC_Web_Internal.h"
#include "JWPLC_Web_Sha256.h"

JWPLC_WebClass JWPLC_Web;

namespace jwweb
{
State S;

Guard::Guard()
{
    if (S.lock)
    {
        xSemaphoreTake(S.lock, portMAX_DELAY);
    }
}
Guard::~Guard()
{
    if (S.lock)
    {
        xSemaphoreGive(S.lock);
    }
}

namespace
{
void copyText(char *dst, size_t size, const char *src)
{
    if (!src)
    {
        src = "";
    }
    strncpy(dst, src, size - 1);
    dst[size - 1] = 0;
}

// Un id sirve en la URL y en JSON sin escapar: letras, dígitos, '_' y '-'.
bool validId(const char *id)
{
    if (!id || !*id || strlen(id) >= ID_LEN)
    {
        return false;
    }
    for (const char *p = id; *p; p++)
    {
        const char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
        {
            return false;
        }
    }
    return true;
}

void ensureLock()
{
    if (!S.lock)
    {
        S.lock = xSemaphoreCreateMutex();
    }
}

int8_t addVar(const char *id, uint8_t type, void *ptr)
{
    ensureLock();
    Guard g;
    if (!validId(id) || S.varCount >= WEB_MAX_VARIABLES)
    {
        return -1;
    }
    for (uint8_t i = 0; i < S.varCount; i++)
    {
        if (strcmp(S.vars[i].id, id) == 0)
        {
            return -1;
        }
    }
    Var &v = S.vars[S.varCount];
    memset(&v, 0, sizeof(v));
    copyText(v.id, sizeof(v.id), id);
    copyText(v.label, sizeof(v.label), id);
    v.type = type;
    v.ptr = ptr;
    v.decimals = 255;
    v.writeRole = WEB_NADIE;
    v.trendSlot = -1;
    return (int8_t)S.varCount++;
}

int8_t findAlarm(const char *id)
{
    for (uint8_t i = 0; i < S.alarmCount; i++)
    {
        if (strcmp(S.alarms[i].id, id) == 0)
        {
            return (int8_t)i;
        }
    }
    return -1;
}

// Código de primer ingreso: 6 dígitos, cambia cada 60 s; se acepta también el
// anterior para quien lo estaba escribiendo justo al cambiar.
char g_code[7] = "";
char g_prevCode[7] = "";
uint32_t g_codeAt = 0;
} // namespace

float readVar(const Var &v)
{
    switch (v.type)
    {
    case T_BOOL:
        return *(volatile bool *)v.ptr ? 1.0f : 0.0f;
    case T_INT:
        return (float)*(volatile int *)v.ptr;
    case T_LONG:
        return (float)*(volatile long *)v.ptr;
    case T_ULONG:
        return (float)*(volatile unsigned long *)v.ptr;
    case T_FLOAT:
        return *(volatile float *)v.ptr;
    default:
        return 0;
    }
}

bool writeVar(Var &v, float value)
{
    if (isnan(value) || isinf(value))
    {
        return false;
    }
    if (v.hasRange && v.type != T_BOOL && (value < v.minV || value > v.maxV))
    {
        return false;
    }
    switch (v.type)
    {
    case T_BOOL:
        *(volatile bool *)v.ptr = value != 0;
        return true;
    case T_INT:
        *(volatile int *)v.ptr = (int)lroundf(value);
        return true;
    case T_LONG:
        *(volatile long *)v.ptr = lroundf(value);
        return true;
    case T_ULONG:
        if (value < 0)
        {
            return false;
        }
        *(volatile unsigned long *)v.ptr = (unsigned long)lroundf(value);
        return true;
    case T_FLOAT:
        *(volatile float *)v.ptr = value;
        return true;
    default:
        return false;
    }
}

void formatVar(const Var &v, char *out, size_t size)
{
    switch (v.type)
    {
    case T_BOOL:
        snprintf(out, size, "%d", *(volatile bool *)v.ptr ? 1 : 0);
        break;
    case T_INT:
        snprintf(out, size, "%d", *(volatile int *)v.ptr);
        break;
    case T_LONG:
        snprintf(out, size, "%ld", *(volatile long *)v.ptr);
        break;
    case T_ULONG:
        snprintf(out, size, "%lu", *(volatile unsigned long *)v.ptr);
        break;
    case T_FLOAT:
    {
        const float f = *(volatile float *)v.ptr;
        if (isnan(f) || isinf(f))
        {
            snprintf(out, size, "null");
        }
        else
        {
            snprintf(out, size, "%.*f", v.decimals == 255 ? 3 : v.decimals, (double)f);
        }
        break;
    }
    default:
        snprintf(out, size, "0");
    }
}

void addEvent(uint8_t alarm, uint8_t kind, const char *user)
{
    AlarmEvent &e = S.events[S.evHead];
    e.t = millis();
    e.alarm = alarm;
    e.kind = kind;
    copyText(e.user, sizeof(e.user), user);
    S.evHead = (uint8_t)((S.evHead + 1) % EV_MAX);
    if (S.evCount < EV_MAX)
    {
        S.evCount++;
    }
}

void addAudit(const char *user, const char *text)
{
    Audit &a = S.audit[S.auditHead];
    a.t = millis();
    copyText(a.user, sizeof(a.user), user);
    copyText(a.text, sizeof(a.text), text);
    S.auditHead = (uint8_t)((S.auditHead + 1) % AUDIT_MAX);
    if (S.auditCount < AUDIT_MAX)
    {
        S.auditCount++;
    }
}

void sampleTrend()
{
    if (!S.trendCount)
    {
        return;
    }
    const uint32_t now = millis();
    if (S.trendFill && now - S.trendLast < (uint32_t)S.trendSeconds * 1000u)
    {
        return;
    }
    S.trendLast = now;
    Guard g;
    for (uint8_t i = 0; i < S.trendCount; i++)
    {
        S.trend[i][S.trendHead] = readVar(S.vars[S.trendVar[i]]);
    }
    S.trendHead = (uint8_t)((S.trendHead + 1) % TREND_POINTS);
    if (S.trendFill < TREND_POINTS)
    {
        S.trendFill++;
    }
}

void randomBytes(uint8_t *out, size_t n)
{
    for (size_t i = 0; i < n; i += 4)
    {
        const uint32_t r = esp_random();
        for (size_t k = 0; k < 4 && i + k < n; k++)
        {
            out[i + k] = (uint8_t)(r >> (8 * k));
        }
    }
}

// Usuarios en NVS (namespace "jwweb"): "n" = cantidad, "u0".."u5" = User.
void loadUsers()
{
    if (S.usersLoaded)
    {
        return;
    }
    S.usersLoaded = true;
    S.userCount = 0;
    Preferences p;
    if (!p.begin("jwweb", false))
    {
        randomBytes(S.secret, sizeof(S.secret));
        return;
    }
    if (p.getBytes("sec", S.secret, sizeof(S.secret)) != sizeof(S.secret))
    {
        randomBytes(S.secret, sizeof(S.secret));
        p.putBytes("sec", S.secret, sizeof(S.secret));
    }
    const uint8_t n = p.getUChar("n", 0);
    for (uint8_t i = 0; i < n && i < WEB_MAX_USUARIOS; i++)
    {
        char key[4] = {'u', (char)('0' + i), 0, 0};
        User u;
        if (p.getBytes(key, &u, sizeof(u)) == sizeof(u) && u.name[0] && u.role >= WEB_VISOR && u.role <= WEB_ADMIN)
        {
            u.name[USER_NAME - 1] = 0;
            S.users[S.userCount++] = u;
        }
    }
    p.end();
}

bool saveUsers()
{
    Preferences p;
    if (!p.begin("jwweb", false))
    {
        return false;
    }
    bool ok = true;
    for (uint8_t i = 0; i < WEB_MAX_USUARIOS; i++)
    {
        char key[4] = {'u', (char)('0' + i), 0, 0};
        if (i < S.userCount)
        {
            ok = p.putBytes(key, &S.users[i], sizeof(User)) == sizeof(User) && ok;
        }
        else
        {
            p.remove(key);
        }
    }
    ok = p.putUChar("n", S.userCount) == 1 && ok;
    p.end();
    return ok;
}

int8_t findUser(const char *name)
{
    for (uint8_t i = 0; i < S.userCount; i++)
    {
        if (strcmp(S.users[i].name, name) == 0)
        {
            return (int8_t)i;
        }
    }
    return -1;
}

const char *currentPairingCode()
{
    if (S.userCount)
    {
        return "";
    }
    const uint32_t now = millis();
    if (!g_code[0] || now - g_codeAt >= 60000u)
    {
        memcpy(g_prevCode, g_code, sizeof(g_code));
        snprintf(g_code, sizeof(g_code), "%06lu", (unsigned long)(esp_random() % 1000000u));
        g_codeAt = now;
        if (S.running)
        {
            // También por el monitor serie, cada vez que cambia.
            Serial.printf("[JWPLC_Web] Primer ingreso: abrir el panel y escribir el codigo %s\n", g_code);
        }
    }
    return g_code;
}

bool checkPairingCode(const char *code)
{
    const char *cur = currentPairingCode();
    if (!code || strlen(code) != 6 || !cur[0])
    {
        return false;
    }
    if (strcmp(code, cur) == 0)
    {
        return true;
    }
    // El anterior vale 15 s más.
    return g_prevCode[0] && millis() - g_codeAt < 15000u && strcmp(code, g_prevCode) == 0;
}
} // namespace jwweb

using namespace jwweb;

// Para el Runtime (MENU > Red): código de primer ingreso y puerto.
extern "C" const char *jwplcWebPairingCode()
{
    return S.running ? currentPairingCode() : "";
}
extern "C" uint16_t jwplcWebScreenPort() { return S.running ? S.port : 0; }
extern "C" uint32_t jwplcWebPairingMsLeft()
{
    if (!S.running || !currentPairingCode()[0])
    {
        return 0;
    }
    const uint32_t used = millis() - g_codeAt;
    return used >= 60000u ? 0 : 60000u - used;
}

// ------------------------------------------------------------- JWPLC_WebVar

bool JWPLC_WebVar::valid() const { return index_ >= 0 && index_ < S.varCount; }

JWPLC_WebVar JWPLC_WebVar::label(const char *text)
{
    if (valid())
    {
        Guard g;
        copyText(S.vars[index_].label, LABEL_LEN, text);
    }
    return *this;
}

JWPLC_WebVar JWPLC_WebVar::unit(const char *text)
{
    if (valid())
    {
        Guard g;
        copyText(S.vars[index_].unit, UNIT_LEN, text);
    }
    return *this;
}

JWPLC_WebVar JWPLC_WebVar::group(const char *text)
{
    if (valid())
    {
        Guard g;
        copyText(S.vars[index_].group, GROUP_LEN, text);
    }
    return *this;
}

JWPLC_WebVar JWPLC_WebVar::range(float minimum, float maximum)
{
    if (valid() && minimum < maximum)
    {
        Guard g;
        S.vars[index_].minV = minimum;
        S.vars[index_].maxV = maximum;
        S.vars[index_].hasRange = true;
    }
    return *this;
}

JWPLC_WebVar JWPLC_WebVar::decimals(uint8_t digits)
{
    if (valid())
    {
        Guard g;
        S.vars[index_].decimals = digits > 6 ? 6 : digits;
    }
    return *this;
}

JWPLC_WebVar JWPLC_WebVar::writable(uint8_t role)
{
    if (valid())
    {
        Guard g;
        S.vars[index_].writeRole = (role >= WEB_VISOR && role <= WEB_ADMIN) ? (role < WEB_OPERADOR ? WEB_OPERADOR : role) : WEB_NADIE;
    }
    return *this;
}

JWPLC_WebVar JWPLC_WebVar::trend()
{
    if (valid())
    {
        Guard g;
        Var &v = S.vars[index_];
        if (v.trendSlot < 0 && S.trendCount < WEB_MAX_TENDENCIAS)
        {
            v.trendSlot = (int8_t)S.trendCount;
            S.trendVar[S.trendCount++] = (uint8_t)index_;
            S.trendFill = 0; // la historia empieza de nuevo con la nueva serie
            S.trendHead = 0;
        }
    }
    return *this;
}

// ------------------------------------------------------------- JWPLC_WebClass

bool JWPLC_WebClass::begin(const char *deviceName, uint16_t port)
{
    ensureLock();
    if (S.running)
    {
        return true;
    }
    copyText(S.name, sizeof(S.name), deviceName && *deviceName ? deviceName : "JWPLC Basic");
    S.port = port;
    loadUsers();
    if (!startServer())
    {
        return false;
    }
    S.running = true;
    currentPairingCode(); // primer código (lo muestra por el monitor serie)
    return true;
}

bool JWPLC_WebClass::begin(const String &deviceName, uint16_t port) { return begin(deviceName.c_str(), port); }
bool JWPLC_WebClass::isRunning() const { return S.running; }
uint16_t JWPLC_WebClass::port() const { return S.port; }

String JWPLC_WebClass::url() const
{
    const uint32_t ip = S.ip;
    if (!S.running || ip == 0)
    {
        return String();
    }
#ifdef JWSIM
    // JWPLC Sim con «Panel web desde el celular»: el Sim publica el puerto del
    // PLC en la red de la PC (puerto + 1000 si es menor que 1024) y deja aquí la
    // IP de la PC, para que el QR y la URL sirvan desde el celular.
    const char *lan = getenv("JWSIM_LAN_HOST");
    if (lan && lan[0])
    {
        const uint16_t p = S.port < 1024 ? (uint16_t)(S.port + 1000) : S.port;
        return "http://" + String(lan) + ":" + String(p) + "/";
    }
#endif
    String s = "http://" + IPAddress(ip).toString();
    if (S.port != 80)
    {
        s += ":" + String(S.port);
    }
    return s + "/";
}

JWPLC_WebVar JWPLC_WebClass::publish(const char *id, bool &variable) { return JWPLC_WebVar(addVar(id, T_BOOL, &variable)); }
JWPLC_WebVar JWPLC_WebClass::publish(const char *id, int &variable) { return JWPLC_WebVar(addVar(id, T_INT, &variable)); }
JWPLC_WebVar JWPLC_WebClass::publish(const char *id, long &variable) { return JWPLC_WebVar(addVar(id, T_LONG, &variable)); }
JWPLC_WebVar JWPLC_WebClass::publish(const char *id, unsigned long &variable) { return JWPLC_WebVar(addVar(id, T_ULONG, &variable)); }
JWPLC_WebVar JWPLC_WebClass::publish(const char *id, float &variable) { return JWPLC_WebVar(addVar(id, T_FLOAT, &variable)); }

bool JWPLC_WebClass::addAlarm(const char *id, const char *text, uint8_t priority)
{
    ensureLock();
    Guard g;
    if (!validId(id) || S.alarmCount >= WEB_MAX_ALARMAS || findAlarm(id) >= 0)
    {
        return false;
    }
    Alarm &a = S.alarms[S.alarmCount++];
    memset(&a, 0, sizeof(a));
    copyText(a.id, sizeof(a.id), id);
    copyText(a.text, sizeof(a.text), text && *text ? text : id);
    a.priority = priority < WEB_ALARMA_BAJA ? WEB_ALARMA_BAJA : (priority > WEB_ALARMA_ALTA ? WEB_ALARMA_ALTA : priority);
    a.acked = true;
    return true;
}

void JWPLC_WebClass::setAlarm(const char *id, bool active)
{
    if (!S.lock)
    {
        return;
    }
    // Las alarmas se declaran en setup(): buscar y comparar sin tomar el candado,
    // así el loop no espera a la tarea web mientras responde por la red. Solo un
    // cambio de estado toma el candado.
    const int8_t i = findAlarm(id);
    if (i < 0 || S.alarms[i].active == active)
    {
        return;
    }
    Guard g;
    Alarm &a = S.alarms[i];
    if (a.active == active)
    {
        return;
    }
    a.active = active;
    a.since = millis();
    if (active)
    {
        a.acked = false;
        a.ackBy[0] = 0;
    }
    addEvent((uint8_t)i, active ? EV_ON : EV_OFF, "");
}

bool JWPLC_WebClass::alarmActive(const char *id) const
{
    if (!S.lock)
    {
        return false;
    }
    Guard g;
    const int8_t i = findAlarm(id);
    return i >= 0 && S.alarms[i].active;
}

uint8_t JWPLC_WebClass::activeAlarms() const
{
    if (!S.lock)
    {
        return 0;
    }
    Guard g;
    uint8_t n = 0;
    for (uint8_t i = 0; i < S.alarmCount; i++)
    {
        n += S.alarms[i].active ? 1 : 0;
    }
    return n;
}

void JWPLC_WebClass::trendEvery(uint16_t seconds)
{
    S.trendSeconds = seconds < 1 ? 1 : (seconds > 3600 ? 3600 : seconds);
}

String JWPLC_WebClass::pairingCode()
{
    if (!S.running)
    {
        return String(); // sin panel no hay primer ingreso
    }
    loadUsers();
    return String(currentPairingCode());
}

String JWPLC_WebClass::pairingURL()
{
    const String code = pairingCode();
    const String base = url();
    if (!code.length() || !base.length())
    {
        return String();
    }
    return base + "?c=" + code;
}

bool JWPLC_WebClass::hasUsers() const
{
    loadUsers();
    return S.userCount > 0;
}

void JWPLC_WebClass::resetUsers()
{
    ensureLock();
    Guard g;
    loadUsers();
    S.userCount = 0;
    saveUsers();
    dropSessions();
    addAudit("equipo", "usuarios borrados desde el equipo");
}

void JWPLC_WebClass::allowKeys(bool allow)
{
    S.keysAllowed = allow;
}

bool JWPLC_WebClass::keysAllowed() const { return S.keysAllowed; }
bool JWPLC_WebClass::viewerConnected() const { return screenViewed(); }
uint8_t JWPLC_WebClass::sessions() const { return openSessions(); }
