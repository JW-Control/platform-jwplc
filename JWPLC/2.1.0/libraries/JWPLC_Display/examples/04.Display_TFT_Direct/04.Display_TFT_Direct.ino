/*
  04.Display_TFT_Direct

  Acceso directo a Adafruit_ST7789 mediante:

      auto &tft = JWPLC_Display.tft();

  Para coexistir correctamente con Ethernet, FRAM y microSD, el dibujo directo
  se realiza dentro de los callbacks USER. El runtime ya posee el mutex SPI
  cuando invoca estos callbacks.

  Este ejemplo conserva el callback legacy de refresh de forma deliberada:
  recibe directamente los snapshots JWPLC_IOState/JWPLC_RTCState del runtime.

  Controles:
  - OK  : entra a USER mediante el wake central de JWPLC_Display.
  - ESC : retorna a IDLE.
*/

#include <JWPLC_Display.h>

static uint8_t lastSecondDrawn = 255;

extern "C" void jwplcUserDisplayEnterCallback()
{
    // Este callback se ejecuta con el bus TFT protegido por el runtime.
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextWrap(false);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 20);
    tft.print("JWPLC TFT");

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setCursor(20, 55);
    tft.print("Dibujo directo en USER");

    lastSecondDrawn = 255;
}

extern "C" void jwplcUserDisplayRefreshCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
    (void)io;

    if (rtc == nullptr || !rtc->valid || rtc->second == lastSecondDrawn)
        return;

    lastSecondDrawn = rtc->second;

    auto &tft = JWPLC_Display.tft();

    // Limpiar sólo la región dinámica evita parpadeos de pantalla completa.
    tft.fillRect(20, 85, 180, 30, ST77XX_BLACK);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 90);

    if (rtc->hour < 10) tft.print('0');
    tft.print(rtc->hour);
    tft.print(':');
    if (rtc->minute < 10) tft.print('0');
    tft.print(rtc->minute);
    tft.print(':');
    if (rtc->second < 10) tft.print('0');
    tft.print(rtc->second);
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    // Alpha8+ no despierta USER por defecto. Este ejemplo deja que el runtime
    // gestione la transición completa OK -> USER y ESC -> IDLE.
    JWPLC_Display.setIdleWakeButton(BTN_OK);
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);

    // 100 ms es suficiente para este ejemplo; el callback sólo redibuja
    // realmente cuando cambia el segundo del RTC.
    JWPLC_Display.setUserRefreshPeriodMs(100);
    JWPLC_Display.clearPendingInput();

    Serial.println("JWPLC Basic - TFT direct");
    Serial.println("OK=USER | ESC=IDLE");
}

void loop()
{
    delay(5);
}
