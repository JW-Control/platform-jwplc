/*
  01.Display_IDLE_Status

  Ejemplo básico de la pantalla IDLE del JWPLC Basic.

  Funciones mostradas:
  - setIdleWakeMode(): define si una tecla puede abrir USER automáticamente.
  - setRunLed(): controla el indicador RUN.
  - setErrCode(): muestra/limpia un código de error de aplicación.
  - setBusLedAuto()/setEthLedAuto(): dejan BUS y ETH en modo diagnóstico automático.
  - forceRedraw(): solicita un redibujado completo de la vista actual.

  Controles:
  - OK    : alterna ERR entre vacío y "TST".
  - DOWN  : alterna RUN.
  - RIGHT : fuerza un redibujado.
*/

#include <JWPLC_Display.h>

static bool runState = true;
static bool testError = false;

void setup()
{
    Serial.begin(115200);
    delay(300);

    // Alpha8 arranca con wake deshabilitado por defecto. Se deja explícito
    // aquí para que el ejemplo sea autoexplicativo.
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_DISABLED);

    JWPLC_Display.setRunLed(runState);
    JWPLC_Display.setErrCode("");
    JWPLC_Display.setBusLedAuto(true);
    JWPLC_Display.setEthLedAuto(true);

    JWPLC_Buttons.clearPendingInput();

    Serial.println("JWPLC Basic - Display IDLE");
    Serial.println("OK=ERR | DOWN=RUN | RIGHT=forceRedraw");
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        testError = !testError;

        // ERR pertenece a la aplicación. Vacío significa sin error.
        JWPLC_Display.setErrCode(testError ? "TST" : "");

        Serial.print("ERR = ");
        Serial.println(testError ? "TST" : "clear");
    }

    if (JWPLC_Buttons.pressed(BTN_DOWN))
    {
        runState = !runState;
        JWPLC_Display.setRunLed(runState);

        Serial.print("RUN = ");
        Serial.println(runState);
    }

    if (JWPLC_Buttons.pressed(BTN_RIGHT))
    {
        JWPLC_Display.forceRedraw();
        Serial.println("Display redraw solicitado");
    }

    delay(5);
}
