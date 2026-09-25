/*
  A14 P5 - FULL_RUNTIME_MASTER

  Objetivo:
  Comparar la referencia TCP-only contra un JWPLC Basic operando
  simultaneamente con sus perifericos normales.

  Perfil:
  - Modbus TCP Server FC03 / 125 registros.
  - Modbus RTU Master cooperativo ~50 Hz hacia Slave ID 2.
  - TFT USER con contenido dinamico cada 100 ms.
  - FRAM write/read/verify/restore cada 250 ms.
  - microSD append cada 1 s con archivo persistente.
  - microSD flush cada 5 registros.
  - microSD read/verify cada 5 s.
  - RTC mediante runtime normal + freshness.
  - TCA/I/O mediante runtime normal + freshness.
  - botonera mediante task normal + muestreo.
  - probe del mutex SPI cada 100 ms.
  - sin conmutacion ciclica de reles.

  Serial:
    R -> reset de estadisticas; conserva RTU activo si ya estaba activo
    G -> iniciar trafico RTU Master
    X -> detener trafico RTU Master
    P -> preflight compacto P5-B
    S -> snapshot completo
*/

#include <Arduino.h>
#include <JWPLC_ModbusTCP.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_Display.h>

#include <string.h>

extern "C"
{
#include "jwplc_peripherals.h"
#include "jwplc_spi_bus.h"
}

// ============================================================================
// Modbus TCP
// ============================================================================

static constexpr uint8_t UNIT_ID = 1;
static constexpr uint16_t SERVER_PORT = 502;

static constexpr uint16_t COIL_COUNT = 2000;
static constexpr uint16_t HOLDING_COUNT = 125;

static uint8_t coils[(COIL_COUNT + 7U) / 8U];
static uint16_t holdingRegisters[HOLDING_COUNT];

// ============================================================================
// Modbus RTU Master cooperativo
// ============================================================================

static constexpr uint8_t RTU_MASTER_LOCAL_ID = 247;
static constexpr uint8_t RTU_TARGET_SLAVE_ID = 2;
static constexpr uint32_t RTU_BAUD = 115200UL;
static constexpr uint32_t RTU_CONFIG = SERIAL_8N1;
static constexpr uint32_t RTU_PERIOD_MS = 20UL;
static constexpr uint32_t RTU_TIMEOUT_MS = 25UL;
static constexpr uint16_t RTU_VERIFY_MAGIC = 0x55AA;

static bool rtuReady = false;
static bool rtuTrafficEnabled = false;
static uint16_t rtuReadValues[2] = {0, 0};

static uint32_t rtuTrafficStartMs = 0;
static uint32_t rtuTrafficDurationMs = 0;
static uint32_t rtuNextRequestMs = 0;

static uint32_t rtuRequestsStarted = 0;
static uint32_t rtuRequestsRejected = 0;
static uint32_t rtuRequestsCompleted = 0;
static uint32_t rtuRequestsSuccess = 0;
static uint32_t rtuRequestsFailed = 0;
static uint32_t rtuVerifyFails = 0;
static uint32_t rtuPeriodsSkipped = 0;

static uint32_t rtuLastServiceUs = 0;
static uint32_t rtuServiceGapMaxUs = 0;

// ============================================================================
// Perfil FULL_RUNTIME_REALISTIC
// ============================================================================

static constexpr uint32_t IO_SAMPLE_PERIOD_MS = 20;
static constexpr uint32_t BUTTON_SAMPLE_PERIOD_MS = 20;
static constexpr uint32_t SPI_PROBE_PERIOD_MS = 100;
static constexpr uint32_t DISPLAY_PERIOD_MS = 100;
static constexpr uint32_t FRAM_PERIOD_MS = 250;
static constexpr uint32_t RTC_SAMPLE_PERIOD_MS = 250;
static constexpr uint32_t SD_APPEND_PERIOD_MS = 1000;
static constexpr uint32_t SD_VERIFY_PERIOD_MS = 5000;

static constexpr uint32_t IO_STALE_LIMIT_MS = 100;
static constexpr uint32_t RTC_STALE_LIMIT_MS = 2500;

static constexpr size_t FRAM_BENCH_BYTES = 32;
static constexpr size_t SD_RECORD_BYTES = 32;
static constexpr uint8_t SD_FLUSH_EVERY_RECORDS = 5U;

static const char SD_BENCH_PATH[] = "/A14S2.LOG";

// ============================================================================
// Estado general
// ============================================================================

static bool readyAnnounced = false;

static uint32_t lastLoopUs = 0;
static uint64_t loopGapSumUs = 0;
static uint32_t loopGapSamples = 0;
static uint32_t loopGapMaxUs = 0;

// ============================================================================
// Perifericos
// ============================================================================

static bool framReady = false;
static uint32_t framBenchAddress = 0;
static uint8_t framBackup[FRAM_BENCH_BYTES];

static bool sdReady = false;
static JWPLCFile sdAppendFile;
static uint8_t sdRecordsSinceFlush = 0;

static uint8_t lastSdRecord[SD_RECORD_BYTES];
static bool lastSdRecordValid = false;
static uint32_t sdSequence = 0;

// ============================================================================
// Display HMI Alpha11 - dirty redraw / on demand
// ============================================================================

enum MasterFieldId : uint8_t
{
    FIELD_ROLE = 1,
    FIELD_TCP_OK,
    FIELD_RTU_OK,
    FIELD_RTU_FAIL,
    FIELD_SD_READY,
    FIELD_ETH_READY
};

static const JWPLC_UIField MASTER_FIELDS[] = {
    JWPLC_UITextField(FIELD_ROLE, 8, 8, "Rol", 10),
    JWPLC_UIValueField(
        FIELD_TCP_OK, 8, 42, "TCP OK", "",
        JWPLC_UIValueFormat(7, 0, false, false)),
    JWPLC_UIValueField(
        FIELD_RTU_OK, 8, 76, "RTU OK", "",
        JWPLC_UIValueFormat(7, 0, false, false)),
    JWPLC_UIValueField(
        FIELD_RTU_FAIL, 8, 110, "RTU FAIL", "",
        JWPLC_UIValueFormat(5, 0, false, false)),
    JWPLC_UIBoolField(
        FIELD_SD_READY, 165, 42, "SD",
        JWPLC_UIBoolText("FAIL", "OK")),
    JWPLC_UIBoolField(
        FIELD_ETH_READY, 165, 76, "ETH",
        JWPLC_UIBoolText("DOWN", "UP"))
};

static uint32_t displayServiceCycles = 0;
static uint32_t displayLastServiceMs = 0;
static uint32_t displayServiceGapMaxMs = 0;

// ============================================================================
// Estadisticas runtime
// ============================================================================
// ============================================================================
// Estadisticas runtime
// ============================================================================

