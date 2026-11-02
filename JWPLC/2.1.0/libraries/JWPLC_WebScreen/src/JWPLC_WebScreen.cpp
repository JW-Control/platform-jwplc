// JWPLC_WebScreen.cpp: servidor HTTP mínimo por el W5500 con la pantalla en vivo.
//
// Una tarea propia (núcleo 0, prioridad baja) atiende una conexión a la vez,
// un pedido por conexión (KEEP_ALIVE desactivado, ver abajo):
//   "/"   la página (JWPLC_WebScreen_Page.h), con ETag: recargar no la reenvía
//   "/f"  los tramos de fila de la TFT que cambiaron desde la versión que tiene
//         la página
//   "/k"  tecla virtual (JWPLC_Buttons.setVirtualDown)
// Cada operación del W5500 toma el bus SPI compartido por su cuenta (acceso
// seguro de JWPLC_Ethernet); la copia de la pantalla está en RAM, así que
// armar la respuesta no toca el bus (regla 1 del CLAUDE.md).
#include "JWPLC_WebScreen.h"

#include <JWPLC_Ethernet.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_TFT_Mirror.h>
#include <string.h>
#include <strings.h>

#include "JWPLC_W5500_SafeAccess.h"
#include "JWPLC_WebScreen_Page.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

JWPLC_WebScreenClass JWPLC_WebScreen;

// La TFT arranca con la copia en RAM cuando esta librería está enlazada.
extern "C" bool jwplcTFTMirrorWanted() { return true; }

// Para Red del Runtime: puerto del servidor (0 si no arrancó).
extern "C" uint16_t jwplcWebScreenPort();

