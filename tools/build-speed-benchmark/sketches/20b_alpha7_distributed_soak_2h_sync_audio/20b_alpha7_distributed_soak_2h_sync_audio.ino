/*
  Gate 7NB.3A-C - Soak 20 async + patrones I/O sincronizados + audio

  Esta variante REUTILIZA el soak 20 validado y sustituye solamente la
  temporizacion autonoma de I/O por un scheduler direccionado M2 -> Slaves.
  No modifica JWPLC_ModbusRTU.

  Objetivos:
    - conservar WiFi, Ethernet, FRAM, RTC, SD, display, RS-485 y Modbus;
    - conservar loopback Q0.x -> I0.x, pulsos y mismatch;
    - M2 gobierna una tabla de patrones distinta por nodo;
    - cada nodo aplica su bitmap desde una tarea de alta prioridad en Core 1;
    - los tres nodos cambian en el mismo instante aunque sus bitmaps difieran;
    - cada bitmap completo produce una nota propia con volumen configurable.

  Comandos extra en M2:
    START  -> inicia el soak completo en modo SHOW/patrones.
    CLACK  -> si esta READY, inicia el soak completo en modo CLACK;
              si ya corre, cambia a CLACK.
    SHOW   -> durante el soak vuelve a los patrones distribuidos.

  Volumen:
    SOAK_BUZZER_VOLUME = 0..255
      0  : mute
      18 : suave
      24 : recomendado para grabar voz + prueba
      48 : fuerte
*/

#include <Arduino.h>

// El soak base se incluye como fuente validada, pero sus escrituras no-cero
// de digitalWriteBlock() quedan bloqueadas en esta variante. Asi evitamos el
// primer click autonomo cuando START pone IO_NODE_STAGGER_MS=0. El scheduler
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

