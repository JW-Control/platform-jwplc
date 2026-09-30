/*
  A14 S2 - full runtime shared-bus soak.

  Same source, two builds:
    JWPLC_S2_ROLE_MASTER=1 -> DUT/Master, expected COM14.
    JWPLC_S2_ROLE_MASTER=0 -> Slave ID 2, expected COM4.

  Master:
    - W5500 TCP server with continuous bidirectional traffic.
    - RX verifies deterministic stream.
    - TX echoes RX using beginWriteAsync()/pollWriteAsync().
    - Modbus RTU Master FC15 -> FC01 -> FC02 against Slave ID 2.
    - FRAM scratch write/read/compare with backup/restore.
    - RTC read/temperature.
    - microSD temporary file write/read/remove.
    - TFT status updates.
    - buttons + digital input sampling.

  Slave:
    - Modbus RTU Slave ID 2.
    - logical coil map only: does NOT drive physical relay outputs.
    - FC02 exposes real JWPLC digital inputs.
    - FRAM/RTC/TFT/buttons/I/O remain active.

  Acceptance is intentionally strict:
    zero corruption, zero SPI lock errors, zero transport errors,
    zero Modbus failures/CRC/timeouts, zero peripheral failures, no resets.
*/

#include <Arduino.h>
#include <JWPLC_Ethernet.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_Display.h>
#include <esp_system.h>

#ifndef JWPLC_S2_ROLE_MASTER
#error "Define JWPLC_S2_ROLE_MASTER=1 for Master or 0 for Slave"
#endif

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t MODBUS_BAUD = 115200UL;
static constexpr uint32_t MODBUS_CONFIG = SERIAL_8N1;
static constexpr uint16_t MODBUS_FRAME_GAP_MS = 2;
static constexpr uint32_t MODBUS_TIMEOUT_MS = 250UL;
static constexpr uint8_t MASTER_LOCAL_ID = 247;
static constexpr uint8_t SLAVE_ID = 2;

static constexpr uint16_t TCP_PORT = 5002;
static constexpr size_t ETH_BUFFER_BYTES = 1024;
static constexpr uint32_t API_LIMIT_US = 10000UL;
static constexpr uint32_t ETH_LOCK_TIMEOUT_MS = 50UL;

static constexpr uint32_t FRAM_PERIOD_MS = 500UL;
static constexpr uint32_t RTC_PERIOD_MS = 1000UL;
static constexpr uint32_t SD_PERIOD_MS = 2000UL;
static constexpr uint32_t IO_PERIOD_MS = 20UL;
static constexpr uint32_t DISPLAY_PERIOD_MS = 250UL;
static constexpr uint32_t MODBUS_CYCLE_GAP_MS = 20UL;
static constexpr uint32_t LINK_CHECK_PERIOD_MS = 250UL;
static constexpr uint32_t LONG_LOOP_CRIT_US = 250000UL;

static constexpr uint16_t FRAM_SCRATCH_BYTES = 128;
static constexpr uint8_t FRAM_RECORD_VERSION = 1;
static const char SD_TEST_PATH[] = "/A14_S2.TMP";

struct FramBackup
{
    uint8_t bytes[FRAM_SCRATCH_BYTES];
};

struct FramProbe
{
    uint32_t magic;
    uint32_t sequence;
    uint32_t uptimeMs;
    uint32_t ethernetBytes;
    uint32_t modbusCycles;
    uint8_t role;
    uint8_t inputMap;
    uint16_t reserved;
};

enum MasterModbusPhase : uint8_t
{
    MB_START_WRITE = 0,
    MB_WAIT_WRITE,
    MB_START_COILS,
    MB_WAIT_COILS,
    MB_START_INPUTS,
    MB_WAIT_INPUTS,
    MB_PAUSE
};

static uint32_t bootId = 0;
static bool running = false;
static bool finishRequested = false;
static bool resultPrinted = false;
static uint32_t requestedDurationMs = 0;
static uint32_t runStartedMs = 0;
static uint32_t trafficStartedMs = 0;

static uint32_t maxLoopUs = 0;
static uint32_t longLoopCritical = 0;
static uint32_t lastLoopStartedUs = 0;

static uint32_t framOk = 0;
static uint32_t framFail = 0;
static uint32_t rtcOk = 0;
static uint32_t rtcFail = 0;
static uint32_t sdOk = 0;
static uint32_t sdFail = 0;
static uint32_t ioSamples = 0;
static uint32_t buttonDownSamples = 0;

