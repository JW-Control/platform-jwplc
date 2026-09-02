/*
  Gate 7NB.3C1 - Soak distribuido con split de cores

  Arquitectura de esta revision:

    Core 1 determinista:
      - loop Arduino / logica de acceptance
      - Modbus RTU
      - TCA / ScanIO del core JWPLC
      - scheduler de salidas sincronizadas
      - RTC
      - JWPLC_Ethernet.service() cooperativo, solo cuando W5500 esta libre
      - FRAM / SD (se difieren mientras W5500 posee SPI)
      - TFT (el driver ya usa mutex SPI y puede diferir refresco)

    Core 0 bloqueante:
      - worker WiFi HTTP existente
      - worker W5500 de esta revision:
          * HTTP Ethernet
          * NTP Ethernet

  El W5500 es el unico periferico SPI cuyo trabajo bloqueante se mueve de core.
  TFT/FRAM/SD permanecen donde estan.

  IMPORTANTE:
  - No modifica JWPLC_ModbusRTU.
  - JWPLC_Ethernet.service() conserva su estado/cache en Core1. No se comparte
    el state machine de la libreria entre cores.
  - El worker W5500 reserva el mutex SPI durante una operacion HTTP/NTP
    completa. Esto garantiza integridad cross-core sin tocar aun el backend
    W5x00. En este gate se mide el beneficio CPU/Core1 primero; una revision
    posterior puede granularizar la retencion del mutex si hace falta.
  - Los datos de trabajo/resultados cruzan cores mediante FreeRTOS queues.
    No se usa volatile como mecanismo de mensajeria.
*/

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <jwplc_spi_bus.h>
#include <esp_err.h>
#include <driver/gpio.h>

extern "C"
{
#include "jwplc_i2c_bridge.h"
}

// ---------------------------------------------------------------------------
// Bloquear exclusivamente los ON del oscilador I/O legacy del soak base.
// Los OFF de seguridad siguen pasando. El scheduler REV6 usa la API de banco
// directa del JWPLC para aplicar sus bitmaps sincronizados.
// ---------------------------------------------------------------------------
static void rev6LegacyDigitalWriteBlock(
    const uint16_t *pins,
    uint8_t count,
    uint8_t bitmap)
{
    if (bitmap == 0)
        jwplc_digitalWriteBlock(pins, count, bitmap);
}

#undef digitalWriteBlock
#define digitalWriteBlock(pins, bitmap) \
    rev6LegacyDigitalWriteBlock( \
        (pins), \
        (uint8_t)(sizeof(pins) / sizeof((pins)[0])), \
        (uint8_t)(bitmap))

#define setup alpha7AsyncBaseSetup
#define loop alpha7AsyncBaseLoop
#include "../20_alpha7_distributed_soak_2h_gate_7NB_async_master/20_alpha7_distributed_soak_2h_gate_7NB_async_master.ino"
#undef setup
#undef loop

#undef digitalWriteBlock
#define digitalWriteBlock(pins, bitmap) \
    jwplc_digitalWriteBlock( \
        (pins), \
        (uint8_t)(sizeof(pins) / sizeof((pins)[0])), \
        (uint8_t)(bitmap))

// ============================================================================
// Control del tick Ethernet automatico del core
// ============================================================================
// Se desactiva el tick automatico de jwplcSystemTask para poder decidir desde
// el loop Core1 si service() puede tocar W5500. Si Core0 esta dentro de un job
// HTTP/NTP, el service cooperativo simplemente se difiere. Asi evitamos que
// linkStatus() confunda BUS_LOCK_TIMEOUT con LINK_OFF durante el job.
uint32_t getJWPLCEthernetPeriod_ms(void)
{
    return 0xFFFFFFFFUL;
}

// ============================================================================
// Configuracion REV6
// ============================================================================

static constexpr uint16_t CMD_SYNC_IO_PHASE = 5;

static constexpr bool REV6_BUZZER_ENABLED = true;
static constexpr uint8_t REV6_BUZZER_VOLUME = 24; // 0..255
static constexpr uint16_t REV6_BUZZER_NOTE_MS = 115;
static constexpr uint8_t REV6_BUZZER_PWM_BITS = 8;

static constexpr uint32_t REV6_SYNC_LEAD_US = 300000UL;
static constexpr uint32_t REV6_TRIGGER_COMP_US = 12000UL;
static constexpr uint32_t REV6_MIN_REMAIN_US = 30000UL;
static constexpr uint32_t REV6_SYNC_RETRY_MS = 250UL;
// Separacion deliberada entre FC06 del scheduler. El Slave usa frameGap=2 ms;
// 4 ms da margen para que todos los nodos multidrop cierren request/response
// ajenos antes de iniciar el siguiente comando, sin bloquear el loop Core1.
static constexpr uint32_t REV6_MODBUS_INTER_TX_GAP_US = 4000UL;
static constexpr uint32_t REV6_MODBUS_INTER_TX_GAP_MS = 4UL;
// El cambio de runtime cooperativo a los comandos Sync del STOP usa el mismo
// silencio. Evita lanzar HR_ETH_OWNER inmediatamente despues de la ultima
// respuesta del scheduler/poll que acaba de drenarse.
static constexpr uint32_t REV6_STOP_QUIET_GAP_US =
    REV6_MODBUS_INTER_TX_GAP_US;
static constexpr uint32_t REV6_STOP_QUIET_GAP_MS =
    REV6_MODBUS_INTER_TX_GAP_MS;
// ETHNEXT usa la misma transicion cooperativo->Sync que STOP. Ademas, despues
// de publicar owner=NONE se deja una gracia suficiente para que un HTTP o NTP
// ya iniciado en el nodo saliente libere W5500 antes de pedir mover el cable.
static constexpr uint32_t REV6_ETHNEXT_QUIET_GAP_US =
    REV6_MODBUS_INTER_TX_GAP_US;
static constexpr uint32_t REV6_ETHNEXT_QUIET_GAP_MS =
    REV6_MODBUS_INTER_TX_GAP_MS;
static constexpr uint32_t REV6_ETHNEXT_RELEASE_GRACE_MS = 2300UL;

// Diagnostico I2C del acceptance. El bus real del JWPLC Basic usa SDA=21,
// SCL=22, TCA6424A=0x22 y RTC=0x68. 0x23 se consulta solo manualmente como
// direccion alternativa del TCA; su ausencia NO es falla.
static constexpr uint8_t REV6_I2C_TCA_ADDR = 0x22;
static constexpr uint8_t REV6_I2C_TCA_ALT_ADDR = 0x23;
static constexpr uint8_t REV6_I2C_RTC_ADDR = 0x68;
static constexpr uint32_t REV6_I2C_LINE_SAMPLE_MS = 1000UL;
static constexpr uint32_t REV6_I2C_COMMISSION_PERIOD_MS = 5000UL;
static constexpr uint32_t REV6_I2C_WATCH_PERIOD_MS = 30000UL;
static constexpr uint32_t REV6_I2C_BUSY_RETRY_MS = 500UL;
static constexpr UBaseType_t REV6_SYNC_TASK_PRIORITY = 3;
static constexpr uint32_t REV6_SYNC_TASK_STACK = 4096UL;
static constexpr BaseType_t REV6_SYNC_TASK_CORE = 1;

static constexpr uint32_t ETH_WORKER_STACK_BYTES = 7168UL;
static constexpr UBaseType_t ETH_WORKER_PRIORITY = 1;
static constexpr BaseType_t ETH_WORKER_CORE = 0;
static constexpr uint32_t ETH_WORKER_SERVICE_PERIOD_MS = 20UL;
static constexpr uint32_t ETH_WORKER_BUS_ACQUIRE_MS = 250UL;
static constexpr size_t ETH_WORKER_BODY_MAX = 512;
static constexpr EventBits_t ETH_WORKER_SPI_BUSY_BIT =
    (EventBits_t)(1U << 0);

// ============================================================================
// Patrones distribuidos
// ============================================================================

struct Rev6PatternStep
{
    uint8_t m2;
    uint8_t s1;
    uint8_t s2;
    uint16_t holdMs;
    const char *name;
};

// El hold minimo queda por encima de IO_VERIFY_DEADLINE_MS=500 ms para que
// cada mascara pueda ser realmente verificada antes de pasar a la siguiente.
static const Rev6PatternStep REV6_SHOW_PATTERN[] = {
    {0x00, 0x00, 0x00, 650, "all-off"},
    {0xFF, 0xFF, 0xFF, 700, "all-on"},
    {0x00, 0x00, 0x00, 650, "all-off"},

    {0x01, 0x01, 0x01, 650, "chase-q0"},
    {0x02, 0x02, 0x02, 650, "chase-q1"},
    {0x04, 0x04, 0x04, 650, "chase-q2"},
    {0x08, 0x08, 0x08, 650, "chase-q3"},
    {0x10, 0x10, 0x10, 650, "chase-q4"},
    {0x20, 0x20, 0x20, 650, "chase-q5"},
    {0x40, 0x40, 0x40, 650, "chase-q6"},
    {0x80, 0x80, 0x80, 700, "chase-q7"},

    {0xAA, 0x55, 0xAA, 700, "alternate-a"},
    {0x55, 0xAA, 0x55, 700, "alternate-b"},
    {0xAA, 0x55, 0xAA, 700, "alternate-a"},
    {0x55, 0xAA, 0x55, 700, "alternate-b"},

    {0xFF, 0x00, 0x00, 700, "wave-m2"},
    {0x00, 0xFF, 0x00, 700, "wave-s1"},
    {0x00, 0x00, 0xFF, 700, "wave-s2"},
    {0xFF, 0x00, 0x00, 700, "wave-m2"},

    {0x81, 0x42, 0x24, 700, "mirror-1"},
    {0x42, 0x24, 0x18, 700, "mirror-2"},
    {0x24, 0x18, 0x24, 700, "mirror-3"},
    {0x18, 0x24, 0x42, 700, "mirror-4"},
    {0x24, 0x42, 0x81, 750, "mirror-5"},

    {0x00, 0x00, 0x00, 750, "all-off"}
};

static constexpr size_t REV6_SHOW_STEP_COUNT =
    sizeof(REV6_SHOW_PATTERN) / sizeof(REV6_SHOW_PATTERN[0]);

static constexpr uint16_t REV6_NOTE_PALETTE[] = {
    392, 440, 494, 523, 587, 659,
    698, 784, 880, 988, 1047, 1175
};

// ============================================================================
// Estado scheduler I/O
// ============================================================================

static constexpr uint8_t REV6_MODE_SHOW = 0;
static constexpr uint8_t REV6_MODE_CLACK = 1;

static constexpr uint8_t REV6_STAGE_IDLE = 0;
static constexpr uint8_t REV6_STAGE_CODE_S1_WAIT = 1;
static constexpr uint8_t REV6_STAGE_CODE_S2_WAIT = 2;
static constexpr uint8_t REV6_STAGE_MASK_S1_WAIT = 3;
static constexpr uint8_t REV6_STAGE_MASK_S2_WAIT = 4;
static constexpr uint8_t REV6_STAGE_DELAY_S1_WAIT = 5;
static constexpr uint8_t REV6_STAGE_TRIGGER_S1_WAIT = 6;
static constexpr uint8_t REV6_STAGE_DELAY_S2_WAIT = 7;
static constexpr uint8_t REV6_STAGE_TRIGGER_S2_WAIT = 8;
static constexpr uint8_t REV6_STAGE_WAIT_APPLY = 9;

struct Rev6PendingMasterRequest
{
    bool pending = false;
    bool dynamicDelay = false;
    uint8_t slaveId = 0;
    uint16_t reg = 0;
    uint16_t value = 0;
    uint8_t nextStage = REV6_STAGE_IDLE;
    const char *abortReason = nullptr;
};

static bool rev6SyncTakeover = false;
static bool rev6SyncCodesPrepared = false;
static bool rev6SyncTxnActive = false;
static uint8_t rev6SyncMode = REV6_MODE_SHOW;
static uint8_t rev6SyncStage = REV6_STAGE_IDLE;
static size_t rev6PatternIndex = 0;
static bool rev6ClackOn = false;

static uint8_t rev6M2Mask = 0;
static uint8_t rev6S1Mask = 0;
static uint8_t rev6S2Mask = 0;
static uint16_t rev6HoldMs = 650;
static const char *rev6PatternName = "idle";

