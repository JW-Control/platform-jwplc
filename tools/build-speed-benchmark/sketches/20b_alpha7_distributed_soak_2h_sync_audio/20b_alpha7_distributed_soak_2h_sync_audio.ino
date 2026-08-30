/*
  Gate 7NB.3A-C - Soak 20 async + I/O master-driven sincronizado + audio

  Esta variante REUTILIZA el soak 20 validado y sustituye solamente la
  temporizacion autonoma de I/O por un scheduler direccionado M2 -> Slaves.
  No modifica JWPLC_ModbusRTU.

  Objetivos:
    - conservar WiFi, Ethernet, FRAM, RTC, SD, display, RS-485 y Modbus;
    - conservar loopback Q0.x -> I0.x, pulsos y mismatch;
    - M2 gobierna ON/OFF y canal para toda la red;
    - cada nodo aplica la salida desde una tarea de alta prioridad en Core 1,
      para no perder sincronismo si el loop Arduino queda ocupado por red;
    - cada cambio genera una nota sincronizada con volumen configurable.

  Volumen:
    SOAK_BUZZER_VOLUME = 0..255
      0  : mute
      18 : suave
      24 : recomendado para grabar voz + prueba
      48 : fuerte
*/

#include <Arduino.h>

// El soak base se incluye como fuente validada, pero sus escrituras no-cero
// de digitalWriteBlock() quedan bloqueadas en esta variante. Así evitamos el
// primer click autónomo cuando START pone IO_NODE_STAGGER_MS=0. El scheduler
// nuevo escribe el banco directamente mediante jwplc_digitalWriteBlock().
static void syncLegacyDigitalWriteBlock(
    const uint16_t *pins,
    uint8_t count,
    uint8_t bitmap)
{
    if (bitmap == 0)
        jwplc_digitalWriteBlock(pins, count, bitmap);
}

