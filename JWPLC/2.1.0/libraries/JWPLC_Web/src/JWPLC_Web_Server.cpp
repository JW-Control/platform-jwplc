// JWPLC_Web_Server.cpp: servidor HTTP propio sobre JWPLC_RedServer (cable, WiFi o los dos).
//
// Una tarea (núcleo 0, prioridad baja) atiende una conexión a la vez, un pedido por
// conexión (Connection: close), como hacía JWPLC_WebScreen. Cada operación del W5500
// toma el bus SPI compartido por su cuenta (JWPLC_RedServer / JWPLC_Ethernet); todo lo
// que se responde está en RAM, así que armar la respuesta no toca el bus.
//
// Seguridad:
// - Sesión en cookie HttpOnly + SameSite=Strict que vence a los 30 min sin uso.
// - Todo POST exige la cabecera "X-JW: 1": un sitio ajeno no puede mandarla sin
//   permiso CORS (que este servidor nunca da), así que no hay CSRF.
// - Ingreso por reto: GET /api/reto da sal + reto de un solo uso; la página manda
//   HMAC(clave derivada, reto). La contraseña no viaja por la red.
// - 5 intentos fallidos seguidos bloquean el ingreso 5 minutos.
#include <JWPLC_Red.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_TFT_Mirror.h>
#include <esp_system.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "JWPLC_Web_Internal.h"
#include "JWPLC_Web_Page.h"
#include "JWPLC_Web_Sha256.h"
#include "freertos/task.h"

// La TFT arranca con la copia en RAM cuando esta librería está enlazada (página Pantalla).
extern "C" bool jwplcTFTMirrorWanted() { return true; }