struct RuntimeStats
{
    uint32_t ioSamples;
    uint32_t ioStale;
    uint32_t ioMaxAgeMs;

    uint32_t buttonSamples;
    uint32_t buttonNotReady;
    uint32_t buttonSampleGapMaxMs;
    uint32_t buttonLastSampleMs;
    uint32_t buttonEventObservations;

    uint32_t rtcSamples;
    uint32_t rtcUnavailable;
    uint32_t rtcStale;
    uint32_t rtcMaxAgeMs;

    uint32_t framCycles;
    uint32_t framFails;
    uint32_t framMaxUs;

    uint32_t sdAppendCycles;
    uint32_t sdAppendFails;
    uint32_t sdAppendMaxUs;
    uint32_t sdFlushCycles;

    uint32_t sdVerifyCycles;
    uint32_t sdVerifyFails;
    uint32_t sdVerifyMaxUs;

    uint32_t spiProbeSamples;
    uint32_t spiProbeFails;
    uint32_t spiProbeOver1ms;
    uint32_t spiProbeOver10ms;
    uint32_t spiProbeMaxWaitUs;
};

static RuntimeStats runtimeStats = {};

// ============================================================================
// Scheduler
// ============================================================================

static uint32_t lastIoSampleMs = 0;
static uint32_t lastButtonSampleMs = 0;
static uint32_t lastSpiProbeMs = 0;
static uint32_t lastFramMs = 0;
static uint32_t lastRtcSampleMs = 0;
static uint32_t lastSdAppendMs = 0;
static uint32_t lastSdVerifyMs = 0;

// ============================================================================
// Helpers
// ============================================================================

static void updateMaxU32(
    uint32_t value,
    uint32_t &currentMax)
{
    if (value > currentMax)
    {
        currentMax = value;
    }
}


static const char *yesNo(bool value)
{
    return value ? "YES" : "NO";
}

// ============================================================================
// Display USER - HMI declarativa
// ============================================================================

static void serviceDisplayTelemetry()
{
    const uint32_t now = millis();

    if ((uint32_t)(now - displayLastServiceMs) < DISPLAY_PERIOD_MS)
    {
        return;
    }

    if (displayLastServiceMs != 0)
    {
        const uint32_t gap =
            (uint32_t)(now - displayLastServiceMs);

        updateMaxU32(
            gap,
            displayServiceGapMaxMs);
    }

    displayLastServiceMs = now;
    ++displayServiceCycles;

    const JWPLCModbusTCPStats &tcp =
        JWPLC_ModbusTCP.stats();

    const uint32_t rtuFailTotal =
        rtuRequestsRejected +
        rtuRequestsFailed +
        rtuVerifyFails;

    JWPLC_Display.setText(
        FIELD_ROLE,
        "MASTER");

    JWPLC_Display.setValue(
        FIELD_TCP_OK,
        tcp.requestsOk);

    JWPLC_Display.setValue(
        FIELD_RTU_OK,
        rtuRequestsSuccess);

    JWPLC_Display.setValue(
        FIELD_RTU_FAIL,
        rtuFailTotal);

    JWPLC_Display.setBool(
        FIELD_SD_READY,
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady());

    JWPLC_Display.setBool(
        FIELD_ETH_READY,
        JWPLC_Ethernet.isReady() &&
        JWPLC_Ethernet.linkUp());
}

// ============================================================================
// FRAM
// ============================================================================
// ============================================================================
// FRAM
// ============================================================================

static bool writeFramBuffer(
    const uint8_t *data,
    size_t len)
{
    if (!JWPLC_FRAM.writeEnable(true))
    {
        return false;
    }

    const bool writeOk =
        JWPLC_FRAM.write(
            framBenchAddress,
            data,
            len);

    const bool disableOk =
        JWPLC_FRAM.writeEnable(false);

    return
        writeOk &&
        disableOk;
}

static void serviceFramWorkload()
{
    ++runtimeStats.framCycles;

    const uint32_t t0 = micros();

    uint8_t pattern[FRAM_BENCH_BYTES];
    uint8_t readback[FRAM_BENCH_BYTES];

    const uint32_t sequence =
        runtimeStats.framCycles;

    for (
        size_t i = 0;
        i < FRAM_BENCH_BYTES;
        ++i)
    {
        pattern[i] =
            (uint8_t)(
                0x5AU ^
                (uint8_t)i ^
                (uint8_t)sequence);
    }

    bool ok = framReady;

    if (ok)
    {
        ok = writeFramBuffer(
            pattern,
            sizeof(pattern));
    }

    if (ok)
    {
        memset(
            readback,
            0,
            sizeof(readback));

        ok =
            JWPLC_FRAM.read(
                framBenchAddress,
                readback,
                sizeof(readback));
    }

    if (
        ok &&
        memcmp(
            pattern,
            readback,
            sizeof(pattern)) != 0)
    {
        ok = false;
    }

    // Restauración inmediata:
    // la prueba no deja persistente el patrón temporal.
    const bool restoreOk =
        framReady &&
        writeFramBuffer(
            framBackup,
            sizeof(framBackup));

    ok =
        ok &&
        restoreOk;

    const uint32_t elapsed =
        (uint32_t)(
            micros() -
            t0);

    updateMaxU32(
        elapsed,
        runtimeStats.framMaxUs);

    if (!ok)
    {
        ++runtimeStats.framFails;
    }
}

// ============================================================================
// microSD
// ============================================================================

static void buildSdRecord(
    uint32_t sequence,
    uint8_t record[SD_RECORD_BYTES])
{
    memset(
        record,
        0,
        SD_RECORD_BYTES);

    record[0] = 'A';
    record[1] = '1';
    record[2] = '4';
    record[3] = 'S';

    record[4] =
        (uint8_t)(
            sequence >>
            24);

    record[5] =
        (uint8_t)(
            sequence >>
            16);

    record[6] =
        (uint8_t)(
            sequence >>
            8);

    record[7] =
        (uint8_t)sequence;

    const uint32_t now =
        millis();

    record[8] =
        (uint8_t)(
            now >>
            24);

    record[9] =
        (uint8_t)(
            now >>
            16);

    record[10] =
        (uint8_t)(
            now >>
            8);

    record[11] =
        (uint8_t)now;

    for (
        size_t i = 12;
        i < SD_RECORD_BYTES;
        ++i)
    {
        record[i] =
            (uint8_t)(
                0xC3U ^
                (uint8_t)i ^
                (uint8_t)sequence);
    }
}

