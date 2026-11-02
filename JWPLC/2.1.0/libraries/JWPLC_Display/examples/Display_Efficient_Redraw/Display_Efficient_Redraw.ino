/*
  Display_Efficient_Redraw

  Ejemplo de buenas prácticas para reducir consumo de recursos al dibujar
  en la TFT ST7789 del JWPLC Basic.

  Idea principal:
  - No usar fillScreen() en cada refresco.
  - Redibujar solo las zonas que cambian.
  - Guardar últimos valores dibujados.
  - Usar snprintf() en lugar de concatenaciones String frecuentes.
  - Resetear la caché de dibujo al volver a entrar a USER.
*/

bool displayConfigured = false;

static uint32_t g_processCounter = 0;
static bool g_runState = false;

static bool g_firstDraw = true;
static bool g_lastRunState = false;
static uint32_t g_lastCounter = 0xFFFFFFFF;

extern "C" void jwplcUIEnter()
{
    // fillScreen() está bien al entrar a una pantalla nueva.
    JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);

    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setTextColor(JWPLC_TFT_CYAN);
    JWPLC_TFT.setCursor(10, 15);
    JWPLC_TFT.print("Proceso");

    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE);

    JWPLC_TFT.setCursor(10, 55);
    JWPLC_TFT.print("Estado:");

    JWPLC_TFT.setCursor(10, 80);
    JWPLC_TFT.print("Contador:");

    JWPLC_TFT.setCursor(10, 120);
    JWPLC_TFT.print("Redibujo parcial");

    // La pantalla acaba de limpiarse: forzar una primera actualización de
    // todos los campos dinámicos, incluso si se reentra antes de que cambien.
    g_firstDraw = true;
    g_lastRunState = !g_runState;
    g_lastCounter = 0xFFFFFFFF;
}

extern "C" void jwplcUIUpdate()
{
    if (g_firstDraw || g_runState != g_lastRunState)
    {
        g_lastRunState = g_runState;

        // Solo limpiar el campo del estado, no toda la pantalla.
        JWPLC_TFT.fillRect(90, 55, 120, 14, JWPLC_TFT_BLACK);
        JWPLC_TFT.setCursor(90, 55);
        JWPLC_TFT.setTextColor(g_runState ? JWPLC_TFT_GREEN : JWPLC_TFT_RED);
        JWPLC_TFT.print(g_runState ? "RUN" : "STOP");
    }

    if (g_firstDraw || g_processCounter != g_lastCounter)
    {
        g_lastCounter = g_processCounter;

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)g_processCounter);

        // Solo limpiar el campo del contador.
        JWPLC_TFT.fillRect(90, 80, 120, 14, JWPLC_TFT_BLACK);
        JWPLC_TFT.setCursor(90, 80);
        JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE);
        JWPLC_TFT.print(buffer);
    }

    g_firstDraw = false;
}

extern "C" void jwplcUIExit()
{
    Serial.println("Saliendo de USER hacia IDLE");
}

void setup()
{
    Serial.begin(115200);
    delay(1200);

    // Por defecto no se despierta USER con una tecla. OK se habilita de forma explícita.
    JWPLC_Display.setIdleWakeButton(BTN_OK);
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
    JWPLC_Display.setIdleTimeoutMs(8000);
    JWPLC_Display.setUserRefreshPeriodMs(200);
    JWPLC_Display.clearPendingInput();

    Serial.println();
    Serial.println("JWPLC_Display efficient redraw test");
    Serial.println("OK=USER | retorno automatico en 8s");
}

void loop()
{
    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;
        Serial.println("Display configurado");
    }

    if (!displayConfigured)
    {
        return;
    }

    static unsigned long lastProcessMs = 0;
    unsigned long now = millis();

    if (now - lastProcessMs >= 1000)
    {
        lastProcessMs = now;

        g_processCounter++;
        g_runState = !g_runState;

        JWPLC_Display.setRunLed(g_runState);
        JWPLC_Display.setErrCode(""); // sin error

        Serial.print("Counter: ");
        Serial.println(g_processCounter);
    }
}
