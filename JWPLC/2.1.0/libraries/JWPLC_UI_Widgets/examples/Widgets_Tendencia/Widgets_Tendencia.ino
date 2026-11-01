/*
  JWPLC Basic - Widgets_Tendencia

  Gráfica de tendencia con JWPLC_UITrend: un horno con control todo/nada.

    - Canal 1 (naranja): temperatura del horno.
    - Canal 2 (cian):    consigna.
    - Umbral (rojo):     alarma a 85 C, línea punteada.

  La gráfica es de barrido, como un registrador: cada muestra dibuja solo su
  columna y borra un hueco por delante del cursor. Nunca se redibuja toda la
  gráfica por muestra.

  Uso: OK sale de IDLE. En la pantalla, ▲ ▼ cambian la consigna en 5 C,
  OK pausa o reanuda la gráfica y ESC vuelve a IDLE.

  El horno se simula en loop() para que el ejemplo funcione sin cableado; en
  una aplicación real, temperatura() leería una entrada analógica.
*/

#include <JWPLC_UI_Widgets.h>

static const JWPLC_UITheme &T = JWPLC_UI_THEME_DARK;

static const uint16_t COLOR_TEMP = 0xFD20;  // naranja
static const uint16_t COLOR_SP = 0x07FF;    // cian
static const uint16_t COLOR_ALARMA = 0xF800; // rojo

static const float T_MIN = 0.0f;
static const float T_MAX = 100.0f;
static const float T_ALARMA = 85.0f;
static const uint32_t PERIODO_MUESTRA_MS = 250; // 292 columnas = 73 s por pantalla

// Gráfica: ocupa todo el ancho, entre la leyenda y el pie.
static const int16_t G_X = 0;
static const int16_t G_Y = 44;
static const int16_t G_W = 320;
static const int16_t G_H = 104;

static JWPLC_UITrend g_tendencia;
// Un byte por columna y canal; la escala vertical ocupa SCALE_W px.
static const int16_t G_PLOT_W = JWPLC_UITrend::plotWidthFor(G_W, true);
static uint8_t g_histTemp[G_PLOT_W];
static uint8_t g_histSp[G_PLOT_W];

// Proceso simulado (lo escribe loop(), lo lee la pantalla).
static volatile float g_temp = 20.0f;
static volatile float g_consigna = 60.0f;
static volatile bool g_calefactor = false;

static bool g_pausa = false;
static uint32_t g_ultimaMuestra = 0;
static bool g_leyendaDibujada = false;
static bool g_pieDibujado = false;
static volatile bool g_pedirIdle = false;

// Un campo de texto de la leyenda: solo se redibuja si su texto cambió, y
// solo se borra su propio rectángulo (sin parpadeo del resto).
struct Campo
{
    int16_t x;
    int16_t w;
    char dibujado[16];
};

static Campo g_campoTemp = {20, 100, ""};
static Campo g_campoSp = {136, 90, ""};
static Campo g_campoEstado = {232, 84, ""};

// ---------------------------------------------------------------- proceso

static float temperatura()
{
    return g_temp;
}

// Horno de primer orden con control todo/nada e histéresis de ±2 C.
static void simularHorno()
{
    static uint32_t ultimo = 0;
    const uint32_t ahora = millis();
    if (ahora - ultimo < 100)
    {
        return;
    }
    ultimo = ahora;

    float t = g_temp;
    const float sp = g_consigna;
    if (t < sp - 2.0f)
    {
        g_calefactor = true;
    }
    else if (t > sp + 2.0f)
    {
        g_calefactor = false;
    }
    const float objetivo = g_calefactor ? 140.0f : 20.0f; // temperatura final
    t += (objetivo - t) * 0.1f / 25.0f;                   // tau = 25 s
    g_temp = t;
}

// ---------------------------------------------------------------- pantalla

static void dibujarEncabezado(JWPLC_TFTClass &tft)
{
    tft.fillRect(0, 0, 320, 22, T.header);
    tft.setFont(JWPLC_FONT_GLCD);
    tft.setTextSize(2);
    tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
    tft.setTextColor(T.headerText, T.header);
    tft.drawString("Horno 1", 8, 11);
    tft.setTextSize(1);
    tft.setTextDatum(JWPLC_TFTDatum::TOP_LEFT);
}

static void dibujarCampo(JWPLC_TFTClass &tft, Campo &c, const char *texto, uint16_t color,
                         bool derecha)
{
    if (strcmp(texto, c.dibujado) == 0)
    {
        return;
    }
    snprintf(c.dibujado, sizeof(c.dibujado), "%s", texto);
    tft.fillRect(c.x, 25, c.w, 16, T.background);
    tft.setFont(JWPLC_FONT_16);
    tft.setTextSize(1);
    tft.setTextColor(color, T.background);
    tft.setTextDatum(derecha ? JWPLC_TFTDatum::MIDDLE_RIGHT : JWPLC_TFTDatum::MIDDLE_LEFT);
    tft.drawString(texto, derecha ? (int16_t)(c.x + c.w - 4) : c.x, 33);
    tft.setFont(JWPLC_FONT_GLCD);
    tft.setTextDatum(JWPLC_TFTDatum::TOP_LEFT);
}

