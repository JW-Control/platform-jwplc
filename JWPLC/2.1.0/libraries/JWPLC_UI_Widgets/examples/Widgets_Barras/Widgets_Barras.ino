/*
  JWPLC Basic - Widgets_Barras

  Barras de valores analógicos con JWPLC_UIBar, como las de los textos de
  aviso del LOGO!: valor entre un mínimo y un máximo, con umbral que cambia
  el color de la barra.

    - Nivel del tanque (horizontal, umbral alto 90 %).
    - Presión (horizontal, umbral 8 bar).
    - Temperatura (vertical, umbral 70 C).

  Los valores se simulan en loop() para que el ejemplo funcione sin
  cableado; en una aplicación real vendrían de las entradas analógicas.
  Uso: OK sale de IDLE y ESC vuelve. Cada barra solo redibuja el tramo que
  cambió.
*/

#include <JWPLC_UI_Widgets.h>

static JWPLC_UITheme T = JWPLC_UI_THEME_DARK;

static JWPLC_UIBar g_nivel;
static JWPLC_UIBar g_presion;
static JWPLC_UIBar g_temp;

static volatile float g_valorNivel = 0;
static volatile float g_valorPresion = 0;
static volatile float g_valorTemp = 0;

static String g_textoDibujado[3];
static volatile bool g_pedirIdle = false;

// Proceso simulado: ondas lentas con distinto periodo.
static void simular()
{
    const float t = millis() / 1000.0f;
    g_valorNivel = 50.0f + 45.0f * sinf(t * 0.35f);
    g_valorPresion = 5.0f + 4.0f * sinf(t * 0.6f + 1.0f);
    g_valorTemp = 45.0f + 35.0f * sinf(t * 0.25f + 2.0f);
}

static void dibujarEncabezado()
{
    JWPLC_TFT.fillRect(0, 0, 320, 22, T.header);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setTextDatum(DATUM_MIDDLE_LEFT);
    JWPLC_TFT.setTextColor(T.headerText, T.header);
    JWPLC_TFT.drawString("Analogicas", 8, 11);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
}

static void etiqueta(const char t[], int16_t x, int16_t y)
{
    JWPLC_TFT.setFont(JWPLC_FONT_16);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(T.textMuted, T.background);
    JWPLC_TFT.drawString(t, x, y);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
}

// Valor grande a la derecha de su etiqueta; solo si cambió el texto.
static void valor(uint8_t i, String texto, int16_t x, int16_t y, int16_t ancho)
{
    if (texto == g_textoDibujado[i])
    {
        return;
    }
    g_textoDibujado[i] = texto;
    JWPLC_TFT.fillRect(x - ancho, y, ancho, 26, T.background);
    JWPLC_TFT.setFont(JWPLC_FONT_26);
    JWPLC_TFT.setTextDatum(DATUM_TOP_RIGHT);
    JWPLC_TFT.setTextColor(T.text, T.background);
    JWPLC_TFT.drawString(texto.c_str(), x, y);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
}

extern "C" void jwplcUIEnter()
{
    // Teclas pulsadas en IDLE: no deben actuar sobre esta pantalla.
    JWPLC_Display.clearPendingInput();

    JWPLC_TFT.fillScreen(T.background);
    dibujarEncabezado();
    etiqueta("Nivel tanque", 8, 30);
    etiqueta("Presion", 8, 92);
    etiqueta("Temp.", 262, 30);

    g_nivel.invalidate();
    g_presion.invalidate();
    g_temp.invalidate();
    for (uint8_t i = 0; i < 3; i++)
    {
        g_textoDibujado[i] = "";
    }

    JWPLC_TFT.setTextColor(T.textMuted, T.background);
    JWPLC_TFT.setCursor(8, 158);
    JWPLC_TFT.print("ESC salir");
}

extern "C" void jwplcUIUpdate()
{
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        g_pedirIdle = true; // goIdle() se llama desde loop(), fuera del batch
    }

    g_nivel.setValue(g_valorNivel);
    g_presion.setValue(g_valorPresion);
    g_temp.setValue(g_valorTemp);
    g_nivel.draw();
    g_presion.draw();
    g_temp.draw();

    valor(0, String(g_valorNivel, 0) + " %", 240, 26, 110);
    valor(1, String(g_valorPresion, 1) + " bar", 240, 88, 120);
    valor(2, String(g_valorTemp, 0), 316, 132, 50);
}

extern "C" void jwplcUIExit()
{
}

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - Widgets_Barras");

    // x, y, ancho, alto, mínimo, máximo (ancho > alto: horizontal)
    g_nivel.begin(8, 56, 232, 22, 0, 100, T);
    g_nivel.setThreshold(90, T.error);

    g_presion.begin(8, 118, 232, 22, 0, 10, T);
    g_presion.setColor(0x041F); // azul
    g_presion.setThreshold(8, T.warn);

    g_temp.begin(270, 50, 30, 80, 0, 100, T); // vertical
    g_temp.setColor(T.warn);
    g_temp.setThreshold(70, T.error);

    JWPLC_Display.setIdleWakeButton(BTN_OK);
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
    JWPLC_Display.setUserRefreshPeriodMs(50);
    JWPLC_Display.clearPendingInput();
}

void loop()
{
    simular();

    if (g_pedirIdle)
    {
        g_pedirIdle = false;
        JWPLC_Display.goIdle();
    }
}