static uint32_t lastFramMs = 0;
static uint32_t lastRtcMs = 0;
static uint32_t lastSdMs = 0;
static uint32_t lastIoMs = 0;
static uint32_t lastDisplayMs = 0;
static uint32_t lastLinkCheckMs = 0;

static bool framReady = false;
static bool framBackupValid = false;
static uint32_t framScratchAddr = 0;
static FramBackup framBackup;
static uint32_t framSequence = 0;

static bool sdReady = false;
static uint8_t inputMap = 0;

static uint8_t coilMap = 0;
static uint8_t discreteInputMap = 0;

#if JWPLC_S2_ROLE_MASTER
static EthernetServer tcpServer(TCP_PORT);
static EthernetClient tcpClient;
static bool serverStarted = false;
static bool clientAccepted = false;
static uint8_t ethBuffer[ETH_BUFFER_BYTES];

static uint64_t ethRxBytes = 0;
static uint64_t ethEchoBytes = 0;
static uint32_t ethRxOps = 0;
static uint32_t ethWriteOps = 0;
static uint32_t ethCorruptionErrors = 0;
static uint32_t ethTransportErrors = 0;
static uint32_t ethSpiLockErrors = 0;
static uint32_t ethBeginWriteMaxUs = 0;
static uint32_t ethPollWriteMaxUs = 0;
static uint32_t ethSpiHoldMaxUs = 0;
static uint64_t ethSpiHoldTotalUs = 0;
static uint32_t ethSpiHoldCount = 0;
static uint16_t pendingEchoLen = 0;
static bool echoWritePending = false;

static MasterModbusPhase mbPhase = MB_START_WRITE;
static uint8_t mbTxBits = 0;
static uint8_t mbRxCoils = 0;
static uint8_t mbRxInputs = 0;
static uint32_t mbCyclesOk = 0;
static uint32_t mbFailures = 0;
static uint32_t mbPatternMismatches = 0;
static uint32_t mbMaxTransactionUs = 0;
static uint32_t mbTxnStartedUs = 0;
static uint32_t mbNextCycleMs = 0;
static uint8_t mbPatternIndex = 0;
#endif

static char serialLine[96];
static size_t serialLineLength = 0;

static void resetRuntimeMetrics()
{
    maxLoopUs = 0;
    longLoopCritical = 0;
    framOk = 0;
    framFail = 0;
    rtcOk = 0;
    rtcFail = 0;
    sdOk = 0;
    sdFail = 0;
    ioSamples = 0;
    buttonDownSamples = 0;
    lastFramMs = millis();
    lastRtcMs = millis();
    lastSdMs = millis();
    lastIoMs = millis();
    lastDisplayMs = millis();
    lastLinkCheckMs = millis();

#if JWPLC_S2_ROLE_MASTER
    ethRxBytes = 0;
    ethEchoBytes = 0;
    ethRxOps = 0;
    ethWriteOps = 0;
    ethCorruptionErrors = 0;
    ethTransportErrors = 0;
    ethSpiLockErrors = 0;
    ethBeginWriteMaxUs = 0;
    ethPollWriteMaxUs = 0;
    ethSpiHoldMaxUs = 0;
    ethSpiHoldTotalUs = 0;
    ethSpiHoldCount = 0;
    pendingEchoLen = 0;
    echoWritePending = false;

    mbPhase = MB_START_WRITE;
    mbTxBits = 0;
    mbRxCoils = 0;
    mbRxInputs = 0;
    mbCyclesOk = 0;
    mbFailures = 0;
    mbPatternMismatches = 0;
    mbMaxTransactionUs = 0;
    mbTxnStartedUs = 0;
    mbNextCycleMs = millis();
    mbPatternIndex = 0;
#endif

    JWPLC_ModbusRTU.resetStats();
}

static bool prepareFram()
{
    const uint32_t size = JWPLC_FRAM.size();
    if (size < FRAM_SCRATCH_BYTES + 64U)
        return false;

    framScratchAddr = size - FRAM_SCRATCH_BYTES;
    if (!JWPLC_FRAM.get(framScratchAddr, framBackup))
        return false;

    framBackupValid = true;
    framReady = true;
    return true;
}

static void restoreFram()
{
    if (framReady && framBackupValid)
    {
        if (!JWPLC_FRAM.put(framScratchAddr, framBackup))
            ++framFail;
    }
}