static uint16_t rev6Sequence = 0;
static uint16_t rev6SlaveLastTrigger = 0;
static uint32_t rev6TargetUs = 0;
static uint32_t rev6NextStepMs = 0;
static uint32_t rev6NextTxnAllowedUs = 0;
static uint32_t rev6TxnStartedUs = 0;
static uint32_t rev6TxnMaxUs = 0;
static uint32_t rev6TxnCount = 0;
static uint32_t rev6TxnTimeoutCount = 0;
static uint32_t rev6RequestRejectCount = 0;
static uint8_t rev6TxnSlave = 0;
static uint16_t rev6TxnReg = 0;
static uint16_t rev6TxnValue = 0;
static JWPLCModbusRTUError rev6LastTxnResult = JWPLC_MODBUS_OK;
static uint32_t rev6SyncFail = 0;
static uint32_t rev6ApplyCount = 0;

// Un timeout antes del primer TRIGGER no invalida el patron: ningun Slave
// fue armado todavia. En ese caso se reinicia UNA vez la secuencia completa
// con un target nuevo. No se reintenta una transaccion TRIGGER ambigua.
static bool rev6RetrySequencePending = false;
static bool rev6RetrySequenceActive = false;
static uint32_t rev6RetryAttempts = 0;
static uint32_t rev6RetryRecovered = 0;
static uint32_t rev6RetryFinalTimeouts = 0;

static Rev6PendingMasterRequest rev6PendingMasterRequest;

static volatile bool rev6ApplyPending = false;
static volatile uint8_t rev6ApplyMask = 0;
static volatile uint32_t rev6ApplyAtUs = 0;
static volatile uint16_t rev6ApplyToken = 0;
static volatile uint16_t rev6AppliedToken = 0;
static volatile uint32_t rev6AppliedAtMs = 0;
static volatile uint16_t rev6InputSampleToken = 0;
static uint32_t rev6IoRaceDiscardCount = 0;
static portMUX_TYPE rev6IoStateMux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t rev6SyncTaskHandle = nullptr;

// STOP seguro: no mezcla la API Sync base con una transaccion cooperativa REV6
// todavia en vuelo. Se conserva como estado de la aplicacion de test.
static bool rev6StopRequested = false;
static uint32_t rev6StopRequestedMs = 0;
static uint32_t rev6StopQuietUntilUs = 0;
static uint32_t rev6StopLastDrainMs = 0;
static uint32_t rev6StopMaxDrainMs = 0;
static uint32_t rev6StopCount = 0;

static constexpr uint8_t REV6_ETHNEXT_STAGE_IDLE = 0;
static constexpr uint8_t REV6_ETHNEXT_STAGE_DRAIN = 1;
static constexpr uint8_t REV6_ETHNEXT_STAGE_RELEASE_WAIT = 2;
static bool rev6EthNextRequested = false;
static bool rev6EthNextAutomatic = false;
static uint8_t rev6EthNextStage = REV6_ETHNEXT_STAGE_IDLE;
static uint32_t rev6EthNextRequestedMs = 0;
static uint32_t rev6EthNextQuietUntilUs = 0;
static uint32_t rev6EthNextReleaseUntilMs = 0;
static uint32_t rev6EthNextLastMs = 0;
static uint32_t rev6EthNextMaxMs = 0;
static uint32_t rev6EthNextCount = 0;
static uint16_t rev6EthNextPreviousOwner = ETH_OWNER_NONE;
static uint16_t rev6EthNextNextOwner = ETH_OWNER_NONE;

struct Rev6I2CDiagStats
{
    uint32_t cycles = 0;
    uint32_t tcaOk = 0;
    uint32_t tcaFail = 0;
    uint32_t rtcOk = 0;
    uint32_t rtcFail = 0;
    uint32_t deferredBusy = 0;
    uint32_t sdaLowSamples = 0;
    uint32_t sclLowSamples = 0;
    uint32_t bothLowSamples = 0;
    uint16_t sdaLowConsecutive = 0;
    uint16_t sclLowConsecutive = 0;
    int lastTcaErr = ESP_OK;
    int lastRtcErr = ESP_OK;
    int lastTcaInputErr = ESP_OK;
    uint8_t lastTcaInput[3] = {0, 0, 0};
    bool haveTcaInput = false;
};

static Rev6I2CDiagStats rev6I2CDiag;
static bool rev6I2CWatchEnabled = true;
static uint32_t rev6I2CLastLineSampleMs = 0;
static uint32_t rev6I2CNextWatchMs = 0;
static void rev6ResetI2CDiagnostics();

static bool rev6AudioActive = false;
static uint32_t rev6AudioStopUs = 0;

static volatile uint8_t rev6ExpectedMask = 0;
static volatile uint8_t rev6PendingRisingMask = 0;
static uint8_t rev6CreditedInputMask = 0;

// ============================================================================
// Estado W5500 worker
// ============================================================================

static constexpr uint8_t ETH_JOB_HTTP = 1;
static constexpr uint8_t ETH_JOB_NTP_INITIAL = 2;
static constexpr uint8_t ETH_JOB_NTP_WINDOW = 3;

struct Rev6EthJob
{
    uint8_t type = 0;
    uint32_t sequence = 0;
    uint8_t ip[4] = {0, 0, 0, 0};
    uint16_t port = 0;
    uint16_t localPort = 0;
    char body[ETH_WORKER_BODY_MAX] = {};
};

struct Rev6EthResult
{
    uint8_t type = 0;
    uint32_t sequence = 0;
    bool busAcquired = false;
    bool ok = false;
    uint32_t latencyMs = 0;
    uint32_t ntpLocal = 0;
};

static QueueHandle_t rev6EthJobQueue = nullptr;
static QueueHandle_t rev6EthResultQueue = nullptr;
static EventGroupHandle_t rev6EthEvents = nullptr;
static TaskHandle_t rev6EthTaskHandle = nullptr;
static bool rev6EthJobOutstanding = false;
static volatile int8_t rev6EthObservedCore = -1;
static uint32_t rev6EthMaxJobMs = 0;
static uint32_t rev6EthBusAcquireFail = 0;
static uint32_t rev6EthFramDeferred = 0;
static uint32_t rev6EthSdDeferred = 0;
static uint32_t rev6LastWorkerDiagMs = 0;

// ============================================================================
// Helpers generales
// ============================================================================

static bool rev6DueUs(uint32_t now, uint32_t target)
{
    return (int32_t)(now - target) >= 0;
}

static uint8_t rev6Popcount(uint8_t value)
{
    uint8_t count = 0;
    while (value)
    {
        count += (uint8_t)(value & 1U);
        value >>= 1U;
    }
    return count;
}

static uint16_t rev6FrequencyForBitmap(uint8_t bitmap)
{
    if (bitmap == 0)
        return 330;

    const uint8_t folded =
        (uint8_t)(bitmap ^ (bitmap >> 4) ^ (rev6Popcount(bitmap) * 3U));

    return REV6_NOTE_PALETTE[
        folded % (sizeof(REV6_NOTE_PALETTE) / sizeof(REV6_NOTE_PALETTE[0]))];
}

static bool rev6EthSpiBusy()
{
    if (rev6EthEvents == nullptr)
        return false;

    return (xEventGroupGetBits(rev6EthEvents) & ETH_WORKER_SPI_BUSY_BIT) != 0;
}

// ============================================================================
// Audio + apply task Core1 alta prioridad
// ============================================================================

static void rev6AudioStop()
{
    if (!rev6AudioActive)
        return;

    ledcWrite(BUZZER_PIN, 0);
    ledcWriteTone(BUZZER_PIN, 0);
    ledcDetach(BUZZER_PIN);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    rev6AudioActive = false;
}

static void rev6AudioStart(uint8_t bitmap)
{
    if (!REV6_BUZZER_ENABLED || REV6_BUZZER_VOLUME == 0)
        return;

    const uint16_t frequency = rev6FrequencyForBitmap(bitmap);

    rev6AudioStop();
    ledcDetach(BUZZER_PIN);

    if (!ledcAttach(BUZZER_PIN, frequency, REV6_BUZZER_PWM_BITS))
        return;

    ledcWriteTone(BUZZER_PIN, frequency);
    ledcWrite(BUZZER_PIN, REV6_BUZZER_VOLUME);
    rev6AudioStopUs =
        micros() + (uint32_t)REV6_BUZZER_NOTE_MS * 1000UL;
    rev6AudioActive = true;
}

static void rev6ApplyNow(uint8_t bitmap, uint16_t token)
{
    const uint8_t previous = ioStress.outputBitmap;
    const uint8_t risingMask =
        (uint8_t)(bitmap & (uint8_t)~previous);

    // Token 0 = generacion invalida/en actualizacion. Como rev6Sequence nunca
    // usa 0, el verifier puede detectar que el task de apply lo interrumpio.
    rev6AppliedToken = 0;

    JWPLC_writeOutputs(bitmap);
    const uint32_t appliedAtMs = millis();

    portENTER_CRITICAL(&rev6IoStateMux);
    ioStress.outputBitmap = bitmap;
    ioStress.phaseStartMs = appliedAtMs;
    ioStress.phaseFailureLatched = false;
    ioStress.pulseDetectedThisOn = false;
    ioStress.onPhase = bitmap != 0;

    rev6ExpectedMask = bitmap;
    rev6PendingRisingMask = risingMask;
    rev6CreditedInputMask = 0;
    rev6AppliedAtMs = appliedAtMs;
    rev6InputSampleToken = 0;

    for (uint8_t bit = 0; bit < 8; ++bit)
    {
        if (risingMask & (uint8_t)(1U << bit))
            ioStress.qPulses[bit]++;
    }

    // Publicar el token al final: desde este punto todo el estado pertenece a
    // una sola generacion coherente y puede ser consumido por el loop.
    rev6AppliedToken = token;
    portEXIT_CRITICAL(&rev6IoStateMux);

    rev6AudioStart(bitmap);
    rev6ApplyCount++;
}

static void rev6SyncApplyTask(void *)
{
    for (;;)
    {
        if (rev6ApplyPending && rev6DueUs(micros(), rev6ApplyAtUs))
        {
            const uint8_t bitmap = rev6ApplyMask;
            const uint16_t token = rev6ApplyToken;

            rev6ApplyPending = false;
            rev6ApplyNow(bitmap, token);
        }

        if (rev6AudioActive && rev6DueUs(micros(), rev6AudioStopUs))
            rev6AudioStop();

        vTaskDelay(1);
    }
}

static void rev6QueueApply(uint8_t bitmap, uint32_t targetUs, uint16_t token)
{
    rev6ApplyMask = bitmap;
    rev6ApplyAtUs = targetUs;
    rev6ApplyToken = token;
    rev6ApplyPending = true;
}

// ============================================================================
// Scheduler distribuido M2 -> S1/S2
// ============================================================================

static void rev6LoadPattern()
{
    if (rev6SyncMode == REV6_MODE_CLACK)
    {
        rev6ClackOn = !rev6ClackOn;
        const uint8_t mask = rev6ClackOn ? 0xFF : 0x00;
        rev6M2Mask = mask;
        rev6S1Mask = mask;
        rev6S2Mask = mask;
        rev6HoldMs = 800;
        rev6PatternName = rev6ClackOn ? "clack-on" : "clack-off";
        return;
    }

    const Rev6PatternStep &step = REV6_SHOW_PATTERN[rev6PatternIndex];
    rev6M2Mask = step.m2;
    rev6S1Mask = step.s1;
    rev6S2Mask = step.s2;
    rev6HoldMs = step.holdMs;
    rev6PatternName = step.name;
    rev6PatternIndex = (rev6PatternIndex + 1U) % REV6_SHOW_STEP_COUNT;
}

static bool rev6MasterRequest(uint8_t slaveId, uint16_t reg, uint16_t value)
{
    if (JWPLC_ModbusRTU.masterBusy())
        return false;

    if (JWPLC_ModbusRTU.masterDone())
        JWPLC_ModbusRTU.clearMasterResult();

    rev6TxnSlave = slaveId;
    rev6TxnReg = reg;
    rev6TxnValue = value;
    rev6TxnStartedUs = micros();

    if (!JWPLC_ModbusRTU.requestWriteSingleRegister(
            slaveId,
            reg,
            value,
            MODBUS_TIMEOUT_MS))
    {
        rev6RequestRejectCount++;
        Serial.print(F("[SYNC REV6] REQUEST REJECT S"));
        Serial.print(slaveId);
        Serial.print(F(" reg="));
        Serial.print(reg);
        Serial.print(F(" value="));
        Serial.print(value);
        Serial.print(F(" err="));
        Serial.println((int)JWPLC_ModbusRTU.lastError());
        return false;
    }

    rev6SyncTxnActive = true;
    return true;
}

