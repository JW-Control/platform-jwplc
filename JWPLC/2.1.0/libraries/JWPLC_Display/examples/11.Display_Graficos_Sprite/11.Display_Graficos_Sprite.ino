/*
  11.Display_Graficos_Sprite

  API gráfica 0.4.0 de JWPLC_TFT (base del Runtime estilo LOGO!):
  - JWPLC_TFTSprite: se dibuja en RAM y se envía a la pantalla de una vez,
    sin parpadeo ni barrido negro.
  - Sprite de 4 bits con paleta: un mapa de 300x120 ocupa ~18 KB de RAM.
  - drawWideLine()/drawPolyline(): cables de 2 px con bordes suaves.
  - setClipRect()/resetClip(): el dibujo no sale del área indicada.

  Uso: OK entra a USER. ◄ ► mueven el bloque seleccionado.

  JWPLC_TFT es la pantalla. Los callbacks jwplcUIEnter/jwplcUIUpdate ya se
  ejecutan con el bus de la TFT adquirido por JWPLC_Display: no llames beginBatch()/endBatch() aquí, y
  si usas setClipRect() termina siempre con resetClip().
*/

// Índices de la paleta del sprite (4 bits = 16 colores)
enum : uint8_t
{
    P_FONDO = 0,
    P_REJILLA,
    P_BLOQUE,
    P_BORDE,
    P_SELECCION,
    P_TEXTO,
    P_CABLE_ON,
    P_CABLE_OFF
};

static JWPLC_TFTSprite g_mapa;
static bool g_spriteOk = false;
static uint8_t g_seleccion = 0;
static uint8_t g_dibujado = 0xFF;
static bool g_entrada = true;

static const int16_t MAPA_X = 10;
static const int16_t MAPA_Y = 30;
static const int16_t MAPA_W = 300;
static const int16_t MAPA_H = 120;

struct Bloque
{
    int16_t x;
    int16_t y;
    char nombre[4];
};

static const Bloque BLOQUES[] = {
    {20, 20, "&"},
    {110, 20, "TON"},
    {200, 50, "SR"},
};

static void prepararPaleta()
{
    g_mapa.setPaletteColor(P_FONDO, 0x0841);
    g_mapa.setPaletteColor(P_REJILLA, 0x1A23);
    g_mapa.setPaletteColor(P_BLOQUE, 0x1082);
    g_mapa.setPaletteColor(P_BORDE, 0x7BEF);
    g_mapa.setPaletteColor(P_SELECCION, 0x5FE0);
    g_mapa.setPaletteColor(P_TEXTO, JWPLC_TFT_WHITE);
    g_mapa.setPaletteColor(P_CABLE_ON, 0x5FE0);
    g_mapa.setPaletteColor(P_CABLE_OFF, 0x39E7);
}

static void dibujarMapa()
{
    g_mapa.fill(P_FONDO);

    for (int16_t x = 8; x < MAPA_W; x += 16)
    {
        for (int16_t y = 8; y < MAPA_H; y += 16)
        {
            g_mapa.drawPixel(x, y, P_REJILLA);
        }
    }

    // Cables: en sprites de 4 bits drawWideLine dibuja trazo sólido.
    g_mapa.drawWideLine(70, 35, 110, 35, 2, P_CABLE_ON);
    g_mapa.drawWideLine(170, 35, 185, 35, 2, P_CABLE_OFF);
    g_mapa.drawWideLine(185, 35, 185, 65, 2, P_CABLE_OFF);
    g_mapa.drawWideLine(185, 65, 200, 65, 2, P_CABLE_OFF);

    g_mapa.setFont(JWPLC_FONT_GLCD);
    g_mapa.setTextDatum(DATUM_MIDDLE_CENTER);

    for (uint8_t i = 0; i < 3; ++i)
    {
        const int16_t x = BLOQUES[i].x;
        const int16_t y = BLOQUES[i].y;
        const bool sel = (i == g_seleccion);
        g_mapa.fillRoundRect(x, y, 60, 30, 3, P_BLOQUE);
        g_mapa.drawRoundRect(x, y, 60, 30, 3, sel ? P_SELECCION : P_BORDE);
        if (sel)
        {
            g_mapa.drawRoundRect(x + 1, y + 1, 58, 28, 3, P_SELECCION);
        }
        g_mapa.setTextColor(sel ? P_SELECCION : P_TEXTO);
        g_mapa.drawString(BLOQUES[i].nombre, x + 30, y + 15);
    }
}

extern "C" void jwplcUIEnter()
{
    // El Display no consume los latches de la aplicación (regla Alpha8): las
    // teclas pulsadas en IDLE siguen pendientes. Se descartan al entrar para
    // que ◄ ► pulsados en IDLE no muevan la selección de esta pantalla.
    JWPLC_Display.clearPendingInput();

    JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);

    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE, JWPLC_TFT_BLACK);
    JWPLC_TFT.setCursor(10, 8);
    JWPLC_TFT.print("Sprite 4 bits + lineas gruesas");

    // Polilínea en pantalla con bordes suavizados sobre negro.
    const int16_t marco[] = {4, 24, 316, 24, 316, 156, 4, 156, 4, 24};
    JWPLC_TFT.drawPolyline(marco, 5, 2, 0x7BEF, JWPLC_TFT_BLACK);

    if (!g_spriteOk)
    {
        g_spriteOk = g_mapa.create(MAPA_W, MAPA_H, 4);
        if (g_spriteOk)
        {
            prepararPaleta();
        }
    }

    if (!g_spriteOk)
    {
        JWPLC_TFT.setCursor(10, 80);
        JWPLC_TFT.print("Sin RAM para el sprite");
    }

    g_entrada = true;
    g_dibujado = 0xFF;
}

extern "C" void jwplcUIUpdate()
{
    if (!g_spriteOk)
    {
        return;
    }

    if (JWPLC_Buttons.pressed(BTN_RIGHT))
    {
        g_seleccion = (g_seleccion + 1) % 3;
    }

    if (JWPLC_Buttons.pressed(BTN_LEFT))
    {
        g_seleccion = (g_seleccion + 2) % 3;
    }

    if (!g_entrada && g_dibujado == g_seleccion)
    {
        return; // dirty refresh: nada cambió
    }

    dibujarMapa();


    // El recorte garantiza que el sprite no pise el marco aunque se
    // empuje desplazado (por ejemplo, al hacer scroll del mapa).
    JWPLC_TFT.setClipRect(MAPA_X, MAPA_Y, MAPA_W, MAPA_H);
    g_mapa.push(MAPA_X, MAPA_Y);
    JWPLC_TFT.resetClip();

    g_dibujado = g_seleccion;
    g_entrada = false;
}

extern "C" void jwplcUIExit()
{
    // La RAM del sprite se conserva para la próxima entrada. Llama
    // g_mapa.destroy() si la aplicación la necesita para otra cosa.
}

void setup()
{
    JWPLC_Display.setIdleWakeButton(BTN_OK);
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
    JWPLC_Display.setUserRefreshPeriodMs(50);
    JWPLC_Display.clearPendingInput();
}

void loop()
{
}
