/*
  JWPLC Basic - Widgets_Parametros

  Pantalla de parámetros al estilo del LOGO! hecha con JWPLC_UI_Widgets:

    - JWPLC_UIList          lista con scroll (▲ ▼)
    - JWPLC_UIDigitEditor   edición por dígito (◄ ► dígito, ▲ ▼ valor)
    - JWPLC_UIDialog        confirmación Sí/No
    - JWPLC_UIToast         aviso temporal en el pie

  Uso: OK sale de IDLE. En la lista, OK edita el parámetro y ESC vuelve a
  IDLE. En el editor, OK pide confirmación y ESC descarta.

  Todo se dibuja con dirty refresh dentro de los callbacks USER, que ya
  corren con el bus SPI tomado por JWPLC_Display.
*/

#include <JWPLC_UI_Widgets.h>

struct Parametro
{
    const char *nombre;
    int32_t valor;    // entero escalado: 125 con 1 decimal = 12.5
    int32_t minimo;
    int32_t maximo;
    uint8_t digitos;
    uint8_t decimales;
    const char *unidad;
};

static Parametro g_param[] = {
    {"T1 Retardo", 125, 0, 9999, 4, 1, "s"},
    {"T2 Apagado", 30, 0, 9999, 4, 1, "s"},
    {"C1 Preset", 250, 1, 99999, 5, 0, "pzs"},
    {"AI1 Umbral", 750, 0, 1000, 4, 1, "%"},
    {"AI1 Histeresis", 25, 0, 200, 3, 1, "%"},
    {"Temp. alarma", -5, -40, 150, 3, 0, "C"},
    {"Velocidad", 1450, 0, 3000, 4, 0, "rpm"},
    {"Ciclo PWM", 50, 0, 100, 3, 0, "%"},
};
static const uint16_t N_PARAM = sizeof(g_param) / sizeof(g_param[0]);

enum Estado : uint8_t
{
    EST_LISTA,
    EST_EDITOR,
    EST_DIALOGO
};

static JWPLC_UIList g_lista;
static JWPLC_UIDigitEditor g_editor;
static JWPLC_UIDialog g_dialogo;
static JWPLC_UIToast g_aviso;

static Estado g_estado = EST_LISTA;
static bool g_pieDibujado = false;
static volatile bool g_pedirIdle = false;
static char g_mensaje[48];

static const JWPLC_UITheme &T = JWPLC_UI_THEME_DARK;

static void formatear(const Parametro &p, int32_t v, char *out, size_t n)
{
    const bool neg = v < 0;
    const uint32_t a = (uint32_t)(neg ? -v : v);
    if (p.decimales == 0)
    {
        snprintf(out, n, "%s%lu %s", neg ? "-" : "", (unsigned long)a, p.unidad);
        return;
    }
    uint32_t div = 1;
    for (uint8_t i = 0; i < p.decimales; i++)
    {
        div *= 10;
    }
    snprintf(out, n, "%s%lu.%0*lu %s", neg ? "-" : "", (unsigned long)(a / div),
             (int)p.decimales, (unsigned long)(a % div), p.unidad);
}

static void filaParametro(uint16_t i, char *izq, size_t nIzq, char *der, size_t nDer, void *)
{
    snprintf(izq, nIzq, "%s", g_param[i].nombre);
    formatear(g_param[i], g_param[i].valor, der, nDer);
}

static void dibujarEncabezado(JWPLC_TFTClass &tft, const char *titulo)
{
    tft.fillRect(0, 0, 320, 22, T.surface);
    tft.setFont(JWPLC_FONT_GLCD);
    tft.setTextSize(2);
    tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
    tft.setTextColor(T.text, T.surface);
    tft.drawString(titulo, 8, 11);
    tft.setTextSize(1);
    tft.setTextDatum(JWPLC_TFTDatum::TOP_LEFT);
}

static void dibujarPie(JWPLC_TFTClass &tft)
{
    tft.fillRect(0, 148, 320, 22, T.background);
    tft.setFont(JWPLC_FONT_GLCD);
    tft.setTextSize(1);
    tft.setTextColor(T.textMuted, T.background);
    tft.setCursor(8, 155);
    if (g_estado == EST_LISTA)
    {
        tft.print("UP/DOWN elegir   OK editar   ESC salir");
    }
    else
    {
        tft.print("<> digito   UP/DOWN valor   OK   ESC");
    }
    g_pieDibujado = true;
}