static bool rev6FinishTxn(const char *label)
{
    if (!JWPLC_ModbusRTU.masterDone())
        return false;

    const uint32_t elapsedUs = micros() - rev6TxnStartedUs;
    if (elapsedUs > rev6TxnMaxUs)
        rev6TxnMaxUs = elapsedUs;

    rev6TxnCount++;

    const bool ok = JWPLC_ModbusRTU.masterSucceeded();
    const JWPLCModbusRTUError result = JWPLC_ModbusRTU.masterResult();
    const uint32_t masterTimeouts = JWPLC_ModbusRTU.stats().masterTimeouts;

    rev6LastTxnResult = result;

    if (result == JWPLC_MODBUS_TIMEOUT)
        rev6TxnTimeoutCount++;

    JWPLC_ModbusRTU.clearMasterResult();
    rev6SyncTxnActive = false;
    rev6NextTxnAllowedUs = micros() + REV6_MODBUS_INTER_TX_GAP_US;

    if (!ok)
    {
        // Todavia no se lachea MBUS: el helper decide si corresponde un retry.
        Serial.print(F("[SYNC REV6] TXN_ERROR "));
        Serial.print(label);
        Serial.print(F(" S"));
        Serial.print(rev6TxnSlave);
        Serial.print(F(" reg="));
        Serial.print(rev6TxnReg);
        Serial.print(F(" value="));
        Serial.print(rev6TxnValue);
        Serial.print(F(" us="));
        Serial.print(elapsedUs);
        Serial.print(F(" err="));
        Serial.print((int)result);
        Serial.print(F(" masterTimeouts="));
        Serial.println(masterTimeouts);
    }

    return ok;
}

static bool rev6RemainingDelayMs(uint16_t &delayMs)
{
    int32_t remaining =
        (int32_t)(rev6TargetUs - micros()) -
        (int32_t)REV6_TRIGGER_COMP_US;

    if (remaining < (int32_t)REV6_MIN_REMAIN_US)
        return false;

    uint32_t value = ((uint32_t)remaining + 500UL) / 1000UL;
    if (value > 60000UL)
        value = 60000UL;

    delayMs = (uint16_t)value;
    return true;
}

static void rev6AbortSync(const char *reason)
{
    rev6SyncFail++;
    rev6SyncTxnActive = false;
    rev6RetrySequencePending = false;
    rev6RetrySequenceActive = false;
    rev6PendingMasterRequest = Rev6PendingMasterRequest{};
    rev6SyncStage = REV6_STAGE_IDLE;
    rev6NextTxnAllowedUs = micros() + REV6_MODBUS_INTER_TX_GAP_US;
    rev6NextStepMs = millis() + REV6_SYNC_RETRY_MS;
    Serial.print(F("[SYNC REV6] ABORT: "));
    Serial.println(reason);
    latchError(ERR_MODBUS, reason);
}

static bool rev6StageAllowsSequenceRetry()
{
    switch (rev6SyncStage)
    {
        case REV6_STAGE_CODE_S1_WAIT:
        case REV6_STAGE_CODE_S2_WAIT:
        case REV6_STAGE_MASK_S1_WAIT:
        case REV6_STAGE_MASK_S2_WAIT:
        case REV6_STAGE_DELAY_S1_WAIT:
            return true;

        // Desde TRIGGER_S1_WAIT el request pudo haber llegado al Slave aunque
        // se haya perdido la respuesta. No repetir una operacion ambigua.
        default:
            return false;
    }
}

static void rev6HandleTxnFailure(const char *reason)
{
    const bool timeout =
        rev6LastTxnResult == JWPLC_MODBUS_TIMEOUT;

    if (timeout &&
        !rev6RetrySequenceActive &&
        rev6StageAllowsSequenceRetry())
    {
        rev6RetryAttempts++;
        rev6RetrySequencePending = true;
        rev6RetrySequenceActive = false;
        rev6SyncTxnActive = false;
        rev6PendingMasterRequest = Rev6PendingMasterRequest{};
        rev6SyncStage = REV6_STAGE_IDLE;
        rev6ApplyPending = false;
        rev6NextTxnAllowedUs = micros() + REV6_MODBUS_INTER_TX_GAP_US;
        rev6NextStepMs = millis() + REV6_MODBUS_INTER_TX_GAP_MS;

        Serial.print(F("[SYNC REV6] RETRY sequence attempt="));
        Serial.print(rev6RetryAttempts);
        Serial.print(F(" reason="));
        Serial.print(reason);
        Serial.print(F(" pattern="));
        Serial.print(rev6PatternName);
        Serial.print(F(" last=S"));
        Serial.print(rev6TxnSlave);
        Serial.print(F("/reg"));
        Serial.println(rev6TxnReg);
        return;
    }

    if (timeout)
        rev6RetryFinalTimeouts++;

    rev6AbortSync(reason);
}

static void rev6QueueMasterRequest(
    uint8_t slaveId,
    uint16_t reg,
    uint16_t value,
    uint8_t nextStage,
    const char *abortReason,
    bool dynamicDelay = false)
{
    rev6PendingMasterRequest.pending = true;
    rev6PendingMasterRequest.dynamicDelay = dynamicDelay;
    rev6PendingMasterRequest.slaveId = slaveId;
    rev6PendingMasterRequest.reg = reg;
    rev6PendingMasterRequest.value = value;
    rev6PendingMasterRequest.nextStage = nextStage;
    rev6PendingMasterRequest.abortReason = abortReason;
}

static bool rev6ServicePendingMasterRequest()
{
    if (!rev6PendingMasterRequest.pending)
        return false;

    if (!rev6DueUs(micros(), rev6NextTxnAllowedUs))
        return true;

    uint16_t value = rev6PendingMasterRequest.value;

    if (rev6PendingMasterRequest.dynamicDelay &&
        !rev6RemainingDelayMs(value))
    {
        const char *reason = rev6PendingMasterRequest.abortReason;
        rev6PendingMasterRequest = Rev6PendingMasterRequest{};
        rev6AbortSync(reason != nullptr ? reason : "delay dinamico");
        return true;
    }

    const uint8_t slaveId = rev6PendingMasterRequest.slaveId;
    const uint16_t reg = rev6PendingMasterRequest.reg;
    const uint8_t nextStage = rev6PendingMasterRequest.nextStage;
    const char *reason = rev6PendingMasterRequest.abortReason;

    rev6PendingMasterRequest = Rev6PendingMasterRequest{};

    if (!rev6MasterRequest(slaveId, reg, value))
    {
        rev6AbortSync(reason != nullptr ? reason : "request pendiente");
        return true;
    }

    rev6SyncStage = nextStage;
    return true;
}

static bool rev6InitialModbusGapReady()
{
    if (!rev6DueUs(micros(), rev6NextTxnAllowedUs))
        return false;

    // finishMasterRuntimePoll() actualiza lastMasterPollMs. Evitar que el
    // primer FC06 sync salga en el mismo instante en que termino FC03/FC06
    // del poll cooperativo base.
    if (lastMasterPollMs != 0 &&
        (uint32_t)(millis() - lastMasterPollMs) <
            REV6_MODBUS_INTER_TX_GAP_MS)
    {
        return false;
    }

    return true;
}

static void rev6BeginStep()
{
    if (!isMaster() || cfg.expectedSlaves != 2)
        return;

    if (masterRuntimePollPhase != MASTER_RT_POLL_IDLE ||
        JWPLC_ModbusRTU.masterBusy() ||
        JWPLC_ModbusRTU.masterDone() ||
        !rev6InitialModbusGapReady())
    {
        return;
    }

    const bool retryingSequence = rev6RetrySequencePending;

    if (retryingSequence)
    {
        // El primer intento ya avanzo rev6PatternIndex. Conservamos las masks
        // actuales y generamos un target nuevo para el mismo patron.
        rev6RetrySequencePending = false;
        rev6RetrySequenceActive = true;

        Serial.print(F("[SYNC REV6] RETRY restart pattern="));
        Serial.println(rev6PatternName);
    }
    else
    {
        rev6RetrySequenceActive = false;
        rev6LoadPattern();
    }

    rev6Sequence++;
    if (rev6Sequence == 0)
        rev6Sequence++;

    rev6TargetUs = micros() + REV6_SYNC_LEAD_US;

    if (!rev6SyncCodesPrepared)
    {
        if (!rev6MasterRequest(1, HR_CMD_CODE, CMD_SYNC_IO_PHASE))
        {
            rev6AbortSync("request code S1");
            return;
        }

        rev6SyncStage = REV6_STAGE_CODE_S1_WAIT;
        return;
    }

    if (!rev6MasterRequest(1, HR_CMD_ARG0, rev6S1Mask))
    {
        rev6AbortSync("request mask S1");
        return;
    }

    rev6SyncStage = REV6_STAGE_MASK_S1_WAIT;
}

static bool rev6SyncWantsModbusPriority()
{
    if (!rev6SyncTakeover || !isMaster())
        return false;

    // Una operacion sync ya iniciada conserva prioridad hasta completarse.
    if (rev6SyncStage != REV6_STAGE_IDLE ||
        rev6PendingMasterRequest.pending ||
        rev6RetrySequencePending)
    {
        return true;
    }

    // Nunca dejar a medias un poll cooperativo que ya empezo. La maquina de
    // estados base necesita seguir siendo llamada para cerrar FC06/FC03.
    if (masterRuntimePollPhase != MASTER_RT_POLL_IDLE)
        return false;

    return (int32_t)(millis() - rev6NextStepMs) >= 0;
}

static void rev6ServiceMasterSync()
{
    if (!rev6SyncTakeover || !isMaster() || cfg.expectedSlaves != 2)
        return;

    if (rev6SyncStage == REV6_STAGE_IDLE)
    {
        if ((int32_t)(millis() - rev6NextStepMs) >= 0)
            rev6BeginStep();
        return;
    }

    if (rev6SyncStage == REV6_STAGE_WAIT_APPLY)
    {
        if (rev6AppliedToken != rev6Sequence)
            return;

        if (rev6RetrySequenceActive)
        {
            rev6RetryRecovered++;
            rev6RetrySequenceActive = false;

            Serial.print(F("[SYNC REV6] RETRY recovered seq="));
            Serial.print(rev6Sequence);
            Serial.print(F(" recovered="));
            Serial.println(rev6RetryRecovered);
        }

        Serial.print(F("PATTERN seq="));
        Serial.print(rev6Sequence);
        Serial.print(F(" name="));
        Serial.print(rev6PatternName);
        Serial.print(F(" M2/S1/S2=0x"));
        if (rev6M2Mask < 16) Serial.print('0');
        Serial.print(rev6M2Mask, HEX);
        Serial.print(F("/0x"));
        if (rev6S1Mask < 16) Serial.print('0');
        Serial.print(rev6S1Mask, HEX);
        Serial.print(F("/0x"));
        if (rev6S2Mask < 16) Serial.print('0');
        Serial.print(rev6S2Mask, HEX);
        Serial.print(F(" tone="));
        Serial.print(rev6FrequencyForBitmap(rev6M2Mask));
        Serial.print('/');
        Serial.print(rev6FrequencyForBitmap(rev6S1Mask));
        Serial.print('/');
        Serial.print(rev6FrequencyForBitmap(rev6S2Mask));
        Serial.println(F("Hz"));

        rev6SyncStage = REV6_STAGE_IDLE;
        rev6NextStepMs = millis() + rev6HoldMs;
        return;
    }

    // Una transaccion terminada programa el siguiente FC06 y conserva
    // prioridad del scheduler, pero el request real espera 4 ms sin bloquear.
    if (rev6PendingMasterRequest.pending)
    {
        (void)rev6ServicePendingMasterRequest();
        return;
    }

    if (rev6SyncTxnActive && !JWPLC_ModbusRTU.masterDone())
        return;

    switch (rev6SyncStage)
    {
        case REV6_STAGE_CODE_S1_WAIT:
            if (!rev6FinishTxn("code-s1"))
            {
                rev6HandleTxnFailure("code S1");
                return;
            }
            rev6QueueMasterRequest(
                2,
                HR_CMD_CODE,
                CMD_SYNC_IO_PHASE,
                REV6_STAGE_CODE_S2_WAIT,
                "request code S2");
            return;

        case REV6_STAGE_CODE_S2_WAIT:
            if (!rev6FinishTxn("code-s2"))
            {
                rev6HandleTxnFailure("code S2");
                return;
            }
            rev6SyncCodesPrepared = true;
            rev6QueueMasterRequest(
                1,
                HR_CMD_ARG0,
                rev6S1Mask,
                REV6_STAGE_MASK_S1_WAIT,
                "request mask S1");
            return;

        case REV6_STAGE_MASK_S1_WAIT:
            if (!rev6FinishTxn("mask-s1"))
            {
                rev6HandleTxnFailure("mask S1");
                return;
            }
            rev6QueueMasterRequest(
                2,
                HR_CMD_ARG0,
                rev6S2Mask,
                REV6_STAGE_MASK_S2_WAIT,
                "request mask S2");
            return;

        case REV6_STAGE_MASK_S2_WAIT:
            if (!rev6FinishTxn("mask-s2"))
            {
                rev6HandleTxnFailure("mask S2");
                return;
            }
            rev6QueueMasterRequest(
                1,
                HR_CMD_ARG1,
                0,
                REV6_STAGE_DELAY_S1_WAIT,
                "delay S1",
                true);
            return;

        case REV6_STAGE_DELAY_S1_WAIT:
            if (!rev6FinishTxn("delay-s1"))
            {
                rev6HandleTxnFailure("delay S1 tx");
                return;
            }
            rev6QueueMasterRequest(
                1,
                HR_CMD_SEQ,
                rev6Sequence,
                REV6_STAGE_TRIGGER_S1_WAIT,
                "trigger S1");
            return;

        case REV6_STAGE_TRIGGER_S1_WAIT:
            if (!rev6FinishTxn("trigger-s1"))
            {
                rev6HandleTxnFailure("trigger S1 tx");
                return;
            }
            rev6QueueMasterRequest(
                2,
                HR_CMD_ARG1,
                0,
                REV6_STAGE_DELAY_S2_WAIT,
                "delay S2",
                true);
            return;

        case REV6_STAGE_DELAY_S2_WAIT:
            if (!rev6FinishTxn("delay-s2"))
            {
                rev6HandleTxnFailure("delay S2 tx");
                return;
            }
            rev6QueueMasterRequest(
                2,
                HR_CMD_SEQ,
                rev6Sequence,
                REV6_STAGE_TRIGGER_S2_WAIT,
                "trigger S2");
            return;

        case REV6_STAGE_TRIGGER_S2_WAIT:
            if (!rev6FinishTxn("trigger-s2"))
            {
                rev6HandleTxnFailure("trigger S2 tx");
                return;
            }
            if ((int32_t)(rev6TargetUs - micros()) <
                (int32_t)REV6_MIN_REMAIN_US)
            {
                rev6AbortSync("target demasiado cercano");
                return;
            }
            rev6QueueApply(rev6M2Mask, rev6TargetUs, rev6Sequence);
            rev6SyncStage = REV6_STAGE_WAIT_APPLY;
            return;

        default:
            rev6AbortSync("stage invalido");
            return;
    }
}

