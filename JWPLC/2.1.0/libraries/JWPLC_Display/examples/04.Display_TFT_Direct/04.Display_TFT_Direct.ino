/*
  04.Display_TFT_Direct

  Acceso directo a Adafruit_ST7789 mediante:

      auto &tft = JWPLC_Display.tft();

  Para coexistir correctamente con Ethernet, FRAM y microSD, el dibujo directo
  se realiza dentro de jwplcUIEnter()/jwplcUIUpdate(). El runtime ya posee el
  mutex SPI cuando invoca estas funciones.

  La hora se consulta mediante JWPLC_Time, vista cacheada y ligera del RTC.

  Controles:
  - OK  : entra a USER mediante el wake central de JWPLC_Display.
  - ESC : retorna a IDLE.
*/

#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>

static uint8_t lastSecondDrawn = 255;

extern "C" void jwplcUIEnter()
{
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

extern "C" void jwplcUIUpdate()
{
    if (!JWPLC_Time.valid() || JWPLC_Time.second() == lastSecondDrawn)
        return;

    lastSecondDrawn = JWPLC_Time.second();

    auto &tft = JWPLC_Display.tft();

    // Limpiar sólo la región dinámica evita parpadeos de pantalla completa.
    tft.fillRect(20, 85, 180, 30, ST77XX_BLACK);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 90);

    if (JWPLC_Time.hour() < 10) tft.print('0');
    tft.print(JWPLC_Time.hour());
    tft.print(':');
    if (JWPLC_Time.minute() < 10) tft.print('0');
    tft.print(JWPLC_Time.minute());
    tft.print(':');
    if (JWPLC_Time.second() < 10) tft.print('0');
    tft.print(JWPLC_Time.second());
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

    // 100 ms es suficiente para este ejemplo; jwplcUIUpdate() sólo redibuja
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
