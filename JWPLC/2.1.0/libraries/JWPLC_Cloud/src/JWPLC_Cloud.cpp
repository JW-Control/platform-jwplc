// JWPLC_Cloud: muestras y alarmas a JW Cloud (POST /v1/plc/datos) desde una tarea aparte.
//
// El loop solo escribe valores (set, alarm) bajo un mutex corto. La tarea toma una muestra cada
// intervalo en un anillo propio y lo vacía en lotes que caben en 8 KB. Cada muestra y cada cambio
// de alarma viajan con su edad en ms: JW Cloud les pone la hora al recibirlos, así el JWPLC no
// necesita reloj y lo guardado sin red llega con su hora real.
#include "JWPLC_Cloud.h"

#include <JWPLC_TLS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>

JWPLC_CloudClass JWPLC_Cloud;

namespace
{
const uint16_t MAX_SAMPLES = 120;        // en el anillo (20 min a 10 s)
const uint8_t MAX_SAMPLES_POST = 60;     // por envío (límite de JW Cloud)
const uint8_t MAX_EVENTS = 32;           // cambios de alarma por enviar
const uint8_t MAX_EVENTS_POST = 20;      // por envío (límite de JW Cloud)
const size_t BODY_MAX = 8000;            // JW Cloud acepta hasta 8192 bytes
const size_t BODY_ROOM = 7200;           // deja lugar para cerrar el JSON
const uint32_t RESPONSE_TIMEOUT_MS = 8000;
const uint8_t MAX_POSTS_PER_ROUND = 4;   // al volver la red, vacía de a poco

struct Var
{
    char id[16];
    char name[41];
    char unit[13];
    uint8_t decimals;
    bool isBool;
    bool hasRange;
    float min;
    float max;
    float value;
    bool hasValue;
};

struct Alarm
{
    char id[16];
    char text[61];
    bool active;
};

struct Event
{
    uint8_t alarm;
    bool on;
    uint32_t ms;
};

struct Sample
{
    uint32_t ms;
    uint16_t mask;
    float v[CLOUD_MAX_VARIABLES];
};

bool validId(const char *id)
{
    if (id == nullptr || id[0] == '\0' || strlen(id) > 15)
        return false;
    for (const char *p = id; *p; p++)
        if (!isalnum((unsigned char)*p) && *p != '_')
            return false;
    return true;
}

// Copia texto UTF-8 sin cortar un carácter por la mitad.
void copyText(char *dst, size_t size, const char *src)
{
    if (src == nullptr)
        src = "";
    size_t n = strlen(src);
    if (n >= size)
    {
        n = size - 1;
        while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80)
            n--;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

// Texto acumulado en un búfer fijo; si no cabe, queda marcado y no se envía.
struct Body
{
    char *buf;
    size_t cap;
    size_t len = 0;
    bool overflow = false;

    void raw(const char *s)
    {
        const size_t n = strlen(s);
        if (len + n >= cap)
        {
            overflow = true;
            return;
        }
        memcpy(buf + len, s, n);
        len += n;
        buf[len] = '\0';
    }
    void ch(char c)
    {
        const char s[2] = {c, '\0'};
        raw(s);
    }
    void str(const char *s)
    {
        ch('"');
        for (const char *p = s; *p; p++)
        {
            const unsigned char c = (unsigned char)*p;
            if (c == '"' || c == '\\')
            {
                ch('\\');
                ch((char)c);
            }
            else if (c < 0x20)
                ch(' ');
            else
                ch((char)c);
        }
        ch('"');
    }
    void num(float v, uint8_t decimals)
    {
        char t[24];
        snprintf(t, sizeof(t), "%.*f", (int)decimals, (double)v);
        raw(t);
    }
    void u32(uint32_t v)
    {
        char t[12];
        snprintf(t, sizeof(t), "%lu", (unsigned long)v);
        raw(t);
    }
};
} // namespace

struct JWPLC_CloudState
{
    SemaphoreHandle_t lock = nullptr;
    Var vars[CLOUD_MAX_VARIABLES];
    uint8_t varCount = 0;
    Alarm alarms[CLOUD_MAX_ALARMS];
    uint8_t alarmCount = 0;
    Event events[MAX_EVENTS];
    uint8_t eventCount = 0;
    bool schemaDirty = true;     // el esquema va en el primer envío y tras cada add*()
    bool alarmsSynced = false;   // al arrancar se manda el estado de todas las alarmas