static void serviceFram()
{
    if (!running || !framReady)
        return;

    const uint32_t now = millis();
    if ((uint32_t)(now - lastFramMs) < FRAM_PERIOD_MS)
        return;

    lastFramMs = now;

    FramProbe written = {};
    written.magic = 0x53324652UL; // S2FR
    written.sequence = ++framSequence;
    written.uptimeMs = now;
#if JWPLC_S2_ROLE_MASTER
    written.ethernetBytes = (uint32_t)ethRxBytes;
    written.modbusCycles = mbCyclesOk;
#else
    written.ethernetBytes = 0;
    written.modbusCycles = JWPLC_ModbusRTU.stats().requestsOk;
#endif
    written.role = JWPLC_S2_ROLE_MASTER ? 1 : 2;
    written.inputMap = inputMap;

    FramProbe readBack = {};
    const bool writeOk = JWPLC_FRAM.writeBlock(
        framScratchAddr,
        written,
        FRAM_RECORD_VERSION);
    const bool readOk = writeOk && JWPLC_FRAM.readBlock(
        framScratchAddr,
        readBack,
        FRAM_RECORD_VERSION);
    const bool same = readOk &&
        memcmp(&written, &readBack, sizeof(written)) == 0;

    if (same)
        ++framOk;
    else
        ++framFail;
}

static void serviceRtc()
{
    if (!running)
        return;

    const uint32_t now = millis();
    if ((uint32_t)(now - lastRtcMs) < RTC_PERIOD_MS)
        return;

    lastRtcMs = now;

    int16_t tempCenti = 0;
    const bool ok = JWPLC_RTC.isPresent() &&
        JWPLC_RTC.readTemperatureCentiC(tempCenti);

    if (ok)
        ++rtcOk;
    else
        ++rtcFail;
}

static void serviceInputsButtons()
{
    if (!running)
        return;

    const uint32_t now = millis();
    if ((uint32_t)(now - lastIoMs) < IO_PERIOD_MS)
        return;

    lastIoMs = now;
    inputMap = JWPLC_readInputs();
    discreteInputMap = inputMap;
    ++ioSamples;

    if (JWPLC_Buttons.isDown(BTN_OK))
        ++buttonDownSamples;
}

static void serviceDisplay()
{
    if (!running)
        return;

    const uint32_t now = millis();
    if ((uint32_t)(now - lastDisplayMs) < DISPLAY_PERIOD_MS)
        return;

    lastDisplayMs = now;
    const bool phase = ((now / 500UL) & 1U) != 0;
    JWPLC_Display.setRunLed(phase);
    JWPLC_Display.setBusLedAuto(true);
#if JWPLC_S2_ROLE_MASTER
    JWPLC_Display.setEthLedAuto(true);
#else
    JWPLC_Display.setEthLedAuto(false);
#endif
    JWPLC_Display.setErrCode("");
}

#if JWPLC_S2_ROLE_MASTER
static bool prepareSd()
{
    if (!JWPLCSD::isEnabled())
        return false;

    if (!JWPLCSD::begin() || !JWPLCSD::isCardPresent())
        return false;

    if (JWPLC_SD.exists(SD_TEST_PATH))
        (void)JWPLC_SD.remove(SD_TEST_PATH);

    sdReady = true;
    return true;
}

static void cleanupSd()
{
    if (sdReady && JWPLC_SD.exists(SD_TEST_PATH))
        (void)JWPLC_SD.remove(SD_TEST_PATH);
}

static void serviceSd()
{
    if (!running || !sdReady)
        return;

    const uint32_t now = millis();
    if ((uint32_t)(now - lastSdMs) < SD_PERIOD_MS)
        return;

    lastSdMs = now;

    if (JWPLC_SD.exists(SD_TEST_PATH) &&
        !JWPLC_SD.remove(SD_TEST_PATH))
    {
        ++sdFail;
        return;
    }

    char expected[64] = {};
    snprintf(
        expected,
        sizeof(expected),
        "S2,%lu,%lu,%lu",
        (unsigned long)now,
        (unsigned long)mbCyclesOk,
        (unsigned long)(ethRxBytes & 0xFFFFFFFFUL));

    JWPLCFile file = JWPLC_SD.open(SD_TEST_PATH, FILE_WRITE);
    if (!file)
    {
        ++sdFail;
        return;
    }

    file.println(expected);
    file.close();

    file = JWPLC_SD.open(SD_TEST_PATH, FILE_READ);
    if (!file)
    {
        ++sdFail;
        return;
    }

    String actual;
    while (file.available())
        actual += (char)file.read();
    file.close();
    actual.trim();

    if (actual == expected)
        ++sdOk;
    else
        ++sdFail;
}

static bool acquireEthernet()
{
    if (!jwplcSPI_acquire(ETH_LOCK_TIMEOUT_MS))
    {
        ++ethSpiLockErrors;
        return false;
    }

    jwplcSPI_deselectAll();
    return true;
}