namespace
{
const uint32_t VIEWER_WINDOW_MS = 3000;
const uint32_t KEY_AUTO_RELEASE_MS = 4000; // la página repite "pulsada" cada 1 s
const uint32_t REQUEST_TIMEOUT_MS = 800;
// Conexión abierta entre pedidos: la página pide el cuadro siguiente al instante,
// así que 1 s sin pedido = la página se fue (o abrió otra conexión).
const uint32_t KEEPALIVE_IDLE_MS = 1000;
// Desactivada (02/11): en la placa con Windows, entrar a PROGRAMA, TARJETA SD,
// RED o DIAGNÓSTICO se volvió más lento que con una conexión por pedido (#215).
// Con false: Connection: close y cuerpo sin largo, como en #215.
const bool KEEP_ALIVE = false;
// Espera larga: si nada cambió, la respuesta sale en cuanto cambia una fila
// (o a los LONG_POLL_MS). Así la página no pregunta a ciegas cada N ms.
const uint32_t LONG_POLL_MS = 400;
const uint32_t LONG_POLL_STEP_MS = 3;
// Trozos de ~1 KB: caben dos en el búfer de envío del W5500 (2 KB), así hay dos
// segmentos en vuelo y Windows responde el ACK al instante. Con trozos de 1400
// solo cabía uno y el ACK retrasado de Windows (hasta 200 ms) frenaba cada trozo:
// un cambio de página (~10 KB) tardaba más de un segundo.
const size_t OUT_CHUNK = 1000;
// Un redibujo dura varios ms: se responde cuando la TFT deja de escribir
// SETTLE_MS (máx. SETTLE_MAX_MS), para no mandar media pantalla y luego otra vez.
const uint32_t SETTLE_MS = 8;
const uint32_t SETTLE_MAX_MS = 150;

EthernetServer *g_server = nullptr;
uint16_t g_port = 80;
bool g_running = false;
bool g_listening = false;
bool g_keysAllowed = true;
volatile uint32_t g_lastViewMs = 0;
volatile bool g_viewed = false;
uint32_t g_keyDownAt[BTN_COUNT];
bool g_keyDown[BTN_COUNT];

uint8_t g_out[OUT_CHUNK + 700]; // un trozo + una fila RLE completa (6 + 320 x 2)
size_t g_outLen = 0;

// Respuesta en curso: la cabecera HTTP espera en g_hdr y sale en el mismo
// segmento que el primer trozo del cuerpo. Con chunked, cada trozo lleva su
// largo en hexadecimal (así la conexión sigue abierta sin conocer el total).
char g_hdr[224];
size_t g_hdrLen = 0;
bool g_chunked = false;
uint8_t g_tx[sizeof(g_hdr) + OUT_CHUNK + 16];

// Bytes de cuerpo por segmento: OUT_CHUNK en total contando cabecera y marcas.
size_t pieceSize()
{
    return OUT_CHUNK - g_hdrLen - (g_chunked ? 12 : 0);
}

// Un segmento: cabecera pendiente + trozo (+ fin del cuerpo si last).
void emit(EthernetClient &c, const uint8_t *data, size_t n, bool last)
{
    size_t t = 0;
    if (g_hdrLen)
    {
        memcpy(g_tx, g_hdr, g_hdrLen);
        t = g_hdrLen;
        g_hdrLen = 0;
    }
    if (g_chunked)
    {
        if (n)
        {
            t += (size_t)snprintf((char *)g_tx + t, 8, "%X\r\n", (unsigned)n);
            memcpy(g_tx + t, data, n);
            t += n;
            g_tx[t++] = '\r';
            g_tx[t++] = '\n';
        }
        if (last)
        {
            memcpy(g_tx + t, "0\r\n\r\n", 5);
            t += 5;
        }
    }
    else if (n)
    {
        memcpy(g_tx + t, data, n);
        t += n;
    }
    if (t)
    {
        c.write(g_tx, t);
    }
}

// Envía trozos completos y deja el resto en el búfer.
void flushChunks(EthernetClient &c)
{
    size_t sent = 0;
    for (;;)
    {
        const size_t piece = pieceSize();
        if (g_outLen - sent < piece)
        {
            break;
        }
        emit(c, g_out + sent, piece, false);
        sent += piece;
    }
    if (sent)
    {
        memmove(g_out, g_out + sent, g_outLen - sent);
        g_outLen -= sent;
    }
}

// Envía todo lo que queda y cierra el cuerpo.
void flushOut(EthernetClient &c)
{
    flushChunks(c);
    emit(c, g_out, g_outLen, true);
    g_outLen = 0;
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

uint32_t argValue(const char *query, const char *name, uint32_t fallback)
{
    if (!query)
    {
        return fallback;
    }
    const size_t n = strlen(name);
    const char *p = query;
    while (*p)
    {
        if (strncmp(p, name, n) == 0 && p[n] == '=')
        {
            return (uint32_t)strtoul(p + n + 1, nullptr, 10);
        }
        p = strchr(p, '&');
        if (!p)
        {
            break;
        }
        p++;
    }
    return fallback;
}

const long BODY_CHUNKED = -1;
const long BODY_NONE = -2; // 204 y 304: sin cuerpo ni Content-Length

// Prepara la cabecera; sale con el primer trozo.
void beginResponse(const char *status, const char *type, long length, const char *extra = "")
{
    g_outLen = 0;
    if (!KEEP_ALIVE && length == BODY_CHUNKED)
    {
        length = BODY_NONE; // el cierre de la conexión marca el fin del cuerpo
    }
    g_chunked = length == BODY_CHUNKED;
    char len[40];
    if (g_chunked)
    {
        snprintf(len, sizeof(len), "Transfer-Encoding: chunked\r\n");
    }
    else if (length == BODY_NONE)
    {
        len[0] = 0;
    }
    else
    {
        snprintf(len, sizeof(len), "Content-Length: %ld\r\n", length);
    }
    const int n = snprintf(g_hdr, sizeof(g_hdr),
                           "HTTP/1.1 %s\r\nContent-Type: %s\r\n%s%sConnection: %s\r\n\r\n",
                           status, type, len, extra, KEEP_ALIVE ? "keep-alive" : "close");
    g_hdrLen = n > 0 ? (size_t)n : 0;
}

// Respuesta corta de texto (errores, 204).
void sendText(EthernetClient &c, const char *status, const char *text = "")
{
    const size_t n = strlen(text);
    beginResponse(status, "text/plain", (long)n, "Cache-Control: no-store\r\n");
    memcpy(g_out, text, n);
    g_outLen = n;
    flushOut(c);
}

// ETag de la página: cambia solo si cambia el firmware (otra página).
const char *pageTag()
{
    static char tag[12] = {0};
    if (!tag[0])
    {
        uint32_t h = 2166136261u;
        for (size_t i = 0; i < sizeof(JWPLC_WEBSCREEN_PAGE) - 1; i++)
        {
            h = (h ^ (uint8_t)JWPLC_WEBSCREEN_PAGE[i]) * 16777619u;
        }
        snprintf(tag, sizeof(tag), "\"%08lx\"", (unsigned long)h);
    }
    return tag;
}

void sendPage(EthernetClient &c, const char *ifNoneMatch)
{
    char extra[64];
    // no-cache: el navegador guarda la página y pregunta con If-None-Match;
    // si no cambió, responde 304 sin cuerpo (recarga instantánea).
    snprintf(extra, sizeof(extra), "Cache-Control: no-cache\r\nETag: %s\r\n", pageTag());
    if (ifNoneMatch && strstr(ifNoneMatch, pageTag()))
    {
        beginResponse("304 Not Modified", "text/html; charset=utf-8", BODY_NONE, extra);
        flushOut(c);
        return;
    }
    const size_t len = sizeof(JWPLC_WEBSCREEN_PAGE) - 1;
    beginResponse("200 OK", "text/html; charset=utf-8", (long)len, extra);
    size_t i = 0;
    while (i < len)
    {
        size_t n = OUT_CHUNK - g_outLen;
        if (n > len - i)
        {
            n = len - i;
        }
        memcpy(g_out + g_outLen, JWPLC_WEBSCREEN_PAGE + i, n);
        g_outLen += n;
        i += n;
        flushChunks(c);
    }
    flushOut(c);
}

void applyKey(uint32_t b, bool down)
{
    if (!g_keysAllowed || b >= BTN_COUNT)
    {
        return;
    }
    g_keyDown[b] = down;
    g_keyDownAt[b] = millis();
    JWPLC_Buttons.setVirtualDown((uint8_t)b, down);
}

// ¿Alguna fila cambió desde la versión que tiene la página?
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

void sendFrame(EthernetClient &c, const char *query)
{
    g_lastViewMs = millis();
    g_viewed = true;
    // Tecla dentro de la misma petición (e=<tecla><1|0>): sin cola detrás del cuadro.
    const uint32_t ev = argValue(query, "e", 0xFFFFFFFFu);
    const bool hasEvent = ev != 0xFFFFFFFFu;
    if (hasEvent)
    {
        applyKey(ev / 10, (ev % 10) != 0);
    }
    if (!jwplcMirrorActive())
    {
        sendText(c, "503 Service Unavailable", "sin memoria para la pantalla");
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
                return; // la página canceló para mandar una tecla
            }
        }
    }
    // Esperar a que termine el redibujo en curso (una sola respuesta por cambio).
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