    char token[101] = "";
    char host[64] = "ota.jw-control.com";
    uint16_t port = 443;
    bool tls = true;
    char program[41] = "";
    uint32_t intervalMs = 10000;

    Sample samples[MAX_SAMPLES];
    uint16_t head = 0;  // la más vieja
    uint16_t count = 0;

    char body[BODY_MAX];
    JWPLC_TLSClient tlsClient;
    JWPLC_RedClient tcpClient;

    TaskHandle_t task = nullptr;
    volatile bool running = false;
    volatile uint8_t status = CLOUD_OFF;
    volatile uint32_t sent = 0;
    char tlsError[32] = "";

    void take() { xSemaphoreTake(lock, portMAX_DELAY); }
    void give() { xSemaphoreGive(lock); }
    int findVar(const char *id)
    {
        for (uint8_t i = 0; i < varCount; i++)
            if (strcmp(vars[i].id, id) == 0)
                return i;
        return -1;
    }
    int findAlarm(const char *id)
    {
        for (uint8_t i = 0; i < alarmCount; i++)
            if (strcmp(alarms[i].id, id) == 0)
                return i;
        return -1;
    }
};

namespace
{
void takeSample(JWPLC_CloudState *s)
{
    Sample smp;
    smp.ms = millis();
    smp.mask = 0;
    s->take();
    for (uint8_t i = 0; i < s->varCount; i++)
    {
        if (!s->vars[i].hasValue || isnan(s->vars[i].value) || isinf(s->vars[i].value))
            continue;
        smp.mask |= (uint16_t)(1u << i);
        smp.v[i] = s->vars[i].value;
    }
    s->give();
    if (smp.mask == 0)
        return;
    if (s->count == MAX_SAMPLES)
    {
        // Sin red por mucho tiempo: se pierde la más vieja.
        s->head = (s->head + 1) % MAX_SAMPLES;
        s->count--;
    }
    s->samples[(s->head + s->count) % MAX_SAMPLES] = smp;
    s->count++;
}

// Arma un lote. Devuelve cuántas muestras y cambios de alarma lleva (para borrarlos si llega).
bool buildBody(JWPLC_CloudState *s, uint16_t &nSamples, uint8_t &nEvents, bool &withSchema)
{
    Body b{s->body, BODY_MAX};
    const uint32_t now = millis();
    nSamples = 0;
    nEvents = 0;

    s->take();
    withSchema = s->schemaDirty;
    b.raw("{\"fw\":");
    b.str(s->program);
    if (withSchema)
    {
        b.raw(",\"esquema\":[");
        for (uint8_t i = 0; i < s->varCount; i++)
        {
            const Var &v = s->vars[i];
            if (i)
                b.ch(',');
            b.raw("{\"id\":");
            b.str(v.id);
            b.raw(",\"n\":");
            b.str(v.name);
            if (v.isBool)
                b.raw(",\"t\":\"b\"}");
            else
            {
                b.raw(",\"t\":\"n\",\"u\":");
                b.str(v.unit);
                b.raw(",\"d\":");
                b.u32(v.decimals);
                if (v.hasRange)
                {
                    b.raw(",\"min\":");
                    b.num(v.min, 4);
                    b.raw(",\"max\":");
                    b.num(v.max, 4);
                }
                b.ch('}');
            }
        }
        b.ch(']');
    }
    // Cambios de alarma (y, la primera vez, el estado de todas para cerrar las que quedaron abiertas).
    Event ev[MAX_EVENTS_POST];
    uint8_t n = 0;
    if (!s->alarmsSynced)
        for (uint8_t i = 0; i < s->alarmCount; i++)
            ev[n++] = {i, s->alarms[i].active, now};
    while (nEvents < s->eventCount && n < MAX_EVENTS_POST)
        ev[n++] = s->events[nEvents++];
    // Al arrancar va el estado de todas: JW Cloud cierra las que este programa ya no declara.
    if (!s->alarmsSynced)
        b.raw(",\"al_completo\":true");
    if (n)
    {
        b.raw(",\"al\":[");
        for (uint8_t i = 0; i < n; i++)
        {
            const Alarm &a = s->alarms[ev[i].alarm];
            if (i)
                b.ch(',');
            b.raw("{\"edad_ms\":");
            b.u32(now - ev[i].ms);
            b.raw(",\"id\":");
            b.str(a.id);
            b.raw(",\"t\":");
            b.str(a.text);
            b.raw(ev[i].on ? ",\"on\":true}" : ",\"on\":false}");
        }
        b.ch(']');
    }
    // Copia de nombres y decimales para no tener el mutex mientras se escriben las muestras.
    uint8_t varCount = s->varCount;
    char ids[CLOUD_MAX_VARIABLES][16];
    uint8_t decimals[CLOUD_MAX_VARIABLES];
    bool isBool[CLOUD_MAX_VARIABLES];
    for (uint8_t i = 0; i < varCount; i++)
    {
        memcpy(ids[i], s->vars[i].id, sizeof(ids[i]));
        decimals[i] = s->vars[i].decimals;
        isBool[i] = s->vars[i].isBool;
    }
    s->give();

    b.raw(",\"m\":[");
    for (uint16_t k = 0; k < s->count && k < MAX_SAMPLES_POST; k++)
    {
        const Sample &m = s->samples[(s->head + k) % MAX_SAMPLES];
        const size_t before = b.len;
        if (k)
            b.ch(',');
        b.raw("{\"edad_ms\":");
        b.u32(now - m.ms);
        b.raw(",\"v\":{");
        bool first = true;
        for (uint8_t i = 0; i < varCount; i++)
        {
            if (!(m.mask & (1u << i)))
                continue;
            if (!first)
                b.ch(',');
            first = false;
            b.str(ids[i]);
            b.ch(':');
            if (isBool[i])
                b.raw(m.v[i] != 0 ? "true" : "false");
            else
                b.num(m.v[i], decimals[i] + 1);
        }
        b.raw("}}");
        if (b.overflow || b.len > BODY_ROOM)
        {
            // Esta muestra va en el próximo lote.
            b.len = before;
            b.buf[b.len] = '\0';
            b.overflow = false;
            break;
        }
        nSamples++;
    }
    b.raw("]}");
    return !b.overflow && (withSchema || n || nSamples);
}

void forget(JWPLC_CloudState *s, uint16_t nSamples, uint8_t nEvents, bool withSchema)
{
    s->head = (s->head + nSamples) % MAX_SAMPLES;
    s->count -= nSamples;
    s->take();
    memmove(s->events, s->events + nEvents, (s->eventCount - nEvents) * sizeof(Event));
    s->eventCount -= nEvents;
    s->alarmsSynced = true;
    if (withSchema)
        s->schemaDirty = false;
    s->give();
}

// POST del lote ya armado. Devuelve el código HTTP (0 si no hubo respuesta).
int post(JWPLC_CloudState *s)
{
    Client *c = s->tls ? (Client *)&s->tlsClient : (Client *)&s->tcpClient;
    if (!c->connect(s->host, s->port))
    {
        if (s->tls && s->tlsClient.lastError() != TLS_ERROR_NO_NETWORK)
        {
            copyText(s->tlsError, sizeof(s->tlsError), s->tlsClient.errorString());
            return -1;
        }
        return 0;
    }
    const size_t len = strlen(s->body);
    char head[320];
    snprintf(head, sizeof(head),
             "POST /v1/plc/datos HTTP/1.1\r\nHost: %s\r\nAuthorization: Bearer %s\r\n"
             "Content-Type: application/json\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
             s->host, s->token, (unsigned)len);
    c->write((const uint8_t *)head, strlen(head));
    size_t off = 0;
    while (off < len)
    {
        const size_t w = c->write((const uint8_t *)s->body + off, len - off > 1024 ? 1024 : len - off);
        if (w == 0)
            break;
        off += w;
    }
    // Primera línea: "HTTP/1.1 200 OK".
    char line[40];
    uint8_t n = 0;
    int code = 0;
    const uint32_t t0 = millis();
    while (millis() - t0 < RESPONSE_TIMEOUT_MS)
    {
        const int a = c->available();
        if (a <= 0)
        {
            if (!c->connected())
                break;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        const int ch = c->read();
        if (ch == '\n')
        {
            line[n] = '\0';
            const char *sp = strchr(line, ' ');
            code = sp ? atoi(sp + 1) : 0;
            break;
        }
        if (ch >= 0 && n < sizeof(line) - 1)
            line[n++] = (char)ch;
    }
    c->stop();
    return code;
}

void sendPending(JWPLC_CloudState *s)
{
    for (uint8_t round = 0; round < MAX_POSTS_PER_ROUND && s->running; round++)
    {
        uint16_t nSamples;
        uint8_t nEvents;
        bool withSchema;
        s->take();
        const bool events = s->eventCount > 0 || !s->alarmsSynced;
        const bool schema = s->schemaDirty;
        s->give();
        if (s->count == 0 && !events && !schema)
            return;
        if (!JWPLC_Red.isReady())
        {
            s->status = CLOUD_NO_NETWORK;
            return;
        }
        if (!buildBody(s, nSamples, nEvents, withSchema))
            return;
        const int code = post(s);
        if (code == 200)
        {
            forget(s, nSamples, nEvents, withSchema);
            s->sent = s->sent + 1;
            s->status = CLOUD_ONLINE;
            continue;
        }
        if (code == -1)
            s->status = CLOUD_TLS_ERROR;
        else if (code == 0)
            s->status = CLOUD_NO_NETWORK;
        else if (code == 401)
            s->status = CLOUD_BAD_TOKEN;
        else
        {
            s->status = CLOUD_SERVER_ERROR;
            // 400/413: el lote no sirve y no se reintenta igual (se descarta para no trabar la cola).
            if (code == 400 || code == 413)
                forget(s, nSamples, nEvents, withSchema);
        }
        return;
    }
}

void cloudTask(void *arg)
{
    JWPLC_CloudState *s = (JWPLC_CloudState *)arg;
    TickType_t last = xTaskGetTickCount();
    // Deja correr el primer loop() para que haya valores.
    vTaskDelay(pdMS_TO_TICKS(1000));
    while (s->running)
    {
        takeSample(s);
        sendPending(s);
        if (!s->running)
            break;
        vTaskDelayUntil(&last, pdMS_TO_TICKS(s->intervalMs));
    }
    s->task = nullptr;
    vTaskDelete(nullptr);
}
} // namespace

JWPLC_CloudState *JWPLC_CloudClass::st()
{
    if (state_ == nullptr)
    {
        state_ = new JWPLC_CloudState();
        state_->lock = xSemaphoreCreateMutex();
    }
    return state_;
}

void JWPLC_CloudClass::begin(const char token[])
{
    JWPLC_CloudState *s = st();
    if (s->running)
        return;
    copyText(s->token, sizeof(s->token), token);
    s->status = CLOUD_STARTING;
    s->running = true;
    if (s->task == nullptr)
        xTaskCreatePinnedToCore(cloudTask, "jwplc_cloud", 10240, s, 1, &s->task, tskNO_AFFINITY);
}

void JWPLC_CloudClass::begin(const String &token)
{
    begin(token.c_str());
}

void JWPLC_CloudClass::end()
{
    if (state_ == nullptr)
        return;
    state_->running = false;
    state_->status = CLOUD_OFF;
}

void JWPLC_CloudClass::setServer(const char host[], uint16_t port, bool tls)
{
    JWPLC_CloudState *s = st();
    s->take();
    copyText(s->host, sizeof(s->host), host);
    s->port = port;
    s->tls = tls;
    s->give();
}

void JWPLC_CloudClass::setInterval(uint32_t ms)
{
    st()->intervalMs = ms < 2000 ? 2000 : ms > 600000 ? 600000 : ms;
}

void JWPLC_CloudClass::setProgram(const char name[])
{
    JWPLC_CloudState *s = st();
    s->take();
    copyText(s->program, sizeof(s->program), name);
    s->give();
}

bool JWPLC_CloudClass::addNumber(const char id[], const char name[], const char unit[], uint8_t decimals)
{
    return addNumber(id, name, unit, decimals, NAN, NAN);
}

bool JWPLC_CloudClass::addNumber(const char id[], const char name[], const char unit[], uint8_t decimals, float min, float max)
{
    JWPLC_CloudState *s = st();
    if (!validId(id))
        return false;
    s->take();
    int i = s->findVar(id);
    if (i < 0)
    {
        if (s->varCount >= CLOUD_MAX_VARIABLES)
        {
            s->give();
            return false;
        }
        i = s->varCount++;
        s->vars[i] = Var();
        copyText(s->vars[i].id, sizeof(s->vars[i].id), id);
    }
    Var &v = s->vars[i];
    copyText(v.name, sizeof(v.name), name);
    copyText(v.unit, sizeof(v.unit), unit);
    v.decimals = decimals > 4 ? 4 : decimals;
    v.isBool = false;
    v.hasRange = !isnan(min) && !isnan(max) && max > min;
    v.min = min;
    v.max = max;
    s->schemaDirty = true;
    s->give();
    return true;
}

bool JWPLC_CloudClass::addBool(const char id[], const char name[])
{
    if (!addNumber(id, name, "", 0))
        return false;
    JWPLC_CloudState *s = st();
    s->take();
    s->vars[s->findVar(id)].isBool = true;
    s->give();
    return true;
}

bool JWPLC_CloudClass::addAlarm(const char id[], const char text[])
{
    JWPLC_CloudState *s = st();
    if (!validId(id))
        return false;
    s->take();
    int i = s->findAlarm(id);
    if (i < 0)
    {
        if (s->alarmCount >= CLOUD_MAX_ALARMS)
        {
            s->give();
            return false;
        }
        i = s->alarmCount++;
        s->alarms[i] = Alarm();
        copyText(s->alarms[i].id, sizeof(s->alarms[i].id), id);
    }
    copyText(s->alarms[i].text, sizeof(s->alarms[i].text), text);
    s->give();
    return true;
}

void JWPLC_CloudClass::setValue(const char id[], float value)
{
    JWPLC_CloudState *s = st();
    s->take();
    const int i = s->findVar(id);
    if (i >= 0)
    {
        s->vars[i].value = value;
        s->vars[i].hasValue = true;
    }
    s->give();
}

void JWPLC_CloudClass::set(const char id[], float value) { setValue(id, value); }
void JWPLC_CloudClass::set(const char id[], double value) { setValue(id, (float)value); }
void JWPLC_CloudClass::set(const char id[], int value) { setValue(id, (float)value); }
void JWPLC_CloudClass::set(const char id[], unsigned int value) { setValue(id, (float)value); }
void JWPLC_CloudClass::set(const char id[], long value) { setValue(id, (float)value); }
void JWPLC_CloudClass::set(const char id[], unsigned long value) { setValue(id, (float)value); }
void JWPLC_CloudClass::set(const char id[], bool value) { setValue(id, value ? 1.0f : 0.0f); }

void JWPLC_CloudClass::alarm(const char id[], bool active)
{
    JWPLC_CloudState *s = st();
    s->take();
    const int i = s->findAlarm(id);
    if (i >= 0 && s->alarms[i].active != active)
    {
        s->alarms[i].active = active;
        if (s->eventCount == MAX_EVENTS)
        {
            // Demasiados cambios sin red: se pierde el más viejo.
            memmove(s->events, s->events + 1, (MAX_EVENTS - 1) * sizeof(Event));
            s->eventCount--;
        }
        s->events[s->eventCount++] = {(uint8_t)i, active, (uint32_t)millis()};
    }
    s->give();
}

uint8_t JWPLC_CloudClass::status()
{
    return state_ ? state_->status : CLOUD_OFF;
}

const char *JWPLC_CloudClass::statusString()
{
    switch (status())
    {
    case CLOUD_STARTING:
        return "CONECTANDO";
    case CLOUD_ONLINE:
        return "EN LINEA";
    case CLOUD_NO_NETWORK:
        return "SIN RED";
    case CLOUD_BAD_TOKEN:
        return "TOKEN NO VALIDO";
    case CLOUD_SERVER_ERROR:
        return "ERROR DEL SERVIDOR";
    case CLOUD_TLS_ERROR:
        return state_->tlsError[0] ? state_->tlsError : "ERROR TLS";
    default:
        return "APAGADO";
    }
}

bool JWPLC_CloudClass::isOnline()
{
    return status() == CLOUD_ONLINE;
}

uint16_t JWPLC_CloudClass::pending()
{
    return state_ ? state_->count : 0;
}

uint32_t JWPLC_CloudClass::sentCount()
{
    return state_ ? state_->sent : 0;
}
