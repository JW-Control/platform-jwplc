/*
  A14 P5-F - DataLog automatic service probe.

  Objetivo:
  confirmar que el core precompilado regenerado ejecuta
  jwplcDataLogTickCallback() desde jwplcSystemTask().

  No llama manualmente JWPLC_SD.serviceDataLogs().
*/

#include <Arduino.h>
#include <JWPLC_GlobalPeripherals.h>

static JWPLCDataLog probeLog;

static constexpr char PROBE_PATH[] =
    "/A14P5F.LOG";

static constexpr size_t RECORD_BYTES =
    32U;

static constexpr size_t BUFFER_BYTES =
    4096U;

static constexpr size_t COMMIT_THRESHOLD_BYTES =
    512U;

static constexpr uint32_t COMMIT_TIMEOUT_MS =
    2000UL;

static constexpr uint32_t WRITE_PERIOD_MS =
    20UL;

static constexpr uint32_t PROBE_DURATION_MS =
    8000UL;

static uint32_t startedMs = 0;
static uint32_t lastWriteMs = 0;
static uint32_t sequence = 0;
static uint32_t writeFails = 0;
static bool resultPrinted = false;

static void buildRecord(
    uint8_t record[RECORD_BYTES])
{
    memset(
        record,
        0,
        RECORD_BYTES);

    record[0] = 'P';
    record[1] = '5';
    record[2] = 'F';

    record[4] =
        (uint8_t)(
            sequence >> 24);

    record[5] =
        (uint8_t)(
            sequence >> 16);

    record[6] =
        (uint8_t)(
            sequence >> 8);

    record[7] =
        (uint8_t)sequence;

    const uint32_t now =
        millis();

    record[8] =
        (uint8_t)(
            now >> 24);

    record[9] =
        (uint8_t)(
            now >> 16);

    record[10] =
        (uint8_t)(
            now >> 8);

    record[11] =
        (uint8_t)now;

    for (
        size_t i = 12;
        i < RECORD_BYTES;
        ++i)
    {
        record[i] =
            (uint8_t)(
                0xA5U ^
                (uint8_t)i ^
                (uint8_t)sequence);
    }
}

static void printResult()
{
    if (resultPrinted)
    {
        return;
    }

    resultPrinted = true;

    const JW_SDDataLogStatus status =
        probeLog.status();

    const bool pass =
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady() &&
        status.active &&
        status.commitCount > 0 &&
        status.committedBytes > 0 &&
        status.failedCommits == 0 &&
        writeFails == 0;

    Serial.println();
    Serial.println(
        "========================================");

    Serial.println(
        " A14 P5-F DATALOG AUTOSERVICE RESULT");

    Serial.println(
        "========================================");

    Serial.print(
        "P5F_SD_READY=");
    Serial.println(
        JWPLCSD::isReady()
            ? "YES"
            : "NO");

    Serial.print(
        "P5F_DATALOG_ACTIVE=");
    Serial.println(
        status.active
            ? "YES"
            : "NO");

    Serial.print(
        "P5F_ACCEPTED_WRITES=");
    Serial.println(
        status.acceptedWrites);

    Serial.print(
        "P5F_ACCEPTED_BYTES=");
    Serial.println(
        (unsigned long long)
            status.acceptedBytes);

    Serial.print(
        "P5F_PENDING_BYTES=");
    Serial.println(
        status.pendingBytes);

    Serial.print(
        "P5F_COMMITTED_BYTES=");
    Serial.println(
        (unsigned long long)
            status.committedBytes);

    Serial.print(
        "P5F_COMMIT_COUNT=");
    Serial.println(
        status.commitCount);

    Serial.print(
        "P5F_FAILED_COMMITS=");
    Serial.println(
        status.failedCommits);

    Serial.print(
        "P5F_WRITE_FAILS=");
    Serial.println(
        writeFails);

    Serial.print(
        "P5F_LAST_ERROR=");
    Serial.println(
        probeLog.lastErrorString());

    Serial.println(
        "P5F_MANUAL_SERVICE_CALL=NO");

    Serial.print(
        "A14_P5F_DATALOG_RESULT=");
    Serial.println(
        pass
            ? "PASS"
            : "FAIL");
}

void setup()
{
    Serial.begin(115200);

    Serial.println(
        "A14_P5F_PROBE_BOOT=YES");

    if (
        !JWPLCSD::isEnabled() ||
        !JWPLCSD::isCardPresent() ||
        !JWPLCSD::isReady())
    {
        Serial.println(
            "A14_P5F_DATALOG_RESULT=FAIL_SD_NOT_READY");

        resultPrinted = true;
        return;
    }

    if (JWPLC_SD.exists(PROBE_PATH))
    {
        if (!JWPLC_SD.remove(PROBE_PATH))
        {
            Serial.println(
                "A14_P5F_DATALOG_RESULT=FAIL_REMOVE");

            resultPrinted = true;
            return;
        }
    }

    const JW_SDDataLogConfig config(
        BUFFER_BYTES,
        COMMIT_THRESHOLD_BYTES,
        COMMIT_TIMEOUT_MS);

    if (!probeLog.begin(
            JWPLC_SD,
            PROBE_PATH,
            config))
    {
        Serial.print(
            "P5F_BEGIN_ERROR=");
        Serial.println(
            probeLog.lastErrorString());

        Serial.println(
            "A14_P5F_DATALOG_RESULT=FAIL_BEGIN");

        resultPrinted = true;
        return;
    }

    startedMs =
        millis();

    lastWriteMs =
        startedMs;

    Serial.println(
        "P5F_DATALOG_BEGIN=PASS");
}

void loop()
{
    if (resultPrinted)
    {
        delay(10);
        return;
    }

    const uint32_t now =
        millis();

    if (
        (uint32_t)(
            now -
            lastWriteMs) >=
        WRITE_PERIOD_MS)
    {
        lastWriteMs = now;

        ++sequence;

        uint8_t record[RECORD_BYTES];

        buildRecord(record);

        const size_t written =
            probeLog.write(
                record,
                sizeof(record));

        if (
            written !=
            sizeof(record))
        {
            ++writeFails;
        }
    }

    if (
        (uint32_t)(
            now -
            startedMs) >=
        PROBE_DURATION_MS)
    {
        printResult();
    }
}
