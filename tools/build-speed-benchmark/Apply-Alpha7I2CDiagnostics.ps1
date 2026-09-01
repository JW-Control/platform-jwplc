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

if ($text.Contains('I2C_DIAG=WATCH30S+COMMAND')) {
    Write-Host 'Diagnostico I2C ya esta aplicado. No se realizaron cambios.'
    exit 0
}

if (-not $text.Contains('SYNC_TIMEOUT_RETRY=SEQUENCE_ONCE_PRE_TRIGGER')) {
    throw 'Aplica primero Apply-Alpha7Rev6SchedulerRetry.ps1. Este diagnostico parte de esa revision.'
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

# 1) Backend I2C oficial del package y nombres de errores ESP-IDF.
$old = @'
#include <jwplc_spi_bus.h>
'@
$new = @'
#include <jwplc_spi_bus.h>
#include <esp_err.h>

extern "C"
{
#include "jwplc_i2c_bridge.h"
}
'@
$text = Replace-Once $text $old $new 'includes I2C diag'

# 2) Configuracion del watcher. Solo se prueban direcciones conocidas y seguras.
$old = @'
static constexpr uint32_t REV6_ETHNEXT_RELEASE_GRACE_MS = 2300UL;
'@
$new = @'
static constexpr uint32_t REV6_ETHNEXT_RELEASE_GRACE_MS = 2300UL;

// Diagnostico I2C del acceptance. El bus real del JWPLC Basic usa SDA=21,
// SCL=22, TCA6424A=0x22 y RTC=0x68. 0x23 se consulta solo manualmente como
// direccion alternativa del TCA; su ausencia NO es falla.
static constexpr uint8_t REV6_I2C_TCA_ADDR = 0x22;
static constexpr uint8_t REV6_I2C_TCA_ALT_ADDR = 0x23;
static constexpr uint8_t REV6_I2C_RTC_ADDR = 0x68;
static constexpr uint32_t REV6_I2C_LINE_SAMPLE_MS = 1000UL;
static constexpr uint32_t REV6_I2C_WATCH_PERIOD_MS = 30000UL;
static constexpr uint32_t REV6_I2C_BUSY_RETRY_MS = 1000UL;
'@
$text = Replace-Once $text $old $new 'config I2C diag'

# 3) Estado/counters. No se mezclan con ERR del soak.
$old = @'
static uint16_t rev6EthNextNextOwner = ETH_OWNER_NONE;

static bool rev6AudioActive = false;
'@
$new = @'
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
};

static Rev6I2CDiagStats rev6I2CDiag;
static bool rev6I2CWatchEnabled = true;
static uint32_t rev6I2CLastLineSampleMs = 0;
static uint32_t rev6I2CNextWatchMs = 0;

static bool rev6AudioActive = false;
'@
$text = Replace-Once $text $old $new 'estado I2C diag'