static void rev6ServiceSlaveSync()
{
    if (!rev6SyncTakeover || !isSlave() || !modbusReady)
        return;

    if (holdingRegisters[HR_CMD_CODE] != CMD_SYNC_IO_PHASE)
        return;

    const uint16_t trigger = holdingRegisters[HR_CMD_SEQ];
    if (trigger == 0 || trigger == rev6SlaveLastTrigger)
        return;

    rev6SlaveLastTrigger = trigger;

    const uint8_t bitmap =
        (uint8_t)(holdingRegisters[HR_CMD_ARG0] & 0x00FFU);
    const uint16_t delayMs =
        holdingRegisters[HR_CMD_ARG1];

    if (delayMs == 0)
    {
        rev6SyncFail++;
        latchError(ERR_MODBUS, "sync delay 0");
        return;
    }

    rev6QueueApply(
        bitmap,
        micros() + (uint32_t)delayMs * 1000UL,
        trigger);
}

static void rev6ServiceLoopback()
{
    if (!rev6SyncTakeover || soakState != SOAK_RUNNING)
        return;

    // Snapshot corto y coherente de la generacion aplicada. El task de apply
    // publica rev6AppliedToken al final de la actualizacion; token 0 significa
    // que una nueva generacion esta en curso y esta muestra debe ignorarse.
    uint16_t tokenBefore = 0;
    uint8_t expectedMask = 0;
    uint8_t risingMask = 0;
    uint32_t phaseStartMs = 0;

    portENTER_CRITICAL(&rev6IoStateMux);
    tokenBefore = rev6AppliedToken;
    expectedMask = rev6ExpectedMask;
    risingMask = rev6PendingRisingMask;
    phaseStartMs = rev6AppliedAtMs;
    portEXIT_CRITICAL(&rev6IoStateMux);

    if (tokenBefore == 0 || phaseStartMs == 0)
        return;

    if (rev6AppliedToken != tokenBefore)
    {
        rev6IoRaceDiscardCount++;
        return;
    }

    const uint32_t scanNow = millis();

    if ((uint32_t)(scanNow - ioStress.lastScanMs) >= IO_SCAN_MS)
    {
        const uint8_t sampledInput = digitalReadBlock(I0_X);
        bool accepted = false;

        portENTER_CRITICAL(&rev6IoStateMux);
        if (rev6AppliedToken == tokenBefore)
        {
            ioStress.lastScanMs = scanNow;
            ioStress.inputBitmap = sampledInput;
            rev6InputSampleToken = tokenBefore;
            accepted = true;
        }
        portEXIT_CRITICAL(&rev6IoStateMux);

        if (!accepted)
        {
            rev6IoRaceDiscardCount++;
            return;
        }
    }

    uint16_t tokenAfter = 0;
    uint16_t sampleToken = 0;
    uint8_t inputBitmap = 0;

    portENTER_CRITICAL(&rev6IoStateMux);
    tokenAfter = rev6AppliedToken;
    sampleToken = rev6InputSampleToken;
    inputBitmap = ioStress.inputBitmap;
    portEXIT_CRITICAL(&rev6IoStateMux);

    if (tokenAfter != tokenBefore)
    {
        rev6IoRaceDiscardCount++;
        return;
    }

    // Nunca comparar un expected nuevo contra una muestra I perteneciente a la
    // generacion anterior. El siguiente scan valido llegara en <= IO_SCAN_MS.
    if (sampleToken != tokenBefore)
        return;

    const uint32_t now = millis();
    const uint32_t age = now - phaseStartMs;

    if (age >= IO_SETTLE_MS && inputBitmap == expectedMask)
    {
        bool stable = false;

        portENTER_CRITICAL(&rev6IoStateMux);
        if (rev6AppliedToken == tokenBefore &&
            rev6InputSampleToken == tokenBefore)
        {
            const uint8_t toCredit =
                (uint8_t)(risingMask &
                          (uint8_t)~rev6CreditedInputMask);

            for (uint8_t bit = 0; bit < 8; ++bit)
            {
                const uint8_t mask = (uint8_t)(1U << bit);
                if (toCredit & mask)
                {
                    ioStress.iPulses[bit]++;
                    rev6CreditedInputMask |= mask;
                }
            }

            ioStress.pulseDetectedThisOn = true;
            stable = true;
        }
        portEXIT_CRITICAL(&rev6IoStateMux);

        if (!stable)
        {
            rev6IoRaceDiscardCount++;
            return;
        }
    }

    if (age >= IO_VERIFY_DEADLINE_MS && inputBitmap != expectedMask)
    {
        bool committed = false;
        bool raced = false;

        // La decision de mismatch se confirma dentro del mismo guard de la
        // generacion. El log/latch se hace fuera de la seccion critica.
        portENTER_CRITICAL(&rev6IoStateMux);
        if (rev6AppliedToken != tokenBefore ||
            rev6InputSampleToken != tokenBefore)
        {
            raced = true;
        }
        else if (!ioStress.phaseFailureLatched)
        {
            ioStress.phaseFailureLatched = true;
            ioStress.mismatches++;
            committed = true;
        }
        portEXIT_CRITICAL(&rev6IoStateMux);

        if (raced)
        {
            rev6IoRaceDiscardCount++;
            return;
        }

        if (committed)
        {
            Serial.print(F("[IO FAIL] node="));
            Serial.print(shortRoleName());
            Serial.print(F(" ch="));
            Serial.print(ioStress.channel);
            Serial.print(F(" token="));
            Serial.print(tokenBefore);
            Serial.print(F(" Q=0x"));
            Serial.print(expectedMask, HEX);
            Serial.print(F(" I=0x"));
            Serial.print(inputBitmap, HEX);
            Serial.println(F(" reason=REV6 input bitmap != output bitmap"));

            latchError(
                ERR_IO,
                "REV6 input bitmap != output bitmap");
        }
    }
}

static void rev6EnterTakeover()
{
    rev6SyncTakeover = true;
    rev6SyncCodesPrepared = false;
    rev6SyncTxnActive = false;
    rev6PendingMasterRequest = Rev6PendingMasterRequest{};
    rev6SyncStage = REV6_STAGE_IDLE;
    rev6PatternIndex = 0;
    rev6ClackOn = false;
    rev6Sequence = 100;
    rev6AppliedToken = 0;
    rev6AppliedAtMs = 0;
    rev6InputSampleToken = 0;
    rev6IoRaceDiscardCount = 0;
    rev6ApplyPending = false;
    rev6ExpectedMask = 0;
    rev6PendingRisingMask = 0;
    rev6CreditedInputMask = 0;
    rev6NextTxnAllowedUs = micros() + REV6_MODBUS_INTER_TX_GAP_US;
    rev6NextStepMs = millis() + 150UL;

    rev6TxnStartedUs = 0;
    rev6TxnMaxUs = 0;
    rev6TxnCount = 0;
    rev6TxnTimeoutCount = 0;
    rev6RequestRejectCount = 0;
    rev6TxnSlave = 0;
    rev6TxnReg = 0;
    rev6TxnValue = 0;
    rev6LastTxnResult = JWPLC_MODBUS_OK;
    rev6SyncFail = 0;
    rev6ApplyCount = 0;
    rev6RetrySequencePending = false;
    rev6RetrySequenceActive = false;
    rev6RetryAttempts = 0;
    rev6RetryRecovered = 0;
    rev6RetryFinalTimeouts = 0;

    rev6StopRequested = false;
    rev6StopRequestedMs = 0;
    rev6StopQuietUntilUs = 0;
    rev6StopLastDrainMs = 0;
    rev6StopMaxDrainMs = 0;
    rev6StopCount = 0;

    rev6ResetI2CDiagnostics();

    rev6EthNextRequested = false;
    rev6EthNextAutomatic = false;
    rev6EthNextStage = REV6_ETHNEXT_STAGE_IDLE;
    rev6EthNextRequestedMs = 0;
    rev6EthNextQuietUntilUs = 0;
    rev6EthNextReleaseUntilMs = 0;
    rev6EthNextLastMs = 0;
    rev6EthNextMaxMs = 0;
    rev6EthNextCount = 0;
    rev6EthNextPreviousOwner = ETH_OWNER_NONE;
    rev6EthNextNextOwner = ETH_OWNER_NONE;

    // Consumir el trigger START existente antes de cambiar CODE a SYNC.
    // Evita interpretar START+ARG1=0 como primer comando sincronizado.
    if (isSlave())
        rev6SlaveLastTrigger = holdingRegisters[HR_CMD_SEQ];

    memset(ioStress.qPulses, 0, sizeof(ioStress.qPulses));
    memset(ioStress.iPulses, 0, sizeof(ioStress.iPulses));
    ioStress.mismatches = 0;
    ioStress.running = false;
    ioStress.waitingInitialStagger = false;
    ioStress.onPhase = false;
    ioStress.phaseFailureLatched = false;
    ioStress.pulseDetectedThisOn = false;
    ioStress.lastScanMs = 0;
    JWPLC_writeOutputs(0x00);
    ioStress.outputBitmap = 0;

    Serial.print(F("[SYNC REV6] takeover=ON mode="));
    Serial.print(rev6SyncMode == REV6_MODE_CLACK ? F("CLACK") : F("SHOW"));
    Serial.print(F(" volume="));
    Serial.print(REV6_BUZZER_VOLUME);
    Serial.print(F("/255 fc06GapMs="));
    Serial.println(REV6_MODBUS_INTER_TX_GAP_MS);
}

static void rev6LeaveTakeover()
{
    rev6SyncTakeover = false;
    rev6SyncTxnActive = false;
    rev6RetrySequencePending = false;
    rev6RetrySequenceActive = false;
    rev6PendingMasterRequest = Rev6PendingMasterRequest{};
    rev6SyncStage = REV6_STAGE_IDLE;
    rev6ApplyPending = false;
    rev6AppliedToken = 0;
    rev6InputSampleToken = 0;
    JWPLC_writeOutputs(0x00);
    ioStress.outputBitmap = 0;
    rev6AudioStop();
    Serial.println(F("[SYNC REV6] takeover=OFF"));
}

static void rev6UpdateTakeover()
{
    if (soakState == SOAK_RUNNING && !rev6SyncTakeover)
        rev6EnterTakeover();
    else if (soakState != SOAK_RUNNING && rev6SyncTakeover)
        rev6LeaveTakeover();
}

static void rev6SetMode(uint8_t mode)
{
    rev6SyncMode = mode == REV6_MODE_CLACK
        ? REV6_MODE_CLACK
        : REV6_MODE_SHOW;

    rev6PatternIndex = 0;
    rev6ClackOn = false;
    rev6NextStepMs = millis();

    Serial.print(F("[SYNC REV6] mode="));
    Serial.println(rev6SyncMode == REV6_MODE_CLACK ? F("CLACK") : F("SHOW"));
}

