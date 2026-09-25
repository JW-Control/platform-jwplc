/*
  A14 P5 - RTU SLAVE 2 + HMI ON DEMAND

  Peer del full-runtime master:
  - Modbus RTU Slave ID 2
  - 115200 8N1
  - HR0: contador de actividad
  - HR1: magic fijo 0x55AA para verificacion del Master
  - HMI Alpha11 declarativa con dirty redraw
  - sin fillScreen() periodico
  - sin callbacks jwplcUserDisplay* legacy

  Serial:
    R -> reset de estadisticas RTU
    S -> snapshot
*/

#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_ModbusRTU.h>

static constexpr uint8_t SLAVE_ID = 2;
static constexpr uint32_t RTU_BAUD = 115200UL;
static constexpr uint32_t RTU_CONFIG = SERIAL_8N1;
static constexpr uint16_t VERIFY_MAGIC = 0x55AA;
static constexpr uint32_t DISPLAY_SERVICE_PERIOD_MS = 100UL;

static uint16_t holding[16] = {};
static bool rtuReady = false;

static uint32_t displayServiceCycles = 0;
static uint32_t displayLastServiceMs = 0;
static uint32_t displayServiceGapMaxMs = 0;

enum SlaveFieldId : uint8_t
{
    FIELD_ROLE = 1,
    FIELD_RX,
    FIELD_TX,
    FIELD_OK,
    FIELD_CRC,
    FIELD_HR0
};

static const JWPLC_UIField SLAVE_FIELDS[] = {
    JWPLC_UITextField(
        FIELD_ROLE, 8, 8, "Rol", 10),

    JWPLC_UIValueField(
        FIELD_RX, 8, 42, "RX", "",
        JWPLC_UIValueFormat(7, 0, false, false)),

    JWPLC_UIValueField(
        FIELD_TX, 8, 76, "TX", "",
        JWPLC_UIValueFormat(7, 0, false, false)),

    JWPLC_UIValueField(
        FIELD_OK, 8, 110, "OK", "",
        JWPLC_UIValueFormat(7, 0, false, false)),

    JWPLC_UIValueField(
        FIELD_CRC, 165, 42, "CRC", "",
        JWPLC_UIValueFormat(5, 0, false, false)),

    JWPLC_UIValueField(
        FIELD_HR0, 165, 76, "HR0", "",
        JWPLC_UIValueFormat(5, 0, false, false))
};

static void updateMaxU32(
    uint32_t value,
    uint32_t &currentMax)
{
    if (value > currentMax)
        currentMax = value;
}

static const char *yesNo(bool value)
{
    return value ? "YES" : "NO";
}

static void serviceDisplayTelemetry()
{
    const uint32_t now = millis();

    if ((uint32_t)(now - displayLastServiceMs) <
        DISPLAY_SERVICE_PERIOD_MS)
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

    const JWPLCModbusRTUStats &s =
        JWPLC_ModbusRTU.stats();

    JWPLC_Display.setText(
        FIELD_ROLE,
        "SLAVE 2");

    JWPLC_Display.setValue(
        FIELD_RX,
        s.rxFrames);

    JWPLC_Display.setValue(
        FIELD_TX,
        s.txFrames);

    JWPLC_Display.setValue(
        FIELD_OK,
        s.requestsOk);

    JWPLC_Display.setValue(
        FIELD_CRC,
        s.crcErrors);

    JWPLC_Display.setValue(
        FIELD_HR0,
        holding[0]);
}

static void resetStats()
{
    JWPLC_ModbusRTU.resetStats();

    displayServiceCycles = 0;
    displayLastServiceMs = millis();
    displayServiceGapMaxMs = 0;
}