static void serviceSdAppend()
{
    ++runtimeStats.sdAppendCycles;

    const uint32_t t0 =
        micros();

    uint8_t record[SD_RECORD_BYTES];

    ++sdSequence;

    buildSdRecord(
        sdSequence,
        record);

    bool ok =
        sdReady &&
        (bool)sdAppendFile &&
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady();

    if (ok)
    {
        const size_t written =
            sdAppendFile.write(
                record,
                sizeof(record));

        ok =
            written ==
            sizeof(record);

        if (ok)
        {
            ++sdRecordsSinceFlush;

            if (
                sdRecordsSinceFlush >=
                SD_FLUSH_EVERY_RECORDS)
            {
                sdAppendFile.flush();

                sdRecordsSinceFlush = 0;

                ++runtimeStats.sdFlushCycles;
            }
        }
    }

    if (ok)
    {
        memcpy(
            lastSdRecord,
            record,
            sizeof(record));

        lastSdRecordValid = true;
        sdReady = true;
    }
    else
    {
        ++runtimeStats.sdAppendFails;
    }

    const uint32_t elapsed =
        (uint32_t)(
            micros() -
            t0);

    updateMaxU32(
        elapsed,
        runtimeStats.sdAppendMaxUs);
}

static void serviceSdVerify()
{
    if (!lastSdRecordValid)
    {
        return;
    }

    ++runtimeStats.sdVerifyCycles;

    const uint32_t t0 =
        micros();

    uint8_t readback[SD_RECORD_BYTES];

    memset(
        readback,
        0,
        sizeof(readback));

    bool ok =
        sdReady &&
        (bool)sdAppendFile &&
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady();

    bool appendClosed = false;
    bool reopenOk = false;

    // --------------------------------------------------------
    // No mantener dos handles simultáneos sobre el mismo archivo.
    //
    // El append se hace durable, se cierra temporalmente,
    // se verifica mediante FILE_READ y luego se reabre.
    // --------------------------------------------------------

    if (ok)
    {
        if (sdRecordsSinceFlush > 0)
        {
            sdAppendFile.flush();

            sdRecordsSinceFlush = 0;

            ++runtimeStats.sdFlushCycles;
        }

        sdAppendFile.close();

        appendClosed =
            !(bool)sdAppendFile;

        ok =
            appendClosed;
    }

    // --------------------------------------------------------
    // Leer y verificar el último registro.
    // --------------------------------------------------------

    if (ok)
    {
        JWPLCFile file =
            JWPLC_SD.open(
                SD_BENCH_PATH,
                FILE_READ);

        if (!file)
        {
            ok = false;
        }
        else
        {
            const uint32_t fileSize =
                file.size();

            if (
                fileSize <
                SD_RECORD_BYTES)
            {
                ok = false;
            }
            else
            {
                ok =
                    file.seek(
                        fileSize -
                        SD_RECORD_BYTES);
            }

            if (ok)
            {
                for (
                    size_t i = 0;
                    i < SD_RECORD_BYTES;
                    ++i)
                {
                    const int value =
                        file.read();

                    if (value < 0)
                    {
                        ok = false;
                        break;
                    }

                    readback[i] =
                        (uint8_t)value;
                }
            }

            file.close();
        }
    }

    if (
        ok &&
        memcmp(
            readback,
            lastSdRecord,
            sizeof(readback)) != 0)
    {
        ok = false;
    }

    // --------------------------------------------------------
    // Recuperar siempre el handle persistente de append
    // después de haberlo cerrado.
    // --------------------------------------------------------

    if (appendClosed)
    {
        sdAppendFile =
            JWPLC_SD.open(
                SD_BENCH_PATH,
                FILE_APPEND);

        reopenOk =
            (bool)sdAppendFile;

        if (!reopenOk)
        {
            sdReady = false;
        }
    }

    ok =
        ok &&
        reopenOk;

    if (!ok)
    {
        ++runtimeStats.sdVerifyFails;
    }

    const uint32_t elapsed =
        (uint32_t)(
            micros() -
            t0);

    updateMaxU32(
        elapsed,
        runtimeStats.sdVerifyMaxUs);
}

// ============================================================================
// RTC / I/O / botones
// ============================================================================

static void sampleIo(
    uint32_t now)
{
    ++runtimeStats.ioSamples;

    const JWPLC_IOState *io =
        jwplcGetIOState();

    if (
        io == nullptr ||
        !io->initialized)
    {
        ++runtimeStats.ioStale;
        return;
    }

    // jwplcSystemTask actualiza last_scan_ms concurrentemente.
    // Capturar primero el timestamp del runtime y millis()
    // después evita un falso underflow de 1 ms.
    const uint32_t lastScanMs =
        io->last_scan_ms;

    const uint32_t nowAfterSnapshot =
        millis();

    const uint32_t age =
        (uint32_t)(
            nowAfterSnapshot -
            lastScanMs);

    (void)now;

    updateMaxU32(
        age,
        runtimeStats.ioMaxAgeMs);

    if (
        age >
        IO_STALE_LIMIT_MS)
    {
        ++runtimeStats.ioStale;
    }

    // Lectura del snapshot lógico.
    volatile uint8_t inputs =
        io->di_logical_bank0;

    volatile uint8_t outputs =
        io->do_bank1;

    (void)inputs;
    (void)outputs;
}

static void sampleButtons(
    uint32_t now)
{
    ++runtimeStats.buttonSamples;

    if (
        runtimeStats.buttonLastSampleMs != 0)
    {
        const uint32_t gap =
            (uint32_t)(
                now -
                runtimeStats.buttonLastSampleMs);

        updateMaxU32(
            gap,
            runtimeStats.buttonSampleGapMaxMs);
    }

    runtimeStats.buttonLastSampleMs =
        now;

    if (!JWPLCButtons::isReady())
    {
        ++runtimeStats.buttonNotReady;
        return;
    }

    uint8_t mask = 0;

    if (JWPLC_Buttons.isDown(BTN_LEFT))
        mask |= (uint8_t)(1U << BTN_LEFT);

    if (JWPLC_Buttons.isDown(BTN_UP))
        mask |= (uint8_t)(1U << BTN_UP);

    if (JWPLC_Buttons.isDown(BTN_RIGHT))
        mask |= (uint8_t)(1U << BTN_RIGHT);

    if (JWPLC_Buttons.isDown(BTN_ESC))
        mask |= (uint8_t)(1U << BTN_ESC);

    if (JWPLC_Buttons.isDown(BTN_OK))
        mask |= (uint8_t)(1U << BTN_OK);

    if (JWPLC_Buttons.isDown(BTN_DOWN))
        mask |= (uint8_t)(1U << BTN_DOWN);

    (void)mask;

    runtimeStats.buttonEventObservations +=
        JWPLC_Buttons.eventCount();
}