static void releaseEthernet(uint32_t startedUs)
{
    const uint32_t holdUs = (uint32_t)(micros() - startedUs);
    ++ethSpiHoldCount;
    ethSpiHoldTotalUs += holdUs;
    if (holdUs > ethSpiHoldMaxUs)
        ethSpiHoldMaxUs = holdUs;
    jwplcSPI_release();
}

static uint8_t expectedStreamByte(uint64_t offset)
{
    return (uint8_t)(((uint32_t)(offset & 0xFFFFFFFFULL) * 29U + 0xA7U) & 0xFFU);
}

static void verifyRx(const uint8_t *data, size_t len)
{
    const uint64_t base = ethRxBytes;
    for (size_t i = 0; i < len; ++i)
    {
        if (data[i] != expectedStreamByte(base + i))
            ++ethCorruptionErrors;
    }
}

static void startServerIfReady()
{
    if (serverStarted || !JWPLC_Ethernet.isReady())
        return;

    if (!acquireEthernet())
        return;

    const uint32_t startedUs = micros();
    tcpServer.begin();
    serverStarted = (bool)tcpServer;
    releaseEthernet(startedUs);
}

static void serviceEthernet()
{
    startServerIfReady();

    if (!serverStarted)
        return;

    if (!acquireEthernet())
        return;

    const uint32_t holdStartedUs = micros();

    if (!clientAccepted)
    {
        EthernetClient incoming = tcpServer.accept();
        if (incoming)
        {
            tcpClient = incoming;
            clientAccepted = true;
            if (running && trafficStartedMs == 0)
                trafficStartedMs = millis();
        }

        releaseEthernet(holdStartedUs);
        return;
    }

    if (echoWritePending)
    {
        const uint32_t startedUs = micros();
        const int state = tcpClient.pollWriteAsync();
        const uint32_t elapsedUs = (uint32_t)(micros() - startedUs);
        if (elapsedUs > ethPollWriteMaxUs)
            ethPollWriteMaxUs = elapsedUs;

        if (elapsedUs > API_LIMIT_US)
            ++ethTransportErrors;

        if (state < 0)
        {
            ++ethTransportErrors;
            echoWritePending = false;
        }
        else if (state > 0)
        {
            ethEchoBytes += pendingEchoLen;
            ++ethWriteOps;
            pendingEchoLen = 0;
            echoWritePending = false;
        }

        releaseEthernet(holdStartedUs);
        return;
    }

    if (!running || finishRequested)
    {
        releaseEthernet(holdStartedUs);
        return;
    }

    const int availableBytes = tcpClient.available();
    if (availableBytes > 0)
    {
        const size_t chunk = min(
            (size_t)availableBytes,
            sizeof(ethBuffer));

        const int got = tcpClient.read(ethBuffer, chunk);
        if (got <= 0)
        {
            ++ethTransportErrors;
            releaseEthernet(holdStartedUs);
            return;
        }

        verifyRx(ethBuffer, (size_t)got);
        ethRxBytes += (uint32_t)got;
        ++ethRxOps;

        const uint32_t startedUs = micros();
        const int state = tcpClient.beginWriteAsync(
            ethBuffer,
            (size_t)got);
        const uint32_t elapsedUs = (uint32_t)(micros() - startedUs);

        if (elapsedUs > ethBeginWriteMaxUs)
            ethBeginWriteMaxUs = elapsedUs;

        if (elapsedUs > API_LIMIT_US)
            ++ethTransportErrors;

        if (state < 0)
        {
            ++ethTransportErrors;
        }
        else if (state == 0)
        {
            pendingEchoLen = (uint16_t)got;
            echoWritePending = true;
        }
        else
        {
            ethEchoBytes += (uint32_t)got;
            ++ethWriteOps;
        }
    }
    else
    {
        // Single status probe for this pass; never probe it twice.
        if (!tcpClient.connected())
        {
            ++ethTransportErrors;
            clientAccepted = false;
        }
    }

    releaseEthernet(holdStartedUs);
}

static const uint8_t MB_PATTERNS[] = {
    0x00, 0x55, 0xAA, 0x0F, 0xF0, 0x33, 0xCC, 0xFF
};

static void recordMbDuration()
{
    const uint32_t elapsedUs = (uint32_t)(micros() - mbTxnStartedUs);
    if (elapsedUs > mbMaxTransactionUs)
        mbMaxTransactionUs = elapsedUs;
}

