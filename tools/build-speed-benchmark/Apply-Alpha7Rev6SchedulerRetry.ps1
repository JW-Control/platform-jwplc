param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
)

$ErrorActionPreference = 'Stop'

$relativePath = "tools\build-speed-benchmark\sketches\20c_alpha7_distributed_soak_core_split_w5500\20c_alpha7_distributed_soak_core_split_w5500.ino"
$target = Join-Path $RepoRoot $relativePath

if (-not (Test-Path -LiteralPath $target)) {
    throw "No se encontro: $target"
}

$text = [System.IO.File]::ReadAllText($target)

if ($text.Contains('SYNC_TIMEOUT_RETRY=SEQUENCE_ONCE_PRE_TRIGGER')) {
    Write-Host 'Retry REV6 ya esta aplicado. No se realizaron cambios.'
    exit 0
}

if (-not $text.Contains('ETHNEXT_SAFE_GAP_MS=')) {
    throw 'El 20c no contiene ETHNEXT seguro. Aplica primero la revision validada de ETHNEXT.'
}

function Replace-Once {
    param(
        [string]$Source,
        [string]$Old,
        [string]$New,
        [string]$Label
    )

    $first = $Source.IndexOf($Old, [System.StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "No se encontro anchor: $Label"
    }

    $second = $Source.IndexOf($Old, $first + $Old.Length, [System.StringComparison]::Ordinal)
    if ($second -ge 0) {
        throw "Anchor duplicado: $Label"
    }

    return $Source.Substring(0, $first) + $New + $Source.Substring($first + $Old.Length)
}

# 1) Estado del retry de secuencia.
$old = @'
static uint32_t rev6TxnTimeoutCount = 0;
static uint32_t rev6RequestRejectCount = 0;
static uint8_t rev6TxnSlave = 0;
static uint16_t rev6TxnReg = 0;
static uint16_t rev6TxnValue = 0;
static uint32_t rev6SyncFail = 0;
static uint32_t rev6ApplyCount = 0;
static Rev6PendingMasterRequest rev6PendingMasterRequest;
'@
$new = @'
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
'@
$text = Replace-Once $text $old $new 'estado retry REV6'

# 2) Guardar el resultado concreto de la ultima transaccion.
$old = @'
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

    if (result == JWPLC_MODBUS_TIMEOUT)
        rev6TxnTimeoutCount++;

    JWPLC_ModbusRTU.clearMasterResult();
    rev6SyncTxnActive = false;
    rev6NextTxnAllowedUs = micros() + REV6_MODBUS_INTER_TX_GAP_US;

    if (!ok)
    {
        // rev6AbortSync() contabiliza una sola falla del scheduler. Aqui solo
        // se registra la transaccion concreta para no duplicar syncFail.
        Serial.print(F("[SYNC REV6] FAIL "));
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
'@
$new = @'
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
'@
$text = Replace-Once $text $old $new 'rev6FinishTxn retry-aware'

# 3) ABORT final + politica de retry seguro.
$old = @'
static void rev6AbortSync(const char *reason)
{
    rev6SyncFail++;
    rev6SyncTxnActive = false;
    rev6PendingMasterRequest = Rev6PendingMasterRequest{};
    rev6SyncStage = REV6_STAGE_IDLE;
    rev6NextTxnAllowedUs = micros() + REV6_MODBUS_INTER_TX_GAP_US;
    rev6NextStepMs = millis() + REV6_SYNC_RETRY_MS;
    Serial.print(F("[SYNC REV6] ABORT: "));
    Serial.println(reason);
    latchError(ERR_MODBUS, reason);
}
'@
$new = @'
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
'@
$text = Replace-Once $text $old $new 'abort + helper retry'

# 4) Reiniciar el mismo patron con target nuevo.
$old = @'
    rev6LoadPattern();

    rev6Sequence++;
'@
$new = @'
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
'@
$text = Replace-Once $text $old $new 'reinicio mismo patron'

# 5) El retry pendiente mantiene prioridad Modbus durante sus 4 ms.
$old = @'
    if (rev6SyncStage != REV6_STAGE_IDLE ||
        rev6PendingMasterRequest.pending)
    {
        return true;
    }
'@
$new = @'
    if (rev6SyncStage != REV6_STAGE_IDLE ||
        rev6PendingMasterRequest.pending ||
        rev6RetrySequencePending)
    {
        return true;
    }
'@
$text = Replace-Once $text $old $new 'prioridad durante retry'

# 6) Recovery solo cuenta al aplicar realmente el patron reintentado.
$old = @'
        if (rev6AppliedToken != rev6Sequence)
            return;

        Serial.print(F("PATTERN seq="));
'@
$new = @'
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
'@
$text = Replace-Once $text $old $new 'contabilizar retry recuperado'

# 7a) WAIT code-s1.
$old = @'
            if (!rev6FinishTxn("code-s1"))
            {
                rev6AbortSync("code S1");
                return;
            }
'@
$new = @'
            if (!rev6FinishTxn("code-s1"))
            {
                rev6HandleTxnFailure("code S1");
                return;
            }
'@
$text = Replace-Once $text $old $new 'failure code-s1'

# 7b) WAIT code-s2.
$old = @'
            if (!rev6FinishTxn("code-s2"))
            {
                rev6AbortSync("code S2");
                return;
            }
'@
$new = @'
            if (!rev6FinishTxn("code-s2"))
            {
                rev6HandleTxnFailure("code S2");
                return;
            }
'@
$text = Replace-Once $text $old $new 'failure code-s2'

# 7c) WAIT mask-s1.
$old = @'
            if (!rev6FinishTxn("mask-s1"))
            {
                rev6AbortSync("mask S1");
                return;
            }
'@
$new = @'
            if (!rev6FinishTxn("mask-s1"))
            {
                rev6HandleTxnFailure("mask S1");
                return;
            }
'@
$text = Replace-Once $text $old $new 'failure mask-s1'

# 7d) WAIT mask-s2.
$old = @'
            if (!rev6FinishTxn("mask-s2"))
            {
                rev6AbortSync("mask S2");
                return;
            }
'@
$new = @'
            if (!rev6FinishTxn("mask-s2"))
            {
                rev6HandleTxnFailure("mask S2");
                return;
            }
'@
$text = Replace-Once $text $old $new 'failure mask-s2'

# 7e) WAIT delay-s1.
$old = @'
            if (!rev6FinishTxn("delay-s1"))
            {
                rev6AbortSync("delay S1 tx");
                return;
            }
'@
$new = @'
            if (!rev6FinishTxn("delay-s1"))
            {
                rev6HandleTxnFailure("delay S1 tx");
                return;
            }
'@
$text = Replace-Once $text $old $new 'failure delay-s1'

# 7f) WAIT trigger-s1: helper lo clasifica como NO reintentable.
$old = @'
            if (!rev6FinishTxn("trigger-s1"))
            {
                rev6AbortSync("trigger S1 tx");
                return;
            }
'@
$new = @'
            if (!rev6FinishTxn("trigger-s1"))
            {
                rev6HandleTxnFailure("trigger S1 tx");
                return;
            }
'@
$text = Replace-Once $text $old $new 'failure trigger-s1'

# 7g) WAIT delay-s2: S1 ya pudo quedar armado, por eso NO reintentable.
$old = @'
            if (!rev6FinishTxn("delay-s2"))
            {
                rev6AbortSync("delay S2 tx");
                return;
            }
'@
$new = @'
            if (!rev6FinishTxn("delay-s2"))
            {
                rev6HandleTxnFailure("delay S2 tx");
                return;
            }
'@
$text = Replace-Once $text $old $new 'failure delay-s2'

# 7h) WAIT trigger-s2: NO reintentable.
$old = @'
            if (!rev6FinishTxn("trigger-s2"))
            {
                rev6AbortSync("trigger S2 tx");
                return;
            }
'@
$new = @'
            if (!rev6FinishTxn("trigger-s2"))
            {
                rev6HandleTxnFailure("trigger S2 tx");
                return;
            }
'@
$text = Replace-Once $text $old $new 'failure trigger-s2'

# 8) Reset al entrar en RUN.
$old = @'
    rev6TxnSlave = 0;
    rev6TxnReg = 0;
    rev6TxnValue = 0;
    rev6SyncFail = 0;
    rev6ApplyCount = 0;

    rev6StopRequested = false;
'@
$new = @'
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
'@
$text = Replace-Once $text $old $new 'reset retry por RUN'

# 9) Al salir del takeover cancelar pendientes, conservando contadores.
$old = @'
    rev6SyncTxnActive = false;
    rev6PendingMasterRequest = Rev6PendingMasterRequest{};
    rev6SyncStage = REV6_STAGE_IDLE;
    rev6ApplyPending = false;
'@
$new = @'
    rev6SyncTxnActive = false;
    rev6RetrySequencePending = false;
    rev6RetrySequenceActive = false;
    rev6PendingMasterRequest = Rev6PendingMasterRequest{};
    rev6SyncStage = REV6_STAGE_IDLE;
    rev6ApplyPending = false;
'@
$text = Replace-Once $text $old $new 'cancelar retry al salir takeover'

# 10) STOP debe terminar un retry ya programado antes de entrar al Sync base.
$old = @'
    if (rev6SyncStage != REV6_STAGE_IDLE ||
        rev6PendingMasterRequest.pending)
    {
        rev6ServiceMasterSync();
        return;
    }

    // Lo mismo para el poll cooperativo base: si empezo FC06/FC03, dejar que
'@
$new = @'
    if (rev6SyncStage != REV6_STAGE_IDLE ||
        rev6PendingMasterRequest.pending ||
        rev6RetrySequencePending)
    {
        rev6ServiceMasterSync();
        return;
    }

    // Lo mismo para el poll cooperativo base: si empezo FC06/FC03, dejar que
'@
$text = Replace-Once $text $old $new 'STOP drena retry pendiente'

# 11) ETHNEXT aplica la misma regla de drenaje.
$old = @'
        if (rev6SyncStage != REV6_STAGE_IDLE ||
            rev6PendingMasterRequest.pending)
        {
            rev6ServiceMasterSync();
            return;
        }

        // Terminar el poll FC06/FC03 ya iniciado, sin iniciar otro.
'@
$new = @'
        if (rev6SyncStage != REV6_STAGE_IDLE ||
            rev6PendingMasterRequest.pending ||
            rev6RetrySequencePending)
        {
            rev6ServiceMasterSync();
            return;
        }

        // Terminar el poll FC06/FC03 ya iniciado, sin iniciar otro.
'@
$text = Replace-Once $text $old $new 'ETHNEXT drena retry pendiente'

# 12) Diagnostico SYNC: timeout bruto + retry attempts/recovered/final.
$old = @'
    Serial.print(F(" timeouts="));
    Serial.print(rev6TxnTimeoutCount);
    Serial.print(F(" rejects="));
'@
$new = @'
    Serial.print(F(" timeouts="));
    Serial.print(rev6TxnTimeoutCount);
    Serial.print(F(" retry="));
    Serial.print(rev6RetryAttempts);
    Serial.print('/');
    Serial.print(rev6RetryRecovered);
    Serial.print('/');
    Serial.print(rev6RetryFinalTimeouts);
    Serial.print(F(" rejects="));
'@
$text = Replace-Once $text $old $new 'diagnostico SYNC retry'

# 13) Diagnostico periodico compacto.
$old = @'
        Serial.print(F(" syncTimeout="));
        Serial.print(rev6TxnTimeoutCount);
        Serial.print(F(" ioRaceDiscard="));
'@
$new = @'
        Serial.print(F(" syncTimeout="));
        Serial.print(rev6TxnTimeoutCount);
        Serial.print(F(" syncRetry="));
        Serial.print(rev6RetryAttempts);
        Serial.print('/');
        Serial.print(rev6RetryRecovered);
        Serial.print('/');
        Serial.print(rev6RetryFinalTimeouts);
        Serial.print(F(" ioRaceDiscard="));
'@
$text = Replace-Once $text $old $new 'diag periodico retry'

# 14) Banner inequívoco de revision.
$old = @'
    Serial.println(F("SYNC_IO_GENERATION_GUARD=ON"));
    Serial.print(F("STOP_SAFE_GAP_MS="));
'@
$new = @'
    Serial.println(F("SYNC_IO_GENERATION_GUARD=ON"));
    Serial.println(F("SYNC_TIMEOUT_RETRY=SEQUENCE_ONCE_PRE_TRIGGER"));
    Serial.print(F("STOP_SAFE_GAP_MS="));
'@
$text = Replace-Once $text $old $new 'banner retry'

[System.IO.File]::WriteAllText(
    $target,
    $text,
    (New-Object System.Text.UTF8Encoding($false)))

Write-Host ''
Write-Host 'OK: retry de secuencia REV6 aplicado en 20c.'
Write-Host 'Politica: 1 retry solo para timeout antes del primer TRIGGER.'
Write-Host 'No modifica JWPLC_ModbusRTU ni la logica funcional de STOP.'
Write-Host ''

& git -C $RepoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw 'git diff --check detecto problemas.'
}

& git -C $RepoRoot diff --stat -- $relativePath
Write-Host ''
Write-Host 'Revisa con:'
Write-Host "  git diff -- $relativePath"
