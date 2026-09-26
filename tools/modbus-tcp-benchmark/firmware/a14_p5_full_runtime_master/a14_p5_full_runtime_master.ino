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
// P5-CAP2: mapa ampliado sólo para benchmark de bloques lógicos.
static constexpr uint16_t HOLDING_COUNT = 1000;

static uint8_t coils[(COIL_COUNT + 7U) / 8U];
static uint16_t holdingRegisters[HOLDING_COUNT];

// ============================================================================
// Modbus RTU Master cooperativo
// ============================================================================

static constexpr uint8_t RTU_MASTER_LOCAL_ID = 247;
static constexpr uint8_t RTU_TARGET_SLAVE_ID = 2;
static constexpr uint32_t RTU_BAUD = 115200UL;
static constexpr uint32_t RTU_CONFIG = SERIAL_8N1;
static constexpr uint32_t RTU_PERIOD_DEFAULT_US = 20000UL;
static constexpr uint32_t RTU_TIMEOUT_MS = 25UL;
static constexpr uint16_t RTU_VERIFY_MAGIC = 0x55AA;

static bool rtuReady = false;
static bool rtuTrafficEnabled = false;
static bool rtuUnpaced = false;
static uint8_t rtuRxFifoFull = 120U;
static uint32_t rtuTargetHz = 50UL;
static uint32_t rtuPeriodUs = RTU_PERIOD_DEFAULT_US;
static uint16_t rtuReadValues[2] = {0, 0};

static uint32_t rtuTrafficStartMs = 0;
static uint32_t rtuTrafficDurationMs = 0;
static uint32_t rtuNextRequestUs = 0;

static uint32_t rtuRequestsStarted = 0;
static uint32_t rtuRequestsRejected = 0;
static uint32_t rtuRequestsCompleted = 0;
static uint32_t rtuRequestsSuccess = 0;
static uint32_t rtuRequestsFailed = 0;
static uint32_t rtuVerifyFails = 0;
static uint32_t rtuPeriodsSkipped = 0;

static uint32_t rtuLastServiceUs = 0;
static uint32_t rtuServiceGapMaxUs = 0;

// H3B.1 qualification-only transaction latency telemetry.
static uint32_t rtuRequestStartUs = 0;
static uint32_t rtuTransactionMaxUs = 0;
static uint32_t rtuTransactionsOver5ms = 0;
static uint32_t rtuTransactionsOver10ms = 0;
static uint32_t rtuTransactionsOver20ms = 0;
static uint32_t rtuLastFailureDurationUs = 0;
static uint32_t rtuMaxFailureDurationUs = 0;
static int32_t rtuLastFailureResult = -1;

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
static constexpr size_t SD_DATALOG_BUFFER_BYTES = 4096U;
static constexpr size_t SD_DATALOG_COMMIT_THRESHOLD_BYTES = 512U;
static constexpr uint32_t SD_DATALOG_COMMIT_TIMEOUT_MS = 5000UL;

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
static JWPLCDataLog sdDataLog;

static uint32_t sdSequence = 0;
static uint32_t sdCommitCountBaseline = 0;
static uint32_t sdFailedCommitBaseline = 0;
static uint64_t sdAcceptedBytesBaseline = 0;
static uint64_t sdCommittedBytesBaseline = 0;

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
        sdDataLog.isActive() &&
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady();

    if (ok)
    {
        const size_t written =
            sdDataLog.write(
                record,
                sizeof(record));

        ok =
            written ==
            sizeof(record);
    }

    if (!ok)
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
    ++runtimeStats.sdVerifyCycles;

    const uint32_t t0 =
        micros();

    const JW_SDDataLogStatus status =
        sdDataLog.status();

    const uint32_t commitCount =
        (
            status.commitCount >=
            sdCommitCountBaseline
        )
            ? (
                status.commitCount -
                sdCommitCountBaseline
              )
            : 0U;

    const uint32_t failedCommits =
        (
            status.failedCommits >=
            sdFailedCommitBaseline
        )
            ? (
                status.failedCommits -
                sdFailedCommitBaseline
              )
            : status.failedCommits;

    runtimeStats.sdFlushCycles =
        commitCount;

    const bool ok =
        sdReady &&
        status.active &&
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady() &&
        failedCommits == 0 &&
        status.lastError == JW_SD_OK;

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

    rtuRequestStartUs = 0;
    rtuTransactionMaxUs = 0;
    rtuTransactionsOver5ms = 0;
    rtuTransactionsOver10ms = 0;
    rtuTransactionsOver20ms = 0;
    rtuLastFailureDurationUs = 0;
    rtuMaxFailureDurationUs = 0;
    rtuLastFailureResult = -1;
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
    rtuNextRequestUs = micros();
}