static bool consumeMbResult()
{
    if (!JWPLC_ModbusRTU.masterDone())
        return false;

    recordMbDuration();
    const bool ok = JWPLC_ModbusRTU.masterSucceeded();
    JWPLC_ModbusRTU.clearMasterResult();

    if (!ok)
        ++mbFailures;

    return ok;
}

static void serviceModbusMaster()
{
    if (!running)
        return;

    JWPLC_ModbusRTU.task();

    const uint32_t now = millis();

    switch (mbPhase)
    {
    case MB_START_WRITE:
        if ((int32_t)(now - mbNextCycleMs) < 0)
            return;

        mbTxBits = MB_PATTERNS[mbPatternIndex];
        mbTxnStartedUs = micros();
        if (JWPLC_ModbusRTU.requestWriteMultipleCoils(
                SLAVE_ID,
                0,
                8,
                &mbTxBits,
                MODBUS_TIMEOUT_MS))
        {
            mbPhase = MB_WAIT_WRITE;
        }
        else
        {
            ++mbFailures;
            mbNextCycleMs = now + MODBUS_CYCLE_GAP_MS;
        }
        return;

    case MB_WAIT_WRITE:
        if (!JWPLC_ModbusRTU.masterDone())
            return;

        if (consumeMbResult())
            mbPhase = MB_START_COILS;
        else
        {
            mbPhase = MB_PAUSE;
            mbNextCycleMs = now + MODBUS_CYCLE_GAP_MS;
        }
        return;

    case MB_START_COILS:
        mbRxCoils = 0;
        mbTxnStartedUs = micros();
        if (JWPLC_ModbusRTU.requestReadCoils(
                SLAVE_ID,
                0,
                8,
                &mbRxCoils,
                MODBUS_TIMEOUT_MS))
        {
            mbPhase = MB_WAIT_COILS;
        }
        else
        {
            ++mbFailures;
            mbPhase = MB_PAUSE;
            mbNextCycleMs = now + MODBUS_CYCLE_GAP_MS;
        }
        return;

    case MB_WAIT_COILS:
        if (!JWPLC_ModbusRTU.masterDone())
            return;

        if (!consumeMbResult())
        {
            mbPhase = MB_PAUSE;
            mbNextCycleMs = now + MODBUS_CYCLE_GAP_MS;
            return;
        }

        if (mbRxCoils != mbTxBits)
            ++mbPatternMismatches;

        mbPhase = MB_START_INPUTS;
        return;

    case MB_START_INPUTS:
        mbRxInputs = 0;
        mbTxnStartedUs = micros();
        if (JWPLC_ModbusRTU.requestReadDiscreteInputs(
                SLAVE_ID,
                0,
                8,
                &mbRxInputs,
                MODBUS_TIMEOUT_MS))
        {
            mbPhase = MB_WAIT_INPUTS;
        }
        else
        {
            ++mbFailures;
            mbPhase = MB_PAUSE;
            mbNextCycleMs = now + MODBUS_CYCLE_GAP_MS;
        }
        return;

    case MB_WAIT_INPUTS:
        if (!JWPLC_ModbusRTU.masterDone())
            return;

        if (consumeMbResult())
            ++mbCyclesOk;

        mbPatternIndex = (uint8_t)((mbPatternIndex + 1U) %
            (sizeof(MB_PATTERNS) / sizeof(MB_PATTERNS[0])));
        mbNextCycleMs = now + MODBUS_CYCLE_GAP_MS;
        mbPhase = MB_PAUSE;
        return;

    case MB_PAUSE:
        if ((int32_t)(now - mbNextCycleMs) >= 0)
            mbPhase = MB_START_WRITE;
        return;
    }
}
#else
static void serviceModbusSlave()
{
    discreteInputMap = JWPLC_readInputs();
    JWPLC_ModbusRTU.task();
}
#endif

static void serviceLoopDiagnostics()
{
    const uint32_t nowUs = micros();

    if (lastLoopStartedUs != 0)
    {
        const uint32_t loopUs = (uint32_t)(nowUs - lastLoopStartedUs);
        if (loopUs > maxLoopUs)
            maxLoopUs = loopUs;
        if (loopUs > LONG_LOOP_CRIT_US)
            ++longLoopCritical;
    }

    lastLoopStartedUs = nowUs;
}

static bool commonRuntimePass()
{
    return framReady &&
        framOk > 0 &&
        framFail == 0 &&
        rtcOk > 0 &&
        rtcFail == 0 &&
        ioSamples > 0 &&
        longLoopCritical == 0;
}