static void sampleRtc(
    uint32_t now)
{
    ++runtimeStats.rtcSamples;

    const JWPLC_RTCState *rtc =
        jwplcGetRTCState();

    if (
        rtc == nullptr ||
        !rtc->present)
    {
        ++runtimeStats.rtcUnavailable;
        return;
    }

    // jwplcSystemTask actualiza last_update_ms concurrentemente.
    // Capturar primero el timestamp del runtime y millis()
    // después evita el mismo falso underflow de freshness.
    const uint32_t lastUpdateMs =
        rtc->last_update_ms;

    const uint32_t nowAfterSnapshot =
        millis();

    const uint32_t age =
        (uint32_t)(
            nowAfterSnapshot -
            lastUpdateMs);

    (void)now;

    updateMaxU32(
        age,
        runtimeStats.rtcMaxAgeMs);

    if (
        age >
        RTC_STALE_LIMIT_MS)
    {
        ++runtimeStats.rtcStale;
    }
}

// ============================================================================
// Probe SPI
// ============================================================================

static void probeSpiMutex()
{
    ++runtimeStats.spiProbeSamples;

    const uint32_t t0 =
        micros();

    const bool acquired =
        jwplcSPI_acquire(50);

    const uint32_t waitUs =
        (uint32_t)(
            micros() -
            t0);

    updateMaxU32(
        waitUs,
        runtimeStats.spiProbeMaxWaitUs);

    if (!acquired)
    {
        ++runtimeStats.spiProbeFails;
        return;
    }

    if (waitUs > 1000UL)
    {
        ++runtimeStats.spiProbeOver1ms;
    }

    if (waitUs > 10000UL)
    {
        ++runtimeStats.spiProbeOver10ms;
    }

    jwplcSPI_release();
}

// ============================================================================
// Modbus RTU Master - 50 Hz cooperativo
// ============================================================================

static void resetRtuTrafficCounters()
{
    JWPLC_ModbusRTU.resetStats();

    rtuTrafficDurationMs = 0;
    rtuRequestsStarted = 0;
    rtuRequestsRejected = 0;
    rtuRequestsCompleted = 0;
    rtuRequestsSuccess = 0;
    rtuRequestsFailed = 0;
    rtuVerifyFails = 0;
    rtuPeriodsSkipped = 0;

    rtuLastServiceUs = micros();
    rtuServiceGapMaxUs = 0;
}

static void startRtuTraffic()
{
    if (!rtuReady)
    {
        return;
    }

    rtuTrafficEnabled = true;
    rtuTrafficStartMs = millis();
    rtuTrafficDurationMs = 0;
    rtuNextRequestMs = millis();
}

static void stopRtuTraffic()
{
    if (rtuTrafficEnabled)
    {
        rtuTrafficDurationMs =
            (uint32_t)(millis() - rtuTrafficStartMs);
    }

    rtuTrafficEnabled = false;
}

static void completeRtuMasterResult()
{
    if (!JWPLC_ModbusRTU.masterDone())
    {
        return;
    }

    ++rtuRequestsCompleted;

    if (JWPLC_ModbusRTU.masterSucceeded())
    {
        ++rtuRequestsSuccess;

        if (rtuReadValues[1] != RTU_VERIFY_MAGIC)
        {
            ++rtuVerifyFails;
        }
    }
    else
    {
        ++rtuRequestsFailed;
    }

    JWPLC_ModbusRTU.clearMasterResult();
}

static void serviceRtuMaster()
{
    const uint32_t nowUs = micros();

    if (rtuLastServiceUs != 0)
    {
        const uint32_t gapUs =
            (uint32_t)(nowUs - rtuLastServiceUs);

        updateMaxU32(
            gapUs,
            rtuServiceGapMaxUs);
    }

    rtuLastServiceUs = nowUs;

    JWPLC_ModbusRTU.task();
    completeRtuMasterResult();

    if (!rtuTrafficEnabled)
    {
        return;
    }

    const uint32_t now = millis();

    if ((int32_t)(now - rtuNextRequestMs) < 0)
    {
        return;
    }

    if (JWPLC_ModbusRTU.masterBusy())
    {
        return;
    }

    uint32_t periodsAdvanced = 0;

    do
    {
        rtuNextRequestMs += RTU_PERIOD_MS;
        ++periodsAdvanced;
    }
    while ((int32_t)(now - rtuNextRequestMs) >= 0);

    if (periodsAdvanced > 1)
    {
        rtuPeriodsSkipped += periodsAdvanced - 1;
    }

    const bool accepted =
        JWPLC_ModbusRTU.requestReadHoldingRegisters(
            RTU_TARGET_SLAVE_ID,
            0,
            2,
            rtuReadValues,
            RTU_TIMEOUT_MS);

    if (accepted)
    {
        ++rtuRequestsStarted;
    }
    else
    {
        ++rtuRequestsRejected;
    }
}

// ============================================================================
// Scheduler del perfil realistic
// ============================================================================

static void serviceRealisticWorkload()
{
    const uint32_t now =
        millis();

    // Máximo un trabajo añadido por vuelta.
    // Modbus TCP vuelve a obtener servicio inmediatamente
    // en la siguiente iteración de loop().

    if (
        (uint32_t)(
            now -
            lastIoSampleMs) >=
        IO_SAMPLE_PERIOD_MS)
    {
        lastIoSampleMs = now;
        sampleIo(now);
        return;
    }

    if (
        (uint32_t)(
            now -
            lastButtonSampleMs) >=
        BUTTON_SAMPLE_PERIOD_MS)
    {
        lastButtonSampleMs = now;
        sampleButtons(now);
        return;
    }

    if (
        (uint32_t)(
            now -
            lastSpiProbeMs) >=
        SPI_PROBE_PERIOD_MS)
    {
        lastSpiProbeMs = now;
        probeSpiMutex();
        return;
    }

    if (
        (uint32_t)(
            now -
            lastFramMs) >=
        FRAM_PERIOD_MS)
    {
        lastFramMs = now;
        serviceFramWorkload();
        return;
    }

    if (
        (uint32_t)(
            now -
            lastRtcSampleMs) >=
        RTC_SAMPLE_PERIOD_MS)
    {
        lastRtcSampleMs = now;
        sampleRtc(now);
        return;
    }

    if (
        (uint32_t)(
            now -
            lastSdAppendMs) >=
        SD_APPEND_PERIOD_MS)
    {
        lastSdAppendMs = now;
        serviceSdAppend();
        return;
    }

    if (
        (uint32_t)(
            now -
            lastSdVerifyMs) >=
        SD_VERIFY_PERIOD_MS)
    {
        lastSdVerifyMs = now;
        serviceSdVerify();
        return;
    }
}

// ============================================================================
// Ready / health
// ============================================================================

static bool ioReady()
{
    const JWPLC_IOState *io =
        jwplcGetIOState();

    return
        io != nullptr &&
        io->initialized;
}

static bool rtcReady()
{
    const JWPLC_RTCState *rtc =
        jwplcGetRTCState();

    return
        rtc != nullptr &&
        rtc->present;
}

static bool fullRuntimeReady()
{
    return
        JWPLC_Display.isReady() &&
        framReady &&
        sdReady &&
        (bool)sdAppendFile &&
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady() &&
        JWPLCButtons::isReady() &&
        ioReady() &&
        rtcReady() &&
        rtuReady;
}