# 4) Helpers + watcher + comando manual. Insertar antes del diagnostico REV6.
$old = @'
static void rev6PrintDiagnostics()
{
'@
$new = @'
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

static void rev6I2CSampleLines(bool verbose)
{
    // No cambiar pinMode: GPIO21/22 ya pertenecen al controlador I2C.
    const int sdaLevel = digitalRead(SDA);
    const int sclLevel = digitalRead(SCL);

    if (sdaLevel == LOW)
    {
        rev6I2CDiag.sdaLowSamples++;
        rev6I2CDiag.sdaLowConsecutive++;
    }
    else
    {
        rev6I2CDiag.sdaLowConsecutive = 0;
    }

    if (sclLevel == LOW)
    {
        rev6I2CDiag.sclLowSamples++;
        rev6I2CDiag.sclLowConsecutive++;
    }
    else
    {
        rev6I2CDiag.sclLowConsecutive = 0;
    }

    if (sdaLevel == LOW && sclLevel == LOW)
        rev6I2CDiag.bothLowSamples++;

    if (verbose ||
        rev6I2CDiag.sdaLowConsecutive == 2 ||
        rev6I2CDiag.sclLowConsecutive == 2)
    {
        Serial.print(F("[I2C LINE] node="));
        Serial.print(shortRoleName());
        Serial.print(F(" SDA="));
        Serial.print(sdaLevel);
        Serial.print(F(" SCL="));
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

    rev6I2CDiag.cycles++;
    rev6I2CDiag.lastTcaErr = tcaResult;
    rev6I2CDiag.lastRtcErr = rtcResult;

    if (tcaResult == ESP_OK)
        rev6I2CDiag.tcaOk++;
    else
        rev6I2CDiag.tcaFail++;

    if (rtcResult == ESP_OK)
        rev6I2CDiag.rtcOk++;
    else
        rev6I2CDiag.rtcFail++;

    if (verbose || tcaResult != ESP_OK || rtcResult != ESP_OK)
    {
        Serial.print(F("[I2C WATCH] node="));
        Serial.print(shortRoleName());
        Serial.print(F(" cycle="));
        Serial.print(rev6I2CDiag.cycles);
        Serial.print(F(" TCA22="));
        Serial.print(tcaResult == ESP_OK ? F("OK") : F("FAIL"));
        Serial.print(F(" RTC68="));
        Serial.print(rtcResult == ESP_OK ? F("OK") : F("FAIL"));
        Serial.print(F(" SDA/SCL="));
        Serial.print(digitalRead(SDA));
        Serial.print('/');
        Serial.println(digitalRead(SCL));
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
    Serial.println(rev6I2CDiag.bothLowSamples);
}

static void rev6PrintI2CDiagnostics()
{
    Serial.println();
    Serial.println(F("---- I2C DIAGNOSTICS ----"));
    Serial.print(F("NODE="));
    Serial.print(shortRoleName());
    Serial.print(F(" SDA_PIN="));
    Serial.print((int)SDA);
    Serial.print(F(" SCL_PIN="));
    Serial.println((int)SCL);

    rev6I2CSampleLines(true);

    Serial.println(F("EXPECTED: TCA6424A@0x22 RTC@0x68; TCA@0x23=ALT/OPTIONAL"));

    // Lecturas de registro 0x00: seguras para los dispositivos conocidos.
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

    if (!rev6I2CWatchEnabled || soakState != SOAK_RUNNING)
        return;

    if ((int32_t)(now - rev6I2CNextWatchMs) < 0)
        return;

    // Si una linea ya esta baja, no lanzar una transaccion que pueda agotar
    // el timeout de 50 ms del bridge. La evidencia de linea trabada es mejor.
    if (digitalRead(SDA) == LOW || digitalRead(SCL) == LOW)
    {
        rev6I2CNextWatchMs = now + REV6_I2C_BUSY_RETRY_MS;
        return;
    }

    if (!rev6I2CCanActiveProbe())
    {
        rev6I2CDiag.deferredBusy++;
        rev6I2CNextWatchMs = now + REV6_I2C_BUSY_RETRY_MS;
        return;
    }

    rev6I2CProbeExpected(true);
    rev6I2CNextWatchMs = now + REV6_I2C_WATCH_PERIOD_MS;
}

static void rev6PrintDiagnostics()
{
'@
$text = Replace-Once $text $old $new 'helpers I2C diag'

# 5) SYNC incluye resumen I2C sin ejecutar probes extra.
$old = @'
    Serial.print(F("STOP pending/lastMs/maxMs/count="));
'@
$new = @'
    rev6PrintI2CSummary();

    Serial.print(F("STOP pending/lastMs/maxMs/count="));
'@
$text = Replace-Once $text $old $new 'SYNC summary I2C'

# 6) Comandos I2C / I2CWATCH.
$old = @'
    if (upper == "SYNC")
    {
        rev6PrintDiagnostics();
        return;
    }
'@
$new = @'
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
'@
$text = Replace-Once $text $old $new 'comandos I2C'

# 7) Reset de contadores al iniciar RUN. Insertar antes del reset ETHNEXT estable.
$old = @'
    rev6EthNextRequested = false;
    rev6EthNextAutomatic = false;
'@
$new = @'
    rev6ResetI2CDiagnostics();

    rev6EthNextRequested = false;
    rev6EthNextAutomatic = false;
'@
$text = Replace-Once $text $old $new 'reset I2C por RUN'

# 8) Banner para identificar el firmware cargado.
$old = @'
    Serial.print(F("STOP_SAFE_GAP_MS="));
'@
$new = @'
    Serial.println(F("I2C_DIAG=WATCH30S+COMMAND"));
    Serial.println(F("I2C_EXPECTED=TCA6424A@0x22,RTC@0x68"));
    Serial.print(F("STOP_SAFE_GAP_MS="));
'@
$text = Replace-Once $text $old $new 'banner I2C'

$old = @'
    Serial.println(F("SYNC_COMMANDS=START/SHOW/CLACK/STOP/SYNC/ETHNEXT"));
'@
$new = @'
    Serial.println(F("SYNC_COMMANDS=START/SHOW/CLACK/STOP/SYNC/ETHNEXT/I2C/I2CWATCH ON|OFF"));
'@
$text = Replace-Once $text $old $new 'banner comandos I2C'

# 9) Servicio automatico al final del loop, despues del trabajo critico Modbus.
$old = @'
    refreshHoldingRegisters();
    servicePeriodicRuntimeDiagnostics();
    rev6ServiceWorkerPeriodicDiag();
'@
$new = @'
    rev6ServiceI2CDiagnostics();

    refreshHoldingRegisters();
    servicePeriodicRuntimeDiagnostics();
    rev6ServiceWorkerPeriodicDiag();
'@
$text = Replace-Once $text $old $new 'service I2C loop'

[System.IO.File]::WriteAllText(
    $target,
    $text,
    [System.Text.UTF8Encoding]::new($false)
)

Write-Host ''
Write-Host 'OK: diagnostico I2C aplicado en 20c.'
Write-Host 'Comandos: I2C | I2CWATCH ON | I2CWATCH OFF'
Write-Host 'Watcher: 30 s, solo direcciones esperadas, sin latch ERR propio.'
Write-Host ''

& git -C $RepoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw 'git diff --check detecto problemas.'
}

& git -C $RepoRoot diff --stat -- $relativePath