// ============================================================================
// STOP Master seguro: drena runtime cooperativo antes de entrar al Sync base
// ============================================================================

static void rev6RequestSafeStop()
{
    if (!isMaster() || soakState != SOAK_RUNNING)
        return;

    if (rev6EthNextRequested)
    {
        Serial.println(F("[STOP REV6] deferred: ETHNEXT pending"));
        return;
    }

    if (rev6StopRequested)
    {
        Serial.println(F("[STOP REV6] already pending"));
        return;
    }

    rev6StopRequested = true;
    rev6StopRequestedMs = millis();
    rev6StopQuietUntilUs = 0;

    Serial.print(F("[STOP REV6] requested stage="));
    Serial.print((int)rev6SyncStage);
    Serial.print(F(" pollPhase="));
    Serial.print((int)masterRuntimePollPhase);
    Serial.print(F(" masterBusy="));
    Serial.println(JWPLC_ModbusRTU.masterBusy() ? 1 : 0);
}

static void rev6ServiceSafeStop()
{
    if (!rev6StopRequested || !isMaster())
        return;

    // Si el scheduler ya habia iniciado una secuencia, terminar SOLO esa
    // secuencia. No se inicia un patron nuevo desde este path.
    if (rev6SyncStage != REV6_STAGE_IDLE ||
        rev6PendingMasterRequest.pending ||
        rev6RetrySequencePending)
    {
        rev6ServiceMasterSync();
        return;
    }

    // Lo mismo para el poll cooperativo base: si empezo FC06/FC03, dejar que
    // su maquina de estados consuma el resultado antes del STOP Sync.
    if (masterRuntimePollPhase != MASTER_RT_POLL_IDLE)
    {
        masterPollOneSlaveCooperative();
        return;
    }

    // Estado defensivo: no debe quedar una txn REV6 activa con stage IDLE,
    // pero si ocurre se drena sin lanzar trabajo nuevo.
    if (rev6SyncTxnActive || JWPLC_ModbusRTU.masterBusy())
    {
        JWPLC_ModbusRTU.task();

        if (!JWPLC_ModbusRTU.masterDone())
            return;

        JWPLC_ModbusRTU.clearMasterResult();
        rev6SyncTxnActive = false;
    }
    else if (JWPLC_ModbusRTU.masterDone())
    {
        JWPLC_ModbusRTU.clearMasterResult();
    }

    // Primera vuelta realmente idle: iniciar un silencio equivalente al que
    // ya demostro estabilidad entre FC06 del scheduler REV6.
    if (rev6StopQuietUntilUs == 0)
    {
        rev6StopQuietUntilUs = micros() + REV6_STOP_QUIET_GAP_US;

        Serial.print(F("[STOP REV6] bus idle; quietGapMs="));
        Serial.println(REV6_STOP_QUIET_GAP_MS);
        return;
    }

    if (!rev6DueUs(micros(), rev6StopQuietUntilUs))
        return;

    rev6StopLastDrainMs = millis() - rev6StopRequestedMs;
    if (rev6StopLastDrainMs > rev6StopMaxDrainMs)
        rev6StopMaxDrainMs = rev6StopLastDrainMs;

    rev6StopCount++;
    rev6StopRequested = false;
    rev6StopQuietUntilUs = 0;
    rev6ApplyPending = false;

    Serial.print(F("[STOP REV6] quiescent drainMs="));
    Serial.println(rev6StopLastDrainMs);

    // Desde aqui la API Sync base entra con el Master libre y tras un frame
    // gap real. Conservamos setEthernetOwner()/stopAllSlaves()/FRAM tal como
    // estaban validados; solo cambia el momento en que se los invoca.
    manualStopMaster();
    rev6UpdateTakeover();

    Serial.println(F("[STOP REV6] complete"));
}

// ============================================================================
// ETHNEXT seguro: drena Modbus + libera owner anterior antes de mover cable
// ============================================================================

static void rev6RequestSafeEthNext(bool automatic)
{
    if (!isMaster() ||
        !masterSoakRunning ||
        soakState != SOAK_RUNNING ||
        rotationSlot >= rotationTotalSlots)
    {
        return;
    }

    if (rev6StopRequested)
    {
        Serial.println(F("[ETHNEXT REV6] rejected: STOP pending"));
        return;
    }

    if (rev6EthNextRequested)
    {
        if (!automatic)
            Serial.println(F("[ETHNEXT REV6] already pending"));
        return;
    }

    rev6EthNextRequested = true;
    rev6EthNextAutomatic = automatic;
    rev6EthNextStage = REV6_ETHNEXT_STAGE_DRAIN;
    rev6EthNextRequestedMs = millis();
    rev6EthNextQuietUntilUs = 0;
    rev6EthNextReleaseUntilMs = 0;
    rev6EthNextPreviousOwner = ethWindow.owner;

    const uint16_t nextSlot = rotationSlot + 1U;
    rev6EthNextNextOwner =
        nextSlot < rotationTotalSlots
            ? ownerForRotationSlot(nextSlot)
            : ETH_OWNER_NONE;

    Serial.print(F("[ETHNEXT REV6] requested source="));
    Serial.print(automatic ? F("AUTO") : F("MANUAL"));
    Serial.print(F(" stage="));
    Serial.print((int)rev6SyncStage);
    Serial.print(F(" pollPhase="));
    Serial.print((int)masterRuntimePollPhase);
    Serial.print(F(" masterBusy="));
    Serial.print(JWPLC_ModbusRTU.masterBusy() ? 1 : 0);
    Serial.print(F(" owner="));
    Serial.print(rev6EthNextPreviousOwner);
    Serial.print(F(" next="));
    Serial.println(rev6EthNextNextOwner);
}

static void rev6PrintEthernetMovePrompt(uint16_t nextOwner)
{
    Serial.println();
    printRule();
    Serial.print(F("MOVE_ETHERNET_CABLE_TO="));

    if (nextOwner == 0)
        Serial.println(F("MASTER"));
    else
    {
        Serial.print(F("S"));
        Serial.println(nextOwner);
    }

    Serial.println(F("Mueve el cable ahora; la ventana espera LINK/READY."));
    printRule();
}

static void rev6FinishSafeEthNext()
{
    rotationSlot++;
    ethWindowStartMs = 0;
    rotationWaitingForReady = true;

    rev6EthNextLastMs = millis() - rev6EthNextRequestedMs;
    if (rev6EthNextLastMs > rev6EthNextMaxMs)
        rev6EthNextMaxMs = rev6EthNextLastMs;

    rev6EthNextCount++;
    rev6EthNextRequested = false;
    rev6EthNextAutomatic = false;
    rev6EthNextStage = REV6_ETHNEXT_STAGE_IDLE;
    rev6EthNextQuietUntilUs = 0;
    rev6EthNextReleaseUntilMs = 0;

    if (rotationSlot >= rotationTotalSlots)
    {
        Serial.print(F("[ETHNEXT REV6] complete finalSlot ms="));
        Serial.println(rev6EthNextLastMs);
        completeSoakMaster();
        return;
    }

    const uint16_t nextOwner = ownerForRotationSlot(rotationSlot);
    rev6EthNextNextOwner = nextOwner;

    // No usar announceEthernetMove(): su tripleBeep()/CMD_BEEP son bloqueantes
    // y contaminarian el gate de handoff. El prompt Serial conserva la accion
    // operativa que necesita el usuario.
    rev6PrintEthernetMovePrompt(nextOwner);
    setEthernetOwner(nextOwner);

    Serial.print(F("[ETHNEXT REV6] complete ms="));
    Serial.print(rev6EthNextLastMs);
    Serial.print(F(" newOwner="));
    Serial.println(nextOwner);
}

static void rev6ServiceSafeEthNext()
{
    if (!rev6EthNextRequested || !isMaster())
        return;

    if (rev6EthNextStage == REV6_ETHNEXT_STAGE_DRAIN)
    {
        // Terminar una secuencia REV6 ya iniciada, sin abrir otra.
        if (rev6SyncStage != REV6_STAGE_IDLE ||
            rev6PendingMasterRequest.pending ||
            rev6RetrySequencePending)
        {
            rev6ServiceMasterSync();
            return;
        }

        // Terminar el poll FC06/FC03 ya iniciado, sin iniciar otro.
        if (masterRuntimePollPhase != MASTER_RT_POLL_IDLE)
        {
            masterPollOneSlaveCooperative();
            return;
        }

        if (rev6SyncTxnActive || JWPLC_ModbusRTU.masterBusy())
        {
            JWPLC_ModbusRTU.task();
            if (!JWPLC_ModbusRTU.masterDone())
                return;

            JWPLC_ModbusRTU.clearMasterResult();
            rev6SyncTxnActive = false;
        }
        else if (JWPLC_ModbusRTU.masterDone())
        {
            JWPLC_ModbusRTU.clearMasterResult();
        }

        // Si el owner actual es M2, no cortar owner mientras Core0 termina un
        // HTTP/NTP ya aceptado. rev6ServiceLocalEthernetWindow() deja de crear
        // jobs nuevos desde que ETHNEXT queda pending.
        if (rev6EthNextPreviousOwner == localNodeOrdinal() &&
            rev6EthJobOutstanding)
        {
            return;
        }

        if (rev6EthNextQuietUntilUs == 0)
        {
            rev6EthNextQuietUntilUs =
                micros() + REV6_ETHNEXT_QUIET_GAP_US;

            Serial.print(F("[ETHNEXT REV6] bus idle; quietGapMs="));
            Serial.println(REV6_ETHNEXT_QUIET_GAP_MS);
            return;
        }

        if (!rev6DueUs(micros(), rev6EthNextQuietUntilUs))
            return;

        setEthernetOwner(ETH_OWNER_NONE);

        // El owner remoto puede estar dentro de HTTP/NTP cuando recibe NONE.
        // 2.3 s cubre NTP_TIMEOUT_MS=1800 ms + adquisicion SPI + margen.
        rev6EthNextReleaseUntilMs =
            millis() + REV6_ETHNEXT_RELEASE_GRACE_MS;
        rev6EthNextStage = REV6_ETHNEXT_STAGE_RELEASE_WAIT;

        Serial.print(F("[ETHNEXT REV6] owner=NONE releaseGraceMs="));
        Serial.println(REV6_ETHNEXT_RELEASE_GRACE_MS);
        return;
    }

    if (rev6EthNextStage == REV6_ETHNEXT_STAGE_RELEASE_WAIT)
    {
        if ((int32_t)(millis() - rev6EthNextReleaseUntilMs) < 0)
            return;

        rev6FinishSafeEthNext();
    }
}

static void rev6ServiceMasterEthernetRotationSafe()
{
    if (!isMaster() ||
        !masterSoakRunning ||
        rotationSlot >= rotationTotalSlots)
    {
        return;
    }

    // La expiracion automatica usa la misma maquina segura que ETHNEXT manual.
    if (ethWindowStartMs != 0 &&
        (uint32_t)(millis() - ethWindowStartMs) >= ethWindowDurationMs)
    {
        rev6RequestSafeEthNext(true);
        return;
    }

    serviceMasterEthernetRotation();
}

// ============================================================================
// W5500 worker Core0: solo operaciones potencialmente bloqueantes
// ============================================================================

static bool rev6WorkerQueueNtp(uint8_t jobType)
{
    if (rev6EthJobQueue == nullptr || rev6EthJobOutstanding)
        return false;

    Rev6EthJob job;
    job.type = jobType;
    job.localPort =
        (uint16_t)(NTP_LOCAL_PORT_BASE + localNodeOrdinal());

    if (xQueueSend(rev6EthJobQueue, &job, 0) != pdTRUE)
        return false;

    rev6EthJobOutstanding = true;
    return true;
}

static bool rev6WorkerQueueHttp()
{
    if (rev6EthJobQueue == nullptr ||
        rev6EthJobOutstanding ||
        !cfg.wifiValid ||
        !JWPLC_Ethernet.isReady())
    {
        return false;
    }

    Rev6EthJob job;
    job.type = ETH_JOB_HTTP;
    job.sequence = ++ethHttp.sequence;
    job.ip[0] = pcIp[0];
    job.ip[1] = pcIp[1];
    job.ip[2] = pcIp[2];
    job.ip[3] = pcIp[3];
    job.port = cfg.pcPort;

    const String body = telemetryBody("eth", job.sequence);
    if (body.length() >= sizeof(job.body))
    {
        Serial.println(F("[ETH WORKER] telemetry body overflow"));
        return false;
    }

    body.toCharArray(job.body, sizeof(job.body));

    if (xQueueSend(rev6EthJobQueue, &job, 0) != pdTRUE)
        return false;

    rev6EthJobOutstanding = true;
    return true;
}

