#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_ModbusRTU.h>

/*
  ModbusRTU_BusLedAuto_Master.ino

  JWPLC Basic como master Modbus RTU.

  Objetivo:
  - Leer holding registers de un JWPLC Basic slave.
  - Escribir un comando en HR[10] del slave.
  - Validar actividad del LED BUS automático.
  - Mantener configuración de JWPLC_Display desde setup().

  Conexión:
  - A con A
  - B con B
  - GND común recomendado

  Parámetros:
  - Master local ID: 1
  - Slave target ID: 2
  - Baud: 115200
  - Config: SERIAL_8N1
*/

static constexpr uint8_t MASTER_LOCAL_ID = 1;
static constexpr uint8_t TARGET_SLAVE_ID = 2;

static constexpr uint32_t MODBUS_BAUD = 115200;
static constexpr uint32_t POLL_PERIOD_MS = 1000;

static constexpr uint32_t IDLE_REFRESH_MS = 500;
static constexpr uint32_t USER_REFRESH_MS = 200;

static uint16_t rxRegs[4];

static uint32_t lastPollMs = 0;
static uint32_t requestCounter = 0;
static uint32_t okCounter = 0;
static uint32_t errorCounter = 0;
static uint16_t commandValue = 0;

static bool lastReadOk = false;
static bool lastWriteOk = false;
static bool readyPrinted = false;

static void pollSlave()
{
    requestCounter++;

    Serial.println();
    Serial.print("MASTER poll #");
    Serial.println(requestCounter);

    lastReadOk = JWPLC_ModbusRTU.readHoldingRegisters(
        TARGET_SLAVE_ID,
        0,
        4,
        rxRegs,
        300
    );

    if (lastReadOk)
    {
        okCounter++;

        Serial.print("READ OK  HR0=");
        Serial.print(rxRegs[0]);
        Serial.print(" HR1=");
        Serial.print(rxRegs[1]);
        Serial.print(" HR2=");
        Serial.print(rxRegs[2]);
        Serial.print(" HR3=");
        Serial.println(rxRegs[3]);
    }
    else
    {
        errorCounter++;

        Serial.print("READ ERROR: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
    }

    commandValue++;

    lastWriteOk = JWPLC_ModbusRTU.writeSingleRegister(
        TARGET_SLAVE_ID,
        10,
        commandValue,
        300
    );

    if (lastWriteOk)
    {
        Serial.print("WRITE OK HR10=");
        Serial.println(commandValue);
    }
    else
    {
        errorCounter++;

        Serial.print("WRITE ERROR: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
    }

    JWPLC_Display.setErrLed(errorCounter > 0);
}

extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(18, 20);
    tft.print("MODBUS MASTER");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

    tft.setCursor(18, 56);
    tft.print("Target slave: ");
    tft.print(TARGET_SLAVE_ID);

    tft.setCursor(18, 72);
    tft.print("OK: poll manual");

    tft.setCursor(18, 88);
    tft.print("ESC: volver a IDLE");
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    auto &tft = JWPLC_Display.tft();

    if (JWPLC_Display.buttonsReady() && JWPLC_Buttons.pressed(BTN_OK))
    {
        pollSlave();
        JWPLC_Display.notifyActivity();
    }

    tft.fillRect(18, 112, 285, 52, ST77XX_BLACK);

    tft.setTextSize(1);

    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(18, 112);
    tft.print("Req:");
    tft.print(requestCounter);
    tft.print(" OK:");
    tft.print(okCounter);
    tft.print(" ERR:");
    tft.print(errorCounter);

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(18, 128);
    tft.print("HR0:");
    tft.print(rxRegs[0]);
    tft.print(" HR1:");
    tft.print(rxRegs[1]);

    tft.setCursor(18, 144);
    tft.print("CMD HR10:");
    tft.print(commandValue);

    tft.setCursor(170, 144);
    tft.print("R:");
    tft.print(lastReadOk ? "OK" : "ER");
    tft.print(" W:");
    tft.print(lastWriteOk ? "OK" : "ER");
}

extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("MASTER: regreso a IDLE");
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("JWPLC Basic - Modbus RTU Master + BUS LED Auto");

    // Display setup-friendly.
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
    // JWPLC_Display.setIdleRefreshPeriodMs(IDLE_REFRESH_MS);
    // JWPLC_Display.setUserRefreshPeriodMs(USER_REFRESH_MS);

    JWPLC_Display.setRunLed(true);
    JWPLC_Display.setErrLed(false);

    // Requiere la mejora nueva de alpha.2.
    JWPLC_Display.setBusLedAuto(true);

    // Ethernet puede quedar automático.
    JWPLC_Display.setEthLedAuto(true);

    for (uint8_t i = 0; i < 4; i++)
    {
        rxRegs[i] = 0;
    }

    if (JWPLC_ModbusRTU.begin(MASTER_LOCAL_ID, MODBUS_BAUD, SERIAL_8N1))
    {
        Serial.println("Modbus RTU master iniciado correctamente.");
    }
    else
    {
        Serial.print("Error iniciando Modbus RTU: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        JWPLC_Display.setErrLed(true);
    }

    JWPLC_ModbusRTU.printStatus(Serial);

    Serial.println("El master consultara al slave cada 1 segundo.");
}

void loop()
{
    uint32_t now = millis();

    if ((uint32_t)(now - lastPollMs) >= POLL_PERIOD_MS)
    {
        lastPollMs = now;
        pollSlave();
    }

    if (!readyPrinted && JWPLC_Display.isReady())
    {
        readyPrinted = true;
        Serial.println("TFT lista. Master en IDLE.");
        Serial.println("BUS debe parpadear con cada lectura/escritura Modbus.");
    }

    delay(5);
}