static void setRtuTargetHz(uint32_t hz)
{
    if (hz == 0)
    {
        return;
    }

    rtuUnpaced = false;
    rtuTargetHz = hz;
    rtuPeriodUs = 1000000UL / hz;
    rtuNextRequestUs = micros();
}

static void setRtuUnpaced()
{
    rtuUnpaced = true;
    rtuTargetHz = 0;
    rtuPeriodUs = 0;
    rtuNextRequestUs = micros();
}

static void setRtuFrameGapUs(uint32_t gapUs)
{
    JWPLC_ModbusRTU.setFrameGapUs(gapUs);
}

static bool setRtuRxFifoFull(uint8_t fifoBytes)
{
    if (!rtuReady || fifoBytes == 0)
    {
        return false;
    }

    if (!JWPLC_RS485.serial().setRxFIFOFull(fifoBytes))
    {
        return false;
    }

    rtuRxFifoFull = fifoBytes;
    return true;
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

static bool setRtuBaud(uint32_t baud)
{
    stopRtuTraffic();

    JWPLC_ModbusRTU.end();

    rtuReady =
        JWPLC_ModbusRTU.begin(
            RTU_MASTER_LOCAL_ID,
            baud,
            RTU_CONFIG);

    if (!rtuReady)
    {
        return false;
    }

    if (!JWPLC_ModbusRTU.motor(ASYNC))
    {
        rtuReady = false;
        return false;
    }

    JWPLC_ModbusRTU.setFrameGapUs(500UL);
    rtuRxFifoFull = 120U;
    resetRtuTrafficCounters();

    return true;
}

static bool setRtu230400ApbForced()
{
    stopRtuTraffic();

    JWPLC_ModbusRTU.end();

    if (!JWPLC_RS485.serial().setClockSource(UART_CLK_SRC_APB))
    {
        rtuReady = false;
        return false;
    }

    rtuReady =
        JWPLC_ModbusRTU.begin(
            RTU_MASTER_LOCAL_ID,
            230400UL,
            RTU_CONFIG);

    if (!rtuReady)
    {
        return false;
    }

    if (!JWPLC_ModbusRTU.motor(ASYNC))
    {
        rtuReady = false;
        return false;
    }

    JWPLC_ModbusRTU.setFrameGapUs(500UL);
    resetRtuTrafficCounters();

    return true;
}

static void completeRtuMasterResult()
{
    if (!JWPLC_ModbusRTU.masterDone())
    {
        return;
    }

    const uint32_t durationUs =
        rtuRequestStartUs == 0
            ? 0U
            : (uint32_t)(micros() - rtuRequestStartUs);

    updateMaxU32(
        durationUs,
        rtuTransactionMaxUs);

    if (durationUs > 5000UL)
    {
        ++rtuTransactionsOver5ms;
    }
    if (durationUs > 10000UL)
    {
        ++rtuTransactionsOver10ms;
    }
    if (durationUs > 20000UL)
    {
        ++rtuTransactionsOver20ms;
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

        rtuLastFailureDurationUs = durationUs;
        updateMaxU32(
            durationUs,
            rtuMaxFailureDurationUs);
        rtuLastFailureResult =
            (int32_t)JWPLC_ModbusRTU.masterResult();
    }

    rtuRequestStartUs = 0;
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

    if (JWPLC_ModbusRTU.masterBusy())
    {
        return;
    }

    if (!rtuUnpaced)
    {
        if ((int32_t)(nowUs - rtuNextRequestUs) < 0)
        {
            return;
        }

        uint32_t periodsAdvanced = 0;

        do
        {
            rtuNextRequestUs += rtuPeriodUs;
            ++periodsAdvanced;
        }
        while ((int32_t)(nowUs - rtuNextRequestUs) >= 0);

        if (periodsAdvanced > 1)
        {
            rtuPeriodsSkipped += periodsAdvanced - 1;
        }
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
        rtuRequestStartUs = micros();
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
        sdDataLog.isActive() &&
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
    // Drenar cualquier dato pendiente antes de capturar los baselines
    // evita atribuir a la nueva ventana commits de la preparación.
    if (sdDataLog.isActive())
    {
        (void)sdDataLog.commit();
    }

    const JW_SDDataLogStatus sdStatus =
        sdDataLog.status();

    sdCommitCountBaseline =
        sdStatus.commitCount;

    sdFailedCommitBaseline =
        sdStatus.failedCommits;

    sdAcceptedBytesBaseline =
        sdStatus.acceptedBytes;

    sdCommittedBytesBaseline =
        sdStatus.committedBytes;

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
    Serial.println(
        JWPLC_ModbusRTU.baudRate());

    Serial.print("RTU_BAUD_EFFECTIVE=");
    Serial.println(
        JWPLC_ModbusRTU.effectiveBaudRate());

    Serial.print("RTU_CLOCK_PROFILE=");
    Serial.println(JWPLC_RS485.clockSourceString());

    Serial.print("RTU_RX_FIFO_FULL=");
    Serial.println(rtuRxFifoFull);

    Serial.print("RTU_RX_MODE=");
    Serial.println(
        JWPLC_ModbusRTU.bulkRxEnabled()
            ? "BULK"
            : "BYTE");

    Serial.print("RTU_SERVER_FRAMING=");
    Serial.println(
        JWPLC_ModbusRTU.earlyServerDispatchEnabled()
            ? "STRUCTURAL"
            : "GAP");

    Serial.print("RTU_MOTOR=");
    Serial.println(
        JWPLC_ModbusRTU.motor() == ASYNC
            ? "ASYNC"
            : "SYNC");

    Serial.print("RTU_TIMEOUT_MS=");
    Serial.println(RTU_TIMEOUT_MS);

    Serial.print("RTU_FRAME_GAP_US=");
    Serial.println(JWPLC_ModbusRTU.frameGapUs());

    Serial.print("RTU_TX_MODE=");
    Serial.println(
        JWPLC_ModbusRTU.queuedTxActive()
            ? "QUEUED"
            : "BLOCKING");

    Serial.print("RTU_TX_QUEUED_REQUESTED=");
    Serial.println(
        yesNo(
            JWPLC_ModbusRTU.queuedTxEnabled()));

    Serial.print("RTU_TX_QUEUED_ACTIVE=");
    Serial.println(
        yesNo(
            JWPLC_ModbusRTU.queuedTxActive()));

    Serial.print("RS485_AUTO_DIRECTION=");
    Serial.println(
        yesNo(
            JWPLC_RS485.autoDirection()));

    Serial.print("RS485_TX_BUFFER_BYTES=");
    Serial.println(
        (unsigned long)
            JWPLC_RS485.txBufferSize());

    Serial.print("RS485_QUEUED_TX_SUPPORTED=");
    Serial.println(
        yesNo(
            JWPLC_RS485.queuedWriteSupported()));

    Serial.print("RTU_RATE_MODE=");
    Serial.println(rtuUnpaced ? "UNPACED" : "PACED");

    Serial.print("RTU_TARGET_HZ=");
    Serial.println(rtuTargetHz);

    Serial.print("RTU_PERIOD_US=");
    Serial.println(rtuPeriodUs);

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

    Serial.print("RTU_RX_BYTES=");
    Serial.println((unsigned long long)rtu.rxBytes);

    Serial.print("RTU_TX_BYTES=");
    Serial.println((unsigned long long)rtu.txBytes);

    Serial.print("RTU_SERVER_DISCARDED_TAILS=");
    Serial.println(rtu.serverDiscardedTails);

    Serial.print("RTU_SERVER_DISCARDED_BYTES=");

    Serial.print("RTU_SERVER_DISCARDED_LAST_LENGTH=");
    Serial.println(rtu.serverDiscardedLastLength);

    Serial.print("RTU_SERVER_DISCARDED_LAST_AGE_US=");
    Serial.println(rtu.serverDiscardedLastAgeUs);

    Serial.print("RTU_SERVER_DISCARDED_MAX_AGE_US=");
    Serial.println(rtu.serverDiscardedMaxAgeUs);

    Serial.print("RTU_SERVER_DISCARDED_LEN1=");
    Serial.println(rtu.serverDiscardedLen1);
    Serial.print("RTU_SERVER_DISCARDED_LEN2=");
    Serial.println(rtu.serverDiscardedLen2);
    Serial.print("RTU_SERVER_DISCARDED_LEN3=");
    Serial.println(rtu.serverDiscardedLen3);
    Serial.print("RTU_SERVER_DISCARDED_LEN4=");
    Serial.println(rtu.serverDiscardedLen4);
    Serial.print("RTU_SERVER_DISCARDED_LEN5=");
    Serial.println(rtu.serverDiscardedLen5);
    Serial.print("RTU_SERVER_DISCARDED_LEN6=");
    Serial.println(rtu.serverDiscardedLen6);
    Serial.print("RTU_SERVER_DISCARDED_LEN7=");
    Serial.println(rtu.serverDiscardedLen7);
    Serial.print("RTU_SERVER_DISCARDED_LEN8=");
    Serial.println(rtu.serverDiscardedLen8);
    Serial.print("RTU_SERVER_DISCARDED_LEN_GT8=");
    Serial.println(rtu.serverDiscardedLenGt8);
    Serial.println((unsigned long long)rtu.serverDiscardedBytes);

    Serial.print("RTU_LAST_ERROR=");
    Serial.println(JWPLC_ModbusRTU.lastErrorString());

    Serial.print("RTU_SERVICE_GAP_MAX_US=");
    Serial.println(rtuServiceGapMaxUs);

    Serial.print("RTU_TRANSACTION_MAX_US=");
    Serial.println(rtuTransactionMaxUs);

    Serial.print("RTU_TRANSACTIONS_OVER_5MS=");
    Serial.println(rtuTransactionsOver5ms);

    Serial.print("RTU_TRANSACTIONS_OVER_10MS=");
    Serial.println(rtuTransactionsOver10ms);

    Serial.print("RTU_TRANSACTIONS_OVER_20MS=");
    Serial.println(rtuTransactionsOver20ms);

    Serial.print("RTU_LAST_FAILURE_DURATION_US=");
    Serial.println(rtuLastFailureDurationUs);

    Serial.print("RTU_MAX_FAILURE_DURATION_US=");
    Serial.println(rtuMaxFailureDurationUs);

    Serial.print("RTU_LAST_FAILURE_RESULT=");
    Serial.println(rtuLastFailureResult);

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

    const JW_SDDataLogStatus sdStatus =
        sdDataLog.status();

    const uint32_t sdCommitCycles =
        (
            sdStatus.commitCount >=
            sdCommitCountBaseline
        )
            ? (
                sdStatus.commitCount -
                sdCommitCountBaseline
              )
            : 0U;

    const uint32_t sdFailedCommits =
        (
            sdStatus.failedCommits >=
            sdFailedCommitBaseline
        )
            ? (
                sdStatus.failedCommits -
                sdFailedCommitBaseline
              )
            : sdStatus.failedCommits;

    const uint64_t sdAcceptedBytes =
        (
            sdStatus.acceptedBytes >=
            sdAcceptedBytesBaseline
        )
            ? (
                sdStatus.acceptedBytes -
                sdAcceptedBytesBaseline
              )
            : 0ULL;

    const uint64_t sdCommittedBytes =
        (
            sdStatus.committedBytes >=
            sdCommittedBytesBaseline
        )
            ? (
                sdStatus.committedBytes -
                sdCommittedBytesBaseline
              )
            : 0ULL;

    runtimeStats.sdFlushCycles =
        sdCommitCycles;

    Serial.print("SD_READY=");
    Serial.println(
        yesNo(
            JWPLCSD::isEnabled() &&
            JWPLCSD::isCardPresent() &&
            JWPLCSD::isReady() &&
            sdStatus.active));

    Serial.println(
        "SD_WORKLOAD_MODE=BUFFERED_DATALOG");

    Serial.print("SD_APPEND_CYCLES=");
    Serial.println(
        runtimeStats.sdAppendCycles);

    Serial.print("SD_APPEND_FAILS=");
    Serial.println(
        runtimeStats.sdAppendFails);

    Serial.print("SD_APPEND_MAX_US=");
    Serial.println(
        runtimeStats.sdAppendMaxUs);

    Serial.print("SD_DATALOG_ACTIVE=");
    Serial.println(
        yesNo(sdStatus.active));

    Serial.print("SD_DATALOG_BUFFER_BYTES=");
    Serial.println(
        sdStatus.capacityBytes);

    Serial.print("SD_DATALOG_PENDING_BYTES=");
    Serial.println(
        sdStatus.pendingBytes);

    Serial.print("SD_DATALOG_COMMIT_THRESHOLD_BYTES=");
    Serial.println(
        sdStatus.commitThresholdBytes);

    Serial.print("SD_DATALOG_COMMIT_TIMEOUT_MS=");
    Serial.println(
        sdStatus.commitTimeoutMs);

    Serial.print("SD_DATALOG_ACCEPTED_BYTES=");
    Serial.println(
        (unsigned long long)sdAcceptedBytes);

    Serial.print("SD_DATALOG_COMMITTED_BYTES=");
    Serial.println(
        (unsigned long long)sdCommittedBytes);

    Serial.print("SD_DATALOG_FAILED_COMMITS=");
    Serial.println(
        sdFailedCommits);

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
    Serial.println(
        yesNo(
            sdNowReady &&
            sdDataLog.isActive()));

    Serial.println(
        "SD_WORKLOAD_MODE=BUFFERED_DATALOG");

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

    Serial.print("RTU_TX_MODE=");
    Serial.println(
        JWPLC_ModbusRTU.queuedTxActive()
            ? "QUEUED"
            : "BLOCKING");

    Serial.print("RS485_AUTO_DIRECTION=");
    Serial.println(
        yesNo(
            JWPLC_RS485.autoDirection()));

    Serial.print("RS485_TX_BUFFER_BYTES=");
    Serial.println(
        (unsigned long)
            JWPLC_RS485.txBufferSize());

    Serial.print("RS485_QUEUED_TX_SUPPORTED=");
    Serial.println(
        yesNo(
            JWPLC_RS485.queuedWriteSupported()));

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
        else if (c == 'A' || c == 'a')
        {
            setRtuTargetHz(50);
            Serial.println("RTU_RATE_HZ=50");
        }
        else if (c == 'B' || c == 'b')
        {
            setRtuTargetHz(100);
            Serial.println("RTU_RATE_HZ=100");
        }
        else if (c == 'C' || c == 'c')
        {
            setRtuTargetHz(150);
            Serial.println("RTU_RATE_HZ=150");
        }
        else if (c == 'D' || c == 'd')
        {
            setRtuTargetHz(200);
            Serial.println("RTU_RATE_HZ=200");
        }
        else if (c == 'E' || c == 'e')
        {
            setRtuTargetHz(250);
            Serial.println("RTU_RATE_HZ=250");
        }
        else if (c == 'F' || c == 'f')
        {
            setRtuTargetHz(300);
            Serial.println("RTU_RATE_HZ=300");
        }
        else if (c == 'U' || c == 'u')
        {
            setRtuUnpaced();
            Serial.println("RTU_RATE_MODE=UNPACED");
        }
        else if (c == 'H' || c == 'h')
        {
            setRtuFrameGapUs(2000UL);
            Serial.println("RTU_FRAME_GAP_US=2000");
        }
        else if (c == 'I' || c == 'i')
        {
            setRtuFrameGapUs(1750UL);
            Serial.println("RTU_FRAME_GAP_US=1750");
        }
        else if (c == 'J' || c == 'j')
        {
            setRtuFrameGapUs(1500UL);
            Serial.println("RTU_FRAME_GAP_US=1500");
        }
        else if (c == 'K' || c == 'k')
        {
            setRtuFrameGapUs(1250UL);
            Serial.println("RTU_FRAME_GAP_US=1250");
        }
        else if (c == 'L' || c == 'l')
        {
            setRtuFrameGapUs(1000UL);
            Serial.println("RTU_FRAME_GAP_US=1000");
        }
        else if (c == 'M' || c == 'm')
        {
            setRtuFrameGapUs(750UL);
            Serial.println("RTU_FRAME_GAP_US=750");
        }
        else if (c == 'N' || c == 'n')
        {
            setRtuFrameGapUs(600UL);
            Serial.println("RTU_FRAME_GAP_US=600");
        }
        else if (c == 'O' || c == 'o')
        {
            setRtuFrameGapUs(500UL);
            Serial.println("RTU_FRAME_GAP_US=500");
        }
        else if (c == 'Q' || c == 'q')
        {
            setRtuFrameGapUs(400UL);
            Serial.println("RTU_FRAME_GAP_US=400");
        }
        else if (c == 'V' || c == 'v')
        {
            setRtuFrameGapUs(350UL);
            Serial.println("RTU_FRAME_GAP_US=350");
        }
        else if (c == 'W' || c == 'w')
        {
            setRtuFrameGapUs(300UL);
            Serial.println("RTU_FRAME_GAP_US=300");
        }
        else if (c == '1')
        {
            setRtuFrameGapUs(200UL);
            Serial.println("RTU_FRAME_GAP_US=200");
        }
        else if (c == '3')
        {
            setRtuFrameGapUs(150UL);
            Serial.println("RTU_FRAME_GAP_US=150");
        }
        else if (c == '4')
        {
            setRtuFrameGapUs(100UL);
            Serial.println("RTU_FRAME_GAP_US=100");
        }
        else if (c == 'T' || c == 't')
        {
            setRtuFrameGapUs(75UL);
            Serial.println("RTU_FRAME_GAP_US=75");
        }
        else if (c == '!')
        {
            setRtuFrameGapUs(50UL);
            Serial.println("RTU_FRAME_GAP_US=50");
        }
        else if (c == 'Y' || c == 'y')
        {
            JWPLC_ModbusRTU.setQueuedTxEnabled(false);
            Serial.println("RTU_TX_MODE=BLOCKING");
        }
        else if (c == 'Z' || c == 'z')
        {
            JWPLC_ModbusRTU.setQueuedTxEnabled(true);
            Serial.println(
                JWPLC_ModbusRTU.queuedTxActive()
                    ? "RTU_TX_MODE=QUEUED"
                    : "RTU_TX_MODE=QUEUED_UNAVAILABLE");
        }
        else if (c == '0')
        {
            Serial.println(
                setRtuBaud(250000UL)
                    ? "RTU_BAUD_REQUESTED=250000"
                    : "RTU_BAUD_REQUESTED=FAIL");
        }
        else if (c == '6')
        {
            Serial.println(
                setRtuBaud(460800UL)
                    ? "RTU_BAUD_REQUESTED=460800"
                    : "RTU_BAUD_REQUESTED=FAIL");
        }
        else if (c == '7')
        {
            Serial.println(
                setRtuBaud(115200UL)
                    ? "RTU_BAUD_REQUESTED=115200"
                    : "RTU_BAUD_EFFECTIVE=FAIL");
        }
        else if (c == '8')
        {
            Serial.println(
                setRtuBaud(230400UL)
                    ? "RTU_BAUD_REQUESTED=230400"
                    : "RTU_BAUD_EFFECTIVE=FAIL");
        }
        else if (c == '@')
        {
            Serial.println(
                setRtu230400ApbForced()
                    ? "RTU_CLOCK_PROFILE=APB_FORCED"
                    : "RTU_CLOCK_PROFILE=FAIL");
        }
        else if (c == '9')
        {
            Serial.println(
                setRtuBaud(500000UL)
                    ? "RTU_BAUD_REQUESTED=500000"
                    : "RTU_BAUD_EFFECTIVE=FAIL");
        }
        else if (c == '[')
        {
            Serial.println(setRtuRxFifoFull(120U) ? "RTU_RX_FIFO_FULL=120" : "RTU_RX_FIFO_FULL=FAIL");
        }
        else if (c == ']')
        {
            Serial.println(setRtuRxFifoFull(32U) ? "RTU_RX_FIFO_FULL=32" : "RTU_RX_FIFO_FULL=FAIL");
        }
        else if (c == '{')
        {
            Serial.println(setRtuRxFifoFull(16U) ? "RTU_RX_FIFO_FULL=16" : "RTU_RX_FIFO_FULL=FAIL");
        }
        else if (c == '}')
        {
            Serial.println(setRtuRxFifoFull(8U) ? "RTU_RX_FIFO_FULL=8" : "RTU_RX_FIFO_FULL=FAIL");
        }
        else if (c == '?')
        {
            Serial.println(setRtuRxFifoFull(1U) ? "RTU_RX_FIFO_FULL=1" : "RTU_RX_FIFO_FULL=FAIL");
        }
        else if (c == '+')
        {
            JWPLC_ModbusRTU.setBulkRxEnabled(true);
            Serial.println("RTU_RX_MODE=BULK");
        }
        else if (c == '-')
        {
            JWPLC_ModbusRTU.setBulkRxEnabled(false);
            Serial.println("RTU_RX_MODE=BYTE");
        }
        else if (c == '(')
        {
            JWPLC_ModbusRTU.setEarlyServerDispatchEnabled(true);
            Serial.println("RTU_SERVER_FRAMING=STRUCTURAL");
        }
        else if (c == ')')
        {
            JWPLC_ModbusRTU.setEarlyServerDispatchEnabled(false);
            Serial.println("RTU_SERVER_FRAMING=GAP");
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
        rtuReady =
            JWPLC_ModbusRTU.motor(ASYNC);

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
        const JW_SDDataLogConfig config(
            SD_DATALOG_BUFFER_BYTES,
            SD_DATALOG_COMMIT_THRESHOLD_BYTES,
            SD_DATALOG_COMMIT_TIMEOUT_MS);

        sdReady =
            sdDataLog.begin(
                JWPLC_SD,
                SD_BENCH_PATH,
                config);
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

    // Modbus TCP se atiende automáticamente desde el package-core
    // antes y después de cada loop(). El sketch sólo mantiene su lógica RTU.
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
