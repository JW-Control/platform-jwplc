/*
  A14.3 PERF-S2 - FULL_RUNTIME_REALISTIC

  Objetivo:
  Comparar la referencia TCP-only contra un JWPLC Basic operando
  simultaneamente con sus perifericos normales.

  Perfil:
  - Modbus TCP Server FC03 / 125 registros.
  - TFT USER con contenido dinamico cada 100 ms.
  - FRAM write/read/verify/restore cada 250 ms.
  - microSD append cada 1 s.
  - microSD read/verify cada 5 s.
  - RTC mediante runtime normal + freshness.
  - TCA/I/O mediante runtime normal + freshness.
  - botonera mediante task normal + muestreo.
  - probe del mutex SPI cada 100 ms.
  - sin conmutacion ciclica de reles.

  Serial:
    R -> reset de estadisticas
    S -> snapshot
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

static uint8_t lastSdRecord[SD_RECORD_BYTES];
static bool lastSdRecordValid = false;
static uint32_t sdSequence = 0;

// ============================================================================
// Display
// ============================================================================

static volatile uint32_t displayFramesTotal = 0;
static volatile uint32_t displayFramesBaseline = 0;
static volatile uint32_t displayLastFrameMs = 0;
static volatile uint32_t displayGapMaxMs = 0;
static volatile uint32_t displayPhase = 0;

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
// Display USER
// ============================================================================

extern "C" bool jwplcUserDisplayRefreshNeededCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    return true;
}

extern "C" void jwplcUserDisplayEnterCallback(void)
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextWrap(false);
    tft.setTextColor(
        ST77XX_CYAN,
        ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setCursor(8, 8);
    tft.print("A14 PERF-S2");

    tft.setTextSize(1);
    tft.setTextColor(
        ST77XX_WHITE,
        ST77XX_BLACK);

    tft.setCursor(8, 34);
    tft.print("FULL_RUNTIME_REALISTIC");

    tft.drawRect(
        7,
        48,
        150,
        91,
        ST77XX_BLUE);

    tft.setCursor(8, 148);
    tft.print("TCP + TFT + FRAM + SD");

    tft.setCursor(8, 160);
    tft.print("RTC + BTN + TCA/I-O");

    displayLastFrameMs = millis();
}

extern "C" void jwplcUserDisplayRefreshCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
    const uint32_t now = millis();

    if (displayLastFrameMs != 0)
    {
        const uint32_t gap =
            (uint32_t)(
                now -
                displayLastFrameMs);

        if (gap > displayGapMaxMs)
        {
            displayGapMaxMs = gap;
        }
    }

    displayLastFrameMs = now;
    displayFramesTotal = displayFramesTotal + 1U;
    displayPhase = displayPhase + 1U;

    auto &tft = JWPLC_Display.tft();

    // --------------------------------------------------------
    // Linea 1: frame
    // --------------------------------------------------------

    tft.fillRect(
        10,
        54,
        142,
        16,
        ST77XX_BLACK);

    tft.setCursor(10, 58);
    tft.setTextSize(1);
    tft.setTextColor(
        ST77XX_WHITE,
        ST77XX_BLACK);

    tft.print("FRAME ");
    tft.print(
        (unsigned long)
        displayFramesTotal);

    // --------------------------------------------------------
    // Linea 2: I/O
    // --------------------------------------------------------

    tft.fillRect(
        10,
        75,
        142,
        16,
        ST77XX_BLACK);

    tft.setCursor(10, 79);

    tft.print("I=");
    tft.print(
        io != nullptr
            ? io->di_logical_bank0
            : 0,
        HEX);

    tft.print(" O=");
    tft.print(
        io != nullptr
            ? io->do_bank1
            : 0,
        HEX);

    // --------------------------------------------------------
    // Linea 3: RTC
    // --------------------------------------------------------

    tft.fillRect(
        10,
        96,
        142,
        16,
        ST77XX_BLACK);

    tft.setCursor(10, 100);

    if (
        rtc != nullptr &&
        rtc->present
    )
    {
        if (rtc->hour < 10)
            tft.print('0');

        tft.print(rtc->hour);
        tft.print(':');

        if (rtc->minute < 10)
            tft.print('0');

        tft.print(rtc->minute);
        tft.print(':');

        if (rtc->second < 10)
            tft.print('0');

        tft.print(rtc->second);
    }
    else
    {
        tft.print("RTC ---");
    }

    // --------------------------------------------------------
    // Barra dinamica
    // --------------------------------------------------------

    tft.fillRect(
        10,
        120,
        140,
        8,
        ST77XX_BLACK);

    const uint16_t width =
        (uint16_t)(
            displayPhase %
            141U);

    if (width > 0)
    {
        tft.fillRect(
            10,
            120,
            width,
            8,
            ST77XX_GREEN);
    }
}

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
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady();

    if (ok)
    {
        JWPLCFile file =
            JWPLC_SD.open(
                SD_BENCH_PATH,
                FILE_APPEND);

        if (!file)
        {
            ok = false;
        }
        else
        {
            const size_t written =
                file.write(
                    record,
                    sizeof(record));

            file.flush();
            file.close();

            ok =
                written ==
                sizeof(record);
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
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady();

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

    const uint32_t age =
        (uint32_t)(
            now -
            io->last_scan_ms);

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

    const uint32_t age =
        (uint32_t)(
            now -
            rtc->last_update_ms);

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
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady() &&
        JWPLCButtons::isReady() &&
        ioReady() &&
        rtcReady();
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

    loopGapSumUs = 0;
    loopGapSamples = 0;
    loopGapMaxUs = 0;
    lastLoopUs = micros();

    runtimeStats = RuntimeStats{};

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

    displayFramesBaseline =
        displayFramesTotal;

    displayGapMaxMs = 0;
    displayLastFrameMs = now;
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

    const uint32_t displayFrames =
        (uint32_t)(
            displayFramesTotal -
            displayFramesBaseline);

    const JWPLC_IOState *io =
        jwplcGetIOState();

    const JWPLC_RTCState *rtc =
        jwplcGetRTCState();

    const uint32_t now =
        millis();

    const uint32_t ioAgeMs =
        (
            io != nullptr &&
            io->initialized
        )
            ? (uint32_t)(
                  now -
                  io->last_scan_ms)
            : 0xFFFFFFFFUL;

    const uint32_t rtcAgeMs =
        (
            rtc != nullptr &&
            rtc->present
        )
            ? (uint32_t)(
                  now -
                  rtc->last_update_ms)
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
    // TFT
    // --------------------------------------------------------

    Serial.print("DISPLAY_READY=");
    Serial.println(
        yesNo(
            JWPLC_Display.isReady()));

    Serial.print("DISPLAY_FRAMES=");
    Serial.println(displayFrames);

    Serial.print("DISPLAY_GAP_MAX_MS=");
    Serial.println(
        (uint32_t)
        displayGapMaxMs);

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
            resetPerfCounters();

            Serial.println(
                "A14_PERF_RESET=PASS");
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

    // --------------------------------------------------------
    // TFT
    // --------------------------------------------------------

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_DISABLED);

    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_DISABLED);

    JWPLC_Display.setUserRefreshPeriodMs(
        DISPLAY_PERIOD_MS);

    JWPLC_Display.enterUserUI();

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

    resetPerfCounters();
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

    // Modbus recibe prioridad en cada vuelta.
    JWPLC_ModbusTCP.task();

    serviceSerialCommands();

    serviceRealisticWorkload();

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
    }
}