static void rev6EthernetWorkerTask(void *)
{
    rev6EthObservedCore = (int8_t)xPortGetCoreID();

    Rev6EthJob job;

    for (;;)
    {
        const BaseType_t gotJob =
            xQueueReceive(
                rev6EthJobQueue,
                &job,
                pdMS_TO_TICKS(ETH_WORKER_SERVICE_PERIOD_MS));

        if (gotJob != pdTRUE)
            continue;

        Rev6EthResult result;
        result.type = job.type;
        result.sequence = job.sequence;

        const uint32_t started = millis();

        if (rev6EthEvents != nullptr)
            xEventGroupSetBits(rev6EthEvents, ETH_WORKER_SPI_BUSY_BIT);

        result.busAcquired =
            jwplcSPI_acquire(ETH_WORKER_BUS_ACQUIRE_MS);

        if (result.busAcquired)
        {
            jwplcSPI_deselectAll();

            if (job.type == ETH_JOB_HTTP)
            {
                EthernetClient client;
                client.setConnectionTimeout(450);
                client.setTimeout(450);

                const IPAddress target(
                    job.ip[0], job.ip[1], job.ip[2], job.ip[3]);

                if (client.connect(target, job.port))
                {
                    client.print(F("POST /jwplc HTTP/1.1\r\nHost: "));
                    client.print(target.toString());
                    client.print(F("\r\nContent-Type: application/json\r\nContent-Length: "));
                    client.print(strlen(job.body));
                    client.print(F("\r\nConnection: close\r\n\r\n"));
                    client.print(job.body);

                    // Nunca ejecutar Modbus desde Core0.
                    result.ok =
                        validateHttpAck(
                            client,
                            job.sequence,
                            false);
                }

                client.stop();
            }
            else
            {
                EthernetUDP udp;

                if (udp.begin(job.localPort))
                {
                    uint8_t packet[48];
                    buildNtpPacket(packet);

                    if (udp.beginPacket(NTP_HOST, NTP_PORT))
                    {
                        udp.write(packet, sizeof(packet));

                        if (udp.endPacket())
                        {
                            const uint32_t ntpStarted = millis();

                            while ((uint32_t)(millis() - ntpStarted) < NTP_TIMEOUT_MS)
                            {
                                const int packetSize = udp.parsePacket();

                                if (packetSize >= 48)
                                {
                                    udp.read(packet, sizeof(packet));

                                    uint32_t utc = 0;
                                    if (decodeNtpPacket(packet, utc))
                                    {
                                        result.ntpLocal =
                                            (uint32_t)(
                                                (int64_t)utc +
                                                LIMA_UTC_OFFSET_SECONDS);
                                        result.ok = true;
                                        break;
                                    }
                                }

                                delay(2);
                            }
                        }
                    }

                    udp.stop();
                }
            }

            jwplcSPI_release();
        }

        if (rev6EthEvents != nullptr)
            xEventGroupClearBits(rev6EthEvents, ETH_WORKER_SPI_BUSY_BIT);

        result.latencyMs = millis() - started;
        xQueueOverwrite(rev6EthResultQueue, &result);
    }
}

static bool rev6StartEthernetWorker()
{
    if (rev6EthTaskHandle != nullptr)
        return true;

    rev6EthJobQueue = xQueueCreate(1, sizeof(Rev6EthJob));
    rev6EthResultQueue = xQueueCreate(1, sizeof(Rev6EthResult));
    rev6EthEvents = xEventGroupCreate();

    if (rev6EthJobQueue == nullptr ||
        rev6EthResultQueue == nullptr ||
        rev6EthEvents == nullptr)
    {
        Serial.println(F("[ETH WORKER] queue/event create FAIL"));
        return false;
    }

    const BaseType_t created =
        xTaskCreatePinnedToCore(
            rev6EthernetWorkerTask,
            "jw_w5500",
            ETH_WORKER_STACK_BYTES,
            nullptr,
            ETH_WORKER_PRIORITY,
            &rev6EthTaskHandle,
            ETH_WORKER_CORE);

    if (created != pdPASS)
    {
        rev6EthTaskHandle = nullptr;
        Serial.println(F("[ETH WORKER] task create FAIL"));
        return false;
    }

    Serial.print(F("[ETH WORKER] created targetCore="));
    Serial.println((int)ETH_WORKER_CORE);
    return true;
}

static void rev6ApplyWindowNtp(uint32_t ntpLocal)
{
    ntpValid = true;

    uint32_t localRtc = 0;
    if (JWPLC_RTC.readUnix(localRtc))
    {
        int32_t drift =
            (int32_t)localRtc - (int32_t)ntpLocal;

        if (drift < -32768) drift = -32768;
        if (drift > 32767) drift = 32767;

        ntpDriftSeconds = (int16_t)drift;

        Serial.print(F("[NTP DRIFT] node="));
        Serial.print(shortRoleName());
        Serial.print(F(" drift="));
        Serial.print(ntpDriftSeconds);
        Serial.println(F("s"));
    }
}

static void rev6ApplyInitialNtp(uint32_t ntpLocal)
{
    initialNtpFailCount = 0;
    syncLocalRtc(ntpLocal);

    if (!rtcSynced || !isMaster())
        return;

    for (uint8_t id = 1; id <= cfg.expectedSlaves; ++id)
    {
        uint32_t currentEpoch = rtcEpoch;
        (void)JWPLC_RTC.readUnix(currentEpoch);

        (void)sendSlaveCommand(
            id,
            CMD_SYNC_RTC,
            0,
            0,
            currentEpoch);
    }

    masterInitialRtcSyncSent = true;
    masterInitialRtcSyncRequested = false;
    soakState = SOAK_WAIT_RTC_SYNC;

    Serial.println(F("[RTC SYNC] Core0 NTP PASS; commands sent to all Slaves"));
}

static void rev6ServiceEthernetResult()
{
    if (rev6EthResultQueue == nullptr)
        return;

    Rev6EthResult result;

    if (xQueueReceive(rev6EthResultQueue, &result, 0) != pdTRUE)
        return;

    rev6EthJobOutstanding = false;

    if (result.latencyMs > rev6EthMaxJobMs)
        rev6EthMaxJobMs = result.latencyMs;

    if (!result.busAcquired)
    {
        rev6EthBusAcquireFail++;

        if (result.type == ETH_JOB_NTP_WINDOW)
            ethWindow.ntpDone = false;

        return;
    }

    if (result.type == ETH_JOB_HTTP)
    {
        ethHttp.lastLatencyMs = result.latencyMs;
        if (result.latencyMs > ethHttp.maxLatencyMs)
            ethHttp.maxLatencyMs = result.latencyMs;

        recordEthHttpResult(result.ok);
        return;
    }

    if (result.type == ETH_JOB_NTP_WINDOW)
    {
        if (result.ok)
            rev6ApplyWindowNtp(result.ntpLocal);
        else
            Serial.println(F("[NTP] Core0 window query failed (HTTP continues)"));

        return;
    }

    if (result.type == ETH_JOB_NTP_INITIAL)
    {
        if (result.ok)
        {
            rev6ApplyInitialNtp(result.ntpLocal);
        }
        else
        {
            initialNtpFailCount++;
            Serial.print(F("[RTC SYNC] Core0 NTP FAIL #"));
            Serial.println(initialNtpFailCount);

            if (initialNtpFailCount == 3)
                latchError(ERR_NTP, "initial Core0 Ethernet NTP failed x3");
        }
    }
}

static void rev6ServiceLocalEthernetWindow()
{
    const bool ownerNow =
        soakState == SOAK_RUNNING &&
        ethWindow.owner == localNodeOrdinal();

    if (ownerNow && !ethWindow.localWasOwner)
        beginLocalEthernetWindow();

    if (!ownerNow && ethWindow.localWasOwner)
    {
        if (!rev6EthJobOutstanding)
            finalizeLocalEthernetWindow();
        return;
    }

    if (!ownerNow || !JWPLC_Ethernet.isReady())
        return;

    // ETHNEXT espera a que un job local ya iniciado termine, pero desde la
    // solicitud no debe aceptar otro HTTP/NTP que vuelva a prolongar el handoff.
    if (isMaster() && rev6EthNextRequested)
        return;

    if (!ethWindow.ntpDone && !rev6EthJobOutstanding)
    {
        if (rev6WorkerQueueNtp(ETH_JOB_NTP_WINDOW))
            ethWindow.ntpDone = true;

        return;
    }

    const uint32_t now = millis();
    if (!rev6EthJobOutstanding &&
        (uint32_t)(now - lastEthHttpMs) >= ETH_HTTP_PERIOD_MS)
    {
        lastEthHttpMs = now;
        (void)rev6WorkerQueueHttp();
    }
}

// ============================================================================
// Commissioning Master sin W5500 bloqueante en Core1
// ============================================================================

static void rev6ServiceMasterCommissioning()
{
    if (!isMaster() || masterSoakRunning)
        return;

    const uint32_t now = millis();

    if ((uint32_t)(now - lastCommissionPrintMs) >= MASTER_COMMISSION_PRINT_MS)
    {
        lastCommissionPrintMs = now;
        printCommissioningTable();
    }

    if (!masterInitialRtcSyncSent)
    {
        if (!allNodesCommissioned())
            return;

        soakState = SOAK_WAIT_RTC_SYNC;

        if (!JWPLC_Ethernet.isReady() || rev6EthJobOutstanding)
            return;

        if (lastInitialNtpAttemptMs != 0 &&
            (uint32_t)(now - lastInitialNtpAttemptMs) < 10000UL)
        {
            return;
        }

        lastInitialNtpAttemptMs = now;

        Serial.println(F("[RTC SYNC] queue NTP via W5500 Core0..."));
        (void)rev6WorkerQueueNtp(ETH_JOB_NTP_INITIAL);
        return;
    }

    if (allNodesRtcSynced() && soakState != SOAK_READY_TO_START)
    {
        soakState = SOAK_READY_TO_START;

        Serial.println();
        printRule();
        Serial.println(F("ALL_COMMISSIONED=PASS"));
        Serial.println(F("RTC_NETWORK_SYNC=PASS"));
        Serial.println(F("READY_FOR_START"));
        Serial.println(F("REV6: Core1 determinista / W5500 blocking Core0"));
        printRule();
        tripleBeep();
    }
}

// ============================================================================
// Serial REV6: agrega SHOW / CLACK / SYNC
// ============================================================================

static void rev6I2CPrintHexByte(uint8_t value)
{
    if (value < 0x10)
        Serial.print('0');
    Serial.print(value, HEX);
}

static void rev6I2CPrintBytes(const __FlashStringHelper *label, const uint8_t *data, size_t length)
{
    Serial.print(label);
    Serial.print('=');
    for (size_t i = 0; i < length; ++i)
    {
        if (i != 0)
            Serial.print(' ');
        rev6I2CPrintHexByte(data[i]);
    }
    Serial.println();
}

static void rev6I2CPrintProbeResult(const char *name, uint8_t address, int result)
{
    Serial.print(name);
    Serial.print(F(" @0x"));
    rev6I2CPrintHexByte(address);
    Serial.print(F("="));
    Serial.print(result == ESP_OK ? F("PASS") : F("FAIL"));
    Serial.print(F(" err="));
    Serial.print(result);
    Serial.print(F("/"));
    Serial.println(esp_err_to_name((esp_err_t)result));
}

static void rev6ResetI2CDiagnostics()
{
    rev6I2CDiag = Rev6I2CDiagStats{};
    rev6I2CLastLineSampleMs = 0;
    rev6I2CNextWatchMs = millis() + REV6_I2C_WATCH_PERIOD_MS;
}

static bool rev6I2CCanActiveProbe()
{
    if (!isMaster())
        return true;

    if (rev6StopRequested || rev6EthNextRequested)
        return false;

    if (rev6SyncStage != REV6_STAGE_IDLE ||
        rev6PendingMasterRequest.pending ||
        rev6RetrySequencePending ||
        rev6SyncTxnActive)
    {
        return false;
    }

    if (masterRuntimePollPhase != MASTER_RT_POLL_IDLE ||
        JWPLC_ModbusRTU.masterBusy())
    {
        return false;
    }

    return true;
}

static int rev6I2CRawSdaLevel()
{
    return gpio_get_level(GPIO_NUM_21);
}

static int rev6I2CRawSclLevel()
{
    return gpio_get_level(GPIO_NUM_22);
}