static uint32_t peripheralFailureCount()
{
    return
        runtimeStats.ioStale +
        runtimeStats.buttonNotReady +
        runtimeStats.rtcUnavailable +
        runtimeStats.rtcStale +
        runtimeStats.framFails +
        runtimeStats.sdAppendFails +
        runtimeStats.sdVerifyFails +
        runtimeStats.spiProbeFails;
}

// ============================================================================
// Reset estadístico
// ============================================================================

static void resetPerfCounters()
{
    JWPLC_ModbusTCP.resetStats();
    resetRtuTrafficCounters();

    displayServiceCycles = 0;
    displayLastServiceMs = millis();
    displayServiceGapMaxMs = 0;

    loopGapSumUs = 0;
    loopGapSamples = 0;
    loopGapMaxUs = 0;
    lastLoopUs = micros();

    runtimeStats = RuntimeStats{};

    // El reset estadístico ocurre fuera de la ventana medida.
    // Se deja el archivo persistente durable y se reinicia el
    // lote para que cada ventana comience alineada a 5 registros.
    if (sdAppendFile)
    {
        sdAppendFile.flush();
        sdRecordsSinceFlush = 0;
    }

    const uint32_t now =
        millis();

    runtimeStats.buttonLastSampleMs =
        now;

    lastIoSampleMs = now;
    lastButtonSampleMs = now;
    lastSpiProbeMs = now;
    lastFramMs = now;
    lastRtcSampleMs = now;
    lastSdAppendMs = now;
    lastSdVerifyMs = now;

}

// ============================================================================
// Snapshot
// ============================================================================

