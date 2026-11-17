/*
  10.Display_Fuentes_Grandes

  Fuentes grandes de JWPLC_TFT 0.5.0, sin escalar la GLCD:

    - JWPLC_FONT_7SEG_48: 7 segmentos de 48 px para el reloj o un valor de
      proceso. Solo tiene « -.0123456789:».
    - JWPLC_FONT_26: 26 px, misma familia que JWPLC_FONT_16, para valores
      y títulos.
    - JWPLC_FONT_16: filas y mensajes (la de los widgets).

  El reloj cuenta desde el arranque (en una aplicación real se leería el
  RTC). Solo se redibuja cuando cambia el texto.
  Uso: OK sale de IDLE y vuelve con ESC.
*/

static String g_reloj = "";
static String g_valor = "";
static volatile bool g_pedirIdle = false;

static const uint16_t FONDO = JWPLC_TFT_BLACK;
static const uint16_t GRIS = 0x9CF3;
static const uint16_t VERDE = 0x2E8B;

static void dibujarFijos()
{
    JWPLC_TFT.fillScreen(FONDO);
    JWPLC_TFT.fillRect(0, 0, 320, 22, 0x39E7);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setTextDatum(DATUM_MIDDLE_LEFT);
    JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE, 0x39E7);
    JWPLC_TFT.drawString("Fuentes grandes", 8, 11);
    JWPLC_TFT.setTextSize(1);

    JWPLC_TFT.setFont(JWPLC_FONT_16);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(GRIS, FONDO);
    JWPLC_TFT.drawString("Hora (7SEG_48)", 8, 28);
    JWPLC_TFT.drawString("Caudal (FONT_26)", 8, 106);
    JWPLC_TFT.drawString("Filas y mensajes: JWPLC_FONT_16", 8, 134);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextColor(GRIS, FONDO);
    JWPLC_TFT.setCursor(8, 156);
    JWPLC_TFT.print("GLCD 5x7: barra de estado y ayudas");
}

static void dibujarReloj(uint32_t ms)
{
    const uint32_t s = ms / 1000;
    char t[12];
    snprintf(t, sizeof(t), "%02lu:%02lu:%02lu", (unsigned long)(s / 3600 % 24),
             (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
    if (g_reloj == t)
    {
        return;
    }
    g_reloj = t;
    JWPLC_TFT.setFont(JWPLC_FONT_7SEG_48);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(VERDE, FONDO);
    JWPLC_TFT.fillRect(8, 46, 304, 50, FONDO);
    JWPLC_TFT.drawString(t, 8, 46);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
}

static void dibujarValor(uint32_t ms)
{
    const float caudal = 12.0f + (float)((ms / 700) % 50) / 10.0f;
    const String v = String(caudal, 1);
    if (v == g_valor)
    {
        return;
    }
    g_valor = v;
    JWPLC_TFT.fillRect(150, 100, 62, 28, FONDO);
    JWPLC_TFT.setFont(JWPLC_FONT_26);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE, FONDO);
    JWPLC_TFT.drawString(v.c_str(), 150, 101);
    JWPLC_TFT.setFont(JWPLC_FONT_16);
    JWPLC_TFT.setTextColor(GRIS, FONDO);
    JWPLC_TFT.drawString("m3/h", 214, 108);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
}

extern "C" void jwplcUIEnter()
{
    // Teclas pulsadas en IDLE: no deben actuar sobre esta pantalla.
    JWPLC_Display.clearPendingInput();
    dibujarFijos();
    g_reloj = "";
    g_valor = "";
}

extern "C" void jwplcUIUpdate()
{
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        g_pedirIdle = true; // goIdle() se llama desde loop(), fuera del batch
    }
    const uint32_t ms = millis();
    dibujarReloj(ms);
    dibujarValor(ms);
}

extern "C" void jwplcUIExit()
{
}

void setup()
{
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