static void rev6I2CSampleLines(bool verbose)
{
    // Lectura fisica de los pads. No usa digitalRead(): en JWPLC esa API pasa
    // por digitalPinToGPIONumber() y no sirve como observacion RAW del bus.
    const int sdaLevel = rev6I2CRawSdaLevel();
    const int sclLevel = rev6I2CRawSclLevel();

    if (sdaLevel == 0)
    {
        rev6I2CDiag.sdaLowSamples++;
        rev6I2CDiag.sdaLowConsecutive++;
    }
    else
    {
        rev6I2CDiag.sdaLowConsecutive = 0;
    }

    if (sclLevel == 0)
    {
        rev6I2CDiag.sclLowSamples++;
        rev6I2CDiag.sclLowConsecutive++;
    }
    else
    {
        rev6I2CDiag.sclLowConsecutive = 0;
    }

    if (sdaLevel == 0 && sclLevel == 0)
        rev6I2CDiag.bothLowSamples++;

    // Un sample LOW aislado puede coincidir con una transferencia valida.
    // Solo imprimir automaticamente si persiste >=3 muestras de 1 s.
    if (verbose ||
        rev6I2CDiag.sdaLowConsecutive == 3 ||
        rev6I2CDiag.sclLowConsecutive == 3)
    {
        Serial.print(F("[I2C RAW] node="));
        Serial.print(shortRoleName());
        Serial.print(F(" GPIO21/SDA="));
        Serial.print(sdaLevel);
        Serial.print(F(" GPIO22/SCL="));
        Serial.print(sclLevel);
        Serial.print(F(" lowConsecutive="));
        Serial.print(rev6I2CDiag.sdaLowConsecutive);
        Serial.print('/');
        Serial.println(rev6I2CDiag.sclLowConsecutive);
    }
}

static void rev6I2CProbeExpected(bool verbose)
{
    const int tcaResult = jwplcI2C_probe(REV6_I2C_TCA_ADDR);
    const int rtcResult = jwplcI2C_probe(REV6_I2C_RTC_ADDR);

    uint8_t tcaInput[3] = {0, 0, 0};
    int tcaInputErr = ESP_FAIL;
    bool tcaInputChanged = false;

    if (tcaResult == ESP_OK)
    {
        tcaInputErr = jwplcI2C_readRegs(
            REV6_I2C_TCA_ADDR,
            0x00,
            3,
            tcaInput);

        if (tcaInputErr == ESP_OK)
        {
            tcaInputChanged =
                !rev6I2CDiag.haveTcaInput ||
                memcmp(tcaInput, rev6I2CDiag.lastTcaInput, 3) != 0;

            memcpy(rev6I2CDiag.lastTcaInput, tcaInput, 3);
            rev6I2CDiag.haveTcaInput = true;
        }
    }

    rev6I2CDiag.cycles++;
    rev6I2CDiag.lastTcaErr = tcaResult;
    rev6I2CDiag.lastRtcErr = rtcResult;
    rev6I2CDiag.lastTcaInputErr = tcaInputErr;

    if (tcaResult == ESP_OK)
        rev6I2CDiag.tcaOk++;
    else
        rev6I2CDiag.tcaFail++;

    if (rtcResult == ESP_OK)
        rev6I2CDiag.rtcOk++;
    else
        rev6I2CDiag.rtcFail++;

    if (verbose ||
        tcaResult != ESP_OK ||
        rtcResult != ESP_OK ||
        tcaInputErr != ESP_OK ||
        tcaInputChanged)
    {
        Serial.print(F("[I2C WATCH] node="));
        Serial.print(shortRoleName());
        Serial.print(F(" cycle="));
        Serial.print(rev6I2CDiag.cycles);
        Serial.print(F(" TCA22="));
        Serial.print(tcaResult == ESP_OK ? F("OK") : F("FAIL"));
        Serial.print(F(" RTC68="));
        Serial.print(rtcResult == ESP_OK ? F("OK") : F("FAIL"));
        Serial.print(F(" RAW21/22="));
        Serial.print(rev6I2CRawSdaLevel());
        Serial.print('/');
        Serial.print(rev6I2CRawSclLevel());
        Serial.print(F(" appRTC="));
        Serial.print(rtcPresent ? F("OK") : F("--"));
        Serial.print(F(" rtcFail="));
        Serial.println(rtcFail);

        if (tcaInputErr == ESP_OK)
            rev6I2CPrintBytes(F("[I2C TCA] INPUT0..2"), tcaInput, 3);
        else
        {
            Serial.print(F("[I2C TCA] input-read FAIL err="));
            Serial.print(tcaInputErr);
            Serial.print('/');
            Serial.println(esp_err_to_name((esp_err_t)tcaInputErr));
        }
    }

    if (verbose || tcaResult != ESP_OK)
        rev6I2CPrintProbeResult("TCA6424A", REV6_I2C_TCA_ADDR, tcaResult);

    if (verbose || rtcResult != ESP_OK)
        rev6I2CPrintProbeResult("RTC", REV6_I2C_RTC_ADDR, rtcResult);
}

static void rev6PrintI2CSummary()
{
    Serial.print(F("I2C watch="));
    Serial.print(rev6I2CWatchEnabled ? F("ON") : F("OFF"));
    Serial.print(F(" cycles="));
    Serial.print(rev6I2CDiag.cycles);
    Serial.print(F(" TCA ok/fail="));
    Serial.print(rev6I2CDiag.tcaOk);
    Serial.print('/');
    Serial.print(rev6I2CDiag.tcaFail);
    Serial.print(F(" RTC ok/fail="));
    Serial.print(rev6I2CDiag.rtcOk);
    Serial.print('/');
    Serial.print(rev6I2CDiag.rtcFail);
    Serial.print(F(" deferred="));
    Serial.print(rev6I2CDiag.deferredBusy);
    Serial.print(F(" lineLow SDA/SCL/both="));
    Serial.print(rev6I2CDiag.sdaLowSamples);
    Serial.print('/');
    Serial.print(rev6I2CDiag.sclLowSamples);
    Serial.print('/');
    Serial.print(rev6I2CDiag.bothLowSamples);
    Serial.print(F(" lastErr TCA/RTC/input="));
    Serial.print(rev6I2CDiag.lastTcaErr);
    Serial.print('/');
    Serial.print(rev6I2CDiag.lastRtcErr);
    Serial.print('/');
    Serial.print(rev6I2CDiag.lastTcaInputErr);
    Serial.print(F(" rawTCA="));
    if (rev6I2CDiag.haveTcaInput)
    {
        rev6I2CPrintHexByte(rev6I2CDiag.lastTcaInput[0]);
        Serial.print('/');
        rev6I2CPrintHexByte(rev6I2CDiag.lastTcaInput[1]);
        Serial.print('/');
        rev6I2CPrintHexByte(rev6I2CDiag.lastTcaInput[2]);
    }
    else
    {
        Serial.print(F("--/--/--"));
    }
    Serial.println();
}

static void rev6PrintI2CDiagnostics()
{
    Serial.println();
    Serial.println(F("---- I2C DIAGNOSTICS ----"));
    Serial.print(F("NODE="));
    Serial.print(shortRoleName());
    Serial.println(F(" RAW_GPIO_SDA=21 RAW_GPIO_SCL=22"));

    rev6I2CSampleLines(true);

    Serial.println(F("EXPECTED: TCA6424A@0x22 RTC@0x68; TCA@0x23=ALT/OPTIONAL"));

    const int tcaResult = jwplcI2C_probe(REV6_I2C_TCA_ADDR);
    const int tcaAltResult = jwplcI2C_probe(REV6_I2C_TCA_ALT_ADDR);
    const int rtcResult = jwplcI2C_probe(REV6_I2C_RTC_ADDR);

    rev6I2CPrintProbeResult("TCA6424A", REV6_I2C_TCA_ADDR, tcaResult);
    rev6I2CPrintProbeResult("TCA_ALT", REV6_I2C_TCA_ALT_ADDR, tcaAltResult);
    rev6I2CPrintProbeResult("RTC", REV6_I2C_RTC_ADDR, rtcResult);

    if (tcaResult == ESP_OK)
    {
        uint8_t input[3] = {};
        uint8_t output[3] = {};
        uint8_t config[3] = {};
        const int inputErr = jwplcI2C_readRegs(REV6_I2C_TCA_ADDR, 0x00, 3, input);
        const int outputErr = jwplcI2C_readRegs(REV6_I2C_TCA_ADDR, 0x04, 3, output);
        const int configErr = jwplcI2C_readRegs(REV6_I2C_TCA_ADDR, 0x0C, 3, config);

        Serial.print(F("TCA_REG_READ err input/output/config="));
        Serial.print(inputErr);
        Serial.print('/');
        Serial.print(outputErr);
        Serial.print('/');
        Serial.println(configErr);

        if (inputErr == ESP_OK)
            rev6I2CPrintBytes(F("TCA_INPUT0..2"), input, 3);
        if (outputErr == ESP_OK)
            rev6I2CPrintBytes(F("TCA_OUTPUT0..2"), output, 3);
        if (configErr == ESP_OK)
            rev6I2CPrintBytes(F("TCA_CONFIG0..2"), config, 3);
    }

    if (rtcResult == ESP_OK)
    {
        uint8_t timeRegs[7] = {};
        uint8_t ctrlStatus[2] = {};
        uint8_t tempRegs[2] = {};
        const int timeErr = jwplcI2C_readRegs(REV6_I2C_RTC_ADDR, 0x00, 7, timeRegs);
        const int ctrlErr = jwplcI2C_readRegs(REV6_I2C_RTC_ADDR, 0x0E, 2, ctrlStatus);
        const int tempErr = jwplcI2C_readRegs(REV6_I2C_RTC_ADDR, 0x11, 2, tempRegs);

        Serial.print(F("RTC_REG_READ err time/ctrl/temp="));
        Serial.print(timeErr);
        Serial.print('/');
        Serial.print(ctrlErr);
        Serial.print('/');
        Serial.println(tempErr);

        if (timeErr == ESP_OK)
            rev6I2CPrintBytes(F("RTC_TIME_00..06"), timeRegs, 7);
        if (ctrlErr == ESP_OK)
            rev6I2CPrintBytes(F("RTC_CTRL_STATUS_0E..0F"), ctrlStatus, 2);
        if (tempErr == ESP_OK)
            rev6I2CPrintBytes(F("RTC_TEMP_RAW_11..12"), tempRegs, 2);
    }

    Serial.print(F("APP RTC present/synced/fail="));
    Serial.print(rtcPresent ? 1 : 0);
    Serial.print('/');
    Serial.print(rtcSynced ? 1 : 0);
    Serial.print('/');
    Serial.println(rtcFail);

    Serial.print(F("APP Q/I/mismatch=0x"));
    rev6I2CPrintHexByte(ioStress.outputBitmap);
    Serial.print(F("/0x"));
    rev6I2CPrintHexByte(ioStress.inputBitmap);
    Serial.print('/');
    Serial.println(ioStress.mismatches);

    rev6PrintI2CSummary();
    Serial.println(F("NOTE: I2C diag no latches ERR; RTC/TCA funcionales conservan su propio ERR."));
    Serial.println(F("-------------------------"));
}

static void rev6ServiceI2CDiagnostics()
{
    const uint32_t now = millis();

    if ((uint32_t)(now - rev6I2CLastLineSampleMs) >= REV6_I2C_LINE_SAMPLE_MS)
    {
        rev6I2CLastLineSampleMs = now;
        rev6I2CSampleLines(false);
    }

    if (!rev6I2CWatchEnabled || soakState == SOAK_NEED_ROLE)
        return;

    if ((int32_t)(now - rev6I2CNextWatchMs) < 0)
        return;

    if (!rev6I2CCanActiveProbe())
    {
        rev6I2CDiag.deferredBusy++;
        rev6I2CNextWatchMs = now + REV6_I2C_BUSY_RETRY_MS;
        return;
    }

    rev6I2CProbeExpected(true);

    const uint32_t period =
        soakState == SOAK_RUNNING
            ? REV6_I2C_WATCH_PERIOD_MS
            : REV6_I2C_COMMISSION_PERIOD_MS;

    rev6I2CNextWatchMs = now + period;
}

