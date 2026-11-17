/*
  04.Display_TFT_Direct

  Dibujo directo en la pantalla con el objeto JWPLC_TFT
  (JWPLC_TFT.fillScreen(), .setCursor(), .print(), .fillRect()...).

  Para coexistir correctamente con Ethernet, FRAM y microSD, el dibujo directo
  se realiza dentro de jwplcUIEnter()/jwplcUIUpdate(). El runtime ya posee el
  mutex SPI cuando invoca estas funciones.

  La hora se consulta mediante JWPLC_Time, vista cacheada y ligera del RTC.

  Controles:
  - OK  : entra a USER mediante el wake central de JWPLC_Display.
  - ESC : retorna a IDLE.
*/

static uint8_t lastSecondDrawn = 255;

extern "C" void jwplcUIEnter()
{
    JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);
    JWPLC_TFT.setTextWrap(false);
    JWPLC_TFT.setTextColor(JWPLC_TFT_CYAN, JWPLC_TFT_BLACK);
    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setCursor(20, 20);
    JWPLC_TFT.print("JWPLC TFT");

    JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE, JWPLC_TFT_BLACK);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setCursor(20, 55);
    JWPLC_TFT.print("Dibujo directo en USER");

    lastSecondDrawn = 255;
}

extern "C" void jwplcUIUpdate()
{
    if (!JWPLC_Time.valid() || JWPLC_Time.second() == lastSecondDrawn)
        return;

    lastSecondDrawn = JWPLC_Time.second();

    // Limpiar sólo la región dinámica evita parpadeos de pantalla completa.
    JWPLC_TFT.fillRect(20, 85, 180, 30, JWPLC_TFT_BLACK);
    JWPLC_TFT.setTextColor(JWPLC_TFT_YELLOW, JWPLC_TFT_BLACK);
    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setCursor(20, 90);

    if (JWPLC_Time.hour() < 10) JWPLC_TFT.print('0');
    JWPLC_TFT.print(JWPLC_Time.hour());
    JWPLC_TFT.print(':');
    if (JWPLC_Time.minute() < 10) JWPLC_TFT.print('0');
    JWPLC_TFT.print(JWPLC_Time.minute());
    JWPLC_TFT.print(':');
    if (JWPLC_Time.second() < 10) JWPLC_TFT.print('0');
    JWPLC_TFT.print(JWPLC_Time.second());
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    // Por defecto no se despierta USER con una tecla. Este ejemplo deja que el runtime
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
