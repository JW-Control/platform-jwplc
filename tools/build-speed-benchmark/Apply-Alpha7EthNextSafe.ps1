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

if ($text.Contains('ETHNEXT_SAFE_GAP_MS=')) {
    Write-Host 'ETHNEXT seguro ya esta aplicado. No se realizaron cambios.'
    exit 0
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

# 1) Constantes ETHNEXT seguro.
$old = @'
static constexpr uint32_t REV6_STOP_QUIET_GAP_MS =
    REV6_MODBUS_INTER_TX_GAP_MS;
static constexpr UBaseType_t REV6_SYNC_TASK_PRIORITY = 3;
'@
$new = @'
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
static constexpr UBaseType_t REV6_SYNC_TASK_PRIORITY = 3;
'@
$text = Replace-Once $text $old $new 'constantes ETHNEXT'

# 2) Estado ETHNEXT.
$old = @'
static uint32_t rev6StopMaxDrainMs = 0;
static uint32_t rev6StopCount = 0;

static bool rev6AudioActive = false;
'@
$new = @'
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

static bool rev6AudioActive = false;
'@
$text = Replace-Once $text $old $new 'estado ETHNEXT'

# 3) Reset de estado por RUN.
$old = @'
    rev6StopMaxDrainMs = 0;
    rev6StopCount = 0;

    // Consumir el trigger START existente antes de cambiar CODE a SYNC.
'@
$new = @'
    rev6StopMaxDrainMs = 0;
    rev6StopCount = 0;

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
'@
$text = Replace-Once $text $old $new 'reset ETHNEXT'

# 4) STOP y ETHNEXT no se pisan.
$old = @'
    if (!isMaster() || soakState != SOAK_RUNNING)
        return;

    if (rev6StopRequested)
'@
$new = @'
    if (!isMaster() || soakState != SOAK_RUNNING)
        return;

    if (rev6EthNextRequested)
    {
        Serial.println(F("[STOP REV6] deferred: ETHNEXT pending"));
        return;
    }

    if (rev6StopRequested)
'@
$text = Replace-Once $text $old $new 'interlock STOP/ETHNEXT'

# 5) Maquina segura de ETHNEXT.
$anchor = @'
    Serial.println(F("[STOP REV6] complete"));
}

// ============================================================================
// W5500 worker Core0: solo operaciones potencialmente bloqueantes
// ============================================================================
'@
$replacement = @'
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
            rev6PendingMasterRequest.pending)
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
'@
$text = Replace-Once $text $anchor $replacement 'maquina ETHNEXT'

# 6) Durante handoff local no crear jobs W5500 nuevos.
$old = @'
    if (!ownerNow || !JWPLC_Ethernet.isReady())
        return;

    if (!ethWindow.ntpDone && !rev6EthJobOutstanding)
'@
$new = @'
    if (!ownerNow || !JWPLC_Ethernet.isReady())
        return;

    // ETHNEXT espera a que un job local ya iniciado termine, pero desde la
    // solicitud no debe aceptar otro HTTP/NTP que vuelva a prolongar el handoff.
    if (isMaster() && rev6EthNextRequested)
        return;

    if (!ethWindow.ntpDone && !rev6EthJobOutstanding)
'@
$text = Replace-Once $text $old $new 'freeze W5500 local durante ETHNEXT'

# 7) Diagnostico ETHNEXT.
$old = @'
    Serial.print('/');
    Serial.println(rev6StopCount);

    Serial.print(F("ETH_WORKER core="));
'@
$new = @'
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
'@
$text = Replace-Once $text $old $new 'diagnostico ETHNEXT'

# 8) Interceptar ETHNEXT antes del handler base sincrono.
$old = @'
    if (upper == "SYNC")
    {
        rev6PrintDiagnostics();
        return;
    }

    handleSerialCommand(line);
'@
$new = @'
    if (upper == "ETHNEXT" &&
        isMaster() &&
        soakState == SOAK_RUNNING)
    {
        rev6RequestSafeEthNext(false);
        return;
    }

    if (upper == "SYNC")
    {
        rev6PrintDiagnostics();
        return;
    }

    handleSerialCommand(line);
'@
$text = Replace-Once $text $old $new 'serial ETHNEXT seguro'

# 9) Identificadores de boot.
$old = @'
    Serial.print(F("STOP_SAFE_GAP_MS="));
    Serial.println(REV6_STOP_QUIET_GAP_MS);
    Serial.print(F("[CORE] W5500 worker observed core="));
    Serial.println((int)rev6EthObservedCore);
    Serial.println(F("SYNC_COMMANDS=START/SHOW/CLACK/STOP/SYNC"));
'@
$new = @'
    Serial.print(F("STOP_SAFE_GAP_MS="));
    Serial.println(REV6_STOP_QUIET_GAP_MS);
    Serial.print(F("ETHNEXT_SAFE_GAP_MS="));
    Serial.println(REV6_ETHNEXT_QUIET_GAP_MS);
    Serial.print(F("ETHNEXT_RELEASE_GRACE_MS="));
    Serial.println(REV6_ETHNEXT_RELEASE_GRACE_MS);
    Serial.print(F("[CORE] W5500 worker observed core="));
    Serial.println((int)rev6EthObservedCore);
    Serial.println(F("SYNC_COMMANDS=START/SHOW/CLACK/STOP/SYNC/ETHNEXT"));
'@
$text = Replace-Once $text $old $new 'boot ETHNEXT'

# 10) Prioridad de la transicion segura en el Master.
$old = @'
        if (rev6StopRequested)
        {
            rev6ServiceSafeStop();
        }
        else if (soakState == SOAK_RUNNING)
'@
$new = @'
        if (rev6StopRequested)
        {
            rev6ServiceSafeStop();
        }
        else if (rev6EthNextRequested)
        {
            rev6ServiceSafeEthNext();
        }
        else if (soakState == SOAK_RUNNING)
'@
$text = Replace-Once $text $old $new 'prioridad Master ETHNEXT'

# 11) La rotacion automatica tambien pasa por la maquina segura.
$old = @'
        // Mientras STOP esta drenando Modbus no iniciar un nuevo handoff de
        // Ethernet desde la rotacion base.
        if (!rev6StopRequested)
            serviceMasterEthernetRotation();
'@
$new = @'
        // STOP y ETHNEXT son transiciones exclusivas. La expiracion automatica
        // de ventana tambien entra por la misma maquina segura del handoff.
        if (!rev6StopRequested && !rev6EthNextRequested)
            rev6ServiceMasterEthernetRotationSafe();
'@
$text = Replace-Once $text $old $new 'rotacion Ethernet segura'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($target, $text, $utf8NoBom)

Push-Location $RepoRoot
try {
    & git diff --check -- $relativePath
    if ($LASTEXITCODE -ne 0) {
        throw 'git diff --check reporto errores.'
    }

    Write-Host ''
    Write-Host 'OK: ETHNEXT seguro aplicado en 20c.'
    & git diff --stat -- $relativePath
    Write-Host ''
    Write-Host 'Revisa con:'
    Write-Host "  git diff -- $relativePath"
}
finally {
    Pop-Location
}
