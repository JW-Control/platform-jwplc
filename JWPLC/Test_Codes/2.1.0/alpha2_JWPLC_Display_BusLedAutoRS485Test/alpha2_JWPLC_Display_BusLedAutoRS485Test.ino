#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_RS485.h>

/*
  Display_BusLedAutoRS485Test.ino

  Objetivo:
  - Validar BUS LED automático en IDLE.
  - BUS debe encenderse brevemente cuando se usa JWPLC_RS485.
  - No requiere otro dispositivo RS-485 para ver TX.
  - Si hay otro equipo transmitiendo, también debe encender por RX.

  Controles:
  - Cualquier botón: USER
  - OK en USER: enviar trama RS-485 de prueba
  - ESC: volver a IDLE
*/

static constexpr uint32_t IDLE_REFRESH_MS = 1000;
static constexpr uint32_t USER_REFRESH_MS = 100;

static bool readyPrinted = false;
static uint32_t txCounter = 0;
static uint32_t rxCounter = 0;

static void sendRs485TestFrame()
{
    const char frame[] = "JWPLC BUS TEST\r\n";

    size_t written = JWPLC_RS485.write((const uint8_t *)frame, sizeof(frame) - 1);

    txCounter++;

    Serial.print("RS485 TX #");
    Serial.print(txCounter);
    Serial.print(" bytes=");
    Serial.println(written);
}

extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(18, 20);
    tft.print("BUS AUTO TEST");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(18, 58);
    tft.print("OK : enviar RS485");

    tft.setCursor(18, 74);
    tft.print("ESC: volver a IDLE");

    tft.setCursor(18, 98);
    tft.print("Mira LED BUS en IDLE");

    Serial.println("Entrando a USER");
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    auto &tft = JWPLC_Display.tft();

    if (JWPLC_Display.buttonsReady() && JWPLC_Buttons.pressed(BTN_OK))
    {
        sendRs485TestFrame();

        tft.fillRect(18, 125, 250, 14, ST77XX_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
        tft.setCursor(18, 125);
        tft.print("TX RS485 #");
        tft.print(txCounter);
    }

    while (JWPLC_RS485.available() > 0)
    {
        int value = JWPLC_RS485.read();

        if (value >= 0)
        {
            rxCounter++;
        }
    }

    tft.fillRect(18, 145, 250, 14, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setCursor(18, 145);
    tft.print("RX bytes: ");
    tft.print(rxCounter);
}

extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("Saliendo a IDLE. Revisar LED BUS.");
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("JWPLC_Display BUS LED AUTO RS485 Test");

    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);

    // JWPLC_Display.setIdleRefreshPeriodMs(IDLE_REFRESH_MS);
    // JWPLC_Display.setUserRefreshPeriodMs(USER_REFRESH_MS);

    JWPLC_Display.setRunLed(true);
    JWPLC_Display.setErrLed(false);

    // Nuevo modo automático BUS.
    JWPLC_Display.setBusLedAuto(true);

    // ETH puede quedar automático también.
    JWPLC_Display.setEthLedAuto(true);

    if (JWPLC_RS485.begin(115200, SERIAL_8N1))
    {
        Serial.println("RS485 iniciado correctamente.");
    }
    else
    {
        Serial.print("RS485 error: ");
        Serial.println(JWPLC_RS485.lastErrorString());
    }

    Serial.println("En IDLE, BUS debe encenderse cuando haya TX/RX RS485.");
}

void loop()
{
    if (!readyPrinted && JWPLC_Display.isReady())
    {
        readyPrinted = true;
        Serial.println("TFT lista.");
        Serial.println("Presiona cualquier boton para USER.");
        Serial.println("Presiona OK para enviar por RS485.");
        Serial.println("Vuelve a IDLE con ESC y observa BUS.");
    }

    delay(10);
}