static void rev6PrintDiagnostics()
{
    Serial.println();
    Serial.println(F("---- REV6 CORE SPLIT ----"));
    Serial.print(F("SYNC mode="));
    Serial.print(rev6SyncMode == REV6_MODE_CLACK ? F("CLACK") : F("SHOW"));
    Serial.print(F(" apply="));
    Serial.print(rev6ApplyCount);
    Serial.print(F(" fail="));
    Serial.print(rev6SyncFail);
    Serial.print(F(" txns="));
    Serial.print(rev6TxnCount);
    Serial.print(F(" timeouts="));
    Serial.print(rev6TxnTimeoutCount);
    Serial.print(F(" retry="));
    Serial.print(rev6RetryAttempts);
    Serial.print('/');
    Serial.print(rev6RetryRecovered);
    Serial.print('/');
    Serial.print(rev6RetryFinalTimeouts);
    Serial.print(F(" rejects="));
    Serial.print(rev6RequestRejectCount);
    Serial.print(F(" maxTxnUs="));
    Serial.print(rev6TxnMaxUs);
    Serial.print(F(" gapMs="));
    Serial.print(REV6_MODBUS_INTER_TX_GAP_MS);
    Serial.print(F(" ioRaceDiscard="));
    Serial.println(rev6IoRaceDiscardCount);

    rev6PrintI2CSummary();

    Serial.print(F("STOP pending/lastMs/maxMs/count="));
    Serial.print(rev6StopRequested ? 1 : 0);
    Serial.print('/');
    Serial.print(rev6StopLastDrainMs);
    Serial.print('/');
    Serial.print(rev6StopMaxDrainMs);
    Serial.print('/');
    Serial.println(rev6StopCount);

    Serial.print(F("ETHNEXT pending/stage/lastMs/maxMs/count="));
    Serial.print(rev6EthNextRequested ? 1 : 0);
    Serial.print('/');
    Serial.print((int)rev6EthNextStage);
    Serial.print('/');
    Serial.print(rev6EthNextLastMs);
    Serial.print('/');
    Serial.print(rev6EthNextMaxMs);
    Serial.print('/');
    Serial.println(rev6EthNextCount);

    Serial.print(F("ETH_WORKER core="));
    Serial.print((int)rev6EthObservedCore);
    Serial.print(F(" busy="));
    Serial.print(rev6EthSpiBusy() ? 1 : 0);
    Serial.print(F(" outstanding="));
    Serial.print(rev6EthJobOutstanding ? 1 : 0);
    Serial.print(F(" maxJobMs="));
    Serial.print(rev6EthMaxJobMs);
    Serial.print(F(" busAcquireFail="));
    Serial.println(rev6EthBusAcquireFail);

    Serial.print(F("SPI_DEFER FRAM/SD="));
    Serial.print(rev6EthFramDeferred);
    Serial.print('/');
    Serial.println(rev6EthSdDeferred);
    Serial.println(F("-------------------------"));
}

static void rev6HandleSerial(String line)
{
    line.trim();
    if (line.length() == 0)
        return;

    String upper = line;
    upper.toUpperCase();

    if (upper == "SHOW")
    {
        rev6SetMode(REV6_MODE_SHOW);
        if (isMaster() && soakState == SOAK_READY_TO_START)
            startSoakMaster();
        return;
    }

    if (upper == "CLACK")
    {
        rev6SetMode(REV6_MODE_CLACK);
        if (isMaster() && soakState == SOAK_READY_TO_START)
            startSoakMaster();
        return;
    }

    if (upper == "STOP" &&
        isMaster() &&
        soakState == SOAK_RUNNING)
    {
        rev6RequestSafeStop();
        return;
    }

    if (upper == "ETHNEXT" &&
        isMaster() &&
        soakState == SOAK_RUNNING)
    {
        rev6RequestSafeEthNext(false);
        return;
    }

    if (upper == "I2C")
    {
        rev6PrintI2CDiagnostics();
        return;
    }

    if (upper == "I2CWATCH ON")
    {
        rev6I2CWatchEnabled = true;
        rev6I2CNextWatchMs = millis() + REV6_I2C_BUSY_RETRY_MS;
        Serial.println(F("I2C_WATCH=ON"));
        return;
    }

    if (upper == "I2CWATCH OFF")
    {
        rev6I2CWatchEnabled = false;
        Serial.println(F("I2C_WATCH=OFF"));
        return;
    }

    if (upper == "SYNC")
    {
        rev6PrintDiagnostics();
        return;
    }

    handleSerialCommand(line);
}

static void rev6ServiceSerial()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();

        if (c == '\r' || c == '\n')
        {
            if (serialLine.length() > 0)
            {
                rev6HandleSerial(serialLine);
                serialLine = "";
            }
        }
        else if (serialLine.length() < 64)
        {
            serialLine += c;
        }
    }
}

static void rev6ServiceWorkerPeriodicDiag()
{
    const uint32_t now = millis();
    if ((uint32_t)(now - rev6LastWorkerDiagMs) < RUNTIME_DIAG_PRINT_MS)
        return;

    rev6LastWorkerDiagMs = now;

    if (soakState == SOAK_RUNNING)
    {
        Serial.print(F("[REV6] loopMaxMs="));
        Serial.print(maxLoopUs / 1000UL);
        Serial.print(F(" ethLatencyMaxMs="));
        Serial.print(ethHttp.maxLatencyMs);
        Serial.print(F(" ethWorkerMaxMs="));
        Serial.print(rev6EthMaxJobMs);
        Serial.print(F(" syncFail="));
        Serial.print(rev6SyncFail);
        Serial.print(F(" syncTimeout="));
        Serial.print(rev6TxnTimeoutCount);
        Serial.print(F(" syncRetry="));
        Serial.print(rev6RetryAttempts);
        Serial.print('/');
        Serial.print(rev6RetryRecovered);
        Serial.print('/');
        Serial.print(rev6RetryFinalTimeouts);
        Serial.print(F(" ioRaceDiscard="));
        Serial.print(rev6IoRaceDiscardCount);
        Serial.print(F(" ioMismatch="));
        Serial.println(ioStress.mismatches);
    }
}

// ============================================================================
// setup / loop REV6
// ============================================================================

void setup()
{
    alpha7AsyncBaseSetup();

    if (rev6SyncTaskHandle == nullptr)
    {
        const BaseType_t created =
            xTaskCreatePinnedToCore(
                rev6SyncApplyTask,
                "jwplcSyncIO",
                REV6_SYNC_TASK_STACK,
                nullptr,
                REV6_SYNC_TASK_PRIORITY,
                &rev6SyncTaskHandle,
                REV6_SYNC_TASK_CORE);

        Serial.print(F("[SYNC REV6] apply task="));
        Serial.println(created == pdPASS ? F("PASS") : F("FAIL"));
    }

    (void)rev6StartEthernetWorker();
    delay(10);

    Serial.println(F("GATE_RT_REV=6 7NB-core1-deterministic-w5500-core0"));
    Serial.print(F("BUZZER_VOLUME="));
    Serial.print(REV6_BUZZER_VOLUME);
    Serial.println(F("/255"));
    Serial.print(F("SYNC_FC06_GAP_MS="));
    Serial.println(REV6_MODBUS_INTER_TX_GAP_MS);
    Serial.println(F("SYNC_IO_GENERATION_GUARD=ON"));
    Serial.println(F("SYNC_TIMEOUT_RETRY=SEQUENCE_ONCE_PRE_TRIGGER"));
    Serial.println(F("I2C_DIAG=WATCH30S+COMMAND"));
    Serial.println(F("I2C_DIAG_V2=RAW_GPIO+COMMISSIONING_PROBE"));
    Serial.println(F("I2C_EXPECTED=TCA6424A@0x22,RTC@0x68"));
    Serial.print(F("STOP_SAFE_GAP_MS="));
    Serial.println(REV6_STOP_QUIET_GAP_MS);
    Serial.print(F("ETHNEXT_SAFE_GAP_MS="));
    Serial.println(REV6_ETHNEXT_QUIET_GAP_MS);
    Serial.print(F("ETHNEXT_RELEASE_GRACE_MS="));
    Serial.println(REV6_ETHNEXT_RELEASE_GRACE_MS);
    Serial.print(F("[CORE] W5500 worker observed core="));
    Serial.println((int)rev6EthObservedCore);
    Serial.println(F("SYNC_COMMANDS=START/SHOW/CLACK/STOP/SYNC/ETHNEXT/I2C/I2CWATCH ON|OFF"));
}

void loop()
{
    loopStartUs = micros();

    rev6ServiceSerial();
    serviceBleQualificationRestart();
    rev6ServiceEthernetResult();
    rev6UpdateTakeover();

    // I/O legacy solo fuera del RUN sincronizado.
    if (!rev6SyncTakeover)
        serviceIO();
    else
        rev6ServiceLoopback();

    serviceRTC();

    if (modbusReady)
    {
        if (isSlave())
            refreshHoldingRegisters();

        JWPLC_ModbusRTU.task();
        serviceModbusDiagnostics();
    }

    if (isSlave() && modbusReady)
    {
        // Durante RUN, HR_CMD_SEQ pertenece al scheduler si CODE==SYNC.
        // STOP/RTC/etc siguen pasando por el handler base.
        if (rev6SyncTakeover &&
            holdingRegisters[HR_CMD_CODE] == CMD_SYNC_IO_PHASE)
        {
            rev6ServiceSlaveSync();
        }
        else
        {
            processSlaveCommand();
        }

        if (holdingRegisters[HR_ETH_OWNER] != ethWindow.owner)
        {
            const uint16_t previousOwner = ethWindow.owner;
            ethWindow.owner = holdingRegisters[HR_ETH_OWNER];

            if (previousOwner == localNodeOrdinal() &&
                ethWindow.owner != localNodeOrdinal() &&
                !rev6EthJobOutstanding)
            {
                finalizeLocalEthernetWindow();
            }
        }
    }

    // Un START/STOP Slave pudo cambiar el estado en este mismo loop.
    rev6UpdateTakeover();

    if (isMaster())
    {
        refreshHoldingRegisters();

        if (rev6StopRequested)
        {
            rev6ServiceSafeStop();
        }
        else if (rev6EthNextRequested)
        {
            rev6ServiceSafeEthNext();
        }
        else if (soakState == SOAK_RUNNING)
        {
            if (rev6SyncWantsModbusPriority())
            {
                rev6ServiceMasterSync();
            }
            else
            {
                masterPollOneSlaveCooperative();
                rev6ServiceMasterSync();
            }
        }
        else
        {
            masterPollOneSlaveSyncCommissioning();
        }

        rev6ServiceMasterCommissioning();
    }

    serviceWiFi();

    // El state machine cooperativo de JWPLC_Ethernet permanece en Core1.
    // Solo corre cuando Core0 no posee el W5500; asi su estado/cache no cruza
    // cores y no interpreta la reserva del bus como un LINK_OFF.
    if (!rev6EthSpiBusy())
    {
        const uint32_t opUs = micros();
        JWPLC_Ethernet.service();
        const uint32_t elapsedUs = micros() - opUs;

        if (elapsedUs > ethernetServiceMaxUs)
            ethernetServiceMaxUs = elapsedUs;
    }

    // W5500 puede reservar SPI durante HTTP/NTP en Core0. FRAM/SD no fallan:
    // simplemente esperan al siguiente loop libre. ScanIO/RTC/Modbus no usan SPI.
    if (!rev6EthSpiBusy())
    {
        const uint32_t opUs = micros();
        serviceFRAM();
        const uint32_t elapsedUs = micros() - opUs;

        if (elapsedUs > framServiceMaxUs)
            framServiceMaxUs = elapsedUs;
    }
    else
    {
        rev6EthFramDeferred++;
    }

    {
        const uint32_t opUs = micros();
        rev6ServiceLocalEthernetWindow();
        rev6ServiceEthernetResult();
        const uint32_t elapsedUs = micros() - opUs;

        // Aqui solo hay scheduling/colas/resultados; la latencia real de red
        // queda en ethHttp.maxLatencyMs / rev6EthMaxJobMs.
        if (elapsedUs > ethernetServiceMaxUs)
            ethernetServiceMaxUs = elapsedUs;
    }

    if (isMaster())
    {
        // STOP y ETHNEXT son transiciones exclusivas. La expiracion automatica
        // de ventana tambien entra por la misma maquina segura del handoff.
        if (!rev6StopRequested && !rev6EthNextRequested)
            rev6ServiceMasterEthernetRotationSafe();

        if (!rev6EthSpiBusy())
        {
            const uint32_t opUs = micros();
            serviceMasterSD();
            const uint32_t elapsedUs = micros() - opUs;

            if (elapsedUs > sdServiceMaxUs)
                sdServiceMaxUs = elapsedUs;
        }
        else
        {
            rev6EthSdDeferred++;
        }
    }

    if (rev6SyncTakeover)
        rev6ServiceLoopback();

    rev6ServiceI2CDiagnostics();

    refreshHoldingRegisters();
    servicePeriodicRuntimeDiagnostics();
    rev6ServiceWorkerPeriodicDiag();

    const uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < minFreeHeap)
        minFreeHeap = freeHeap;

    const uint32_t loopUs = micros() - loopStartUs;
    lastLoopUs = loopUs;

    if (loopUs > maxLoopUs)
        maxLoopUs = loopUs;

    if (loopUs >= LONG_LOOP_WARN_US)
        longLoop50msCount++;

    if (loopUs >= LONG_LOOP_CRIT_US)
        longLoop250msCount++;

    delay(1);
}