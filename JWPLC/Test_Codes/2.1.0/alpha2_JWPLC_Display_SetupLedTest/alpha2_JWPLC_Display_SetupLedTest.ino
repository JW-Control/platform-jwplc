#include <Arduino.h>
#include <JWPLC_Display.h>

/*
  JWPLC_Display_SetupLedTest.ino

  Objetivo:
  - Validar que las configuraciones de JWPLC_Display hechas en setup()
    no sean pisadas cuando la TFT termina de inicializar.
  - Validar LEDs IDLE configurados desde setup().
  - Validar transición IDLE -> USER y USER -> IDLE.

  Resultado esperado en IDLE:
  - RUN apagado
  - ERR encendido
  - BUS encendido
  - ETH verde manual

  Controles:
  - Cualquier botón: IDLE -> USER
  - ESC: USER -> IDLE
*/

static constexpr uint32_t USER_REFRESH_MS = 250;   // 4 FPS, suficiente para test
static constexpr uint32_t IDLE_REFRESH_MS = 1000;  // 1 FPS en IDLE

static bool readyPrinted = false;
static uint32_t userFrames = 0;

extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    userFrames = 0;

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(20, 25);
    tft.print("USER MODE");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(20, 60);
    tft.print("ESC: volver a IDLE");

    tft.setCursor(20, 80);
    tft.print("Refresh USER: ");
    tft.print(USER_REFRESH_MS);
    tft.print(" ms");

    Serial.println("Entrando a USER");
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    auto &tft = JWPLC_Display.tft();

    userFrames++;

    tft.fillRect(20, 110, 180, 20, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(20, 110);
    tft.print("Frames USER: ");
    tft.print(userFrames);
}

extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("Saliendo de USER hacia IDLE");
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("JWPLC_Display setup LED test");

    // =====================================================
    // Configuraciones hechas directamente desde setup()
    // antes de que necesariamente la TFT este lista.
    // =====================================================

    // IDLE -> USER con cualquier botón.
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);

    // USER -> IDLE con ESC.
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);

    // Periodos de refresco.
    JWPLC_Display.setIdleRefreshPeriodMs(IDLE_REFRESH_MS);
    JWPLC_Display.setUserRefreshPeriodMs(USER_REFRESH_MS);

    // =====================================================
    // Test principal:
    // Estos valores NO deben ser pisados por jwplcDisplayBeginCallback().
    // =====================================================

    JWPLC_Display.setRunLed(false);       // RUN apagado
    JWPLC_Display.setErrLed(true);        // ERR encendido
    JWPLC_Display.setBusLed(true);        // BUS encendido

    JWPLC_Display.setEthLedAuto(false);   // ETH manual
    JWPLC_Display.setEthLed(true);        // ETH verde

    Serial.println("Configuracion aplicada desde setup:");
    Serial.println("RUN = OFF");
    Serial.println("ERR = ON");
    Serial.println("BUS = ON");
    Serial.println("ETH = ON manual");
}

void loop()
{
    if (!readyPrinted && JWPLC_Display.isReady())
    {
        readyPrinted = true;

        Serial.println("TFT lista.");
        Serial.println("Revisar pantalla IDLE:");
        Serial.println("- RUN debe estar apagado");
        Serial.println("- ERR debe estar encendido");
        Serial.println("- BUS debe estar encendido");
        Serial.println("- ETH debe estar verde manual");
        Serial.println("Presiona cualquier boton para entrar a USER.");
    }

    delay(10);
}