static void dibujarLeyenda(JWPLC_TFTClass &tft, bool forzar)
{
    if (forzar)
    {
        tft.fillRect(0, 24, 320, 18, T.background);
        tft.fillRect(8, 29, 8, 8, COLOR_TEMP);
        tft.fillRect(124, 29, 8, 8, COLOR_SP);
        g_campoTemp.dibujado[0] = '\0';
        g_campoSp.dibujado[0] = '\0';
        g_campoEstado.dibujado[0] = '\0';
    }

    char texto[16];
    const int t10 = (int)(temperatura() * 10.0f + 0.5f);
    snprintf(texto, sizeof(texto), "Temp %d.%d C", t10 / 10, t10 % 10);
    dibujarCampo(tft, g_campoTemp, texto, T.text, false);

    snprintf(texto, sizeof(texto), "SP %d C", (int)g_consigna);
    dibujarCampo(tft, g_campoSp, texto, T.text, false);

    if (g_pausa)
    {
        dibujarCampo(tft, g_campoEstado, "PAUSA", T.warn, true);
    }
    else if (g_calefactor)
    {
        dibujarCampo(tft, g_campoEstado, "Calef. ON", T.ok, true);
    }
    else
    {
        dibujarCampo(tft, g_campoEstado, "Calef. OFF", T.textMuted, true);
    }
}

static void dibujarPie(JWPLC_TFTClass &tft)
{
    tft.fillRect(0, 150, 320, 20, T.background);
    tft.setFont(JWPLC_FONT_GLCD);
    tft.setTextSize(1);
    tft.setTextColor(T.textMuted, T.background);
    tft.setCursor(8, 156);
    tft.print("73 s/pant.  UP/DN consigna  OK pausa  ESC");
    g_pieDibujado = true;
}

extern "C" void jwplcUIEnter()
{
    // Teclas pulsadas en IDLE: no deben actuar sobre esta pantalla.
    JWPLC_Display.clearPendingInput();

    auto &tft = JWPLC_Display.tft();
    tft.fillScreen(T.background);
    dibujarEncabezado(tft);

    // El historial se conserva entre visitas: solo se redibuja.
    g_tendencia.invalidate();
    g_leyendaDibujada = false;
    g_pieDibujado = false;
}

extern "C" void jwplcUIUpdate()
{
    auto &tft = JWPLC_Display.tft();

    if (JWPLC_Buttons.pressed(BTN_UP) && g_consigna < 90.0f)
    {
        g_consigna = g_consigna + 5.0f;
    }
    if (JWPLC_Buttons.pressed(BTN_DOWN) && g_consigna > 20.0f)
    {
        g_consigna = g_consigna - 5.0f;
    }
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        g_pausa = !g_pausa;
    }
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        g_pedirIdle = true; // goIdle() se llama desde loop(), fuera del batch
    }

    const uint32_t ahora = millis();
    if (g_pausa)
    {
        g_ultimaMuestra = ahora;
    }
    else if (ahora - g_ultimaMuestra >= PERIODO_MUESTRA_MS)
    {
        // Periodo fijo sin deriva; tras una pausa larga no se recupera de golpe.
        g_ultimaMuestra = (ahora - g_ultimaMuestra < 2 * PERIODO_MUESTRA_MS)
                              ? g_ultimaMuestra + PERIODO_MUESTRA_MS
                              : ahora;
        const float muestra[2] = {temperatura(), g_consigna};
        g_tendencia.push(muestra);
    }

    g_tendencia.draw(tft);
    dibujarLeyenda(tft, !g_leyendaDibujada);
    g_leyendaDibujada = true;
    if (!g_pieDibujado)
    {
        dibujarPie(tft);
    }
}

extern "C" void jwplcUIExit()
{
}

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - Widgets_Tendencia");

    g_tendencia.begin(G_X, G_Y, G_W, G_H, T_MIN, T_MAX, T);
    g_tendencia.showScale(0); // valores enteros a la izquierda
    g_tendencia.addChannel(COLOR_TEMP, g_histTemp, sizeof(g_histTemp));
    g_tendencia.addChannel(COLOR_SP, g_histSp, sizeof(g_histSp));
    g_tendencia.addThreshold(T_ALARMA, COLOR_ALARMA);

    JWPLC_Display.setIdleWakeButton(BTN_OK);
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
    // La pantalla usa ESC: la vuelta a IDLE la decide el sketch.
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
    JWPLC_Display.setUserRefreshPeriodMs(30);
    JWPLC_Display.clearPendingInput();
}

void loop()
{
    simularHorno();

    if (g_pedirIdle)
    {
        g_pedirIdle = false;
        JWPLC_Display.goIdle();
    }
}
