#include <Arduino.h>
#include <JWPLC_Display.h>

static bool requestedUserScreen = false;

extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    const int16_t w = tft.width();
    const int16_t h = tft.height();

    tft.fillScreen(ST77XX_BLACK);

    // Marco y diagonales: primitivas base de Adafruit_GFX.
    tft.drawRect(2, 2, w - 4, h - 4, ST77XX_WHITE);
    tft.drawLine(4, 4, w - 5, h - 5, ST77XX_RED);
    tft.drawLine(w - 5, 4, 4, h - 5, ST77XX_GREEN);

    // Figuras adicionales para forzar rutas GFX distintas.
    tft.fillCircle(w / 4, h / 2, 18, ST77XX_BLUE);
    tft.drawCircle((3 * w) / 4, h / 2, 18, ST77XX_YELLOW);

    tft.fillTriangle(
        w / 2, h / 2 - 28,
        w / 2 - 20, h / 2 + 8,
        w / 2 + 20, h / 2 + 8,
        ST77XX_CYAN);

    // Texto renderizado a través de Adafruit_GFX.
    tft.fillRect(8, 12, w - 16, 48, ST77XX_BLACK);
    tft.setTextWrap(false);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(12, 18);
    tft.print("GFX");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(12, 42);
    tft.print("ALPHA5 PHYSICAL PASS");

    Serial.println("[GFX] USER screen dibujada");
    Serial.print("[GFX] TFT size: ");
    Serial.print(w);
    Serial.print('x');
    Serial.println(h);
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io,
                                                  const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;
}

extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("[GFX] USER screen cerrada");
}

void setup()
{
    Serial.begin(115200);
    delay(800);

    Serial.println();
    Serial.println("JWPLC Alpha5 - gate fisico Adafruit_GFX precompilada");

    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_DISABLED);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
    JWPLC_Display.setRunLed(true);
    JWPLC_Display.setErrLed(false);
}

void loop()
{
    if (!requestedUserScreen && JWPLC_Display.isReady())
    {
        requestedUserScreen = true;

        Serial.println("[GFX] Display ready: YES");
        JWPLC_Display.enterUserUI();
    }

    delay(20);
}