namespace jwweb
{
namespace
{
const uint32_t REQUEST_TIMEOUT_MS = 800;
const uint32_t SESSION_IDLE_MS = 30UL * 60UL * 1000UL;
const uint8_t MAX_SESSIONS = 4;
const uint8_t MAX_FAILS = 5;
const uint32_t LOCK_MS = 5UL * 60UL * 1000UL;
const uint32_t CHALLENGE_MS = 60000;
const size_t MAX_BODY = 640;

// Pantalla en vivo (antes JWPLC_WebScreen).
const uint32_t VIEWER_WINDOW_MS = 3000;
const uint32_t KEY_AUTO_RELEASE_MS = 4000;
const uint32_t LONG_POLL_MS = 400;
const uint32_t LONG_POLL_STEP_MS = 3;
const uint32_t SETTLE_MS = 8;
const uint32_t SETTLE_MAX_MS = 150;
// Trozos de ~1 KB: caben dos en el búfer de envío del W5500 (2 KB) y Windows
// responde el ACK al instante (ver JWPLC_WebScreen, alpha.16).
const size_t OUT_CHUNK = 1000;

JWPLC_RedServer *g_server = nullptr;
uint8_t g_net = RED_NONE; // red por la que llegó el pedido en curso
volatile uint32_t g_lastViewMs = 0;
volatile bool g_viewed = false;
uint32_t g_keyDownAt[BTN_COUNT];
bool g_keyDown[BTN_COUNT];

// ------------------------------------------------------------- salida

uint8_t g_out[OUT_CHUNK + 700];
size_t g_outLen = 0;
char g_hdr[640];
size_t g_hdrLen = 0;
uint8_t g_tx[sizeof(g_hdr) + OUT_CHUNK + 16];
JWPLC_RedClient *g_cur = nullptr;

size_t pieceSize() { return OUT_CHUNK > g_hdrLen ? OUT_CHUNK - g_hdrLen : 64; }

void emit(const uint8_t *data, size_t n)
{
    size_t t = 0;
    if (g_hdrLen)
    {
        memcpy(g_tx, g_hdr, g_hdrLen);
        t = g_hdrLen;
        g_hdrLen = 0;
    }
    if (n)
    {
        memcpy(g_tx + t, data, n);
        t += n;
    }
    if (t && g_cur)
    {
        g_cur->write(g_tx, t);
    }
}

void flushChunks()
{
    size_t sent = 0;
    for (;;)
    {
        const size_t piece = pieceSize();
        if (g_outLen - sent < piece)
        {
            break;
        }
        emit(g_out + sent, piece);
        sent += piece;
    }
    if (sent)
    {
        memmove(g_out, g_out + sent, g_outLen - sent);
        g_outLen -= sent;
    }
}

void flushOut()
{
    flushChunks();
    emit(g_out, g_outLen);
    g_outLen = 0;
}

void put(const void *data, size_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    while (n)
    {
        size_t room = OUT_CHUNK - (g_outLen < OUT_CHUNK ? g_outLen : OUT_CHUNK);
        if (!room)
        {
            flushChunks();
            room = OUT_CHUNK - g_outLen;
        }
        const size_t take = n < room ? n : room;
        memcpy(g_out + g_outLen, p, take);
        g_outLen += take;
        p += take;
        n -= take;
    }
}
void puts_(const char *s) { put(s, strlen(s)); }
void putf(const char *fmt, ...)
{
    char tmp[160];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0)
    {
        put(tmp, (size_t)n < sizeof(tmp) ? (size_t)n : sizeof(tmp) - 1);
    }
}
// Cadena JSON con comillas y escapes.
void putStr(const char *s)
{
    put("\"", 1);
    for (const char *p = s; *p; p++)
    {
        const unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\')
        {
            const char e[2] = {'\\', (char)c};
            put(e, 2);
        }
        else if (c < 0x20)
        {
            putf("\\u%04x", c);
        }
        else
        {
            put(p, 1);
        }
    }
    put("\"", 1);
}
void put8(uint8_t v) { g_out[g_outLen++] = v; }
void put16(uint16_t v)
{
    put8((uint8_t)v);
    put8((uint8_t)(v >> 8));
}
void put32(uint32_t v)
{
    put16((uint16_t)v);
    put16((uint16_t)(v >> 16));
}

const char *SECURITY_HEADERS = "X-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer\r\n";

// Cabecera; sale junto con el primer trozo. Sin largo: el cierre marca el fin.
void beginResponse(const char *status, const char *type, long length = -1, const char *extra = "")
{
    g_outLen = 0;
    char len[32] = "";
    if (length >= 0)
    {
        snprintf(len, sizeof(len), "Content-Length: %ld\r\n", length);
    }
    const int n = snprintf(g_hdr, sizeof(g_hdr), "HTTP/1.1 %s\r\nContent-Type: %s\r\n%s%s%sConnection: close\r\n\r\n",
                           status, type, len, SECURITY_HEADERS, extra);
    g_hdrLen = n > 0 ? ((size_t)n < sizeof(g_hdr) ? (size_t)n : sizeof(g_hdr) - 1) : 0;
}

void sendText(const char *status, const char *text = "")
{
    const size_t n = strlen(text);
    beginResponse(status, "text/plain; charset=utf-8", (long)n, "Cache-Control: no-store\r\n");
    put(text, n);
    flushOut();
}

void beginJson(const char *extra = "")
{
    char e[200];
    snprintf(e, sizeof(e), "Cache-Control: no-store\r\n%s", extra);
    beginResponse("200 OK", "application/json; charset=utf-8", -1, e);
}

void sendOk() { beginJson(), puts_("{\"ok\":true}"), flushOut(); }
void sendError(const char *status, const char *message)
{
    beginResponse(status, "application/json; charset=utf-8", -1, "Cache-Control: no-store\r\n");
    puts_("{\"ok\":false,\"error\":");
    putStr(message);
    puts_("}");
    flushOut();
}

// ------------------------------------------------------------- pedido

struct Request
{
    bool post = false;
    char path[48];
    char query[160];
    char cookie[40];  // valor de jw=
    char inm[24];     // If-None-Match
    bool csrfOk = false;
    bool gzipOk = false;
    char body[MAX_BODY + 1];
    size_t bodyLen = 0;
};
Request g_req;

int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// Campo de un formulario urlencoded ("a=1&b=2"). false si no está.
bool param(const char *src, const char *name, char *out, size_t size)
{
    if (!src || !size)
    {
        return false;
    }
    const size_t n = strlen(name);
    const char *p = src;
    while (*p)
    {
        if (strncmp(p, name, n) == 0 && p[n] == '=')
        {
            p += n + 1;
            size_t o = 0;
            while (*p && *p != '&' && o < size - 1)
            {
                char c = *p++;
                if (c == '+')
                {
                    c = ' ';
                }
                else if (c == '%' && hexValue(p[0]) >= 0 && hexValue(p[1]) >= 0)
                {
                    c = (char)((hexValue(p[0]) << 4) | hexValue(p[1]));
                    p += 2;
                }
                out[o++] = c;
            }
            out[o] = 0;
            return true;
        }
        p = strchr(p, '&');
        if (!p)
        {
            break;
        }
        p++;
    }
    out[0] = 0;
    return false;
}

uint32_t argValue(const char *query, const char *name, uint32_t fallback)
{
    char v[16];
    return param(query, name, v, sizeof(v)) && v[0] ? (uint32_t)strtoul(v, nullptr, 10) : fallback;
}

// ------------------------------------------------------------- sesiones

struct Session
{
    bool used;
    uint8_t token[16];
    char user[USER_NAME];
    uint8_t role;
    uint32_t lastSeen;
};
Session g_sessions[MAX_SESSIONS];

struct Challenge
{
    bool used;
    char user[USER_NAME];
    uint8_t nonce[16];
    uint32_t t;
};
Challenge g_challenges[4];

uint8_t g_fails = 0;
uint32_t g_lockedAt = 0;
bool g_locked = false;

bool lockedOut()
{
    if (g_locked && millis() - g_lockedAt >= LOCK_MS)
    {
        g_locked = false;
        g_fails = 0;
    }
    return g_locked;
}

void noteFail(const char *who)
{
    if (++g_fails >= MAX_FAILS)
    {
        g_locked = true;
        g_lockedAt = millis();
        Guard g;
        addAudit(who, "ingreso bloqueado 5 min por 5 intentos fallidos");
    }
}

Session *currentSession()
{
    uint8_t tok[16];
    if (!fromHex(g_req.cookie, tok, sizeof(tok)))
    {
        return nullptr;
    }
    const uint32_t now = millis();
    for (Session &s : g_sessions)
    {
        if (!s.used)
        {
            continue;
        }
        if (now - s.lastSeen > SESSION_IDLE_MS)
        {
            s.used = false;
            continue;
        }
        if (sameBytes(s.token, tok, sizeof(tok)))
        {
            s.lastSeen = now;
            return &s;
        }
    }
    return nullptr;
}

// Crea la sesión y deja lista la cabecera Set-Cookie en cookieHdr.
void openSession(const char *user, uint8_t role, char *cookieHdr, size_t size)
{
    Session *slot = nullptr;
    for (Session &s : g_sessions)
    {
        if (!s.used || millis() - s.lastSeen > SESSION_IDLE_MS)
        {
            slot = &s;
            break;
        }
    }
    if (!slot)
    {
        slot = &g_sessions[0];
        for (Session &s : g_sessions)
        {
            if (s.lastSeen < slot->lastSeen)
            {
                slot = &s;
            }
        }
    }
    slot->used = true;
    randomBytes(slot->token, sizeof(slot->token));
    strncpy(slot->user, user, USER_NAME - 1);
    slot->user[USER_NAME - 1] = 0;
    slot->role = role;
    slot->lastSeen = millis();
    char hex[33];
    toHex(slot->token, sizeof(slot->token), hex);
    snprintf(cookieHdr, size, "Set-Cookie: jw=%s; Path=/; HttpOnly; SameSite=Strict\r\n", hex);
}

const char *roleName(uint8_t r)
{
    switch (r)
    {
    case WEB_VISOR:
        return "visor";
    case WEB_OPERADOR:
        return "operador";
    case WEB_TECNICO:
        return "tecnico";
    case WEB_ADMIN:
        return "admin";
    default:
        return "";
    }
}

uint8_t roleFromName(const char *s)
{
    if (strcmp(s, "visor") == 0)
        return WEB_VISOR;
    if (strcmp(s, "operador") == 0)
        return WEB_OPERADOR;
    if (strcmp(s, "tecnico") == 0)
        return WEB_TECNICO;
    if (strcmp(s, "admin") == 0)
        return WEB_ADMIN;
    return 0;
}

bool validUserName(const char *s)
{
    const size_t n = strlen(s);
    if (n < 2 || n >= USER_NAME)
    {
        return false;
    }
    for (const char *p = s; *p; p++)
    {
        const char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-'))
        {
            return false;
        }
    }
    return true;
}

// Sal + clave derivada que manda la página al crear un usuario o cambiar la clave.
bool readCredentials(User &u)
{
    char salt[40], key[72];
    param(g_req.body, "s", salt, sizeof(salt));
    param(g_req.body, "k", key, sizeof(key));
    return fromHex(salt, u.salt, sizeof(u.salt)) && fromHex(key, u.key, sizeof(u.key));
}

// ------------------------------------------------------------- página

const char *pageTag()
{
    static char tag[12] = {0};
    if (!tag[0])
    {
        uint32_t h = 2166136261u;
        for (size_t i = 0; i < sizeof(JWPLC_WEB_PAGE_GZ); i++)
        {
            h = (h ^ JWPLC_WEB_PAGE_GZ[i]) * 16777619u;
        }
        snprintf(tag, sizeof(tag), "\"%08lx\"", (unsigned long)h);
    }
    return tag;
}

void sendPage()
{
    char extra[360];
    // CSP: solo recursos propios; la página no carga nada de afuera.
    snprintf(extra, sizeof(extra),
             "Cache-Control: no-cache\r\nETag: %s\r\nContent-Encoding: gzip\r\n"
             "Content-Security-Policy: default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; img-src 'self' data:; base-uri 'none'; form-action 'self'; frame-ancestors 'none'\r\n",
             pageTag());
    if (g_req.inm[0] && strstr(g_req.inm, pageTag()))
    {
        beginResponse("304 Not Modified", "text/html; charset=utf-8", 0, extra);
        flushOut();
        return;
    }
    const size_t len = sizeof(JWPLC_WEB_PAGE_GZ);
    beginResponse("200 OK", "text/html; charset=utf-8", (long)len, extra);
    put(JWPLC_WEB_PAGE_GZ, len);
    flushOut();
}

// ------------------------------------------------------------- API

void apiState(Session *s)
{
    beginJson();
    puts_("{\"nombre\":");
    putStr(S.name);
    putf(",\"reclamado\":%s,\"usuario\":", S.userCount ? "true" : "false");
    putStr(s ? s->user : "");
    putf(",\"rol\":\"%s\",\"bloqueado\":%s,\"t\":%lu}", s ? roleName(s->role) : "", lockedOut() ? "true" : "false",
         (unsigned long)millis());
    flushOut();
}

void apiClaim()
{
    if (S.userCount)
    {
        sendError("409 Conflict", "el equipo ya tiene usuarios");
        return;
    }
    if (lockedOut())
    {
        sendError("429 Too Many Requests", "demasiados intentos; espera 5 minutos");
        return;
    }
    char code[8], name[USER_NAME + 4];
    param(g_req.body, "c", code, sizeof(code));
    param(g_req.body, "u", name, sizeof(name));
    if (!checkPairingCode(code))
    {
        noteFail("primer ingreso");
        sendError("403 Forbidden", "código incorrecto o vencido");
        return;
    }
    User u;
    memset(&u, 0, sizeof(u));
    if (!validUserName(name) || !readCredentials(u))
    {
        sendError("400 Bad Request", "usuario o clave no válidos");
        return;
    }
    strncpy(u.name, name, USER_NAME - 1);
    u.role = WEB_ADMIN;
    {
        Guard g;
        S.users[0] = u;
        S.userCount = 1;
        if (!saveUsers())
        {
            S.userCount = 0;
            sendError("500 Internal Server Error", "no se pudo guardar el usuario");
            return;
        }
        addAudit(u.name, "reclamó el equipo (primer ingreso)");
    }
    g_fails = 0;
    char cookie[120];
    openSession(u.name, u.role, cookie, sizeof(cookie));
    beginJson(cookie);
    puts_("{\"ok\":true}");
    flushOut();
}

void apiChallenge()
{
    char name[USER_NAME + 4];
    param(g_req.query, "u", name, sizeof(name));
    const int8_t ui = findUser(name);
    uint8_t salt[16];
    if (ui >= 0)
    {
        memcpy(salt, S.users[ui].salt, sizeof(salt));
    }
    else
    {
        // Sal falsa pero estable: no revela qué usuarios existen.
        uint8_t h[32];
        hmacSha256(S.secret, sizeof(S.secret), name, strlen(name), h);
        memcpy(salt, h, sizeof(salt));
    }
    Challenge *slot = &g_challenges[0];
    for (Challenge &c : g_challenges)
    {
        if (!c.used || millis() - c.t > CHALLENGE_MS)
        {
            slot = &c;
            break;
        }
        if (c.t < slot->t)
        {
            slot = &c;
        }
    }
    slot->used = true;
    strncpy(slot->user, name, USER_NAME - 1);
    slot->user[USER_NAME - 1] = 0;
    randomBytes(slot->nonce, sizeof(slot->nonce));
    slot->t = millis();
    char saltHex[33], nonceHex[33];
    toHex(salt, sizeof(salt), saltHex);
    toHex(slot->nonce, sizeof(slot->nonce), nonceHex);
    beginJson();
    putf("{\"sal\":\"%s\",\"iter\":%lu,\"reto\":\"%s\"}", saltHex, (unsigned long)PBKDF2_ITER, nonceHex);
    flushOut();
}

void apiLogin()
{
    if (lockedOut())
    {
        sendError("429 Too Many Requests", "demasiados intentos; espera 5 minutos");
        return;
    }
    char name[USER_NAME + 4], nonceHex[40], proofHex[72];
    param(g_req.body, "u", name, sizeof(name));
    param(g_req.body, "r", nonceHex, sizeof(nonceHex));
    param(g_req.body, "p", proofHex, sizeof(proofHex));
    uint8_t nonce[16], proof[32];
    Challenge *ch = nullptr;
    if (fromHex(nonceHex, nonce, sizeof(nonce)))
    {
        for (Challenge &c : g_challenges)
        {
            if (c.used && millis() - c.t <= CHALLENGE_MS && strcmp(c.user, name) == 0 &&
                sameBytes(c.nonce, nonce, sizeof(nonce)))
            {
                ch = &c;
                break;
            }
        }
    }
    if (ch)
    {
        ch->used = false; // un solo uso
    }
    const int8_t ui = findUser(name);
    bool ok = false;
    if (ch && ui >= 0 && fromHex(proofHex, proof, sizeof(proof)))
    {
        uint8_t expect[32];
        hmacSha256(S.users[ui].key, sizeof(S.users[ui].key), nonce, sizeof(nonce), expect);
        ok = sameBytes(expect, proof, sizeof(expect));
    }
    if (!ok)
    {
        noteFail(name[0] ? name : "?");
        {
            Guard g;
            addAudit(name[0] ? name : "?", "ingreso fallido");
        }
        sendError("403 Forbidden", lockedOut() ? "demasiados intentos; espera 5 minutos" : "usuario o clave incorrectos");
        return;
    }
    g_fails = 0;
    {
        Guard g;
        addAudit(S.users[ui].name, "ingresó");
    }
    char cookie[120];
    openSession(S.users[ui].name, S.users[ui].role, cookie, sizeof(cookie));
    beginJson(cookie);
    putf("{\"ok\":true,\"rol\":\"%s\"}", roleName(S.users[ui].role));
    flushOut();
}

void apiLogout(Session *s)
{
    if (s)
    {
        s->used = false;
    }
    beginJson("Set-Cookie: jw=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict\r\n");
    puts_("{\"ok\":true}");
    flushOut();
}

void apiDefs(Session *s)
{
    beginJson();
    puts_("{\"nombre\":");
    putStr(S.name);
    putf(",\"rol\":\"%s\",\"pantalla\":%s,\"teclas\":%s,\"tendenciaSeg\":%u,\"vars\":[", roleName(s->role),
         jwplcMirrorActive() ? "true" : "false", S.keysAllowed ? "true" : "false", (unsigned)S.trendSeconds);
    Guard g;
    for (uint8_t i = 0; i < S.varCount; i++)
    {
        const Var &v = S.vars[i];
        if (i)
        {
            puts_(",");
        }
        puts_("{\"id\":");
        putStr(v.id);
        puts_(",\"nombre\":");
        putStr(v.label);
        puts_(",\"unidad\":");
        putStr(v.unit);
        puts_(",\"grupo\":");
        putStr(v.group);
        putf(",\"tipo\":\"%s\"", v.type == T_BOOL ? "bool" : (v.type == T_FLOAT ? "real" : "entero"));
        if (v.hasRange)
        {
            putf(",\"min\":%g,\"max\":%g", (double)v.minV, (double)v.maxV);
        }
        if (v.decimals != 255)
        {
            putf(",\"dec\":%u", (unsigned)v.decimals);
        }
        putf(",\"escribe\":\"%s\",\"tendencia\":%s}", roleName(v.writeRole), v.trendSlot >= 0 ? "true" : "false");
    }
    puts_("],\"alarmas\":[");
    for (uint8_t i = 0; i < S.alarmCount; i++)
    {
        if (i)
        {
            puts_(",");
        }
        puts_("{\"id\":");
        putStr(S.alarms[i].id);
        puts_(",\"texto\":");
        putStr(S.alarms[i].text);
        putf(",\"prioridad\":%u}", (unsigned)S.alarms[i].priority);
    }
    puts_("]}");
    flushOut();
}

void apiLive()
{
    beginJson();
    putf("{\"t\":%lu,\"v\":{", (unsigned long)millis());
    Guard g;
    char val[24];
    for (uint8_t i = 0; i < S.varCount; i++)
    {
        formatVar(S.vars[i], val, sizeof(val));
        putf("%s\"%s\":%s", i ? "," : "", S.vars[i].id, val);
    }
    puts_("},\"a\":{");
    for (uint8_t i = 0; i < S.alarmCount; i++)
    {
        const Alarm &a = S.alarms[i];
        putf("%s\"%s\":{\"activa\":%s,\"reconocida\":%s,\"desde\":%lu,\"por\":", i ? "," : "", a.id,
             a.active ? "true" : "false", a.acked ? "true" : "false", (unsigned long)a.since);
        putStr(a.ackBy);
        puts_("}");
    }
    puts_("}}");
    flushOut();
}

void apiWrite(Session *s)
{
    char id[ID_LEN + 4], val[24];
    param(g_req.body, "id", id, sizeof(id));
    param(g_req.body, "v", val, sizeof(val));
    char *end = nullptr;
    const float f = strtof(val, &end);
    if (!val[0] || !end || *end)
    {
        sendError("400 Bad Request", "valor no numérico");
        return;
    }
    Guard g;
    for (uint8_t i = 0; i < S.varCount; i++)
    {
        Var &v = S.vars[i];
        if (strcmp(v.id, id) != 0)
        {
            continue;
        }
        if (v.writeRole == WEB_NADIE || s->role < v.writeRole)
        {
            sendError("403 Forbidden", "tu rol no puede cambiar esta variable");
            return;
        }
        char before[24];
        formatVar(v, before, sizeof(before));
        if (!writeVar(v, f))
        {
            sendError("400 Bad Request", "valor fuera de rango");
            return;
        }
        char after[24], text[56];
        formatVar(v, after, sizeof(after));
        snprintf(text, sizeof(text), "%s: %s -> %s", v.id, before, after);
        addAudit(s->user, text);
        sendOk();
        return;
    }
    sendError("404 Not Found", "variable desconocida");
}

void apiAck(Session *s)
{
    char id[ID_LEN + 4];
    param(g_req.body, "id", id, sizeof(id));
    Guard g;
    for (uint8_t i = 0; i < S.alarmCount; i++)
    {
        Alarm &a = S.alarms[i];
        if (strcmp(a.id, id) != 0)
        {
            continue;
        }
        if (!a.acked)
        {
            a.acked = true;
            strncpy(a.ackBy, s->user, ID_LEN - 1);
            a.ackBy[ID_LEN - 1] = 0;
            addEvent(i, EV_ACK, s->user);
        }
        sendOk();
        return;
    }
    sendError("404 Not Found", "alarma desconocida");
}

void apiEvents()
{
    beginJson();
    putf("{\"t\":%lu,\"eventos\":[", (unsigned long)millis());
    Guard g;
    for (uint8_t k = 0; k < S.evCount; k++)
    {
        const AlarmEvent &e = S.events[(S.evHead + EV_MAX - 1 - k) % EV_MAX];
        putf("%s{\"t\":%lu,\"alarma\":\"%s\",\"tipo\":\"%s\",\"por\":", k ? "," : "", (unsigned long)e.t,
             e.alarm < S.alarmCount ? S.alarms[e.alarm].id : "",
             e.kind == EV_ON ? "activa" : (e.kind == EV_OFF ? "normal" : "reconocida"));
        putStr(e.user);
        puts_("}");
    }
    puts_("]}");
    flushOut();
}

void apiTrend(bool csv)
{
    Guard g;
    const uint8_t n = S.trendFill;
    const uint8_t start = (uint8_t)((S.trendHead + TREND_POINTS - n) % TREND_POINTS);
    const uint32_t lastT = S.trendLast;
    if (csv)
    {
        beginResponse("200 OK", "text/csv; charset=utf-8", -1,
                      "Cache-Control: no-store\r\nContent-Disposition: attachment; filename=\"tendencia.csv\"\r\n");
        puts_("segundos_atras");
        for (uint8_t i = 0; i < S.trendCount; i++)
        {
            putf(";%s", S.vars[S.trendVar[i]].id);
        }
        puts_("\r\n");
        for (uint8_t k = 0; k < n; k++)
        {
            putf("%lu", (unsigned long)((n - 1 - k) * (uint32_t)S.trendSeconds));
            for (uint8_t i = 0; i < S.trendCount; i++)
            {
                putf(";%.3f", (double)S.trend[i][(start + k) % TREND_POINTS]);
            }
            puts_("\r\n");
        }
        flushOut();
        return;
    }
    beginJson();
    putf("{\"t\":%lu,\"ultimo\":%lu,\"paso\":%u,\"series\":{", (unsigned long)millis(), (unsigned long)lastT,
         (unsigned)S.trendSeconds);
    for (uint8_t i = 0; i < S.trendCount; i++)
    {
        putf("%s\"%s\":[", i ? "," : "", S.vars[S.trendVar[i]].id);
        for (uint8_t k = 0; k < n; k++)
        {
            const float f = S.trend[i][(start + k) % TREND_POINTS];
            putf(k ? ",%.3f" : "%.3f", (double)f);
        }
        puts_("]");
    }
    puts_("}}");
    flushOut();
}

void apiDevice(Session *s)
{
    beginJson();
    puts_("{\"nombre\":");
    putStr(S.name);
    putf(",\"ip\":\"%s\",\"red\":\"%s\",\"t\":%lu,\"heap\":%lu,\"heapMin\":%lu,\"sesiones\":%u,\"usuarios\":%u",
         IPAddress(S.ip).toString().c_str(), g_net == RED_WIFI ? "WiFi" : "Ethernet", (unsigned long)millis(), (unsigned long)ESP.getFreeHeap(),
         (unsigned long)ESP.getMinFreeHeap(), (unsigned)openSessions(), (unsigned)S.userCount);
    putf(",\"variables\":%u,\"alarmas\":%u,\"reinicio\":%d,\"pantalla\":%s,\"version\":\"0.1.0\"",
         (unsigned)S.varCount, (unsigned)S.alarmCount, (int)esp_reset_reason(), jwplcMirrorActive() ? "true" : "false");
    if (s->role >= WEB_TECNICO)
    {
        puts_(",\"auditoria\":[");
        Guard g;
        for (uint8_t k = 0; k < S.auditCount; k++)
        {
            const Audit &a = S.audit[(S.auditHead + AUDIT_MAX - 1 - k) % AUDIT_MAX];
            putf("%s{\"t\":%lu,\"por\":", k ? "," : "", (unsigned long)a.t);
            putStr(a.user);
            puts_(",\"texto\":");
            putStr(a.text);
            puts_("}");
        }
        puts_("]");
    }
    puts_("}");
    flushOut();
}

void apiUsers()
{
    beginJson();
    puts_("{\"usuarios\":[");
    Guard g;
    for (uint8_t i = 0; i < S.userCount; i++)
    {
        putf("%s{\"nombre\":", i ? "," : "");
        putStr(S.users[i].name);
        putf(",\"rol\":\"%s\"}", roleName(S.users[i].role));
    }
    putf("],\"max\":%u}", (unsigned)WEB_MAX_USUARIOS);
    flushOut();
}

void apiUserCreate(Session *s)
{
    char name[USER_NAME + 4], role[12];
    param(g_req.body, "u", name, sizeof(name));
    param(g_req.body, "rol", role, sizeof(role));
    User u;
    memset(&u, 0, sizeof(u));
    const uint8_t r = roleFromName(role);
    if (!validUserName(name) || !r || !readCredentials(u))
    {
        sendError("400 Bad Request", "datos no válidos (usuario: minúsculas, dígitos, . _ -)");
        return;
    }
    Guard g;
    if (findUser(name) >= 0)
    {
        sendError("409 Conflict", "ese usuario ya existe");
        return;
    }
    if (S.userCount >= WEB_MAX_USUARIOS)
    {
        sendError("409 Conflict", "no caben más usuarios");
        return;
    }
    strncpy(u.name, name, USER_NAME - 1);
    u.role = r;
    S.users[S.userCount++] = u;
    if (!saveUsers())
    {
        S.userCount--;
        sendError("500 Internal Server Error", "no se pudo guardar");
        return;
    }
    char text[56];
    snprintf(text, sizeof(text), "creó el usuario %s (%s)", u.name, roleName(r));
    addAudit(s->user, text);
    sendOk();
}

void apiUserDelete(Session *s)
{
    char name[USER_NAME + 4];
    param(g_req.body, "u", name, sizeof(name));
    Guard g;
    const int8_t ui = findUser(name);
    if (ui < 0)
    {
        sendError("404 Not Found", "usuario desconocido");
        return;
    }
    if (strcmp(name, s->user) == 0)
    {
        sendError("409 Conflict", "no puedes borrarte a ti mismo");
        return;
    }
    for (uint8_t i = (uint8_t)ui; i + 1 < S.userCount; i++)
    {
        S.users[i] = S.users[i + 1];
    }
    S.userCount--;
    saveUsers();
    // Cierra las sesiones de ese usuario.
    for (Session &x : g_sessions)
    {
        if (x.used && strcmp(x.user, name) == 0)
        {
            x.used = false;
        }
    }
    char text[56];
    snprintf(text, sizeof(text), "borró el usuario %s", name);
    addAudit(s->user, text);
    sendOk();
}

void apiPassword(Session *s)
{
    Guard g;
    const int8_t ui = findUser(s->user);
    User u = ui >= 0 ? S.users[ui] : User();
    if (ui < 0 || !readCredentials(u))
    {
        sendError("400 Bad Request", "datos no válidos");
        return;
    }
    S.users[ui] = u;
    saveUsers();
    // Las otras sesiones del mismo usuario se cierran.
    for (Session &x : g_sessions)
    {
        if (x.used && &x != s && strcmp(x.user, s->user) == 0)
        {
            x.used = false;
        }
    }
    addAudit(s->user, "cambió su clave");
    sendOk();
}

// ------------------------------------------------------------- pantalla

void applyKey(uint32_t b, bool down)
{
    if (!S.keysAllowed || b >= BTN_COUNT)
    {
        return;
    }
    g_keyDown[b] = down;
    g_keyDownAt[b] = millis();
    JWPLC_Buttons.setVirtualDown((uint8_t)b, down);
}

void releaseStaleKeys()
{
    const uint32_t now = millis();
    for (uint8_t b = 0; b < BTN_COUNT; b++)
    {
        if (g_keyDown[b] && (!S.keysAllowed || now - g_keyDownAt[b] > KEY_AUTO_RELEASE_MS))
        {
            g_keyDown[b] = false;
            JWPLC_Buttons.setVirtualDown(b, false);
        }
    }
}

bool changedSince(uint32_t since, uint16_t knownColors)
{
    if (jwplcMirrorPaletteCount() != knownColors)
    {
        return true;
    }
    const uint16_t h = jwplcMirrorHeight();
    for (uint16_t y = 0; y < h; y++)
    {
        if (jwplcMirrorRowVersion(y) >= since)
        {
            return true;
        }
    }
    return false;
}

// Cuadro "JWS2" (mismo formato que JWPLC_WebScreen en alpha.16).
void sendFrame(Session *s, JWPLC_RedClient &c)
{
    const char *query = g_req.query;
    g_lastViewMs = millis();
    g_viewed = true;
    const uint32_t ev = argValue(query, "e", 0xFFFFFFFFu);
    const bool hasEvent = ev != 0xFFFFFFFFu;
    if (hasEvent && s->role >= WEB_OPERADOR)
    {
        applyKey(ev / 10, (ev % 10) != 0);
    }
    if (!jwplcMirrorActive())
    {
        sendText("503 Service Unavailable", "sin memoria para la pantalla");
        return;
    }
    const uint32_t since = argValue(query, "v", 0);
    const uint16_t known = (uint16_t)argValue(query, "p", 0);
    if (since != 0 && !hasEvent && argValue(query, "w", 0) != 0)
    {
        const uint32_t t0 = millis();
        while (!changedSince(since, known) && millis() - t0 < LONG_POLL_MS)
        {
            vTaskDelay(pdMS_TO_TICKS(LONG_POLL_STEP_MS));
            if (!c.connected())
            {
                return;
            }
        }
    }
    {
        const uint32_t t0 = millis();
        uint32_t seen = jwplcMirrorWrites();
        uint32_t quietSince = t0;
        while (millis() - t0 < SETTLE_MAX_MS && millis() - quietSince < SETTLE_MS)
        {
            vTaskDelay(pdMS_TO_TICKS(LONG_POLL_STEP_MS));
            const uint32_t now = jwplcMirrorWrites();
            if (now != seen)
            {
                seen = now;
                quietSince = millis();
            }
        }
    }
    const uint32_t snap = jwplcMirrorSnapshot();
    const uint16_t w = jwplcMirrorWidth();
    const uint16_t h = jwplcMirrorHeight();
    const uint16_t palN = jwplcMirrorPaletteCount();
    const uint16_t from = known <= palN ? known : 0;

    beginResponse("200 OK", "application/octet-stream", -1, "Cache-Control: no-store\r\n");
    put8('J');
    put8('W');
    put8('S');
    put8('2');
    put32(snap);
    put16(w);
    put16(h);
    put16(from);
    put16((uint16_t)(palN - from));
    for (uint16_t i = from; i < palN; i++)
    {
        put16(jwplcMirrorPaletteColor(i));
        if (g_outLen >= OUT_CHUNK)
        {
            flushChunks();
        }
    }
    for (uint16_t y = 0; y < h; y++)
    {
        if (since != 0 && jwplcMirrorRowVersion(y) < since)
        {
            continue;
        }
        const uint8_t *row = jwplcMirrorRow(y);
        if (!row)
        {
            break;
        }
        uint16_t x = 0, end = w;
        if (since == 0 || !jwplcMirrorRowSpan(y, since, &x, &end))
        {
            x = 0;
            end = w;
        }
        put16(y);
        put16(x);
        put16((uint16_t)(end - x));
        while (x < end)
        {
            const uint8_t ix = row[x];
            uint16_t run = 1;
            while (x + run < end && run < 256 && row[x + run] == ix)
            {
                run++;
            }
            put8((uint8_t)(run - 1));
            put8(ix);
            x = (uint16_t)(x + run);
        }
        if (g_outLen >= OUT_CHUNK)
        {
            flushChunks();
        }
    }
    put16(0xFFFF);
    flushOut();
}

void sendKey(Session *s)
{
    const uint32_t b = argValue(g_req.query, "b", 255);
    const bool down = argValue(g_req.query, "d", 0) != 0;
    if (!S.keysAllowed || b >= BTN_COUNT || s->role < WEB_OPERADOR)
    {
        sendError("403 Forbidden", "teclas no permitidas");
        return;
    }
    applyKey(b, down);
    beginResponse("204 No Content", "text/plain", 0, "Cache-Control: no-store\r\n");
    flushOut();
}

// ------------------------------------------------------------- despacho

// Lee la cabecera y el cuerpo. false = cerrar sin responder.
bool readRequest(JWPLC_RedClient &c)
{
    Request &r = g_req;
    memset(&r, 0, sizeof(r));
    char line[256];
    size_t ln = 0;
    bool first = true, headersDone = false;
    long contentLength = 0;
    uint32_t t0 = millis();
    while (!headersDone || r.bodyLen < (size_t)contentLength)
    {
        if (millis() - t0 >= REQUEST_TIMEOUT_MS || !c.connected())
        {
            return false;
        }
        const int avail = c.available();
        if (avail <= 0)
        {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
        uint8_t buf[128];
        const int got = c.read(buf, avail < (int)sizeof(buf) ? (size_t)avail : sizeof(buf));
        for (int i = 0; i < got; i++)
        {
            const char ch = (char)buf[i];
            if (headersDone)
            {
                if (r.bodyLen < MAX_BODY)
                {
                    r.body[r.bodyLen] = ch;
                }
                r.bodyLen++;
                continue;
            }
            if (ch == '\r')
            {
                continue;
            }
            if (ch != '\n')
            {
                if (ln < sizeof(line) - 1)
                {
                    line[ln++] = ch;
                }
                continue;
            }
            line[ln] = 0;
            if (first)
            {
                first = false;
                // "GET /ruta?consulta HTTP/1.1"
                char *sp = strchr(line, ' ');
                if (!sp)
                {
                    return false;
                }
                *sp = 0;
                r.post = strcmp(line, "POST") == 0;
                if (!r.post && strcmp(line, "GET") != 0)
                {
                    sendText("405 Method Not Allowed");
                    return false;
                }
                char *target = sp + 1;
                char *sp2 = strchr(target, ' ');
                if (sp2)
                {
                    *sp2 = 0;
                }
                char *q = strchr(target, '?');
                if (q)
                {
                    *q++ = 0;
                    strncpy(r.query, q, sizeof(r.query) - 1);
                }
                strncpy(r.path, target, sizeof(r.path) - 1);
            }
            else if (ln == 0)
            {
                headersDone = true;
                if (contentLength > (long)MAX_BODY)
                {
                    sendText("413 Payload Too Large");
                    return false;
                }
            }
            else if (strncasecmp(line, "If-None-Match:", 14) == 0)
            {
                const char *v = line + 14;
                while (*v == ' ')
                    v++;
                strncpy(r.inm, v, sizeof(r.inm) - 1);
            }
            else if (strncasecmp(line, "Content-Length:", 15) == 0)
            {
                contentLength = atol(line + 15);
            }
            else if (strncasecmp(line, "X-JW:", 5) == 0)
            {
                r.csrfOk = true;
            }
            else if (strncasecmp(line, "Accept-Encoding:", 16) == 0)
            {
                r.gzipOk = strstr(line + 16, "gzip") != nullptr;
            }
            else if (strncasecmp(line, "Cookie:", 7) == 0)
            {
                const char *p = strstr(line + 7, "jw=");
                if (p)
                {
                    p += 3;
                    size_t o = 0;
                    while (*p && *p != ';' && *p != ' ' && o < sizeof(r.cookie) - 1)
                    {
                        r.cookie[o++] = *p++;
                    }
                    r.cookie[o] = 0;
                }
            }
            ln = 0;
        }
    }
    r.body[r.bodyLen < MAX_BODY ? r.bodyLen : MAX_BODY] = 0;
    return !first;
}

void handle(JWPLC_RedClient &c)
{
    if (!readRequest(c))
    {
        return;
    }
    const Request &r = g_req;
    const char *p = r.path;
    if (r.post && !r.csrfOk)
    {
        sendError("403 Forbidden", "falta la cabecera X-JW");
        return;
    }
    if (!r.post && (strcmp(p, "/") == 0 || strcmp(p, "/index.html") == 0))
    {
        if (!r.gzipOk)
        {
            sendText("406 Not Acceptable", "el navegador debe aceptar gzip");
            return;
        }
        sendPage();
        return;
    }
    Session *s = currentSession();
    if (!r.post && strcmp(p, "/api/estado") == 0)
    {
        apiState(s);
        return;
    }
    if (r.post && strcmp(p, "/api/reclamar") == 0)
    {
        apiClaim();
        return;
    }
    if (!r.post && strcmp(p, "/api/reto") == 0)
    {
        apiChallenge();
        return;
    }
    if (r.post && strcmp(p, "/api/entrar") == 0)
    {
        apiLogin();
        return;
    }
    if (r.post && strcmp(p, "/api/salir") == 0)
    {
        apiLogout(s);
        return;
    }
    if (!s)
    {
        sendError("401 Unauthorized", "inicia sesión");
        return;
    }
    // Rutas con sesión.
    if (!r.post)
    {
        if (strcmp(p, "/api/defs") == 0)
            apiDefs(s);
        else if (strcmp(p, "/api/vivo") == 0)
            apiLive();
        else if (strcmp(p, "/api/eventos") == 0)
            apiEvents();
        else if (strcmp(p, "/api/tendencia") == 0)
            apiTrend(false);
        else if (strcmp(p, "/api/tendencia.csv") == 0)
            apiTrend(true);
        else if (strcmp(p, "/api/equipo") == 0)
            apiDevice(s);
        else if (strcmp(p, "/api/usuarios") == 0 && s->role >= WEB_ADMIN)
            apiUsers();
        else if (strcmp(p, "/f") == 0)
            sendFrame(s, c);
        else if (strcmp(p, "/k") == 0)
            sendKey(s);
        else if (strcmp(p, "/api/usuarios") == 0)
            sendError("403 Forbidden", "solo el administrador");
        else
            sendError("404 Not Found", "no existe");
        return;
    }
    if (strcmp(p, "/api/escribir") == 0)
        apiWrite(s);
    else if (strcmp(p, "/api/reconocer") == 0 && s->role >= WEB_OPERADOR)
        apiAck(s);
    else if (strcmp(p, "/api/clave") == 0)
        apiPassword(s);
    else if (strcmp(p, "/api/usuarios/crear") == 0 && s->role >= WEB_ADMIN)
        apiUserCreate(s);
    else if (strcmp(p, "/api/usuarios/borrar") == 0 && s->role >= WEB_ADMIN)
        apiUserDelete(s);
    else if (strncmp(p, "/api/", 5) == 0)
        sendError("403 Forbidden", "tu rol no puede hacer esto");
    else
        sendError("404 Not Found", "no existe");
}

// Guarda la IP en S.ip una vez por segundo (y al instante mientras no hay IP).
void refreshIP()
{
    static uint32_t last = 0;
    if (S.ip != 0 && millis() - last < 1000)
    {
        return;
    }
    last = millis();
    S.ip = JWPLC_Red.isReady() ? (uint32_t)JWPLC_Red.localIP() : 0;
}

void webTask(void *)
{
    for (;;)
    {
        releaseStaleKeys();
        sampleTrend();
        refreshIP();
        currentPairingCode(); // rota el código a tiempo (y lo avisa por el monitor serie)
        // JWPLC_RedServer vuelve a escuchar solo cuando la red regresa y toma el bus
        // SPI por su cuenta en cada operación del W5500.
        JWPLC_RedClient client = g_server->available();
        if (client)
        {
            g_net = client.network();
            g_cur = &client;
            handle(client);
            g_cur = nullptr;
            client.stop();
            continue; // la página pide seguido: atender sin esperar
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
} // namespace

bool startServer()
{
    if (!jwplcMirrorActive())
    {
        jwplcMirrorStart(); // sin memoria: el panel funciona igual, sin la página Pantalla
    }
    g_server = new JWPLC_RedServer(S.port);
    if (!g_server)
    {
        return false;
    }
    g_server->begin();
    return xTaskCreatePinnedToCore(webTask, "jwplc_web", 7168, nullptr, 1, nullptr, 0) == pdPASS;
}

uint8_t openSessions()
{
    uint8_t n = 0;
    const uint32_t now = millis();
    for (const Session &s : g_sessions)
    {
        n += (s.used && now - s.lastSeen <= SESSION_IDLE_MS) ? 1 : 0;
    }
    return n;
}

void dropSessions()
{
    for (Session &s : g_sessions)
    {
        s.used = false;
    }
}

bool screenViewed() { return g_viewed && (millis() - g_lastViewMs) < VIEWER_WINDOW_MS; }
} // namespace jwweb