    beginResponse("200 OK", "application/octet-stream", BODY_CHUNKED, "Cache-Control: no-store\r\n");
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
            flushChunks(c);
        }
    }
    for (uint16_t y = 0; y < h; y++)
    {
        // La TFT guarda la versión después de escribir la fila (JWPLC_TFT_Mirror):
        // una fila a medias al tomar la versión anterior quedó >= since.
        if (since != 0 && jwplcMirrorRowVersion(y) < since)
        {
            continue;
        }
        const uint8_t *row = jwplcMirrorRow(y);
        if (!row)
        {
            break;
        }
        // Si la página ya tiene el resto de la fila, solo el tramo que cambió.
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
            flushChunks(c);
        }
    }
    put16(0xFFFF);
    flushOut(c);
}

void sendKey(EthernetClient &c, const char *query)
{
    const uint32_t b = argValue(query, "b", 255);
    const bool down = argValue(query, "d", 0) != 0;
    if (!g_keysAllowed || b >= BTN_COUNT)
    {
        sendText(c, "403 Forbidden");
        return;
    }
    applyKey(b, down);
    beginResponse("204 No Content", "text/plain", BODY_NONE, "Cache-Control: no-store\r\n");
    flushOut(c);
}

void releaseStaleKeys()
{
    const uint32_t now = millis();
    for (uint8_t b = 0; b < BTN_COUNT; b++)
    {
        if (g_keyDown[b] && now - g_keyDownAt[b] > KEY_AUTO_RELEASE_MS)
        {
            g_keyDown[b] = false;
            JWPLC_Buttons.setVirtualDown(b, false);
        }
    }
}

// ¿Hay otra conexión con un pedido esperando? (la página se recargó o abrió otra)
bool otherClientWaiting(const EthernetClient &c)
{
    EthernetClient o;
    {
        jwplc_w5500_candidate::BusScope bus(20);
        if (bus)
        {
            o = g_server->available();
        }
    }
    return o && o.getSocketNumber() != c.getSocketNumber();
}