static void printSnapshot()
{
    const JWPLCModbusRTUStats &s =
        JWPLC_ModbusRTU.stats();

    Serial.println();
    Serial.println(
        "========================================");
    Serial.println(
        " A14 P5 RTU SLAVE SNAPSHOT");
    Serial.println(
        "========================================");

    Serial.print("SLAVE_READY=");
    Serial.println(
        yesNo(
            rtuReady &&
            JWPLC_Display.isReady()));

    Serial.print("RTU_READY=");
    Serial.println(yesNo(rtuReady));

    Serial.println("RTU_ROLE=SLAVE");

    Serial.print("RTU_SLAVE_ID=");
    Serial.println(SLAVE_ID);

    Serial.print("RTU_BAUD=");
    Serial.println(RTU_BAUD);

    Serial.print("RTU_FRAME_GAP_MS=");
    Serial.println(JWPLC_ModbusRTU.frameGapMs());

    Serial.print("RTU_FRAME_GAP_US=");
    Serial.println(JWPLC_ModbusRTU.frameGapUs());

    Serial.print("RTU_RX_FRAMES=");
    Serial.println(s.rxFrames);

    Serial.print("RTU_TX_FRAMES=");
    Serial.println(s.txFrames);

    Serial.print("RTU_REQUESTS_OK=");
    Serial.println(s.requestsOk);

    Serial.print("RTU_CRC_ERRORS=");
    Serial.println(s.crcErrors);

    Serial.print("RTU_EXCEPTIONS_SENT=");
    Serial.println(s.exceptionsSent);

    Serial.print("RTU_MASTER_TIMEOUTS=");
    Serial.println(s.masterTimeouts);

    Serial.print("RTU_LAST_ERROR=");
    Serial.println(
        JWPLC_ModbusRTU.lastErrorString());

    Serial.print("SLAVE_HR0=");
    Serial.println(holding[0]);

    Serial.print("SLAVE_HR1=");
    Serial.println(holding[1]);

    Serial.print("DISPLAY_READY=");
    Serial.println(
        yesNo(
            JWPLC_Display.isReady()));

    Serial.println(
        "DISPLAY_RENDER_MODE=HMI_ON_DEMAND_DIRTY");

    Serial.println(
        "DISPLAY_REFRESH_MODE=USER_REFRESH_ON_DEMAND");

    Serial.print("DISPLAY_SERVICE_CYCLES=");
    Serial.println(displayServiceCycles);

    Serial.print("DISPLAY_SERVICE_GAP_MAX_MS=");
    Serial.println(displayServiceGapMaxMs);

    Serial.println(
        "A14_P5_SLAVE_SNAPSHOT=END");
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c =
            (char)Serial.read();

        if (c == 'R' || c == 'r')
        {
            resetStats();

            Serial.println(
                "A14_P5_SLAVE_RESET=PASS");
        }
        else if (c == '2')
        {
            JWPLC_ModbusRTU.setFrameGapUs(2000UL);
            Serial.println("RTU_FRAME_GAP_US=2000");
            Serial.println("RTU_FRAME_GAP_MS=2");
        }
        else if (c == '5')
        {
            JWPLC_ModbusRTU.setFrameGapMs(5);
            Serial.println("RTU_FRAME_GAP_MS=5");
        }
        else if (c == 'H' || c == 'h')
        {
            JWPLC_ModbusRTU.setFrameGapUs(2000UL);
            Serial.println("RTU_FRAME_GAP_US=2000");
        }
        else if (c == 'I' || c == 'i')
        {
            JWPLC_ModbusRTU.setFrameGapUs(1750UL);
            Serial.println("RTU_FRAME_GAP_US=1750");
        }
        else if (c == 'J' || c == 'j')
        {
            JWPLC_ModbusRTU.setFrameGapUs(1500UL);
            Serial.println("RTU_FRAME_GAP_US=1500");
        }
        else if (c == 'K' || c == 'k')
        {
            JWPLC_ModbusRTU.setFrameGapUs(1250UL);
            Serial.println("RTU_FRAME_GAP_US=1250");
        }
        else if (c == 'L' || c == 'l')
        {
            JWPLC_ModbusRTU.setFrameGapUs(1000UL);
            Serial.println("RTU_FRAME_GAP_US=1000");
        }
        else if (c == 'S' || c == 's')
        {
            printSnapshot();
        }
    }
}

void setup()
{
    Serial.begin(115200);

    holding[0] = 0;
    holding[1] = VERIFY_MAGIC;

    for (uint8_t i = 2; i < 16; ++i)
    {
        holding[i] =
            (uint16_t)(
                0x2000U +
                i);
    }

    JWPLC_ModbusRTU.setHoldingRegisters(
        holding,
        16);

    rtuReady =
        JWPLC_ModbusRTU.begin(
            SLAVE_ID,
            RTU_BAUD,
            RTU_CONFIG);

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_DISABLED);

    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_DISABLED);

    JWPLC_Display.setUserRefreshMode(
        USER_REFRESH_ON_DEMAND);

    JWPLC_Display.setUserRefreshPeriodMs(
        DISPLAY_SERVICE_PERIOD_MS);

    const bool hmiReady =
        JWPLC_Display.setFields(
            SLAVE_FIELDS,
            sizeof(SLAVE_FIELDS) /
                sizeof(SLAVE_FIELDS[0]));

    if (hmiReady)
    {
        JWPLC_Display.setText(
            FIELD_ROLE,
            "SLAVE 2");

        JWPLC_Display.setValue(
            FIELD_RX,
            0);

        JWPLC_Display.setValue(
            FIELD_TX,
            0);

        JWPLC_Display.setValue(
            FIELD_OK,
            0);

        JWPLC_Display.setValue(
            FIELD_CRC,
            0);

        JWPLC_Display.setValue(
            FIELD_HR0,
            0);

        JWPLC_Display.enterUserUI();
    }

    resetStats();

    Serial.println();
    Serial.println(
        "A14 P5 - RTU SLAVE 2");

    Serial.print("RTU_READY_BOOT=");
    Serial.println(yesNo(rtuReady));

    Serial.print("HMI_READY_BOOT=");
    Serial.println(yesNo(hmiReady));

    Serial.println(
        "DISPLAY_RENDER_MODE_BOOT=HMI_ON_DEMAND_DIRTY");

    Serial.println(
        "A14_P5_ROLE=SLAVE");
}

void loop()
{
    JWPLC_ModbusRTU.task();

    holding[0] =
        (uint16_t)(
            (millis() / 100UL) &
            0xFFFFU);

    holding[1] =
        VERIFY_MAGIC;

    serviceSerial();
    serviceDisplayTelemetry();
}