// Paleta pentatonica/diatonica deliberadamente consonante para video.
// La seleccion NO depende solo de la cantidad de Q: usa la firma completa
// del bitmap, asi 0xAA y 0x55 pueden sonar distinto aunque ambos tengan 4 Q.
static constexpr uint16_t SYNC_NOTE_HZ[8] = {
    523, 587, 659, 784, 880, 1047, 1175, 1319
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

enum SyncPatternMode : uint8_t
{
    SYNC_MODE_SHOW = 0,
    SYNC_MODE_CLACK
};

struct SyncPatternStep
{
    uint8_t m2;
    uint8_t s1;
    uint8_t s2;
    uint16_t holdMs;
    const char *name;
};

// Version integrada del juego validado en Gate 21B.
// El hold minimo supera IO_VERIFY_DEADLINE_MS=500 ms para que el loopback
// tenga tiempo de verificar el bitmap completo antes del siguiente patron.
static const SyncPatternStep SYNC_SHOW_PATTERN[] = {
    {0x00, 0x00, 0x00, 700, "all-off"},
    {0xFF, 0xFF, 0xFF, 900, "all-on"},
    {0x00, 0x00, 0x00, 700, "all-off"},

    // Chase comun: misma Q en las tres placas.
    {0x01, 0x01, 0x01, 700, "chase-q0"},
    {0x02, 0x02, 0x02, 700, "chase-q1"},
    {0x04, 0x04, 0x04, 700, "chase-q2"},
    {0x08, 0x08, 0x08, 700, "chase-q3"},
    {0x10, 0x10, 0x10, 700, "chase-q4"},
    {0x20, 0x20, 0x20, 700, "chase-q5"},
    {0x40, 0x40, 0x40, 700, "chase-q6"},
    {0x80, 0x80, 0x80, 800, "chase-q7"},

    // Alternancia cruzada: varios relays por nodo y notas distintas.
    {0xAA, 0x55, 0xAA, 900, "alternate-a"},
    {0x55, 0xAA, 0x55, 900, "alternate-b"},
    {0xAA, 0x55, 0xAA, 900, "alternate-a"},
    {0x55, 0xAA, 0x55, 900, "alternate-b"},

    // Ola por placas: cada frontera cambia sincronizadamente aunque solo
    // uno de los tres bancos quede activo.
    {0xFF, 0x00, 0x00, 850, "wave-m2"},
    {0x00, 0xFF, 0x00, 850, "wave-s1"},
    {0x00, 0x00, 0xFF, 850, "wave-s2"},
    {0xFF, 0x00, 0x00, 850, "wave-m2"},

    // Figuras simetricas: tres bitmaps distintos -> trio de notas.
    {0x81, 0x42, 0x24, 850, "mirror-1"},
    {0x42, 0x24, 0x18, 850, "mirror-2"},
    {0x24, 0x18, 0x24, 850, "mirror-3"},
    {0x18, 0x24, 0x42, 850, "mirror-4"},
    {0x24, 0x42, 0x81, 950, "mirror-5"},

    {0x00, 0x00, 0x00, 1000, "all-off"}
};

static constexpr size_t SYNC_SHOW_STEP_COUNT =
    sizeof(SYNC_SHOW_PATTERN) / sizeof(SYNC_SHOW_PATTERN[0]);

static bool syncTakeover = false;
static bool syncMasterDispatch = false;
static bool syncMasterWaitingApply = false;
static bool syncTxnActive = false;
static uint8_t syncTargetSlave = 1;
static SyncDispatchField syncField = SYNC_FIELD_ARG0;
static SyncPatternMode syncPatternMode = SYNC_MODE_SHOW;
static uint16_t syncSequence = 0;
static uint32_t syncMasterTargetUs = 0;
static uint32_t syncNextPhaseMs = 0;
static uint32_t syncTxnStartedUs = 0;
static uint32_t syncTxnMaxUs = 0;
static uint32_t syncFailCount = 0;
static uint32_t syncApplyCount = 0;
static size_t syncShowIndex = 0;
static bool syncClackOn = false;

static uint8_t syncActiveM2Mask = 0;
static uint8_t syncActiveS1Mask = 0;
static uint8_t syncActiveS2Mask = 0;
static uint16_t syncActiveHoldMs = 800;
static const char *syncActiveName = "idle";

static uint16_t syncSlaveLastTrigger = 0;

static volatile bool syncApplyPending = false;
static volatile uint8_t syncApplyMask = 0;
static volatile uint8_t syncApplyChannel = 0;
static volatile uint32_t syncApplyAtUs = 0;
static volatile uint16_t syncApplyToken = 0;
static volatile uint16_t syncAppliedToken = 0;

// Estado de verificacion multibit. Cada transicion 0->1 suma un pulso Q;
// el pulso I se confirma cuando el bitmap fisico coincide tras settle.
static volatile uint8_t syncExpectedMask = 0;
static volatile uint8_t syncPendingRisingMask = 0;
static volatile uint8_t syncDetectedRisingMask = 0;

static bool syncAudioActive = false;
static uint32_t syncAudioStopAtUs = 0;
static TaskHandle_t syncApplyTaskHandle = nullptr;
static String syncSerialLine;

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

static uint8_t syncFirstActiveChannel(uint8_t mask)
{
    for (uint8_t i = 0; i < 8; ++i)
    {
        if (mask & (uint8_t)(1U << i))
            return i;
    }

    return 0;
}

static uint16_t syncFrequencyForMask(uint8_t mask)
{
    if (mask == 0)
        return SYNC_OFF_NOTE_HZ;

    const uint8_t lowNibble = (uint8_t)(mask & 0x0FU);
    const uint8_t highNibble = (uint8_t)(mask >> 4);
    const uint8_t count = syncPopcount8(mask);

    // Firma simple del bitmap completo. El factor 3 del nibble alto evita
    // que espejos con igual popcount caigan sistematicamente en la misma nota.
    const uint8_t signature =
        (uint8_t)(lowNibble + 3U * highNibble + count);

    return SYNC_NOTE_HZ[signature & 0x07U];
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
    const uint8_t rising = (uint8_t)(mask & (uint8_t)~previous);

    // Bypass deliberado de setOutputs(): esa funcion pertenece al oscilador
    // legacy y sus ON quedan bloqueados por esta variante.
    jwplc_digitalWriteBlock(Q0_X, 8, mask);
    ioStress.outputBitmap = mask;
    ioStress.channel = channel;
    ioStress.onPhase = (mask != 0);
    ioStress.phaseStartMs = millis();
    ioStress.phaseFailureLatched = false;
    ioStress.pulseDetectedThisOn = false;

    syncExpectedMask = mask;
    syncPendingRisingMask = rising;
    syncDetectedRisingMask = 0;

    for (uint8_t i = 0; i < 8; ++i)
    {
        if (rising & (uint8_t)(1U << i))
            ioStress.qPulses[i]++;
    }

    syncAudioStart(syncFrequencyForMask(mask));

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

static uint8_t syncMaskForSlave(uint8_t slaveId)
{
    if (slaveId == 1)
        return syncActiveS1Mask;

    if (slaveId == 2)
        return syncActiveS2Mask;

    // Fallback para M4/M8: repetir la familia M2/S1/S2 sin introducir
    // broadcast. El acceptance actual sigue siendo M2 + S1 + S2.
    switch ((slaveId - 1U) % 3U)
    {
        case 0: return syncActiveS1Mask;
        case 1: return syncActiveS2Mask;
        default: return syncActiveM2Mask;
    }
}

static void syncLoadNextPattern()
{
    if (syncPatternMode == SYNC_MODE_CLACK)
    {
        syncClackOn = !syncClackOn;
        const uint8_t mask = syncClackOn ? 0xFF : 0x00;

        syncActiveM2Mask = mask;
        syncActiveS1Mask = mask;
        syncActiveS2Mask = mask;
        syncActiveHoldMs = 800;
        syncActiveName = syncClackOn ? "clack-on" : "clack-off";
        return;
    }

    const SyncPatternStep &step = SYNC_SHOW_PATTERN[syncShowIndex];
    syncActiveM2Mask = step.m2;
    syncActiveS1Mask = step.s1;
    syncActiveS2Mask = step.s2;
    syncActiveHoldMs = step.holdMs;
    syncActiveName = step.name;

    syncShowIndex = (syncShowIndex + 1U) % SYNC_SHOW_STEP_COUNT;
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
    const uint8_t slaveMask = syncMaskForSlave(syncTargetSlave);

    switch (syncField)
    {
        case SYNC_FIELD_ARG0:
            value =
                ((uint16_t)syncFirstActiveChannel(slaveMask) << 8) |
                slaveMask;
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

    syncLoadNextPattern();

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

static void syncPrintAppliedPattern()
{
    Serial.print(F("PATTERN seq="));
    Serial.print(syncSequence);
    Serial.print(F(" name="));
    Serial.print(syncActiveName);
    Serial.print(F(" M2/S1/S2=0x"));

    if (syncActiveM2Mask < 16) Serial.print('0');
    Serial.print(syncActiveM2Mask, HEX);
    Serial.print(F("/0x"));
    if (syncActiveS1Mask < 16) Serial.print('0');
    Serial.print(syncActiveS1Mask, HEX);
    Serial.print(F("/0x"));
    if (syncActiveS2Mask < 16) Serial.print('0');
    Serial.print(syncActiveS2Mask, HEX);

    Serial.print(F(" tone="));
    Serial.print(syncFrequencyForMask(syncActiveM2Mask));
    Serial.print('/');
    Serial.print(syncFrequencyForMask(syncActiveS1Mask));
    Serial.print('/');
    Serial.print(syncFrequencyForMask(syncActiveS2Mask));
    Serial.println(F("Hz"));
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
                syncActiveM2Mask,
                syncFirstActiveChannel(syncActiveM2Mask),
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
        syncPrintAppliedPattern();
        syncNextPhaseMs = millis() + syncActiveHoldMs;
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
    const uint8_t expected = syncExpectedMask;

    if (age >= IO_SETTLE_MS && ioStress.inputBitmap == expected)
    {
        const uint8_t newlyDetected =
            (uint8_t)(syncPendingRisingMask &
                      (uint8_t)~syncDetectedRisingMask);

        if (newlyDetected != 0)
        {
            for (uint8_t i = 0; i < 8; ++i)
            {
                if (newlyDetected & (uint8_t)(1U << i))
                    ioStress.iPulses[i]++;
            }

            syncDetectedRisingMask |= newlyDetected;
        }

        ioStress.pulseDetectedThisOn = true;
    }

    if (age < IO_VERIFY_DEADLINE_MS || ioStress.phaseFailureLatched)
        return;

    const uint8_t missingRising =
        (uint8_t)(syncPendingRisingMask &
                  (uint8_t)~syncDetectedRisingMask);

    if (ioStress.inputBitmap != expected)
    {
        recordIoMismatch("sync input bitmap != pattern bitmap");
        return;
    }

    if (missingRising != 0)
        recordIoMismatch("sync expected rising inputs missing");
}

static const char *syncModeName()
{
    return syncPatternMode == SYNC_MODE_CLACK ? "CLACK" : "SHOW";
}

static void syncSetMode(uint8_t mode)
{
    syncPatternMode = (SyncPatternMode)mode;

    if (mode == SYNC_MODE_SHOW)
        syncShowIndex = 0;
    else
        syncClackOn = false;

    if (syncTakeover &&
        !syncMasterDispatch &&
        !syncMasterWaitingApply)
    {
        syncNextPhaseMs = millis() + 50UL;
    }

    Serial.print(F("[SYNC IO] mode="));
    Serial.println(syncModeName());
}

static void syncEnterTakeover()
{
    syncTakeover = true;
    syncMasterDispatch = false;
    syncMasterWaitingApply = false;
    syncTxnActive = false;
    syncSequence = 0;
    syncSlaveLastTrigger = 0;
    syncAppliedToken = 0;
    syncApplyPending = false;
    syncExpectedMask = 0;
    syncPendingRisingMask = 0;
    syncDetectedRisingMask = 0;
    syncShowIndex = 0;
    syncClackOn = false;
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

    Serial.print(F("[SYNC IO] takeover=ON mode="));
    Serial.print(syncModeName());
    Serial.print(F(" volume="));
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
    syncExpectedMask = 0;
    syncPendingRisingMask = 0;
    syncDetectedRisingMask = 0;
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

// ============================================================================
// Overlay Serial: agrega CLACK/SHOW sin tocar el parser del soak base.
// El overlay consume Serial primero; el resto se delega a handleSerialCommand().
// ============================================================================

static void syncHandleSerialCommand(String line)
{
    line.trim();
    if (line.length() == 0)
        return;

    String upper = line;
    upper.toUpperCase();

    if (upper == "CLACK")
    {
        if (!isMaster())
        {
            Serial.println(F("CLACK solo se envia al Master."));
            return;
        }

        syncSetMode(SYNC_MODE_CLACK);

        if (soakState != SOAK_RUNNING)
            handleSerialCommand("START");

        return;
    }

    if (upper == "SHOW")
    {
        if (!isMaster())
        {
            Serial.println(F("SHOW solo se envia al Master."));
            return;
        }

        syncSetMode(SYNC_MODE_SHOW);

        if (soakState != SOAK_RUNNING)
            Serial.println(F("SHOW armado. Envia START para iniciar soak completo."));

        return;
    }

    if (upper == "START")
    {
        syncSetMode(SYNC_MODE_SHOW);

        if (soakState == SOAK_RUNNING)
        {
            Serial.println(F("SOAK ya RUNNING; modo cambiado a SHOW."));
            return;
        }
    }

    handleSerialCommand(line);
}

static void syncServiceSerialOverlay()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();

        if (c == '\r' || c == '\n')
        {
            if (syncSerialLine.length() > 0)
            {
                syncHandleSerialCommand(syncSerialLine);
                syncSerialLine = "";
            }
        }
        else if (syncSerialLine.length() < 64)
        {
            syncSerialLine += c;
        }
    }
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

    Serial.println(F("GATE_RT_REV=5 7NB-sync-patterns-audio"));
    Serial.print(F("BUZZER_VOLUME="));
    Serial.print(SOAK_BUZZER_VOLUME);
    Serial.println(F("/255"));
    Serial.println(F("SYNC_COMMANDS=START/SHOW/CLACK/STOP"));
}

void loop()
{
    // Consumir comandos antes de que serviceSerial() del soak base los lea.
    syncServiceSerialOverlay();
    syncOverlayBeforeBaseLoop();

    // Mientras M2 arma un patron, evitamos que el poll periodico del soak
    // consuma el masterDone() de nuestras FC06. task() sigue ejecutandose.
    const uint8_t expectedSlavesSaved = cfg.expectedSlaves;
    if (isMaster() && syncMasterDispatch)
        cfg.expectedSlaves = 0;

    alpha7AsyncBaseLoop();

    cfg.expectedSlaves = expectedSlavesSaved;

    syncOverlayAfterBaseLoop();
}
