#include "JWPLC_Buzzer.h"

#include "jwplc_hardware_config.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// =====================================================
// JWPLC_Buzzer: buzzer integrado sin delay() ni service()
// =====================================================
// Misma técnica que los ejemplos Display_Tetris/Display_FlappyBird:
// ledcWriteTone() fija la frecuencia y ledcWrite() el duty (volumen). La
// diferencia es que aquí el paso de nota lo da un esp_timer de una sola
// vez, así que no depende de que el sketch llame a nada en loop().
//
// El GPIO se toma con LEDC en el primer uso (no en el arranque), para no
// configurar el pin si la aplicación nunca usa el buzzer.

JWPLC_BuzzerClass JWPLC_Buzzer;

#if JWPLC_HAS_BUZZER

namespace
{
    enum Phase : uint8_t
    {
        PHASE_IDLE = 0,
        PHASE_TONE,     // tone()/beep() en curso
        PHASE_NOTE_ON,  // nota de melodía sonando
        PHASE_NOTE_GAP  // corte entre notas
    };

    constexpr uint8_t PWM_RES_BITS = 8;
    constexpr uint32_t PWM_BASE_HZ = 2000;

    struct State
    {
        SemaphoreHandle_t lock = nullptr;
        esp_timer_handle_t timer = nullptr;
        bool attached = false;
        bool outputOn = false;

        uint8_t volume = 32;
        bool enabled = true;

        Phase phase = PHASE_IDLE;
        int64_t dueUs = 0;

        // Melodía
        const JWPLC_BuzzerNote *notes = nullptr;
        size_t count = 0;
        size_t index = 0;
        bool loop = false;
        bool melody = false;    // hay una melodía (activa o suspendida por tone)
        uint16_t speed = 100;
        uint32_t noteDurMs = 0;
        uint32_t noteGateMs = 0;
    };

    State g;
    portMUX_TYPE g_createMux = portMUX_INITIALIZER_UNLOCKED;

    void onTimer(void *);

    // Crea el mutex y el timer una sola vez (seguro ante dos tareas a la vez).
    bool ensureCreated()
    {
        if (g.lock != nullptr && g.timer != nullptr)
            return true;

        SemaphoreHandle_t lock = xSemaphoreCreateMutex();
        if (lock == nullptr)
            return false;

        bool created = false;
        portENTER_CRITICAL(&g_createMux);
        if (g.lock == nullptr)
        {
            g.lock = lock;
            created = true;
        }
        portEXIT_CRITICAL(&g_createMux);
        if (!created)
            vSemaphoreDelete(lock);

        xSemaphoreTake(g.lock, portMAX_DELAY);
        if (g.timer == nullptr)
        {
            esp_timer_create_args_t args = {};
            args.callback = &onTimer;
            args.arg = nullptr;
            args.dispatch_method = ESP_TIMER_TASK;
            args.name = "jwplc_buzzer";
            esp_timer_create(&args, &g.timer);
        }
        xSemaphoreGive(g.lock);
        return g.timer != nullptr;
    }

    void ensureAttached()
    {
        if (g.attached)
            return;
        pinMode(JWPLC_BUZZER_PIN, OUTPUT);
        digitalWrite(JWPLC_BUZZER_PIN, LOW);
        ledcAttach(JWPLC_BUZZER_PIN, PWM_BASE_HZ, PWM_RES_BITS);
        ledcWrite(JWPLC_BUZZER_PIN, 0);
        g.attached = true;
    }

    void outputOff()
    {
        if (g.attached && g.outputOn)
            ledcWrite(JWPLC_BUZZER_PIN, 0);
        g.outputOn = false;
    }

    void outputTone(uint16_t freq)
    {
        if (freq == 0 || !g.enabled || g.volume == 0)
        {
            outputOff();
            return;
        }
        ensureAttached();
        ledcWriteTone(JWPLC_BUZZER_PIN, freq); // fija la frecuencia (duty 50 %)
        ledcWrite(JWPLC_BUZZER_PIN, g.volume); // y el duty = volumen
        g.outputOn = true;
    }

    // Con el mutex tomado.
    void schedule(uint32_t ms)
    {
        esp_timer_stop(g.timer);
        const uint64_t us = (uint64_t)(ms == 0 ? 1 : ms) * 1000ULL;
        g.dueUs = esp_timer_get_time() + (int64_t)us;
        esp_timer_start_once(g.timer, us);
    }

    void cancelTimer()
    {
        esp_timer_stop(g.timer);
        g.dueUs = 0;
    }

    // Con el mutex tomado: arranca la nota g.index de la melodía.
    void startNote()
    {
        const JWPLC_BuzzerNote &n = g.notes[g.index];
        uint32_t dur = ((uint32_t)n.durMs * 100UL) / (g.speed ? g.speed : 100);
        if (dur < 20)
            dur = 20;
        g.noteDurMs = dur;
        g.noteGateMs = (dur * 90UL) / 100UL;

        if (n.freq == 0)
        {
            outputOff();
            g.phase = PHASE_NOTE_GAP;
            g.noteGateMs = 0;
            schedule(dur);
            return;
        }
        outputTone(n.freq);
        g.phase = PHASE_NOTE_ON;
        schedule(g.noteGateMs);
    }