static void printSnapshot()
{
    const JWPLCModbusTCPStats &s =
        JWPLC_ModbusTCP.stats();

    uint32_t loopGapAvgUs = 0;

    if (loopGapSamples > 0)
    {
        loopGapAvgUs =
            (uint32_t)(
                loopGapSumUs /
                loopGapSamples);
    }

    const JWPLC_IOState *io =
        jwplcGetIOState();

    const JWPLC_RTCState *rtc =
        jwplcGetRTCState();

    // Los timestamps son publicados por jwplcSystemTask.
    // Capturarlos antes de millis() evita observar un timestamp
    // futuro por una actualización concurrente entre lecturas.
    const uint32_t ioLastScanMs =
        (
            io != nullptr &&
            io->initialized
        )
            ? io->last_scan_ms
            : 0U;

    const uint32_t rtcLastUpdateMs =
        (
            rtc != nullptr &&
            rtc->present
        )
            ? rtc->last_update_ms
            : 0U;

    const uint32_t now =
        millis();

    const uint32_t ioAgeMs =
        (
            io != nullptr &&
            io->initialized
        )
            ? (uint32_t)(
                  now -
                  ioLastScanMs)
            : 0xFFFFFFFFUL;

    const uint32_t rtcAgeMs =
        (
            rtc != nullptr &&
            rtc->present
        )
            ? (uint32_t)(
                  now -
                  rtcLastUpdateMs)
            : 0xFFFFFFFFUL;

    Serial.println();
    Serial.println(
        "========================================");
    Serial.println(
        " A14.3 PERF-S2 FULL RUNTIME SNAPSHOT");
    Serial.println(
        "========================================");

    Serial.println(
        "FULL_RUNTIME_PROFILE=REALISTIC");

    Serial.print(
        "FULL_RUNTIME_READY=");
    Serial.println(
        yesNo(
            fullRuntimeReady()));

    // --------------------------------------------------------
    // Modbus TCP
    // --------------------------------------------------------

    Serial.print("CLIENT_CONNECTIONS=");
    Serial.println(s.clientConnections);

    Serial.print("RX_FRAMES=");
    Serial.println(s.rxFrames);

    Serial.print("TX_FRAMES=");
    Serial.println(s.txFrames);

    Serial.print("REQUESTS_OK=");
    Serial.println(s.requestsOk);

    Serial.print("EXCEPTIONS_SENT=");
    Serial.println(s.exceptionsSent);

    Serial.print("PROTOCOL_ERRORS=");
    Serial.println(s.protocolErrors);

    Serial.print("FRAME_TIMEOUTS=");
    Serial.println(s.frameTimeouts);

    Serial.print("BUS_LOCK_TIMEOUTS=");
    Serial.println(s.busLockTimeouts);

    Serial.print("LOOP_GAP_AVG_US=");
    Serial.println(loopGapAvgUs);

    Serial.print("LOOP_GAP_MAX_US=");
    Serial.println(loopGapMaxUs);

    Serial.print("SERVER_READY=");
    Serial.println(
        yesNo(
            JWPLC_ModbusTCP.serverReady()));

    Serial.print("CLIENT_CONNECTED=");
    Serial.println(
        yesNo(
            JWPLC_ModbusTCP.clientConnected()));

    // --------------------------------------------------------
    // Ethernet
    // --------------------------------------------------------

    Serial.print("ETH_READY=");
    Serial.println(
        yesNo(
            JWPLC_Ethernet.isReady()));

    Serial.print("ETH_LINK=");
    Serial.println(
        JWPLC_Ethernet.linkUp()
            ? "UP"
            : "DOWN");

    Serial.print("ETH_IP=");
    Serial.println(
        JWPLC_Ethernet.localIP());

    Serial.print("COMBINED_RUNTIME_READY=");
    Serial.println(
        yesNo(
            fullRuntimeReady() &&
            JWPLC_ModbusTCP.serverReady() &&
            JWPLC_Ethernet.isReady() &&
            JWPLC_Ethernet.linkUp() &&
            rtuReady &&
            rtuTrafficEnabled));

    // --------------------------------------------------------
    // Modbus RTU Master
    // --------------------------------------------------------

    const JWPLCModbusRTUStats &rtu =
        JWPLC_ModbusRTU.stats();

    uint32_t rtuDurationMs =
        rtuTrafficDurationMs;

    if (rtuTrafficEnabled)
    {
        rtuDurationMs =
            (uint32_t)(millis() - rtuTrafficStartMs);
    }

    Serial.print("RTU_READY=");
    Serial.println(yesNo(rtuReady));

    Serial.println("RTU_ROLE=MASTER");

    Serial.print("RTU_TARGET_SLAVE_ID=");
    Serial.println(RTU_TARGET_SLAVE_ID);

    Serial.print("RTU_BAUD=");
    Serial.println(RTU_BAUD);

    Serial.print("RTU_TIMEOUT_MS=");
    Serial.println(RTU_TIMEOUT_MS);

    Serial.print("RTU_TRAFFIC_ENABLED=");
    Serial.println(yesNo(rtuTrafficEnabled));

    Serial.print("RTU_TRAFFIC_DURATION_MS=");
    Serial.println(rtuDurationMs);

    Serial.print("RTU_REQUESTS_STARTED=");
    Serial.println(rtuRequestsStarted);

    Serial.print("RTU_REQUESTS_REJECTED=");
    Serial.println(rtuRequestsRejected);

    Serial.print("RTU_REQUESTS_COMPLETED=");
    Serial.println(rtuRequestsCompleted);

    Serial.print("RTU_REQUESTS_SUCCESS=");
    Serial.println(rtuRequestsSuccess);

    Serial.print("RTU_REQUESTS_FAILED=");
    Serial.println(rtuRequestsFailed);

    Serial.print("RTU_VERIFY_FAILS=");
    Serial.println(rtuVerifyFails);

    Serial.print("RTU_PERIODS_SKIPPED=");
    Serial.println(rtuPeriodsSkipped);

    Serial.print("RTU_RX_FRAMES=");
    Serial.println(rtu.rxFrames);

    Serial.print("RTU_TX_FRAMES=");
    Serial.println(rtu.txFrames);

    Serial.print("RTU_CRC_ERRORS=");
    Serial.println(rtu.crcErrors);

    Serial.print("RTU_MASTER_TIMEOUTS=");
    Serial.println(rtu.masterTimeouts);

    Serial.print("RTU_LAST_ERROR=");
    Serial.println(JWPLC_ModbusRTU.lastErrorString());

    Serial.print("RTU_SERVICE_GAP_MAX_US=");
    Serial.println(rtuServiceGapMaxUs);

    // --------------------------------------------------------
    // TFT
    // --------------------------------------------------------

    Serial.print("DISPLAY_READY=");
    Serial.println(
        yesNo(
            JWPLC_Display.isReady()));

    Serial.println(
        "DISPLAY_RENDER_MODE=HMI_ON_DEMAND_DIRTY");

    Serial.println(
        "DISPLAY_REFRESH_MODE=USER_REFRESH_ON_DEMAND");

    Serial.print("DISPLAY_FRAMES=");
    Serial.println(displayServiceCycles);

    Serial.print("DISPLAY_GAP_MAX_MS=");
    Serial.println(displayServiceGapMaxMs);

    // --------------------------------------------------------
    // FRAM
    // --------------------------------------------------------

    Serial.print("FRAM_READY=");
    Serial.println(
        yesNo(
            framReady));

    Serial.print("FRAM_CYCLES=");
    Serial.println(
        runtimeStats.framCycles);

    Serial.print("FRAM_FAILS=");
    Serial.println(
        runtimeStats.framFails);

    Serial.print("FRAM_MAX_US=");
    Serial.println(
        runtimeStats.framMaxUs);

    // --------------------------------------------------------
    // SD
    // --------------------------------------------------------

    Serial.print("SD_READY=");
    Serial.println(
        yesNo(
            JWPLCSD::isEnabled() &&
            JWPLCSD::isCardPresent() &&
            JWPLCSD::isReady()));

    Serial.print("SD_APPEND_CYCLES=");
    Serial.println(
        runtimeStats.sdAppendCycles);

    Serial.print("SD_APPEND_FAILS=");
    Serial.println(
        runtimeStats.sdAppendFails);

    Serial.print("SD_APPEND_MAX_US=");
    Serial.println(
        runtimeStats.sdAppendMaxUs);

    Serial.print("SD_APPEND_FILE_OPEN=");
    Serial.println(
        yesNo(
            (bool)sdAppendFile));

    Serial.print("SD_FLUSH_EVERY_RECORDS=");
    Serial.println(
        SD_FLUSH_EVERY_RECORDS);

    Serial.print("SD_RECORDS_SINCE_FLUSH=");
    Serial.println(
        sdRecordsSinceFlush);

    Serial.print("SD_FLUSH_CYCLES=");
    Serial.println(
        runtimeStats.sdFlushCycles);

    Serial.print("SD_VERIFY_CYCLES=");
    Serial.println(
        runtimeStats.sdVerifyCycles);

    Serial.print("SD_VERIFY_FAILS=");
    Serial.println(
        runtimeStats.sdVerifyFails);

    Serial.print("SD_VERIFY_MAX_US=");
    Serial.println(
        runtimeStats.sdVerifyMaxUs);

    // --------------------------------------------------------
    // RTC
    // --------------------------------------------------------

    Serial.print("RTC_PRESENT=");
    Serial.println(
        yesNo(
            rtc != nullptr &&
            rtc->present));

    Serial.print("RTC_SAMPLES=");
    Serial.println(
        runtimeStats.rtcSamples);

    Serial.print("RTC_UNAVAILABLE=");
    Serial.println(
        runtimeStats.rtcUnavailable);

    Serial.print("RTC_STALE=");
    Serial.println(
        runtimeStats.rtcStale);

    Serial.print("RTC_MAX_AGE_MS=");
    Serial.println(
        runtimeStats.rtcMaxAgeMs);

    Serial.print("RTC_CURRENT_AGE_MS=");
    Serial.println(
        rtcAgeMs);

    // --------------------------------------------------------
    // TCA / I-O
    // --------------------------------------------------------

    Serial.print("IO_INITIALIZED=");
    Serial.println(
        yesNo(
            io != nullptr &&
            io->initialized));

    Serial.print("IO_SAMPLES=");
    Serial.println(
        runtimeStats.ioSamples);

    Serial.print("IO_STALE=");
    Serial.println(
        runtimeStats.ioStale);

    Serial.print("IO_MAX_AGE_MS=");
    Serial.println(
        runtimeStats.ioMaxAgeMs);

    Serial.print("IO_CURRENT_AGE_MS=");
    Serial.println(
        ioAgeMs);

    // --------------------------------------------------------
    // Botonera
    // --------------------------------------------------------

    Serial.print("BUTTONS_READY=");
    Serial.println(
        yesNo(
            JWPLCButtons::isReady()));

    Serial.print("BUTTON_SAMPLES=");
    Serial.println(
        runtimeStats.buttonSamples);

    Serial.print("BUTTON_NOT_READY=");
    Serial.println(
        runtimeStats.buttonNotReady);

    Serial.print("BUTTON_SAMPLE_GAP_MAX_MS=");
    Serial.println(
        runtimeStats.buttonSampleGapMaxMs);

    Serial.print("BUTTON_EVENT_OBSERVATIONS=");
    Serial.println(
        runtimeStats.buttonEventObservations);

    // --------------------------------------------------------
    // SPI ownership
    // --------------------------------------------------------

    Serial.print("SPI_PROBE_SAMPLES=");
    Serial.println(
        runtimeStats.spiProbeSamples);

    Serial.print("SPI_PROBE_FAILS=");
    Serial.println(
        runtimeStats.spiProbeFails);

    Serial.print("SPI_PROBE_MAX_WAIT_US=");
    Serial.println(
        runtimeStats.spiProbeMaxWaitUs);

    Serial.print("SPI_PROBE_OVER_1MS=");
    Serial.println(
        runtimeStats.spiProbeOver1ms);

    Serial.print("SPI_PROBE_OVER_10MS=");
    Serial.println(
        runtimeStats.spiProbeOver10ms);

    Serial.print("PERIPHERAL_FAILURE_COUNT=");
    Serial.println(
        peripheralFailureCount());

    Serial.println(
        "A14_PERF_SNAPSHOT=END");
}

