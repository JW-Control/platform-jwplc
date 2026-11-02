/*
  JWPLC Basic - Widgets_Tendencia

  Gráfica de tendencia con JWPLC_UITrend: un horno con control todo/nada.

    - Canal 1 (naranja): temperatura del horno.
    - Canal 2 (cian):    consigna.
    - Umbral (rojo):     alarma a 85 C, línea punteada.
    - Escala vertical a la izquierda (0, 25, 50, 75, 100 y la alarma).

  La gráfica es de barrido, como un registrador: cada muestra dibuja solo su
  columna y borra un hueco por delante del cursor. Nunca se redibuja toda la
  gráfica por muestra.

  Uso: OK sale de IDLE. En la pantalla, ▲ ▼ cambian la consigna en 5 C,
  OK pausa o reanuda la gráfica y ESC vuelve a IDLE.

  El horno se simula en loop() para que el ejemplo funcione sin cableado; en
  una aplicación real, temperatura() leería una entrada analógica.

  JWPLC_TFT es la pantalla. Los callbacks jwplcUIEnter/jwplcUIUpdate ya se
  ejecutan con el bus de la TFT adquirido por JWPLC_Display.
*/

#include <JWPLC_UI_Widgets.h>

static JWPLC_UITheme T = JWPLC_UI_THEME_DARK;

static const uint16_t COLOR_TEMP = 0xFD20;   // naranja
static const uint16_t COLOR_SP = 0x07FF;     // cian
static const uint16_t COLOR_ALARMA = 0xF800; // rojo

static const uint32_t PERIODO_MUESTRA_MS = 250; // 292 columnas = 73 s por pantalla

static JWPLC_UITrend g_tendencia;

// Proceso simulado (lo escribe loop(), lo lee la pantalla).
static volatile float g_temp = 20.0f;
static volatile float g_consigna = 60.0f;
static volatile bool g_calefactor = false;

static bool g_pausa = false;
static uint32_t g_ultimaMuestra = 0;
static bool g_leyendaDibujada = false;
static bool g_pieDibujado = false;
static volatile bool g_pedirIdle = false;

// Campos de texto de la leyenda: cada uno se redibuja solo si su texto
// cambió, y solo se borra su propio rectángulo (sin parpadeo del resto).
enum : uint8_t
{
    CAMPO_TEMP = 0,
    CAMPO_SP,
    CAMPO_ESTADO,
    N_CAMPOS
};
static const int16_t CAMPO_X[N_CAMPOS] = {20, 136, 232};
static const int16_t CAMPO_W[N_CAMPOS] = {100, 90, 84};
static String g_campoDibujado[N_CAMPOS];

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

static void dibujarEncabezado()
{
    JWPLC_TFT.fillRect(0, 0, 320, 22, T.header);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setTextDatum(DATUM_MIDDLE_LEFT);
    JWPLC_TFT.setTextColor(T.headerText, T.header);
    JWPLC_TFT.drawString("Horno 1", 8, 11);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
}

static void dibujarCampo(uint8_t campo, String texto, uint16_t color, bool derecha)
{
    if (texto == g_campoDibujado[campo])
    {
        return;
    }
    g_campoDibujado[campo] = texto;

    const int16_t x = CAMPO_X[campo];
    const int16_t w = CAMPO_W[campo];
    JWPLC_TFT.fillRect(x, 25, w, 16, T.background);
    JWPLC_TFT.setFont(JWPLC_FONT_16);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextColor(color, T.background);
    JWPLC_TFT.setTextDatum(derecha ? DATUM_MIDDLE_RIGHT : DATUM_MIDDLE_LEFT);
    JWPLC_TFT.drawString(texto.c_str(), derecha ? (int16_t)(x + w - 4) : x, 33);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
}

static void dibujarLeyenda(bool forzar)
{
    if (forzar)
    {
        JWPLC_TFT.fillRect(0, 24, 320, 18, T.background);
        JWPLC_TFT.fillRect(8, 29, 8, 8, COLOR_TEMP);
        JWPLC_TFT.fillRect(124, 29, 8, 8, COLOR_SP);
        for (uint8_t i = 0; i < N_CAMPOS; i++)
        {
            g_campoDibujado[i] = "";
        }
    }

    dibujarCampo(CAMPO_TEMP, "Temp " + String(temperatura(), 1) + " C", T.text, false);
    dibujarCampo(CAMPO_SP, "SP " + String((int)g_consigna) + " C", T.text, false);

    if (g_pausa)
    {
        dibujarCampo(CAMPO_ESTADO, "PAUSA", T.warn, true);
    }
    else if (g_calefactor)
    {
        dibujarCampo(CAMPO_ESTADO, "Calef. ON", T.ok, true);
    }
    else
    {
        dibujarCampo(CAMPO_ESTADO, "Calef. OFF", T.textMuted, true);
    }
}

static void dibujarPie()
{
    JWPLC_TFT.fillRect(0, 150, 320, 20, T.background);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextColor(T.textMuted, T.background);
    JWPLC_TFT.setCursor(8, 156);
    JWPLC_TFT.print("73 s/pant.  UP/DN consigna  OK pausa  ESC");
    g_pieDibujado = true;
}

extern "C" void jwplcUIEnter()
{
    // Teclas pulsadas en IDLE: no deben actuar sobre esta pantalla.
    JWPLC_Display.clearPendingInput();

    JWPLC_TFT.fillScreen(T.background);
    dibujarEncabezado();

    // El historial se conserva entre visitas: solo se redibuja.
    g_tendencia.invalidate();
    g_leyendaDibujada = false;
    g_pieDibujado = false;
}

extern "C" void jwplcUIUpdate()
{
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
        g_tendencia.push(temperatura(), g_consigna);
    }

    g_tendencia.draw();
    dibujarLeyenda(!g_leyendaDibujada);
    g_leyendaDibujada = true;
    if (!g_pieDibujado)
    {
        dibujarPie();
    }
}

extern "C" void jwplcUIExit()
{
}

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - Widgets_Tendencia");

    // x, y, ancho, alto, valor mínimo, valor máximo
    g_tendencia.begin(0, 44, 320, 104, 0, 100, T);
    g_tendencia.showScale(0); // valores enteros a la izquierda
    g_tendencia.addChannel(COLOR_TEMP);
    g_tendencia.addChannel(COLOR_SP);
    g_tendencia.addThreshold(85, COLOR_ALARMA);

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