    // Con el mutex tomado: pasa a la nota siguiente o termina.
    void nextNote()
    {
        g.index++;
        if (g.index >= g.count)
        {
            if (!g.loop)
            {
                g.melody = false;
                g.phase = PHASE_IDLE;
                outputOff();
                return;
            }
            g.index = 0;
        }
        startNote();
    }

    void onTimer(void *)
    {
        if (g.lock == nullptr)
            return;
        xSemaphoreTake(g.lock, portMAX_DELAY);

        // Un tone()/stop() posterior reprogramó o canceló: este disparo quedó viejo.
        if (g.dueUs == 0 || esp_timer_get_time() + 200 < g.dueUs)
        {
            xSemaphoreGive(g.lock);
            return;
        }
        g.dueUs = 0;

        switch (g.phase)
        {
        case PHASE_TONE:
            outputOff();
            if (g.melody && g.count > 0)
                startNote(); // retoma la melodía desde la nota en curso
            else
                g.phase = PHASE_IDLE;
            break;

        case PHASE_NOTE_ON:
            outputOff();
            g.phase = PHASE_NOTE_GAP;
            if (g.noteDurMs > g.noteGateMs)
                schedule(g.noteDurMs - g.noteGateMs);
            else
                nextNote();
            break;

        case PHASE_NOTE_GAP:
            nextNote();
            break;

        default:
            break;
        }

        xSemaphoreGive(g.lock);
    }

    bool lock()
    {
        if (!ensureCreated())
            return false;
        xSemaphoreTake(g.lock, portMAX_DELAY);
        return true;
    }

    void unlock()
    {
        xSemaphoreGive(g.lock);
    }
}

void JWPLC_BuzzerClass::setVolume(uint8_t level)
{
    if (!lock())
        return;
    g.volume = level;
    if (g.outputOn)
    {
        if (level == 0 || !g.enabled)
            outputOff();
        else
            ledcWrite(JWPLC_BUZZER_PIN, level);
    }
    unlock();
}

uint8_t JWPLC_BuzzerClass::volume() const
{
    return g.volume;
}

void JWPLC_BuzzerClass::setEnabled(bool enabled)
{
    if (!lock())
        return;
    g.enabled = enabled;
    if (!enabled)
        outputOff();
    unlock();
}

bool JWPLC_BuzzerClass::enabled() const
{
    return g.enabled;
}

bool JWPLC_BuzzerClass::tone(uint16_t freq, uint16_t durMs)
{
    if (!lock())
        return false;
    cancelTimer();
    outputTone(freq);
    g.phase = PHASE_TONE;
    if (durMs > 0)
        schedule(durMs);
    unlock();
    return true;
}

bool JWPLC_BuzzerClass::beep(uint16_t durMs, uint16_t freq)
{
    return tone(freq, durMs == 0 ? 60 : durMs);
}

bool JWPLC_BuzzerClass::play(const JWPLC_BuzzerNote *notes, size_t count, bool loop, uint16_t speedPercent)
{
    if (notes == nullptr || count == 0)
        return false;
    if (!lock())
        return false;
    cancelTimer();
    g.notes = notes;
    g.count = count;
    g.index = 0;
    g.loop = loop;
    g.speed = speedPercent == 0 ? 100 : speedPercent;
    g.melody = true;
    startNote();
    unlock();
    return true;
}

void JWPLC_BuzzerClass::stop()
{
    if (!lock())
        return;
    cancelTimer();
    g.melody = false;
    g.phase = PHASE_IDLE;
    outputOff();
    unlock();
}

bool JWPLC_BuzzerClass::isPlaying() const
{
    return g.melody;
}

bool JWPLC_BuzzerClass::isBusy() const
{
    return g.phase != PHASE_IDLE;
}

#else // !JWPLC_HAS_BUZZER: variante sin buzzer, la API compila y no hace nada.

void JWPLC_BuzzerClass::setVolume(uint8_t) {}
uint8_t JWPLC_BuzzerClass::volume() const { return 0; }
void JWPLC_BuzzerClass::setEnabled(bool) {}
bool JWPLC_BuzzerClass::enabled() const { return false; }
bool JWPLC_BuzzerClass::tone(uint16_t, uint16_t) { return false; }
bool JWPLC_BuzzerClass::beep(uint16_t, uint16_t) { return false; }
bool JWPLC_BuzzerClass::play(const JWPLC_BuzzerNote *, size_t, bool, uint16_t) { return false; }
void JWPLC_BuzzerClass::stop() {}
bool JWPLC_BuzzerClass::isPlaying() const { return false; }
bool JWPLC_BuzzerClass::isBusy() const { return false; }

#endif
