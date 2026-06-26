#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_ModbusRTU.h>

/*
  ModbusRTU_BusLedAuto_Slave.ino

  JWPLC Basic como esclavo Modbus RTU.

  Objetivo:
  - Responder a un JWPLC Basic master por RS-485.
  - Validar actividad del LED BUS automático.
  - Mantener configuración de JWPLC_Display desde setup().

  Conexión:
  - A con A
  - B con B
  - GND común recomendado

  Parámetros:
  - Slave ID: 2
  - Baud: 115200
  - Config: SERIAL_8N1

  Registros Holding:
  - HR[0]  = contador de vida
  - HR[1]  = segundos desde arranque
  - HR[2]  = contador local de atención
  - HR[10] = comando recibido desde master
*/

static constexpr uint8_t SLAVE_ID = 2;
static constexpr uint32_t MODBUS_BAUD = 115200;
static constexpr uint32_t IDLE_REFRESH_MS = 500;
static constexpr uint32_t USER_REFRESH_MS = 250;

static uint16_t holdingRegs[16];

static uint32_t lastUpdateMs = 0;
static uint32_t lastPrintMs = 0;
static uint32_t localTaskCounter = 0;
static bool readyPrinted = false;

extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(18, 20);
    tft.print("MODBUS SLAVE");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(18, 56);
    tft.print("ID: ");
    tft.print(SLAVE_ID);

    tft.setCursor(18, 72);
    tft.print("Baud: ");
    tft.print(MODBUS_BAUD);

    tft.setCursor(18, 96);
    tft.print("ESC: volver a IDLE");
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    auto &tft = JWPLC_Display.tft();

    tft.fillRect(18, 120, 260, 40, ST77XX_BLACK);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);

    tft.setCursor(18, 120);
    tft.print("HR0 vida: ");
    tft.print(holdingRegs[0]);

    tft.setCursor(18, 134);
    tft.print("HR10 cmd: ");
    tft.print(holdingRegs[10]);

    tft.setCursor(18, 148);
    tft.print("Ready: ");
    tft.print(JWPLC_ModbusRTU.isReady() ? "SI" : "NO");
}

extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("SLAVE: regreso a IDLE");
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("JWPLC Basic - Modbus RTU Slave + BUS LED Auto");

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

    for (uint8_t i = 0; i < 16; i++)
    {
        holdingRegs[i] = 0;
    }

    JWPLC_ModbusRTU.setHoldingRegisters(holdingRegs, 16);

    if (JWPLC_ModbusRTU.begin(SLAVE_ID, MODBUS_BAUD, SERIAL_8N1))
    {
        Serial.println("Modbus RTU slave iniciado correctamente.");
    }
    else
    {
        Serial.print("Error iniciando Modbus RTU: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        JWPLC_Display.setErrLed(true);
    }

    JWPLC_ModbusRTU.printStatus(Serial);
}

void loop()
{
    // Mantener atendiendo peticiones Modbus.
    JWPLC_ModbusRTU.task();

    uint32_t now = millis();

    if ((uint32_t)(now - lastUpdateMs) >= 1000)
    {
        lastUpdateMs = now;

        holdingRegs[0]++;
        holdingRegs[1] = (uint16_t)(now / 1000UL);
        holdingRegs[2] = (uint16_t)localTaskCounter;

        localTaskCounter++;

        Serial.print("SLAVE HR0=");
        Serial.print(holdingRegs[0]);
        Serial.print(" HR1=");
        Serial.print(holdingRegs[1]);
        Serial.print(" HR10=");
        Serial.println(holdingRegs[10]);
    }

    if (!readyPrinted && JWPLC_Display.isReady())
    {
        readyPrinted = true;
        Serial.println("TFT lista. Slave en IDLE.");
        Serial.println("Cuando el master consulte/escriba, BUS debe parpadear por actividad RS-485.");
    }

    if ((uint32_t)(now - lastPrintMs) >= 5000)
    {
        lastPrintMs = now;
        JWPLC_ModbusRTU.printStatus(Serial);
    }

    delay(2);
}
