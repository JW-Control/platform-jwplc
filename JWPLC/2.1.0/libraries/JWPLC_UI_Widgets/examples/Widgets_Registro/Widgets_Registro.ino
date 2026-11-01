/*
  JWPLC Basic - Widgets_Registro

  Registro de eventos con JWPLC_UILog: cada línea nueva entra por abajo y lo
  anterior sube con el scroll de región (JWPLC_TFTSprite.scroll()), así que
  solo se dibuja la línea nueva.

  Los eventos se simulan cada 1,5 s (entradas, salidas y alarmas) para que
  el ejemplo funcione sin cableado.
  Uso: OK sale de IDLE. En la pantalla, OK borra el registro y ESC vuelve a
  IDLE.
*/

#include <JWPLC_UI_Widgets.h>

static JWPLC_UITheme T = JWPLC_UI_THEME_DARK;
static JWPLC_UILog g_registro;

static uint32_t g_ultimoEvento = 0;
static uint16_t g_numEvento = 0;
static volatile bool g_pedirIdle = false;

static const char EVENTOS[][24] = {
    "I0.0 Marcha ON",
    "Q0.0 Bomba 1 ON",
    "AI1 Nivel 82 %",
    "ALARMA Nivel alto",
    "I0.1 Paro ON",
    "Q0.0 Bomba 1 OFF",
    "Alarma reconocida",
    "Modo RUN",
};
static const uint8_t N_EVENTOS = sizeof(EVENTOS) / sizeof(EVENTOS[0]);

// Hora desde el arranque como hh:mm:ss.
static String hora(uint32_t ms)
{
    const uint32_t s = ms / 1000;
    char t[12];
    snprintf(t, sizeof(t), "%02lu:%02lu:%02lu", (unsigned long)(s / 3600),
             (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
    return String(t);
}

static void dibujarEncabezado()
{
    JWPLC_TFT.fillRect(0, 0, 320, 22, T.header);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setTextDatum(DATUM_MIDDLE_LEFT);
    JWPLC_TFT.setTextColor(T.headerText, T.header);
    JWPLC_TFT.drawString("Eventos", 8, 11);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
}

extern "C" void jwplcUIEnter()
{
    // Teclas pulsadas en IDLE: no deben actuar sobre esta pantalla.
    JWPLC_Display.clearPendingInput();

    JWPLC_TFT.fillScreen(T.background);
    dibujarEncabezado();
    JWPLC_TFT.setTextColor(T.textMuted, T.background);
    JWPLC_TFT.setCursor(8, 158);
    JWPLC_TFT.print("OK borrar   ESC salir");

    // El contenido del registro se conserva entre visitas: solo se envía.
    g_registro.invalidate();
}

extern "C" void jwplcUIUpdate()
{
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        g_registro.clear();
    }
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        g_pedirIdle = true; // goIdle() se llama desde loop(), fuera del batch
    }

    const uint32_t ahora = millis();
    if (ahora - g_ultimoEvento >= 1500)
    {
        g_ultimoEvento = ahora;
        const uint8_t i = (uint8_t)(g_numEvento % N_EVENTOS);
        uint16_t color = T.text;
        if (i == 3)
        {
            color = T.error;
        }
        else if (i == 6 || i == 7)
        {
            color = T.ok;
        }
        g_registro.add(hora(ahora) + "  " + EVENTOS[i], color);
        g_numEvento++;
    }

    g_registro.draw();
}

extern "C" void jwplcUIExit()
{
}

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - Widgets_Registro");

    // x, y, ancho, alto: 7 líneas de 17 px
    g_registro.begin(2, 28, 316, 119, T);

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