// ============================================================================
// Preflight serial compacto
// ============================================================================
//
// P5-B no debe usar el snapshot completo como polling de readiness:
// imprimir varios KB por Serial a 115200 perturba el loop cooperativo y puede
// fabricar timeouts RTU. Este bloque captura primero el estado y luego emite
// sólo las claves necesarias. Cualquier perturbación causada por esta salida
// queda fuera de la ventana formal y se limpia con R antes de medir.
// ============================================================================

static void printP5Preflight()
{
    const JWPLCModbusRTUStats &rtu =
        JWPLC_ModbusRTU.stats();

    const bool fullReady =
        fullRuntimeReady();

    const bool serverReady =
        JWPLC_ModbusTCP.serverReady();

    const bool ethReady =
        JWPLC_Ethernet.isReady();

    const bool ethLink =
        JWPLC_Ethernet.linkUp();

    const bool sdNowReady =
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady();

    const bool displayReady =
        JWPLC_Display.isReady();

    const bool combinedReady =
        fullReady &&
        serverReady &&
        ethReady &&
        ethLink &&
        rtuReady &&
        rtuTrafficEnabled;

    const uint32_t success =
        rtuRequestsSuccess;

    const uint32_t failed =
        rtuRequestsFailed;

    const uint32_t verifyFails =
        rtuVerifyFails;

    const uint32_t crcErrors =
        rtu.crcErrors;

    const uint32_t masterTimeouts =
        rtu.masterTimeouts;

    const uint32_t peripheralFailures =
        peripheralFailureCount();

    uint32_t rtuDurationMs =
        rtuTrafficDurationMs;

    if (rtuTrafficEnabled)
    {
        rtuDurationMs =
            (uint32_t)(
                millis() -
                rtuTrafficStartMs);
    }

    Serial.print("FULL_RUNTIME_READY=");
    Serial.println(yesNo(fullReady));

    Serial.print("COMBINED_RUNTIME_READY=");
    Serial.println(yesNo(combinedReady));

    Serial.print("SERVER_READY=");
    Serial.println(yesNo(serverReady));

    Serial.print("ETH_READY=");
    Serial.println(yesNo(ethReady));

    Serial.print("ETH_LINK=");
    Serial.println(ethLink ? "UP" : "DOWN");

    Serial.print("ETH_IP=");
    Serial.println(JWPLC_Ethernet.localIP());

    Serial.print("SD_READY=");
    Serial.println(yesNo(sdNowReady));

    Serial.print("DISPLAY_READY=");
    Serial.println(yesNo(displayReady));

    Serial.println(
        "DISPLAY_RENDER_MODE=HMI_ON_DEMAND_DIRTY");

    Serial.println(
        "DISPLAY_REFRESH_MODE=USER_REFRESH_ON_DEMAND");

    Serial.print("RTU_READY=");
    Serial.println(yesNo(rtuReady));

    Serial.println("RTU_ROLE=MASTER");

    Serial.print("RTU_TARGET_SLAVE_ID=");
    Serial.println(RTU_TARGET_SLAVE_ID);

    Serial.print("RTU_TRAFFIC_ENABLED=");
    Serial.println(yesNo(rtuTrafficEnabled));

    Serial.print("RTU_TRAFFIC_DURATION_MS=");
    Serial.println(rtuDurationMs);

    Serial.print("RTU_TIMEOUT_MS=");
    Serial.println(RTU_TIMEOUT_MS);

    Serial.print("RTU_REQUESTS_STARTED=");
    Serial.println(rtuRequestsStarted);

    Serial.print("RTU_REQUESTS_REJECTED=");
    Serial.println(rtuRequestsRejected);

    Serial.print("RTU_REQUESTS_COMPLETED=");
    Serial.println(rtuRequestsCompleted);

    Serial.print("RTU_REQUESTS_SUCCESS=");
    Serial.println(success);

    Serial.print("RTU_REQUESTS_FAILED=");
    Serial.println(failed);

    Serial.print("RTU_PERIODS_SKIPPED=");
    Serial.println(rtuPeriodsSkipped);

    Serial.print("RTU_VERIFY_FAILS=");
    Serial.println(verifyFails);

    Serial.print("RTU_CRC_ERRORS=");
    Serial.println(crcErrors);

    Serial.print("RTU_MASTER_TIMEOUTS=");
    Serial.println(masterTimeouts);

    Serial.print("RTU_SERVICE_GAP_MAX_US=");
    Serial.println(rtuServiceGapMaxUs);

    Serial.print("LOOP_GAP_MAX_US=");
    Serial.println(loopGapMaxUs);

    Serial.print("FRAM_MAX_US=");
    Serial.println(runtimeStats.framMaxUs);

    Serial.print("SD_APPEND_MAX_US=");
    Serial.println(runtimeStats.sdAppendMaxUs);

    Serial.print("SD_VERIFY_MAX_US=");
    Serial.println(runtimeStats.sdVerifyMaxUs);

    Serial.print("DISPLAY_GAP_MAX_MS=");
    Serial.println(displayServiceGapMaxMs);

    Serial.print("PERIPHERAL_FAILURE_COUNT=");
    Serial.println(peripheralFailures);

    Serial.println("A14_P5_PREFLIGHT=END");
}

// ============================================================================
// Serial
// ============================================================================

static void serviceSerialCommands()
{
    while (Serial.available() > 0)
    {
        const char c =
            (char)Serial.read();

        if (
            c == 'R' ||
            c == 'r')
        {
            const bool wasEnabled =
                rtuTrafficEnabled;

            stopRtuTraffic();
            resetPerfCounters();

            if (wasEnabled)
            {
                startRtuTraffic();
            }

            Serial.println(
                "A14_PERF_RESET=PASS");
        }
        else if (
            c == 'G' ||
            c == 'g')
        {
            startRtuTraffic();

            Serial.println(
                rtuTrafficEnabled
                    ? "RTU_MASTER_TRAFFIC=ON"
                    : "RTU_MASTER_TRAFFIC=FAIL");
        }
        else if (
            c == 'X' ||
            c == 'x')
        {
            stopRtuTraffic();

            Serial.println(
                "RTU_MASTER_TRAFFIC=OFF");
        }
        else if (
            c == 'P' ||
            c == 'p')
        {
            printP5Preflight();
        }
        else if (
            c == 'S' ||
            c == 's')
        {
            printSnapshot();
        }
    }
}

