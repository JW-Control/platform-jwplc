#include "JWPLC_Runtime.h"

#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_IdleScreen.h>
#include <string.h>

#include "rt/RtCore.h"
#include "ui/RtUI.h"

extern "C"
{
#include "jwplc_peripherals.h"
}

JWPLC_RuntimeClass JWPLC_Runtime;

namespace jwrt
{

Core core;

namespace
{
struct QueuedCommand
{
    uint8_t cmd;
    uint32_t seq;
    CommandArgs args;
};

QueueHandle_t g_queue = nullptr;
TaskHandle_t g_task = nullptr;
volatile uint32_t g_postSeq = 0;
portMUX_TYPE g_seqMux = portMUX_INITIALIZER_UNLOCKED;

const uint32_t SCAN_PERIOD_MS = 10;
const uint32_t RETAIN_PERIOD_MS = 500;

uint8_t g_lastQ = 0xFF;
uint64_t g_lastAlarms = 0;
uint32_t g_lastRetainSave = 0;
bool g_retainPending = false;

// Día de la semana de una fecha (0 = lunes).
uint8_t weekday(uint16_t y, uint8_t m, uint8_t d)
{
    static const uint8_t T[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3)
    {
        y -= 1;
    }
    const uint8_t sunday0 = (uint8_t)((y + y / 4 - y / 100 + y / 400 + T[(m - 1) % 12] + d) % 7);
    return (uint8_t)((sunday0 + 6) % 7);
}

void logEvent(uint8_t code, uint8_t arg)
{
    Event e;
    memset(&e, 0, sizeof(e));
    e.code = code;
    e.arg = arg;
    e.uptimeS = millis() / 1000;
    const JWPLC_RTCState *rtc = jwplcGetRTCState();
    if (rtc && rtc->valid && rtc->year >= 2000)
    {
        e.year = (uint8_t)(rtc->year - 2000);
        e.month = rtc->month;
        e.day = rtc->day;
        e.hour = rtc->hour;
        e.minute = rtc->minute;
        e.second = rtc->second;
    }
    core.store.addEvent(e);
}

void setResult(bool ok, const char text[])
{
    strncpy(core.resultText, text, sizeof(core.resultText) - 1);
    core.resultText[sizeof(core.resultText) - 1] = 0;
    core.result = ok ? RESULT_OK : RESULT_FAIL;
}

void writeOutputs(uint8_t q)
{
    if (q != g_lastQ)
    {
        JWPLC_writeOutputs(q);
        g_lastQ = q;
    }
}

void saveRetainedNow()
{
    uint32_t values[MAX_BLOCKS];
    for (uint8_t b = 0; b < MAX_BLOCKS; b++)
    {
        values[b] = core.engine.retained(b);
    }
    core.store.saveRetained(values);
    core.engine.clearRetainedDirty();
    g_retainPending = false;
    g_lastRetainSave = millis();
}

void restoreRetained()
{
    uint32_t values[MAX_BLOCKS];
    if (core.store.loadRetained(values))
    {
        for (uint8_t b = 0; b < MAX_BLOCKS; b++)
        {
            core.engine.setRetained(b, values[b]);
        }
    }
}

void enterRun()
{
    core.engine.reset();
    restoreRetained();
    core.testMode = false;
    core.mode = MODE_RUN;
    JWPLC_Display.setRunLed(true);
    if (!core.store.config().lastRun)
    {
        core.store.config().lastRun = 1;
        core.store.saveConfig();
    }
    logEvent(EV_RUN, 0);
}

void enterStop()
{
    if (core.mode == MODE_RUN)
    {
        saveRetainedNow();
    }
    core.mode = MODE_STOP;
    writeOutputs(0);
    JWPLC_Display.setRunLed(false);
    JWPLC_Display.setErrCode("");
    g_lastAlarms = 0;
    if (core.store.config().lastRun)
    {
        core.store.config().lastRun = 0;
        core.store.saveConfig();
    }
    logEvent(EV_STOP, 0);
}

// Validación mínima antes de cargar: tipos conocidos y fuentes válidas.
bool validate(const Program &p, char why[], size_t size)
{
    if (p.count > MAX_BLOCKS)
    {
        snprintf(why, size, "Demasiados bloques");
        return false;
    }
    for (uint8_t b = 0; b < p.count; b++)
    {
        if (!typeInfo(p.blocks[b].type))
        {
            snprintf(why, size, "B%02u: tipo desconocido", b + 1);
            return false;
        }
        for (uint8_t k = 0; k < MAX_PINS; k++)
        {
            const uint8_t s = p.blocks[b].in[k];
            if (srcIsBlock(s) && srcBlock(s) >= p.count)
            {
                snprintf(why, size, "B%02u: fuente invalida", b + 1);
                return false;
            }
        }
    }
    return true;
}

void doCommand(const QueuedCommand &qc)
{
    const CommandArgs &a = qc.args;
    switch (qc.cmd)
    {
    case CMD_RUN:
        if (core.mode != MODE_RUN)
        {
            enterRun();
        }
        setResult(true, "RUN");
        break;
    case CMD_STOP:
        if (core.mode != MODE_STOP)
        {
            enterStop();
        }
        setResult(true, "STOP");
        break;
    case CMD_APPLY_DRAFT:
    {
        char why[40];
        if (!validate(core.draft, why, sizeof(why)))
        {
            setResult(false, why);
            break;
        }
        const bool wasRun = core.mode == MODE_RUN;
        if (wasRun)
        {
            saveRetainedNow();
        }
        if (!core.store.saveProgram(core.draft))
        {
            setResult(false, "No se pudo escribir la FRAM");
            break;
        }
        core.engine.load(core.draft);
        restoreRetained();
        logEvent(EV_PROGRAM_SAVED, core.draft.count);
        setResult(true, "Programa guardado en FRAM");
        break;
    }
    case CMD_SET_PARAM:
    {
        core.engine.setParam(a.block, a.p1, a.p2, a.aux);
        Program &p = core.draft;
        p = core.engine.program();
        if (core.store.saveProgram(p))
        {
            logEvent(EV_PARAM, a.block + 1);
            setResult(true, "Parametro guardado");
        }
        else
        {
            setResult(false, "No se pudo escribir la FRAM");
        }
        break;
    }
    case CMD_ERASE_PROGRAM:
        clearProgram(core.draft, "Sin nombre");
        if (core.mode == MODE_RUN)
        {
            enterStop();
        }
        core.store.saveProgram(core.draft);
        core.store.clearRetained();
        core.engine.load(core.draft);
        logEvent(EV_PROGRAM_ERASED, 0);
        setResult(true, "Programa borrado");
        break;
    case CMD_LOAD_DEMO:
        demoProgram(core.draft);
        core.store.saveProgram(core.draft);
        core.store.clearRetained();
        core.engine.load(core.draft);
        logEvent(EV_PROGRAM_SAVED, core.draft.count);
        setResult(true, "Programa de ejemplo cargado");
        break;
    case CMD_SD_SAVE:
    {
        char path[48];
        if (core.store.sdSave(core.engine.program(), path, sizeof(path)))
        {
            logEvent(EV_SD_SAVED, 0);
            setResult(true, path);
        }
        else
        {
            setResult(false, "Sin tarjeta o error de escritura");
        }
        break;
    }
    case CMD_SD_LIST:
        core.sdFileCount = core.store.sdList(core.sdFiles, 12);
        setResult(core.store.sdReady(), core.store.sdReady() ? "" : "Sin tarjeta");
        break;
    case CMD_SD_LOAD:
    {
        Program p;
        if (!core.store.sdLoad(a.text, p))
        {
            setResult(false, "Archivo invalido");
            break;
        }
        if (core.mode == MODE_RUN)
        {
            enterStop();
        }
        core.draft = p;
        core.store.saveProgram(p);
        core.store.clearRetained();
        core.engine.load(p);
        logEvent(EV_SD_LOADED, p.count);
        setResult(true, "Programa cargado (STOP)");
        break;
    }
    case CMD_SD_EXPORT:
    {
        char path[48];
        const bool ok = core.store.sdExportEvents(path, sizeof(path));
        setResult(ok, ok ? path : "Sin tarjeta o error de escritura");
        break;
    }
    case CMD_SET_CLOCK:
    {
        JW_RTC::DateTime dt;
        dt.year = (uint16_t)(a.p1 / 10000);
        dt.month = (uint8_t)((a.p1 / 100) % 100);
        dt.day = (uint8_t)(a.p1 % 100);
        dt.hour = (uint8_t)(a.p2 / 10000);
        dt.minute = (uint8_t)((a.p2 / 100) % 100);
        dt.second = (uint8_t)(a.p2 % 100);
        dt.dayOfWeek = (uint8_t)((weekday(dt.year, dt.month, dt.day) + 1) % 7 + 1); // 1 = domingo
        const bool ok = JWPLC_RTC.write(dt);
        if (ok)
        {
            JWPLC_RTC.clearOscillatorStopFlag();
            jwplcSystemTickRTC();
            logEvent(EV_CLOCK_SET, 0);
        }
        setResult(ok, ok ? "Reloj ajustado" : "El RTC no responde");
        break;
    }
    case CMD_SAVE_CONFIG:
        setResult(core.store.saveConfig(), "Configuracion guardada");
        break;
    case CMD_CLEAR_EVENTS:
        core.store.clearEvents();
        setResult(true, "Eventos borrados");
        break;
    case CMD_CLEAR_RETAIN:
        core.store.clearRetained();
        core.engine.reset();
        setResult(true, "Retentivos en cero");
        break;
    case CMD_EVENT:
        logEvent(a.block, a.aux);
        setResult(true, "");
        break;
    case CMD_TEST_Q:
        if (core.mode == MODE_STOP)
        {
            core.testMode = true;
            core.testQ = a.aux;
            writeOutputs(a.aux);
        }
        setResult(core.mode == MODE_STOP, "");
        break;
    case CMD_TEST_END:
        core.testMode = false;
        if (core.mode == MODE_STOP)
        {
            writeOutputs(0);
        }
        setResult(true, "");
        break;
    case CMD_NET_REFRESH:
    {
        core.linkUp = JWPLC_Ethernet.isReady() && JWPLC_Ethernet.linkUp();
        if (JWPLC_Ethernet.isReady())
        {
            snprintf(core.ip, sizeof(core.ip), "%s", JWPLC_Ethernet.localIP().toString().c_str());
            snprintf(core.mask, sizeof(core.mask), "%s", JWPLC_Ethernet.subnetMask().toString().c_str());
            snprintf(core.gateway, sizeof(core.gateway), "%s", JWPLC_Ethernet.gatewayIP().toString().c_str());
        }
        else
        {
            strcpy(core.ip, "-");
            strcpy(core.mask, "-");
            strcpy(core.gateway, "-");
        }
        setResult(true, "");
        break;
    }
    default:
        break;
    }
    core.doneSeq = qc.seq;
}

void updateAlarms()
{
    const uint64_t a = core.mode == MODE_RUN ? core.engine.alarms() : 0;
    if (a == g_lastAlarms)
    {
        return;
    }
    const uint64_t changed = a ^ g_lastAlarms;
    for (uint8_t b = 0; b < MAX_BLOCKS; b++)
    {
        if ((changed >> b) & 1)
        {
            logEvent(((a >> b) & 1) ? EV_ALARM_ON : EV_ALARM_OFF, b + 1);
        }
    }
    g_lastAlarms = a;
    // ERR de la pantalla RUN: "A" + número del primer aviso activo.
    char code[5] = "";
    for (uint8_t b = 0; b < MAX_BLOCKS; b++)
    {
        if ((a >> b) & 1)
        {
            snprintf(code, sizeof(code), "A%02u", b + 1);
            break;
        }
    }
    JWPLC_Display.setErrCode(code);
}

void task(void *)
{
    TickType_t last = xTaskGetTickCount();
    for (;;)
    {
        QueuedCommand qc;
        while (xQueueReceive(g_queue, &qc, 0) == pdTRUE)
        {
            doCommand(qc);
        }

        if (core.mode == MODE_RUN)
        {
            const uint32_t t0 = micros();
            core.engine.scan(JWPLC_readInputs(), millis(), clockNow());
            writeOutputs(core.engine.outputs());
            const uint32_t us = micros() - t0;
            core.scanUsLast = us;
            if (us > core.scanUsMax)
            {
                core.scanUsMax = us;
            }
            core.scanUsAvg = core.scanCount == 0 ? us : (core.scanUsAvg * 15 + us) / 16;
            core.scanCount = core.scanCount + 1;

            if (core.engine.retainedDirty())
            {
                g_retainPending = true;
                core.engine.clearRetainedDirty();
            }
            if (g_retainPending && millis() - g_lastRetainSave >= RETAIN_PERIOD_MS)
            {
                saveRetainedNow();
            }
        }
        else if (!core.testMode)
        {
            writeOutputs(0);
        }
        updateAlarms();

        // Vuelta a la pantalla RUN (fuera del batch de pantalla).
        if (rtui::takeIdleRequest())
        {
            JWPLC_Display.goIdle();
        }
        else if (!JWPLC_Display.isIdleMode() && !rtui::editing && core.store.config().idleMinutes &&
                 millis() - rtui::lastKeyMs() > core.store.config().idleMinutes * 60000UL)
        {
            JWPLC_Display.goIdle();
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(SCAN_PERIOD_MS));
    }
}
} // namespace

uint32_t post(Command cmd, const CommandArgs &args)
{
    QueuedCommand qc;
    qc.cmd = cmd;
    qc.args = args;
    portENTER_CRITICAL(&g_seqMux);
    qc.seq = ++g_postSeq;
    portEXIT_CRITICAL(&g_seqMux);
    core.result = 0;
    if (!g_queue || xQueueSend(g_queue, &qc, 0) != pdTRUE)
    {
        core.doneSeq = qc.seq; // cola llena: la orden se descarta
        core.result = RESULT_FAIL;
    }
    return qc.seq;
}

uint32_t post(Command cmd)
{
    CommandArgs a;
    memset(&a, 0, sizeof(a));
    return post(cmd, a);
}

bool done(uint32_t seq)
{
    return (int32_t)(core.doneSeq - seq) >= 0;
}

ClockNow clockNow()
{
    ClockNow c = {false, 0, 0};
    const JWPLC_RTCState *rtc = jwplcGetRTCState();
    if (rtc && rtc->valid && rtc->year >= 2000)
    {
        c.valid = true;
        c.weekday = weekday(rtc->year, rtc->month, rtc->day);
        c.minute = (uint16_t)(rtc->hour * 60 + rtc->minute);
    }
    return c;
}

bool clockText(char out[], size_t size)
{
    const JWPLC_RTCState *rtc = jwplcGetRTCState();
    if (!rtc || !rtc->valid)
    {
        snprintf(out, size, "--:--");
        return false;
    }
    snprintf(out, size, "%02u:%02u", rtc->hour, rtc->minute);
    return true;
}

bool dateText(char out[], size_t size)
{
    static const char *const DAYS[7] = {"Lu", "Ma", "Mi", "Ju", "Vi", "Sa", "Do"};
    const JWPLC_RTCState *rtc = jwplcGetRTCState();
    if (!rtc || !rtc->valid || rtc->year < 2000)
    {
        snprintf(out, size, "--/--/--");
        return false;
    }
    snprintf(out, size, "%s %02u/%02u/%02u", DAYS[weekday(rtc->year, rtc->month, rtc->day)],
             rtc->day, rtc->month, (unsigned)(rtc->year % 100));
    return true;
}

} // namespace jwrt