#if JWPLC_S2_ROLE_MASTER
static void printMasterSnapshot()
{
    Serial.print("S2_ROLE=MASTER\n");
    Serial.print("S2_BOOT_ID="); Serial.println(bootId);
    Serial.print("S2_READY="); Serial.println(
        serverStarted &&
        JWPLC_ModbusRTU.isReady() &&
        framReady &&
        sdReady
            ? "YES" : "NO");
    Serial.print("S2_ETH_READY="); Serial.println(
        JWPLC_Ethernet.isReady() ? "YES" : "NO");
    Serial.print("S2_ETH_LINK="); Serial.println(
        JWPLC_Ethernet.linkUp() ? "UP" : "DOWN");
    Serial.print("S2_IP="); Serial.println(JWPLC_Ethernet.localIP());
    Serial.print("S2_RUNNING="); Serial.println(running ? "YES" : "NO");
    Serial.println("A14_S2_MASTER_SNAPSHOT=END");
}

static void printMasterResult()
{
    if (resultPrinted)
        return;

    resultPrinted = true;

    const JWPLCModbusRTUStats &mb = JWPLC_ModbusRTU.stats();

    const bool pass =
        commonRuntimePass() &&
        sdReady &&
        sdOk > 0 &&
        sdFail == 0 &&
        ethRxBytes > 0 &&
        ethEchoBytes == ethRxBytes &&
        ethCorruptionErrors == 0 &&
        ethTransportErrors == 0 &&
        ethSpiLockErrors == 0 &&
        ethBeginWriteMaxUs <= API_LIMIT_US &&
        ethPollWriteMaxUs <= API_LIMIT_US &&
        ethSpiHoldMaxUs <= API_LIMIT_US &&
        mbCyclesOk > 0 &&
        mbFailures == 0 &&
        mbPatternMismatches == 0 &&
        mb.crcErrors == 0 &&
        mb.masterTimeouts == 0 &&
        JWPLC_Ethernet.linkUp();

    Serial.print("S2_RESULT="); Serial.println(pass ? "PASS" : "FAIL");
    Serial.print("S2_BOOT_ID="); Serial.println(bootId);
    Serial.print("S2_DURATION_MS="); Serial.println(
        trafficStartedMs == 0 ? 0 : (uint32_t)(millis() - trafficStartedMs));
    Serial.print("S2_ETH_RX_BYTES="); Serial.println((unsigned long long)ethRxBytes);
    Serial.print("S2_ETH_ECHO_BYTES="); Serial.println((unsigned long long)ethEchoBytes);
    Serial.print("S2_ETH_RX_OPS="); Serial.println(ethRxOps);
    Serial.print("S2_ETH_WRITE_OPS="); Serial.println(ethWriteOps);
    Serial.print("S2_ETH_CORRUPTION_ERRORS="); Serial.println(ethCorruptionErrors);
    Serial.print("S2_ETH_TRANSPORT_ERRORS="); Serial.println(ethTransportErrors);
    Serial.print("S2_ETH_SPI_LOCK_ERRORS="); Serial.println(ethSpiLockErrors);
    Serial.print("S2_ETH_BEGIN_WRITE_MAX_US="); Serial.println(ethBeginWriteMaxUs);
    Serial.print("S2_ETH_POLL_WRITE_MAX_US="); Serial.println(ethPollWriteMaxUs);
    Serial.print("S2_ETH_SPI_HOLD_MAX_US="); Serial.println(ethSpiHoldMaxUs);
    Serial.print("S2_ETH_SPI_HOLD_TOTAL_US="); Serial.println((unsigned long long)ethSpiHoldTotalUs);
    Serial.print("S2_ETH_SPI_HOLD_COUNT="); Serial.println(ethSpiHoldCount);
    Serial.print("S2_MODBUS_CYCLES_OK="); Serial.println(mbCyclesOk);
    Serial.print("S2_MODBUS_FAILURES="); Serial.println(mbFailures);
    Serial.print("S2_MODBUS_PATTERN_MISMATCHES="); Serial.println(mbPatternMismatches);
    Serial.print("S2_MODBUS_MAX_TRANSACTION_US="); Serial.println(mbMaxTransactionUs);
    Serial.print("S2_MODBUS_RX_FRAMES="); Serial.println(mb.rxFrames);
    Serial.print("S2_MODBUS_TX_FRAMES="); Serial.println(mb.txFrames);
    Serial.print("S2_MODBUS_REQUESTS_OK="); Serial.println(mb.requestsOk);
    Serial.print("S2_MODBUS_CRC_ERRORS="); Serial.println(mb.crcErrors);
    Serial.print("S2_MODBUS_TIMEOUTS="); Serial.println(mb.masterTimeouts);
    Serial.print("S2_FRAM_OK="); Serial.println(framOk);
    Serial.print("S2_FRAM_FAIL="); Serial.println(framFail);
    Serial.print("S2_RTC_OK="); Serial.println(rtcOk);
    Serial.print("S2_RTC_FAIL="); Serial.println(rtcFail);
    Serial.print("S2_SD_OK="); Serial.println(sdOk);
    Serial.print("S2_SD_FAIL="); Serial.println(sdFail);
    Serial.print("S2_IO_SAMPLES="); Serial.println(ioSamples);
    Serial.print("S2_BUTTON_DOWN_SAMPLES="); Serial.println(buttonDownSamples);
    Serial.print("S2_MAX_LOOP_US="); Serial.println(maxLoopUs);
    Serial.print("S2_LONG_LOOP_CRITICAL="); Serial.println(longLoopCritical);
    Serial.print("S2_LINK_FINAL="); Serial.println(JWPLC_Ethernet.linkUp() ? "UP" : "DOWN");
    Serial.println("A14_S2_MASTER_CASE=END");
}

