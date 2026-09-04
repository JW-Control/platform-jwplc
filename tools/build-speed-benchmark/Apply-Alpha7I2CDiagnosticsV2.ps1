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

if ($text.Contains('I2C_DIAG_V2=RAW_GPIO+COMMISSIONING_PROBE')) {
    Write-Host 'Diagnostico I2C V2 ya esta aplicado. No se realizaron cambios.'
    exit 0
}

if (-not $text.Contains('I2C_DIAG=WATCH30S+COMMAND')) {
    throw 'Falta diagnostico I2C V1. Aplica primero Apply-Alpha7I2CDiagnostics.ps1.'
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

# 1) Lectura RAW de pads I2C, sin digitalRead() ni remapeo Arduino.
$old = @'
#include <esp_err.h>

extern "C"
'@
$new = @'
#include <esp_err.h>
#include <driver/gpio.h>

extern "C"
'@
$text = Replace-Once $text $old $new 'include driver gpio'

# 2) Periodo de commissioning mas corto que el watcher de RUN.
$old = @'
static constexpr uint32_t REV6_I2C_LINE_SAMPLE_MS = 1000UL;
static constexpr uint32_t REV6_I2C_WATCH_PERIOD_MS = 30000UL;
static constexpr uint32_t REV6_I2C_BUSY_RETRY_MS = 1000UL;
'@
$new = @'
static constexpr uint32_t REV6_I2C_LINE_SAMPLE_MS = 1000UL;
static constexpr uint32_t REV6_I2C_COMMISSION_PERIOD_MS = 5000UL;
static constexpr uint32_t REV6_I2C_WATCH_PERIOD_MS = 30000UL;
static constexpr uint32_t REV6_I2C_BUSY_RETRY_MS = 500UL;
'@
$text = Replace-Once $text $old $new 'periodo I2C commissioning'

# 3) Extender estado con imagen RAW del TCA para capturar DI espontaneas.
$old = @'
    int lastTcaErr = ESP_OK;
    int lastRtcErr = ESP_OK;
};
'@
$new = @'
    int lastTcaErr = ESP_OK;
    int lastRtcErr = ESP_OK;
    int lastTcaInputErr = ESP_OK;
    uint8_t lastTcaInput[3] = {0, 0, 0};
    bool haveTcaInput = false;
};
'@
$text = Replace-Once $text $old $new 'estado RAW TCA'

# 4) Helpers de nivel fisico. GPIO21/22 son los pads reales del bridge I2C.
$old = @'
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
'@
$new = @'
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
'@
$text = Replace-Once $text $old $new 'sample RAW GPIO21/22'

# 5) Probe esperado ahora tambien captura los 3 bancos INPUT del TCA.
$old = @'
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
'@
$new = @'
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
'@
$text = Replace-Once $text $old $new 'probe esperado V2'

# 6) Resumen incluye los ultimos errores del bridge y RAW INPUT del TCA.
$old = @'
    Serial.print(F(" lineLow SDA/SCL/both="));
    Serial.print(rev6I2CDiag.sdaLowSamples);
    Serial.print('/');
    Serial.print(rev6I2CDiag.sclLowSamples);
    Serial.print('/');
    Serial.println(rev6I2CDiag.bothLowSamples);
}
'@
$new = @'
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
'@
$text = Replace-Once $text $old $new 'summary V2'

# 7) Manual: aclarar que la linea mostrada es RAW y no remapeada.
$old = @'
    Serial.print(F("NODE="));
    Serial.print(shortRoleName());
    Serial.print(F(" SDA_PIN="));
    Serial.print((int)SDA);
    Serial.print(F(" SCL_PIN="));
    Serial.println((int)SCL);
'@
$new = @'
    Serial.print(F("NODE="));
    Serial.print(shortRoleName());
    Serial.println(F(" RAW_GPIO_SDA=21 RAW_GPIO_SCL=22"));
'@
$text = Replace-Once $text $old $new 'manual RAW pins'

# 8) Watcher tambien opera en commissioning. No bloquea el probe por nivel LOW:
#    justamente queremos obtener el codigo de error real del bridge.
$old = @'
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
'@
$new = @'
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
'@
$text = Replace-Once $text $old $new 'watcher commissioning V2'

# 9) Banner inequívoco.
$old = @'
    Serial.println(F("I2C_DIAG=WATCH30S+COMMAND"));
    Serial.println(F("I2C_EXPECTED=TCA6424A@0x22,RTC@0x68"));
'@
$new = @'
    Serial.println(F("I2C_DIAG=WATCH30S+COMMAND"));
    Serial.println(F("I2C_DIAG_V2=RAW_GPIO+COMMISSIONING_PROBE"));
    Serial.println(F("I2C_EXPECTED=TCA6424A@0x22,RTC@0x68"));
'@
$text = Replace-Once $text $old $new 'banner I2C V2'

[System.IO.File]::WriteAllText($target, $text, [System.Text.UTF8Encoding]::new($false))

Write-Host ''
Write-Host 'OK: diagnostico I2C V2 aplicado en 20c.'
Write-Host 'Cambios: GPIO21/22 RAW, probes en commissioning, RAW INPUT0..2 del TCA.'
Write-Host 'No modifica JWPLC_ModbusRTU ni el core precompilado.'
Write-Host ''

& git -C $RepoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw 'git diff --check detecto problemas.'
}

& git -C $RepoRoot diff --stat -- $relativePath