// Atiende un pedido de la conexión. false = cerrarla (se fue, error o espera
// sin pedido). Entre pedidos espera hasta waitMs al siguiente.
bool serve(EthernetClient &c, uint32_t waitMs)
{
    char req[200];
    char line[200];
    char inm[24];
    size_t n = 0, ln = 0;
    bool first = true, done = false;
    req[0] = 0;
    inm[0] = 0;
    uint32_t t0 = millis();
    uint32_t lastOther = t0;
    bool started = false;
    // Primera línea ("GET /ruta?consulta HTTP/1.1") e If-None-Match; lo demás se descarta.
    while (!done)
    {
        const uint32_t limit = started ? REQUEST_TIMEOUT_MS : waitMs;
        if (millis() - t0 >= limit || !c.connected())
        {
            return false;
        }
        const int avail = c.available();
        if (avail <= 0)
        {
            if (!started && millis() - lastOther >= 20)
            {
                lastOther = millis();
                if (otherClientWaiting(c))
                {
                    return false;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
        if (!started)
        {
            started = true;
            t0 = millis();
        }
        uint8_t buf[128];
        const int got = c.read(buf, avail < (int)sizeof(buf) ? (size_t)avail : sizeof(buf));
        for (int i = 0; i < got && !done; i++)
        {
            const char ch = (char)buf[i];
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
                memcpy(req, line, ln + 1);
                n = ln;
                first = false;
            }
            else if (ln == 0)
            {
                done = true; // línea vacía: fin de la cabecera
            }
            else if (strncasecmp(line, "If-None-Match:", 14) == 0)
            {
                const char *v = line + 14;
                while (*v == ' ')
                {
                    v++;
                }
                strncpy(inm, v, sizeof(inm) - 1);
                inm[sizeof(inm) - 1] = 0;
            }
            ln = 0;
        }
    }
    if (n == 0 || strncmp(req, "GET ", 4) != 0)
    {
        sendText(c, "400 Bad Request");
        return false;
    }
    char *path = req + 4;
    char *sp = strchr(path, ' ');
    if (sp)
    {
        *sp = 0;
    }
    char *query = strchr(path, '?');
    if (query)
    {
        *query++ = 0;
    }
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)
    {
        sendPage(c, inm);
    }
    else if (strcmp(path, "/f") == 0)
    {
        sendFrame(c, query);
    }
    else if (strcmp(path, "/k") == 0)
    {
        sendKey(c, query);
    }
    else
    {
        sendText(c, "404 Not Found");
    }
    return true;
}

void webTask(void *)
{
    for (;;)
    {
        releaseStaleKeys();
        if (!JWPLC_Ethernet.isReady())
        {
            g_listening = false;
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        if (!g_listening)
        {
            jwplc_w5500_candidate::BusScope bus(50);
            if (bus)
            {
                g_server->begin();
                g_listening = true;
            }
        }
        EthernetClient client;
        {
            jwplc_w5500_candidate::BusScope bus(20);
            if (bus)
            {
                client = g_server->available();
            }
        }
        if (client)
        {
            // Misma conexión para los pedidos siguientes (sin abrir y cerrar TCP por cuadro).
            bool more = serve(client, REQUEST_TIMEOUT_MS);
            while (KEEP_ALIVE && more && JWPLC_Ethernet.isReady())
            {
                releaseStaleKeys();
                more = serve(client, KEEPALIVE_IDLE_MS);
            }
            client.stop();
            continue; // la página pide seguido: atender sin esperar
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
} // namespace

bool JWPLC_WebScreenClass::begin(uint16_t port)
{
    if (g_running)
    {
        return true;
    }
    if (!jwplcMirrorActive() && !jwplcMirrorStart())
    {
        return false;
    }
    g_port = port;
    g_server = new EthernetServer(port);
    if (!g_server)
    {
        return false;
    }
    if (xTaskCreatePinnedToCore(webTask, "jwplc_web", 6144, nullptr, 1, nullptr, 0) != pdPASS)
    {
        return false;
    }
    g_running = true;
    return true;
}

bool JWPLC_WebScreenClass::isRunning() const { return g_running; }
extern "C" uint16_t jwplcWebScreenPort() { return g_running ? g_port : 0; }
uint16_t JWPLC_WebScreenClass::port() const { return g_port; }

bool JWPLC_WebScreenClass::viewerConnected() const
{
    return g_viewed && (millis() - g_lastViewMs) < VIEWER_WINDOW_MS;
}

String JWPLC_WebScreenClass::url() const
{
    if (!g_running || !JWPLC_Ethernet.isReady())
    {
        return String();
    }
    String s = "http://" + JWPLC_Ethernet.localIP().toString();
    if (g_port != 80)
    {
        s += ":" + String(g_port);
    }
    return s + "/";
}

void JWPLC_WebScreenClass::allowKeys(bool allow)
{
    g_keysAllowed = allow;
    if (!allow)
    {
        for (uint8_t b = 0; b < BTN_COUNT; b++)
        {
            g_keyDown[b] = false;
            JWPLC_Buttons.setVirtualDown(b, false);
        }
    }
}

bool JWPLC_WebScreenClass::keysAllowed() const { return g_keysAllowed; }