static void cleanupMaster()
{
    if (acquireEthernet())
    {
        const uint32_t holdStartedUs = micros();
        tcpClient.cancelWriteAsync();
        tcpClient.cancelStopAsync();
        tcpClient.cancelConnectAsync();
        releaseEthernet(holdStartedUs);
    }

    restoreFram();
    cleanupSd();
    JWPLC_Display.setRunLed(true);
}
#else
static void printSlaveSnapshot()
{
    const JWPLCModbusRTUStats &mb = JWPLC_ModbusRTU.stats();

    Serial.print("S2_ROLE=SLAVE\n");
    Serial.print("S2_BOOT_ID="); Serial.println(bootId);
    Serial.print("S2_READY="); Serial.println(
        JWPLC_ModbusRTU.isReady() && framReady ? "YES" : "NO");
    Serial.print("S2_RUNNING="); Serial.println(running ? "YES" : "NO");
    Serial.print("S2_MODBUS_RX_FRAMES="); Serial.println(mb.rxFrames);
    Serial.print("S2_MODBUS_TX_FRAMES="); Serial.println(mb.txFrames);
    Serial.println("A14_S2_SLAVE_SNAPSHOT=END");
}

static void printSlaveResult()
{
    const JWPLCModbusRTUStats &mb = JWPLC_ModbusRTU.stats();

    const bool pass =
        commonRuntimePass() &&
        mb.rxFrames > 0 &&
        mb.txFrames > 0 &&
        mb.crcErrors == 0;

    Serial.print("S2_SLAVE_RESULT="); Serial.println(pass ? "PASS" : "FAIL");
    Serial.print("S2_BOOT_ID="); Serial.println(bootId);
    Serial.print("S2_MODBUS_RX_FRAMES="); Serial.println(mb.rxFrames);
    Serial.print("S2_MODBUS_TX_FRAMES="); Serial.println(mb.txFrames);
    Serial.print("S2_MODBUS_REQUESTS_OK="); Serial.println(mb.requestsOk);
    Serial.print("S2_MODBUS_CRC_ERRORS="); Serial.println(mb.crcErrors);
    Serial.print("S2_FRAM_OK="); Serial.println(framOk);
    Serial.print("S2_FRAM_FAIL="); Serial.println(framFail);
    Serial.print("S2_RTC_OK="); Serial.println(rtcOk);
    Serial.print("S2_RTC_FAIL="); Serial.println(rtcFail);
    Serial.print("S2_IO_SAMPLES="); Serial.println(ioSamples);
    Serial.print("S2_BUTTON_DOWN_SAMPLES="); Serial.println(buttonDownSamples);
    Serial.print("S2_MAX_LOOP_US="); Serial.println(maxLoopUs);
    Serial.print("S2_LONG_LOOP_CRITICAL="); Serial.println(longLoopCritical);
    Serial.println("A14_S2_SLAVE_CASE=END");
}
#endif

static void startRun(uint32_t durationSeconds)
{
    resetRuntimeMetrics();
    requestedDurationMs = durationSeconds * 1000UL;
    runStartedMs = millis();
    trafficStartedMs = 0;
    finishRequested = false;
    resultPrinted = false;
    running = true;

#if JWPLC_S2_ROLE_MASTER
    clientAccepted = false;
#else
    (void)durationSeconds;
#endif
}

