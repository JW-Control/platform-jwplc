#include <Arduino.h>
#include <JWPLC_Display.h>

/*
  Display_SetupLedAutoTest.ino

  Objetivo:
  - Validar que setEthLedAuto(true) configurado desde setup()
    no sea pisado por la inicialización interna de la TFT.
  - Validar que RUN / ERR / BUS también respeten configuración desde setup().
  - Validar transición IDLE -> USER y USER -> IDLE.

  Resultado esperado al iniciar en IDLE:
  - RUN encendido
  - ERR apagado
  - BUS apagado
  - ETH en modo automático:
      depende del estado real de Ethernet.
      Puede verse apagado, rojo o verde según estado/link.

  Controles:
  - Cualquier botón: entra a USER.
  - OK en USER: cambia escenario de LEDs.
  - ESC en USER: vuelve a IDLE para observar los LEDs.
*/

static constexpr uint32_t IDLE_REFRESH_MS = 1000;
static constexpr uint32_t USER_REFRESH_MS = 200;

enum LedScenario : uint8_t
{
    SCENARIO_AUTO_NORMAL = 0,
    SCENARIO_AUTO_WITH_FLAGS,
    SCENARIO_MANUAL_ETH_OFF,
    SCENARIO_MANUAL_ETH_ON,
    SCENARIO_COUNT
};

static LedScenario scenario = SCENARIO_AUTO_NORMAL;
static bool readyPrinted = false;
static uint32_t userFrames = 0;

static const char *scenarioName(LedScenario s)
{
    switch (s)
    {
    case SCENARIO_AUTO_NORMAL:
        return "AUTO normal";

    case SCENARIO_AUTO_WITH_FLAGS:
        return "AUTO + ERR/BUS";

    case SCENARIO_MANUAL_ETH_OFF:
        return "MANUAL ETH OFF";

    case SCENARIO_MANUAL_ETH_ON:
        return "MANUAL ETH ON";

    default:
        return "desconocido";
    }
}

static void applyLedScenario()
{
    switch (scenario)
    {
    case SCENARIO_AUTO_NORMAL:
        JWPLC_Display.setRunLed(true);
        JWPLC_Display.setErrLed(false);
        JWPLC_Display.setBusLed(false);
        JWPLC_Display.setEthLedAuto(true);
        break;

    case SCENARIO_AUTO_WITH_FLAGS:
        JWPLC_Display.setRunLed(true);
        JWPLC_Display.setErrLed(true);
        JWPLC_Display.setBusLed(true);
        JWPLC_Display.setEthLedAuto(true);
        break;

    case SCENARIO_MANUAL_ETH_OFF:
        JWPLC_Display.setRunLed(false);
        JWPLC_Display.setErrLed(true);
        JWPLC_Display.setBusLed(false);
        JWPLC_Display.setEthLedAuto(false);
        JWPLC_Display.setEthLed(false);
        break;

    case SCENARIO_MANUAL_ETH_ON:
        JWPLC_Display.setRunLed(false);
        JWPLC_Display.setErrLed(false);
        JWPLC_Display.setBusLed(true);
        JWPLC_Display.setEthLedAuto(false);
        JWPLC_Display.setEthLed(true);
        break;

    default:
        break;
    }

    Serial.print("Escenario aplicado: ");
    Serial.println(scenarioName(scenario));
}

static void nextScenario()
{
    scenario = (LedScenario)((uint8_t)(scenario + 1) % SCENARIO_COUNT);
    applyLedScenario();
}

extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    userFrames = 0;

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(18, 22);
    tft.print("LED AUTO TEST");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

    tft.setCursor(18, 58);
    tft.print("OK : cambiar escenario");

    tft.setCursor(18, 74);
    tft.print("ESC: volver a IDLE");

    tft.setCursor(18, 98);
    tft.print("Escenario actual:");

    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(18, 114);
    tft.print(scenarioName(scenario));

    Serial.println("Entrando a USER");
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    auto &tft = JWPLC_Display.tft();

    userFrames++;

    if (JWPLC_Display.buttonsReady() && JWPLC_Buttons.pressed(BTN_OK))
    {
        nextScenario();

        tft.fillRect(18, 114, 220, 12, ST77XX_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
        tft.setCursor(18, 114);
        tft.print(scenarioName(scenario));

        tft.fillRect(18, 138, 270, 12, ST77XX_BLACK);
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.setCursor(18, 138);
        tft.print("Presiona ESC y revisa IDLE");
    }

    tft.fillRect(230, 4, 80, 12, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setCursor(230, 4);
    tft.print("F:");
    tft.print(userFrames);
}

extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("Saliendo de USER hacia IDLE");
    Serial.print("Revisar LEDs IDLE para escenario: ");
    Serial.println(scenarioName(scenario));
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("JWPLC_Display - Setup LED Auto Test");

    // Configuración directa desde setup().
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);

    //JWPLC_Display.setIdleRefreshPeriodMs(IDLE_REFRESH_MS);
    //JWPLC_Display.setUserRefreshPeriodMs(USER_REFRESH_MS);

    // Escenario inicial aplicado desde setup(), antes de que la TFT
    // necesariamente esté lista.
    scenario = SCENARIO_AUTO_NORMAL;
    applyLedScenario();

    Serial.println("Configuracion inicial hecha desde setup().");
    Serial.println("Esperado en IDLE:");
    Serial.println("- RUN ON");
    Serial.println("- ERR OFF");
    Serial.println("- BUS OFF");
    Serial.println("- ETH AUTO segun estado real de Ethernet");
}

void loop()
{
    if (!readyPrinted && JWPLC_Display.isReady())
    {
        readyPrinted = true;

        Serial.println("TFT lista.");
        Serial.println("Presiona cualquier boton para entrar a USER.");
        Serial.println("Luego presiona OK para cambiar escenario.");
        Serial.println("Presiona ESC para volver a IDLE y revisar LEDs.");
    }

    delay(10);
}