using namespace jwrt;

bool JWPLC_RuntimeClass::begin()
{
    if (g_task)
    {
        return true;
    }
    core.framOk = core.store.begin();

    Program p;
    bool have = core.framOk && core.store.loadProgram(p);
    if (!have)
    {
        if (_demoIfEmpty)
        {
            demoProgram(p);
        }
        else
        {
            clearProgram(p, "Sin nombre");
        }
        if (core.framOk)
        {
            core.store.saveProgram(p);
            core.store.clearRetained();
        }
    }
    core.engine.load(p);
    core.draft = p;
    logEvent(EV_POWER_ON, 0);

    // Pantalla: la RUN es la IDLE de siempre, con el título del Runtime.
    JWPLCIdleScreen::setTitle("JWPLC Runtime");
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED); // la UI decide
    JWPLC_Display.setUserRefreshPeriodMs(40);
    JWPLC_Display.setRunLed(false);
    JWPLC_Display.clearPendingInput();
    rtui::begin();

    g_queue = xQueueCreate(8, sizeof(QueuedCommand));
    JWPLC_writeOutputs(0);
    g_lastQ = 0;

    const Config &cfg = core.store.config();
    const bool start = p.count > 0 &&
                       (cfg.startMode == START_RUN || (cfg.startMode == START_LAST && cfg.lastRun));
    if (start)
    {
        enterRun();
    }

    xTaskCreatePinnedToCore(task, "jwplc_runtime", 6144, nullptr, 2, &g_task, 1);
    return g_task != nullptr;
}

void JWPLC_RuntimeClass::run()
{
    post(CMD_RUN);
}

void JWPLC_RuntimeClass::stop()
{
    post(CMD_STOP);
}

bool JWPLC_RuntimeClass::isRunning() const
{
    return core.mode == MODE_RUN;
}

bool JWPLC_RuntimeClass::output(uint8_t number) const
{
    return number >= 1 && number <= NUM_Q && ((core.engine.outputs() >> (number - 1)) & 1);
}

bool JWPLC_RuntimeClass::marker(uint8_t number) const
{
    return number >= 1 && number <= NUM_M && ((core.engine.markers() >> (number - 1)) & 1);
}

uint8_t JWPLC_RuntimeClass::blockCount() const
{
    return core.engine.program().count;
}
