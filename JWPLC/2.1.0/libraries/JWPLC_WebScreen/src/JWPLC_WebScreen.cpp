// JWPLC_WebScreen.cpp: servidor HTTP mínimo por el W5500 con la pantalla en vivo.
//
// Una tarea propia (núcleo 0, prioridad baja) atiende una conexión a la vez:
//   "/"   la página (JWPLC_WebScreen_Page.h)
//   "/f"  las filas de la TFT que cambiaron desde la versión que tiene la página
//   "/k"  tecla virtual (JWPLC_Buttons.setVirtualDown)
// Cada operación del W5500 toma el bus SPI compartido por su cuenta (acceso
// seguro de JWPLC_Ethernet); la copia de la pantalla está en RAM, así que
// armar la respuesta no toca el bus (regla 1 del CLAUDE.md).
#include "JWPLC_WebScreen.h"

#include <JWPLC_Ethernet.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_TFT_Mirror.h>
#include <string.h>

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
const size_t OUT_CHUNK = 1400;

EthernetServer *g_server = nullptr;
uint16_t g_port = 80;
bool g_running = false;
bool g_listening = false;
bool g_keysAllowed = true;
volatile uint32_t g_lastViewMs = 0;
volatile bool g_viewed = false;
uint32_t g_keyDownAt[BTN_COUNT];
bool g_keyDown[BTN_COUNT];

uint8_t g_out[OUT_CHUNK + 700]; // un trozo + una fila RLE completa (2 + 320 x 2)
size_t g_outLen = 0;

void flushOut(EthernetClient &c)
{
    if (g_outLen)
    {
        c.write(g_out, g_outLen);
        g_outLen = 0;
    }
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

void sendHeader(EthernetClient &c, const char *status, const char *type)
{
    char h[200];
    snprintf(h, sizeof(h),
             "HTTP/1.1 %s\r\nContent-Type: %s\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
             status, type);
    c.write((const uint8_t *)h, strlen(h));
}

void sendPage(EthernetClient &c)
{
    sendHeader(c, "200 OK", "text/html; charset=utf-8");
    const size_t len = sizeof(JWPLC_WEBSCREEN_PAGE) - 1;
    for (size_t i = 0; i < len; i += OUT_CHUNK)
    {
        const size_t n = (len - i) < OUT_CHUNK ? (len - i) : OUT_CHUNK;
        c.write((const uint8_t *)JWPLC_WEBSCREEN_PAGE + i, n);
    }
}

void sendFrame(EthernetClient &c, const char *query)
{
    g_lastViewMs = millis();
    g_viewed = true;
    if (!jwplcMirrorActive())
    {
        sendHeader(c, "503 Service Unavailable", "text/plain");
        c.write((const uint8_t *)"sin memoria para la pantalla", 28);
        return;
    }
    const uint32_t since = argValue(query, "v", 0);
    const uint16_t known = (uint16_t)argValue(query, "p", 0);
    const uint32_t snap = jwplcMirrorSnapshot();
    const uint16_t w = jwplcMirrorWidth();
    const uint16_t h = jwplcMirrorHeight();
    const uint16_t palN = jwplcMirrorPaletteCount();
    const uint16_t from = known <= palN ? known : 0;

    sendHeader(c, "200 OK", "application/octet-stream");
    g_outLen = 0;
    put8('J');
    put8('W');
    put8('S');
    put8('1');
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
            flushOut(c);
        }
    }
    for (uint16_t y = 0; y < h; y++)
    {
        // +1: una fila que se dibujaba justo al tomar la versión anterior se reenvía.
        if (since != 0 && jwplcMirrorRowVersion(y) + 1 < since)
        {
            continue;
        }
        const uint8_t *row = jwplcMirrorRow(y);
        if (!row)
        {
            break;
        }
        put16(y);
        uint16_t x = 0;
        while (x < w)
        {
            const uint8_t ix = row[x];
            uint16_t run = 1;
            while (x + run < w && run < 256 && row[x + run] == ix)
            {
                run++;
            }
            put8((uint8_t)(run - 1));
            put8(ix);
            x = (uint16_t)(x + run);
        }
        if (g_outLen >= OUT_CHUNK)
        {
            flushOut(c);
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
        sendHeader(c, "403 Forbidden", "text/plain");
        return;
    }
    g_keyDown[b] = down;
    g_keyDownAt[b] = millis();
    JWPLC_Buttons.setVirtualDown((uint8_t)b, down);
    sendHeader(c, "204 No Content", "text/plain");
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

void serve(EthernetClient &c)
{
    char req[200];
    size_t n = 0;
    const uint32_t t0 = millis();
    bool lineDone = false;
    // Solo interesa la primera línea ("GET /ruta?consulta HTTP/1.1"); el resto se descarta.
    while (millis() - t0 < REQUEST_TIMEOUT_MS && c.connected())
    {
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
            if (!lineDone)
            {
                if (buf[i] == '\r' || buf[i] == '\n')
                {
                    lineDone = true;
                }
                else if (n < sizeof(req) - 1)
                {
                    req[n++] = (char)buf[i];
                }
            }
        }
        if (lineDone && c.available() <= 0)
        {
            break;
        }
    }
    req[n] = 0;
    if (!lineDone || strncmp(req, "GET ", 4) != 0)
    {
        sendHeader(c, "400 Bad Request", "text/plain");
        return;
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
        sendPage(c);
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
        sendHeader(c, "404 Not Found", "text/plain");
    }
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
            serve(client);
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