#undef digitalWriteBlock
#define digitalWriteBlock(pins, bitmap) \
    syncLegacyDigitalWriteBlock( \
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
// Configuracion del overlay sincronizado
// ============================================================================

static constexpr uint16_t CMD_SYNC_IO_PHASE = 5;

static constexpr bool SOAK_BUZZER_ENABLED = true;
static constexpr uint8_t SOAK_BUZZER_VOLUME = 24; // 0..255
static constexpr uint8_t SOAK_BUZZER_PWM_RES_BITS = 8;
static constexpr uint16_t SOAK_BUZZER_NOTE_MS = 115;

// Gate 21B dio ~6 ms max por FC06. Dejamos margen para M2..M8.
static constexpr uint32_t SYNC_LEAD_BASE_US = 220000UL;
static constexpr uint32_t SYNC_LEAD_PER_SLAVE_US = 35000UL;
static constexpr uint32_t SYNC_TRIGGER_COMP_US = 7000UL;
static constexpr uint32_t SYNC_MIN_REMAINING_US = 20000UL;
static constexpr uint32_t SYNC_RETRY_MS = 250UL;

static constexpr UBaseType_t SYNC_APPLY_TASK_PRIORITY = 3;
static constexpr uint32_t SYNC_APPLY_TASK_STACK = 4096UL;
static constexpr BaseType_t SYNC_APPLY_TASK_CORE = 1;

// Escala C5..C6. Q0_0..Q0_7 ascienden de nota.
static constexpr uint16_t SYNC_NOTE_HZ[8] = {
    523, 587, 659, 698, 784, 880, 988, 1047
};
static constexpr uint16_t SYNC_OFF_NOTE_HZ = 392;

enum SyncDispatchField : uint8_t
{
    SYNC_FIELD_ARG0 = 0,
    SYNC_FIELD_CODE,
    SYNC_FIELD_DELAY,
    SYNC_FIELD_TRIGGER,
    SYNC_FIELD_DONE
};

static bool syncTakeover = false;
static bool syncMasterDispatch = false;
static bool syncMasterWaitingApply = false;
static bool syncTxnActive = false;
static bool syncPhaseCurrentlyOn = false;
static uint8_t syncChannel = 0;
static uint8_t syncTargetSlave = 1;
static SyncDispatchField syncField = SYNC_FIELD_ARG0;
static uint16_t syncSequence = 0;
static uint8_t syncDesiredMask = 0;
static uint32_t syncMasterTargetUs = 0;
static uint32_t syncNextPhaseMs = 0;
static uint32_t syncTxnStartedUs = 0;
static uint32_t syncTxnMaxUs = 0;
static uint32_t syncFailCount = 0;
static uint32_t syncApplyCount = 0;

static uint16_t syncSlaveLastTrigger = 0;

static volatile bool syncApplyPending = false;
static volatile uint8_t syncApplyMask = 0;
static volatile uint8_t syncApplyChannel = 0;
static volatile uint32_t syncApplyAtUs = 0;
static volatile uint16_t syncApplyToken = 0;
static volatile uint16_t syncAppliedToken = 0;

static bool syncAudioActive = false;
static uint32_t syncAudioStopAtUs = 0;
static TaskHandle_t syncApplyTaskHandle = nullptr;

static bool syncDueUs(uint32_t now, uint32_t target)
{
    return (int32_t)(now - target) >= 0;
}

static uint8_t syncPopcount8(uint8_t value)
{
    uint8_t count = 0;
    while (value)
    {
        count += (uint8_t)(value & 1U);
        value >>= 1U;
    }
    return count;
}

static uint16_t syncFrequencyForPattern(uint8_t mask, uint8_t channel)
{
    if (mask == 0)
        return SYNC_OFF_NOTE_HZ;

    const uint8_t safeChannel = (channel < 8) ? channel : 0;
    uint16_t frequency = SYNC_NOTE_HZ[safeChannel];

    // Si en futuros patrones hay varias Q activas, la cantidad de bits
    // desplaza ligeramente la nota. Patrones iguales -> notas iguales;
    // patrones distintos -> notas potencialmente distintas.
    const uint8_t count = syncPopcount8(mask);
    if (count > 1)
        frequency = (uint16_t)(frequency + (uint16_t)(count - 1U) * 28U);

    return frequency;
}

static void syncAudioStop()
{
    if (!syncAudioActive)
        return;

    ledcWrite(BUZZER_PIN, 0);
    ledcWriteTone(BUZZER_PIN, 0);
    ledcDetach(BUZZER_PIN);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    syncAudioActive = false;
}

static void syncAudioStart(uint16_t frequency)
{
    if (!SOAK_BUZZER_ENABLED || SOAK_BUZZER_VOLUME == 0 || frequency == 0)
        return;

    // El soak base usa tone() para avisos esporadicos. Para las notas del
    // patron soltamos cualquier canal previo y usamos LEDC directo, porque
    // tone() no ofrece control de volumen/duty.
    syncAudioStop();
    ledcDetach(BUZZER_PIN);

    if (!ledcAttach(
            BUZZER_PIN,
            frequency,
            SOAK_BUZZER_PWM_RES_BITS))
    {
        return;
    }

    ledcWriteTone(BUZZER_PIN, frequency);
    ledcWrite(BUZZER_PIN, SOAK_BUZZER_VOLUME);
    syncAudioStopAtUs = micros() + (uint32_t)SOAK_BUZZER_NOTE_MS * 1000UL;
    syncAudioActive = true;
}

static void syncApplyNow(uint8_t mask, uint8_t channel, uint16_t token)
{
    const uint8_t previous = ioStress.outputBitmap;

    // Bypass deliberado de setOutputs(): esa funcion pertenece al oscilador
    // legacy y sus ON quedan bloqueados por esta variante.
    jwplc_digitalWriteBlock(Q0_X, 8, mask);
    ioStress.outputBitmap = mask;
    ioStress.channel = channel;
    ioStress.onPhase = (mask != 0);
    ioStress.phaseStartMs = millis();
    ioStress.phaseFailureLatched = false;
    ioStress.pulseDetectedThisOn = false;

    const uint8_t rising = (uint8_t)(mask & (uint8_t)~previous);
    if (rising != 0 && channel < 8 && (rising & (uint8_t)(1U << channel)))
        ioStress.qPulses[channel]++;

    syncAudioStart(syncFrequencyForPattern(mask, channel));

    syncApplyCount++;
    syncAppliedToken = token;
}

static void syncApplyTask(void *)
{
    for (;;)
    {
        if (syncApplyPending && syncDueUs(micros(), syncApplyAtUs))
        {
            const uint8_t mask = syncApplyMask;
            const uint8_t channel = syncApplyChannel;
            const uint16_t token = syncApplyToken;

            syncApplyPending = false;
            syncApplyNow(mask, channel, token);
        }

        if (syncAudioActive && syncDueUs(micros(), syncAudioStopAtUs))
            syncAudioStop();

        vTaskDelay(1);
    }
}

static void syncQueueApply(
    uint8_t mask,
    uint8_t channel,
    uint32_t targetUs,
    uint16_t token)
{
    syncApplyMask = mask;
    syncApplyChannel = channel;
    syncApplyAtUs = targetUs;
    syncApplyToken = token;
    syncApplyPending = true;
}

static uint32_t syncLeadUs()
{
    uint32_t slaves = cfg.expectedSlaves;
    if (slaves > MAX_SLAVES)
        slaves = MAX_SLAVES;

    return SYNC_LEAD_BASE_US + slaves * SYNC_LEAD_PER_SLAVE_US;
}

static void syncAbortDispatch(const char *reason)
{
    syncMasterDispatch = false;
    syncTxnActive = false;
    syncMasterWaitingApply = false;
    syncField = SYNC_FIELD_ARG0;
    syncFailCount++;
    syncNextPhaseMs = millis() + SYNC_RETRY_MS;

    Serial.print(F("[SYNC IO] FAIL: "));
    Serial.println(reason);
    latchError(ERR_MODBUS, reason);
}

static bool syncRemainingDelayMs(uint16_t &delayMs)
{
    const int32_t remainingUs =
        (int32_t)(syncMasterTargetUs - micros()) -
        (int32_t)SYNC_TRIGGER_COMP_US;

    if (remainingUs < (int32_t)SYNC_MIN_REMAINING_US)
        return false;

    uint32_t value = ((uint32_t)remainingUs + 500UL) / 1000UL;
    if (value > 60000UL)
        value = 60000UL;

    delayMs = (uint16_t)value;
    return true;
}

static uint16_t syncFieldAddress()
{
    switch (syncField)
    {
        case SYNC_FIELD_ARG0: return HR_CMD_ARG0;
        case SYNC_FIELD_CODE: return HR_CMD_CODE;
        case SYNC_FIELD_DELAY: return HR_CMD_ARG1;
        case SYNC_FIELD_TRIGGER: return HR_CMD_SEQ;
        default: return HR_CMD_SEQ;
    }
}

static bool syncFieldValue(uint16_t &value)
{
    switch (syncField)
    {
        case SYNC_FIELD_ARG0:
            value = ((uint16_t)syncChannel << 8) | syncDesiredMask;
            return true;

        case SYNC_FIELD_CODE:
            value = CMD_SYNC_IO_PHASE;
            return true;

        case SYNC_FIELD_DELAY:
            return syncRemainingDelayMs(value);

        case SYNC_FIELD_TRIGGER:
            value = syncSequence;
            return true;

        default:
            return false;
    }
}

static void syncAdvanceField()
{
    if (syncField < SYNC_FIELD_TRIGGER)
    {
        syncField = (SyncDispatchField)((uint8_t)syncField + 1U);
        return;
    }

    syncField = SYNC_FIELD_ARG0;
    syncTargetSlave++;

    if (syncTargetSlave > cfg.expectedSlaves)
        syncField = SYNC_FIELD_DONE;
}

static bool syncStartCurrentWrite()
{
    if (JWPLC_ModbusRTU.masterBusy() || JWPLC_ModbusRTU.masterDone())
        return false;

    uint16_t value = 0;
    if (!syncFieldValue(value))
    {
        syncAbortDispatch("target demasiado cerca para armar I/O");
        return false;
    }

    syncTxnStartedUs = micros();

    if (!JWPLC_ModbusRTU.requestWriteSingleRegister(
            syncTargetSlave,
            syncFieldAddress(),
            value,
            MODBUS_TIMEOUT_MS))
    {
        return false;
    }

    syncTxnActive = true;
    return true;
}

static void syncBeginMasterPhase()
{
    if (!isMaster() || !syncTakeover || cfg.expectedSlaves == 0)
        return;

    if (masterRuntimePollPhase != MASTER_RT_POLL_IDLE ||
        JWPLC_ModbusRTU.masterBusy() ||
        JWPLC_ModbusRTU.masterDone())
    {
        return;
    }

    syncDesiredMask =
        syncPhaseCurrentlyOn
            ? 0x00
            : (uint8_t)(1U << syncChannel);

    syncSequence++;
    if (syncSequence == 0)
        syncSequence++;

    syncMasterTargetUs = micros() + syncLeadUs();
    syncTargetSlave = 1;
    syncField = SYNC_FIELD_ARG0;
    syncMasterDispatch = true;
    syncMasterWaitingApply = false;
    syncTxnActive = false;
}

static void syncServiceMasterDispatch()
{
    if (!isMaster() || !syncTakeover)
        return;

    // Durante el despacho esta rutina es la unica propietaria del motor Master.
    if (syncMasterDispatch)
    {
        JWPLC_ModbusRTU.task();

        if (syncTxnActive)
        {
            if (!JWPLC_ModbusRTU.masterDone())
                return;

            const uint32_t elapsedUs = micros() - syncTxnStartedUs;
            if (elapsedUs > syncTxnMaxUs)
                syncTxnMaxUs = elapsedUs;

            const bool ok = JWPLC_ModbusRTU.masterSucceeded();
            const JWPLCModbusRTUError result = JWPLC_ModbusRTU.masterResult();
            JWPLC_ModbusRTU.clearMasterResult();
            syncTxnActive = false;

            if (!ok)
            {
                Serial.print(F("[SYNC IO] FC06 err="));
                Serial.println((int)result);
                syncAbortDispatch("FC06 direccionado de sync fallo");
                return;
            }

            syncAdvanceField();
        }

        if (syncField == SYNC_FIELD_DONE)
        {
            const int32_t remainingUs =
                (int32_t)(syncMasterTargetUs - micros());

            if (remainingUs < (int32_t)SYNC_MIN_REMAINING_US)
            {
                syncAbortDispatch("arming termino demasiado cerca del target");
                return;
            }

            syncQueueApply(
                syncDesiredMask,
                syncChannel,
                syncMasterTargetUs,
                syncSequence);

            syncMasterDispatch = false;
            syncMasterWaitingApply = true;
            return;
        }

        (void)syncStartCurrentWrite();
        return;
    }

    if (syncMasterWaitingApply)
    {
        if (syncAppliedToken != syncSequence)
            return;

        syncMasterWaitingApply = false;

        if (syncDesiredMask != 0)
        {
            syncPhaseCurrentlyOn = true;
        }
        else
        {
            syncPhaseCurrentlyOn = false;
            syncChannel = (uint8_t)((syncChannel + 1U) & 0x07U);
        }

        syncNextPhaseMs = millis() + IO_PHASE_MS;
        return;
    }

    if ((int32_t)(millis() - syncNextPhaseMs) >= 0)
        syncBeginMasterPhase();
}

static void syncServiceSlaveSchedule()
{
    if (!isSlave() || !syncTakeover || !modbusReady)
        return;

    const uint16_t trigger = holdingRegisters[HR_CMD_SEQ];

    if (holdingRegisters[HR_CMD_CODE] != CMD_SYNC_IO_PHASE ||
        trigger == 0 ||
        trigger == syncSlaveLastTrigger)
    {
        return;
    }

    syncSlaveLastTrigger = trigger;

    const uint16_t arg0 = holdingRegisters[HR_CMD_ARG0];
    const uint8_t channel = (uint8_t)(arg0 >> 8);
    const uint8_t mask = (uint8_t)(arg0 & 0x00FFU);
    const uint16_t delayMs = holdingRegisters[HR_CMD_ARG1];

    if (channel >= 8 || delayMs == 0)
    {
        syncFailCount++;
        latchError(ERR_MODBUS, "sync IO command invalido");
        return;
    }

    syncQueueApply(
        mask,
        channel,
        micros() + (uint32_t)delayMs * 1000UL,
        trigger);
}

static void syncServiceLoopbackVerification()
{
    if (!syncTakeover || soakState != SOAK_RUNNING)
        return;

    const uint32_t now = millis();

    if ((uint32_t)(now - ioStress.lastScanMs) >= IO_SCAN_MS)
    {
        ioStress.lastScanMs = now;
        ioStress.inputBitmap = digitalReadBlock(I0_X);
    }

    const uint32_t age = now - ioStress.phaseStartMs;

    if (ioStress.onPhase)
    {
        const uint8_t expected = (uint8_t)(1U << ioStress.channel);

        if (age >= IO_SETTLE_MS &&
            ioStress.inputBitmap == expected &&
            !ioStress.pulseDetectedThisOn)
        {
            ioStress.pulseDetectedThisOn = true;
            ioStress.iPulses[ioStress.channel]++;
        }

        if (age >= IO_VERIFY_DEADLINE_MS &&
            !ioStress.pulseDetectedThisOn)
        {
            recordIoMismatch("sync expected input pulse missing");
        }

        if (age >= IO_VERIFY_DEADLINE_MS &&
            ioStress.inputBitmap != expected)
        {
            recordIoMismatch("sync input bitmap != output bitmap");
        }
    }
    else if (age >= IO_VERIFY_DEADLINE_MS && ioStress.inputBitmap != 0)
    {
        recordIoMismatch("sync input remained ON after output OFF");
    }
}

static void syncEnterTakeover()
{
    syncTakeover = true;
    syncMasterDispatch = false;
    syncMasterWaitingApply = false;
    syncTxnActive = false;
    syncPhaseCurrentlyOn = false;
    syncChannel = 0;
    syncSequence = 0;
    syncSlaveLastTrigger = 0;
    syncAppliedToken = 0;
    syncApplyPending = false;
    syncNextPhaseMs = millis() + 150UL;

    // El start legacy pudo incrementar software antes de que este overlay
    // tomara control; la corrida sincronizada empieza con contadores limpios.
    memset(ioStress.qPulses, 0, sizeof(ioStress.qPulses));
    memset(ioStress.iPulses, 0, sizeof(ioStress.iPulses));
    ioStress.mismatches = 0;

    // Desactiva exclusivamente el oscilador I/O autonomo del soak base.
    // El resto del acceptance permanece intacto.
    ioStress.running = false;
    ioStress.waitingInitialStagger = false;
    ioStress.onPhase = false;
    ioStress.phaseFailureLatched = false;
    ioStress.pulseDetectedThisOn = false;
    setOutputs(0x00);

    Serial.print(F("[SYNC IO] takeover=ON volume="));
    Serial.print(SOAK_BUZZER_VOLUME);
    Serial.println(F("/255"));
}

static void syncLeaveTakeover()
{
    syncTakeover = false;
    syncMasterDispatch = false;
    syncMasterWaitingApply = false;
    syncTxnActive = false;
    syncApplyPending = false;
    setOutputs(0x00);
    syncAudioStop();
    Serial.println(F("[SYNC IO] takeover=OFF"));
}

static void syncOverlayBeforeBaseLoop()
{
    if (syncTakeover)
    {
        // Evita que serviceIO() del soak base vuelva a crear un reloj local.
        ioStress.running = false;
        ioStress.waitingInitialStagger = false;
    }
}

static void syncOverlayAfterBaseLoop()
{
    if (soakState == SOAK_RUNNING && !syncTakeover)
        syncEnterTakeover();
    else if (soakState != SOAK_RUNNING && syncTakeover)
        syncLeaveTakeover();

    if (!syncTakeover)
        return;

    syncServiceSlaveSchedule();
    syncServiceLoopbackVerification();
    syncServiceMasterDispatch();
}

void setup()
{
    alpha7AsyncBaseSetup();

    if (syncApplyTaskHandle == nullptr)
    {
        const BaseType_t created = xTaskCreatePinnedToCore(
            syncApplyTask,
            "jwplcSyncIO",
            SYNC_APPLY_TASK_STACK,
            nullptr,
            SYNC_APPLY_TASK_PRIORITY,
            &syncApplyTaskHandle,
            SYNC_APPLY_TASK_CORE);

        Serial.print(F("[SYNC IO] apply task="));
        Serial.println(created == pdPASS ? F("PASS") : F("FAIL"));
    }

    Serial.println(F("GATE_RT_REV=4 7NB-sync-addressed-audio"));
    Serial.print(F("BUZZER_VOLUME="));
    Serial.print(SOAK_BUZZER_VOLUME);
    Serial.println(F("/255"));
}

void loop()
{
    syncOverlayBeforeBaseLoop();

    // Mientras M2 arma una fase, evitamos que el poll periodico del soak
    // consuma el masterDone() de nuestras FC06. task() sigue ejecutandose.
    const uint8_t expectedSlavesSaved = cfg.expectedSlaves;
    if (isMaster() && syncMasterDispatch)
        cfg.expectedSlaves = 0;

    alpha7AsyncBaseLoop();

    cfg.expectedSlaves = expectedSlavesSaved;

    syncOverlayAfterBaseLoop();
}