static void abrirEditor(JWPLC_TFTClass &tft)
{
    const Parametro &p = g_param[g_lista.selected()];
    g_estado = EST_EDITOR;

    tft.fillRect(0, 24, 320, 124, T.background);
    tft.fillRoundRect(10, 30, 300, 112, 6, T.surface);
    tft.setFont(JWPLC_FONT_GLCD);
    tft.setTextSize(2);
    tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
    tft.setTextColor(T.text, T.surface);
    tft.drawString(p.nombre, 22, 46);

    char rango[40];
    char a[16];
    char b[16];
    formatear(p, p.minimo, a, sizeof(a));
    formatear(p, p.maximo, b, sizeof(b));
    snprintf(rango, sizeof(rango), "Min %s  Max %s", a, b);
    tft.setTextSize(1);
    tft.setTextColor(T.textMuted, T.surface);
    tft.drawString(rango, 22, 124);
    tft.setTextDatum(JWPLC_TFTDatum::TOP_LEFT);

    g_editor.begin(0, 72, p.digitos, p.decimales, p.unidad, T);
    g_editor.setRange(p.minimo, p.maximo);
    g_editor.setValue(p.valor);
    // Centrado según el ancho real del editor.
    g_editor.begin((int16_t)(160 - g_editor.width() / 2 - 12), 72, p.digitos, p.decimales, p.unidad, T);
    g_editor.setRange(p.minimo, p.maximo);
    g_editor.setValue(p.valor);
    g_pieDibujado = false;
}

static void volverALista()
{
    g_estado = EST_LISTA;
    g_lista.invalidate();
    g_pieDibujado = false;
}

extern "C" void jwplcUIEnter()
{
    // Teclas pulsadas en IDLE: no deben actuar sobre esta pantalla.
    JWPLC_Display.clearPendingInput();

    auto &tft = JWPLC_Display.tft();
    tft.fillScreen(T.background);
    dibujarEncabezado(tft, "Parametros");

    g_lista.begin(0, 24, 320, 124, 20, T);
    g_lista.setRows(N_PARAM, filaParametro);
    g_aviso.begin(4, 148, 312, 20, T);

    g_estado = EST_LISTA;
    g_pieDibujado = false;
}

extern "C" void jwplcUIUpdate()
{
    auto &tft = JWPLC_Display.tft();

    switch (g_estado)
    {
    case EST_LISTA:
        g_lista.handleKeys();
        if (JWPLC_Buttons.pressed(BTN_OK))
        {
            abrirEditor(tft);
            break;
        }
        if (JWPLC_Buttons.pressed(BTN_ESC))
        {
            g_pedirIdle = true; // goIdle() se llama desde loop(), fuera del batch
        }
        break;

    case EST_EDITOR:
    {
        const JWPLC_UIDigitEditor::Result r = g_editor.handleKeys();
        if (r == JWPLC_UIDigitEditor::ACCEPTED)
        {
            const Parametro &p = g_param[g_lista.selected()];
            char v[16];
            formatear(p, g_editor.value(), v, sizeof(v));
            snprintf(g_mensaje, sizeof(g_mensaje), "%s = %s", p.nombre, v);
            g_estado = EST_DIALOGO;
            g_dialogo.open("Guardar cambio?", g_mensaje, true, T);
            break;
        }
        if (r == JWPLC_UIDigitEditor::CANCELLED)
        {
            volverALista();
            g_aviso.show("Edicion descartada", 1500, T.warn);
            break;
        }
        g_editor.draw(tft);
        break;
    }

    case EST_DIALOGO:
    {
        const JWPLC_UIDialog::Result r = g_dialogo.handleKeys();
        if (r == JWPLC_UIDialog::YES)
        {
            Parametro &p = g_param[g_lista.selected()];
            p.valor = g_editor.value();
            Serial.printf("GUARDADO %s\n", g_mensaje);
            volverALista();
            g_aviso.show("Guardado", 1500, T.ok);
        }
        else if (r == JWPLC_UIDialog::NO)
        {
            volverALista();
            g_aviso.show("Sin cambios", 1500, T.warn);
        }
        else
        {
            g_dialogo.draw(tft);
        }
        break;
    }
    }

    if (g_estado == EST_LISTA && g_lista.rowCount())
    {
        g_lista.draw(tft);
    }

    // El pie muestra la ayuda o, por un momento, el aviso.
    if (g_aviso.visible())
    {
        if (g_aviso.draw(tft))
        {
            g_pieDibujado = false;
        }
    }
    if (!g_aviso.visible() && !g_pieDibujado && g_estado != EST_DIALOGO)
    {
        dibujarPie(tft);
    }
}

extern "C" void jwplcUIExit()
{
    g_estado = EST_LISTA;
    g_dialogo.close();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - Widgets_Parametros");

    JWPLC_Display.setIdleWakeButton(BTN_OK);
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
    // La pantalla usa ESC: la vuelta a IDLE la decide el sketch.
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
    JWPLC_Display.setUserRefreshPeriodMs(30);
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
