/*
  JWPLC Basic - Widgets_QR

  Código QR generado en el equipo con JWPLC_UIQR (sin librerías externas).
  Muestra dos códigos: el de vinculación del celular (enlace con el número
  de equipo y un código de 6 dígitos) y el de la red Wi-Fi del equipo.

  Uso: OK sale de IDLE. En la pantalla, ◄ ► cambian de código y ESC vuelve
  a IDLE.

  El QR se calcula una vez con setText() y se dibuja con draw() solo al
  cambiar: nunca en cada ciclo.
*/

#include <JWPLC_UI_Widgets.h>

static JWPLC_UITheme T = JWPLC_UI_THEME_DARK;
static JWPLC_UIQR g_qr;

static const char ID_EQUIPO[] = "JWPLC-0001";
static const char CODIGO[] = "482913"; // en una aplicación real, uno nuevo por vinculación

static uint8_t g_pagina = 0;
static bool g_dibujado = false;
static volatile bool g_pedirIdle = false;

static void dibujarEncabezado(const char titulo[])
{
    JWPLC_TFT.fillRect(0, 0, 320, 22, T.header);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setTextDatum(DATUM_MIDDLE_LEFT);
    JWPLC_TFT.setTextColor(T.headerText, T.header);
    JWPLC_TFT.drawString(titulo, 8, 11);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
}

static void texto(const char t[], int16_t y, uint16_t color)
{
    JWPLC_TFT.setFont(JWPLC_FONT_16);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(color, T.background);
    JWPLC_TFT.drawString(t, 166, y);
}

static void dibujarPagina()
{
    JWPLC_TFT.fillRect(0, 22, 320, 148, T.background);
    String contenido;
    if (g_pagina == 0)
    {
        dibujarEncabezado("Vincular celular");
        contenido = String("jwplc://vincular?id=") + ID_EQUIPO + "&c=" + CODIGO;
    }
    else
    {
        dibujarEncabezado("Red Wi-Fi");
        contenido = String("WIFI:T:WPA;S:") + ID_EQUIPO + ";P:jw12345678;;";
    }

    if (!g_qr.setText(contenido.c_str()))
    {
        texto("Texto muy largo", 60, T.error);
        return;
    }
    Serial.println("QR v" + String(g_qr.version()) + " " + contenido);

    // Escala que entra en 146 px de alto, centrado a la izquierda.
    uint8_t escala = 146 / g_qr.drawSize(1);
    if (escala < 1)
    {
        escala = 1;
    }
    const int16_t lado = g_qr.drawSize(escala);
    g_qr.draw((int16_t)(80 - lado / 2), (int16_t)(24 + (146 - lado) / 2), escala);

    if (g_pagina == 0)
    {
        texto("Escanee con", 34, T.text);
        texto("el celular", 52, T.text);
        texto("Equipo", 80, T.textMuted);
        texto(ID_EQUIPO, 98, T.text);
        texto("Codigo", 120, T.textMuted);
        JWPLC_TFT.setFont(JWPLC_FONT_26);
        JWPLC_TFT.setTextColor(T.accent, T.background);
        JWPLC_TFT.drawString(CODIGO, 166, 138);
    }
    else
    {
        texto("Red", 34, T.textMuted);
        texto(ID_EQUIPO, 52, T.text);
        texto("Clave", 80, T.textMuted);
        texto("jw12345678", 98, T.text);
    }
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
}

extern "C" void jwplcUIEnter()
{
    // Teclas pulsadas en IDLE: no deben actuar sobre esta pantalla.
    JWPLC_Display.clearPendingInput();
    JWPLC_TFT.fillScreen(T.background);
    g_dibujado = false;
}

extern "C" void jwplcUIUpdate()
{
    if (JWPLC_Buttons.pressed(BTN_RIGHT) || JWPLC_Buttons.pressed(BTN_LEFT))
    {
        g_pagina = (uint8_t)(1 - g_pagina);
        g_dibujado = false;
    }
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        g_pedirIdle = true; // goIdle() se llama desde loop(), fuera del batch
    }
    if (!g_dibujado)
    {
        dibujarPagina();
        g_dibujado = true;
    }
}

extern "C" void jwplcUIExit()
{
}

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - Widgets_QR");

    JWPLC_Display.setIdleWakeButton(BTN_OK);
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
    JWPLC_Display.setUserRefreshPeriodMs(50);
    JWPLC_Display.clearPendingInput();
}

void loop()
{
    if (g_pedirIdle)
    {
        g_pedirIdle = false;
        JWPLC_Display.goIdle();
    }
}