// ============================================================================
// Setup
// ============================================================================

void setup()
{
    Serial.begin(115200);

    // --------------------------------------------------------
    // Modbus data
    // --------------------------------------------------------

    for (
        uint16_t i = 0;
        i < sizeof(coils);
        ++i)
    {
        coils[i] =
            (uint8_t)(
                0xA5U ^
                (uint8_t)i);
    }

    for (
        uint16_t i = 0;
        i < HOLDING_COUNT;
        ++i)
    {
        holdingRegisters[i] =
            (uint16_t)(
                0x1000U +
                i);
    }

    JWPLC_ModbusTCP.setCoils(
        coils,
        COIL_COUNT);

    JWPLC_ModbusTCP.setHoldingRegisters(
        holdingRegisters,
        HOLDING_COUNT);

    // --------------------------------------------------------
    // Modbus RTU Master
    // --------------------------------------------------------

    rtuReady =
        JWPLC_ModbusRTU.begin(
            RTU_MASTER_LOCAL_ID,
            RTU_BAUD,
            RTU_CONFIG);

    if (rtuReady)
    {
        JWPLC_ModbusRTU.setFrameGapMs(2);
        resetRtuTrafficCounters();
    }

    // --------------------------------------------------------
    // FRAM no destructiva
    // --------------------------------------------------------

    const uint32_t framSize =
        JWPLC_FRAM.size();

    framReady =
        framSize >=
        (FRAM_BENCH_BYTES + 32U);

    if (framReady)
    {
        framBenchAddress =
            framSize -
            64U;

        framReady =
            JWPLC_FRAM.read(
                framBenchAddress,
                framBackup,
                sizeof(framBackup));
    }

    // --------------------------------------------------------
    // microSD
    // --------------------------------------------------------

    sdReady =
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady();

    if (
        sdReady &&
        JWPLC_SD.exists(
            SD_BENCH_PATH))
    {
        sdReady =
            JWPLC_SD.remove(
                SD_BENCH_PATH);
    }

    if (sdReady)
    {
        sdAppendFile =
            JWPLC_SD.open(
                SD_BENCH_PATH,
                FILE_APPEND);

        sdReady =
            (bool)sdAppendFile;

        sdRecordsSinceFlush = 0;
    }

    // --------------------------------------------------------
    // TFT / HMI Alpha11
    // --------------------------------------------------------

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_DISABLED);

    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_DISABLED);

    JWPLC_Display.setUserRefreshMode(
        USER_REFRESH_ON_DEMAND);

    JWPLC_Display.setUserRefreshPeriodMs(
        DISPLAY_PERIOD_MS);

    if (!JWPLC_Display.setFields(
            MASTER_FIELDS,
            sizeof(MASTER_FIELDS) /
                sizeof(MASTER_FIELDS[0])))
    {
        Serial.println(
            "A14_P5_MASTER_HMI_FIELDS=FAIL");
    }
    else
    {
        JWPLC_Display.setText(
            FIELD_ROLE,
            "MASTER");

        JWPLC_Display.setValue(
            FIELD_TCP_OK,
            0);

        JWPLC_Display.setValue(
            FIELD_RTU_OK,
            0);

        JWPLC_Display.setValue(
            FIELD_RTU_FAIL,
            0);

        JWPLC_Display.setBool(
            FIELD_SD_READY,
            sdReady);

        JWPLC_Display.setBool(
            FIELD_ETH_READY,
            JWPLC_Ethernet.isReady() &&
            JWPLC_Ethernet.linkUp());

        JWPLC_Display.enterUserUI();

        Serial.println(
            "A14_P5_MASTER_HMI_FIELDS=PASS");
    }

    // --------------------------------------------------------
    // Modbus TCP Server
    // --------------------------------------------------------

    if (
        !JWPLC_ModbusTCP.beginServer(
            UNIT_ID,
            SERVER_PORT))
    {
        Serial.println(
            "A14_PERF_SERVER_CONFIG=FAIL");

        return;
    }

    Serial.println(
        "A14_PERF_SERVER_CONFIG=PASS");

    Serial.println(
        "FULL_RUNTIME_PROFILE=REALISTIC");

    Serial.print(
        "DISPLAY_READY_BOOT=");
    Serial.println(
        yesNo(
            JWPLC_Display.isReady()));

    Serial.print(
        "FRAM_READY_BOOT=");
    Serial.println(
        yesNo(
            framReady));

    Serial.print(
        "SD_READY_BOOT=");
    Serial.println(
        yesNo(
            sdReady));

    Serial.print(
        "RTC_READY_BOOT=");
    Serial.println(
        yesNo(
            rtcReady()));

    Serial.print(
        "BUTTONS_READY_BOOT=");
    Serial.println(
        yesNo(
            JWPLCButtons::isReady()));

    Serial.print(
        "IO_READY_BOOT=");
    Serial.println(
        yesNo(
            ioReady()));

    Serial.print(
        "RTU_READY_BOOT=");
    Serial.println(
        yesNo(
            rtuReady));

    Serial.println(
        "DISPLAY_RENDER_MODE_BOOT=HMI_ON_DEMAND_DIRTY");

    resetPerfCounters();

    // P5 final: el Master RTU queda activo desde boot.
    // El runner vuelve a alinear los contadores con R justo antes
    // de la ventana formal TCP; R conserva este estado activo.
    startRtuTraffic();

    Serial.print("RTU_TRAFFIC_AUTO_START=");
    Serial.println(
        rtuTrafficEnabled
            ? "YES"
            : "NO");
}

// ============================================================================
// Loop
// ============================================================================

void loop()
{
    const uint32_t nowUs =
        micros();

    if (lastLoopUs != 0)
    {
        const uint32_t gap =
            (uint32_t)(
                nowUs -
                lastLoopUs);

        loopGapSumUs += gap;
        ++loopGapSamples;

        if (gap > loopGapMaxUs)
        {
            loopGapMaxUs = gap;
        }
    }

    lastLoopUs = nowUs;

    // Comunicaciones reciben prioridad en cada vuelta.
    JWPLC_ModbusTCP.task();
    serviceRtuMaster();

    serviceSerialCommands();

    serviceRealisticWorkload();
    serviceDisplayTelemetry();

    if (
        !readyAnnounced &&
        JWPLC_ModbusTCP.serverReady())
    {
        readyAnnounced = true;

        Serial.print(
            "A14_PERF_SERVER_READY=PASS IP=");

        Serial.print(
            JWPLC_Ethernet.localIP());

        Serial.print(" PORT=");
        Serial.print(SERVER_PORT);

        Serial.print(" UNIT_ID=");
        Serial.println(UNIT_ID);

        Serial.print(
            "FULL_RUNTIME_READY=");

        Serial.println(
            yesNo(
                fullRuntimeReady()));

        Serial.println(
            "A14_P5_ROLE=MASTER");
    }
}