static void stopRun()
{
    running = false;
    finishRequested = true;

#if JWPLC_S2_ROLE_MASTER
    cleanupMaster();
    Serial.println("S2_MASTER_CLEANUP=PASS");
#else
    restoreFram();
    printSlaveResult();
    Serial.println("S2_SLAVE_CLEANUP=PASS");
#endif
}

static void handleLine(char *line)
{
    if (strcmp(line, "S") == 0 || strcmp(line, "s") == 0)
    {
#if JWPLC_S2_ROLE_MASTER
        printMasterSnapshot();
#else
        printSlaveSnapshot();
#endif
        return;
    }

#if JWPLC_S2_ROLE_MASTER
    unsigned long seconds = 0;
    if (sscanf(line, "START %lu", &seconds) == 1 &&
        seconds >= 60UL &&
        seconds <= 3600UL)
    {
        startRun((uint32_t)seconds);
        Serial.println("S2_MASTER_START=PASS");
        return;
    }
#else
    if (strcmp(line, "START") == 0)
    {
        startRun(0);
        Serial.println("S2_SLAVE_START=PASS");
        return;
    }
#endif

    if (strcmp(line, "STOP") == 0)
    {
        stopRun();
        return;
    }

    Serial.println("S2_COMMAND=INVALID");
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();

        if (c == '\r')
            continue;

        if (c == '\n')
        {
            serialLine[serialLineLength] = '\0';
            if (serialLineLength > 0)
                handleLine(serialLine);
            serialLineLength = 0;
            continue;
        }

        if (serialLineLength + 1 < sizeof(serialLine))
            serialLine[serialLineLength++] = c;
        else
            serialLineLength = 0;
    }
}

void setup()
{
    Serial.begin(SERIAL_BAUD);
    bootId = esp_random();

    JWPLC_Display.setRunLed(true);
    JWPLC_Display.setBusLedAuto(true);
#if JWPLC_S2_ROLE_MASTER
    JWPLC_Display.setEthLedAuto(true);
#else
    JWPLC_Display.setEthLedAuto(false);
#endif
    JWPLC_Display.setErrCode("");
    JWPLC_Display.goIdle();

    framReady = prepareFram();

#if JWPLC_S2_ROLE_MASTER
    sdReady = prepareSd();

    (void)JWPLC_ModbusRTU.end();
    (void)JWPLC_RS485.end();
    const bool modbusReady = JWPLC_ModbusRTU.begin(
        MASTER_LOCAL_ID,
        MODBUS_BAUD,
        MODBUS_CONFIG);
#else
    coilMap = 0;
    discreteInputMap = JWPLC_readInputs();
    JWPLC_ModbusRTU.setCoils(&coilMap, 8);
    JWPLC_ModbusRTU.setDiscreteInputs(&discreteInputMap, 8);

    (void)JWPLC_ModbusRTU.end();
    (void)JWPLC_RS485.end();
    const bool modbusReady = JWPLC_ModbusRTU.begin(
        SLAVE_ID,
        MODBUS_BAUD,
        MODBUS_CONFIG);
#endif

    if (modbusReady)
        JWPLC_ModbusRTU.setFrameGapMs(MODBUS_FRAME_GAP_MS);

    JWPLC_ModbusRTU.resetStats();

    Serial.print("A14_S2_BOOT="); Serial.println(bootId);
    Serial.print("S2_ROLE=");
#if JWPLC_S2_ROLE_MASTER
    Serial.println("MASTER");
#else
    Serial.println("SLAVE");
#endif
    Serial.print("S2_MODBUS_BEGIN="); Serial.println(modbusReady ? "PASS" : "FAIL");
    Serial.print("S2_FRAM_READY="); Serial.println(framReady ? "YES" : "NO");
#if JWPLC_S2_ROLE_MASTER
    Serial.print("S2_SD_READY="); Serial.println(sdReady ? "YES" : "NO");
#endif
}

void loop()
{
    serviceLoopDiagnostics();
    serviceSerial();

#if JWPLC_S2_ROLE_MASTER
    serviceEthernet();
    serviceModbusMaster();
#else
    serviceModbusSlave();
#endif

    serviceFram();
    serviceRtc();
#if JWPLC_S2_ROLE_MASTER
    serviceSd();
#endif
    serviceInputsButtons();
    serviceDisplay();

#if JWPLC_S2_ROLE_MASTER
    if (running && trafficStartedMs != 0 &&
        (uint32_t)(millis() - trafficStartedMs) >= requestedDurationMs)
    {
        finishRequested = true;
    }

    if (running && finishRequested && !echoWritePending)
    {
        running = false;
        printMasterResult();
    }
#endif

    yield();
}
