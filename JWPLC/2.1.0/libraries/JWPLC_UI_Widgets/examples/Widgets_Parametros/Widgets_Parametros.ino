/*
  JWPLC Basic - Widgets_Parametros

  Pantalla de parámetros al estilo del LOGO! hecha con JWPLC_UI_Widgets:

    - JWPLC_UIList          lista con scroll (▲ ▼)
    - JWPLC_UIDigitEditor   edición por dígito (◄ ► dígito, ▲ ▼ valor)
    - JWPLC_UIDialog        confirmación Sí/No
    - JWPLC_UIToast         aviso temporal en el pie

  Uso: OK sale de IDLE. En la lista, OK edita el parámetro y ESC vuelve a
  IDLE. En el editor, OK pide confirmación y ESC descarta.

  JWPLC_TFT es la pantalla. Todo se dibuja con dirty refresh dentro de los
  callbacks USER, que ya corren con el bus SPI tomado por JWPLC_Display.
*/

#include <JWPLC_UI_Widgets.h>

struct Parametro
{
    char nombre[16];
    int32_t valor;    // entero escalado: 125 con 1 decimal = 12.5
    int32_t minimo;
    int32_t maximo;
    uint8_t digitos;
    uint8_t decimales;
    char unidad[4];
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

enum : uint8_t
{
    EST_LISTA,
    EST_EDITOR,
    EST_DIALOGO
};

static JWPLC_UITheme T = JWPLC_UI_THEME_DARK;

static JWPLC_UIList g_lista;
static JWPLC_UIDigitEditor g_editor;
static JWPLC_UIDialog g_dialogo;
static JWPLC_UIToast g_aviso;

static uint8_t g_estado = EST_LISTA;
static bool g_pieDibujado = false;
static volatile bool g_pedirIdle = false;
static String g_mensaje;

// Valor escalado con sus decimales y unidad: 125 con 1 decimal -> "12.5 s".
static String formatear(uint16_t i, int32_t v)
{
    const uint8_t dec = g_param[i].decimales;
    String txt = String(v < 0 ? -v : v);
    while (dec && txt.length() <= dec)
    {
        txt = "0" + txt;
    }
    if (dec)
    {
        const uint16_t ent = txt.length() - dec;
        txt = txt.substring(0, ent) + "." + txt.substring(ent);
    }
    return String(v < 0 ? "-" : "") + txt + " " + g_param[i].unidad;
}

static void filaParametro(uint16_t i, JWPLC_UIRow fila)
{
    fila.left(g_param[i].nombre);
    fila.right(formatear(i, g_param[i].valor));
}

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

static void dibujarPie()
{
    JWPLC_TFT.fillRect(0, 148, 320, 22, T.background);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextColor(T.textMuted, T.background);
    JWPLC_TFT.setCursor(8, 155);
    if (g_estado == EST_LISTA)
    {
        JWPLC_TFT.print("UP/DOWN elegir   OK editar   ESC salir");
    }
    else
    {
        JWPLC_TFT.print("<> digito   UP/DOWN valor   OK   ESC");
    }
    g_pieDibujado = true;
}

static void abrirEditor()
{
    const uint16_t i = g_lista.selected();
    g_estado = EST_EDITOR;

    JWPLC_TFT.fillRect(0, 24, 320, 124, T.background);
    JWPLC_TFT.fillRoundRect(10, 30, 300, 112, 6, T.surface);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setTextDatum(DATUM_MIDDLE_LEFT);
    JWPLC_TFT.setTextColor(T.text, T.surface);
    JWPLC_TFT.drawString(g_param[i].nombre, 22, 46);

    const String rango = "Min " + formatear(i, g_param[i].minimo) +
                         "  Max " + formatear(i, g_param[i].maximo);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextColor(T.textMuted, T.surface);
    JWPLC_TFT.drawString(rango.c_str(), 22, 124);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);

    // Primero con x = 0 para conocer el ancho real (con signo si el rango
    // admite negativos) y luego centrado.
    g_editor.begin(0, 72, g_param[i].digitos, g_param[i].decimales, g_param[i].unidad, T);
    g_editor.setRange(g_param[i].minimo, g_param[i].maximo);
    const int16_t x = 160 - g_editor.width() / 2 - 12;
    g_editor.begin(x, 72, g_param[i].digitos, g_param[i].decimales, g_param[i].unidad, T);
    g_editor.setRange(g_param[i].minimo, g_param[i].maximo);
    g_editor.setValue(g_param[i].valor);
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

    JWPLC_TFT.fillScreen(T.background);
    dibujarEncabezado("Parametros");

    g_lista.begin(0, 24, 320, 124, 20, T);
    g_lista.setRows(N_PARAM, filaParametro);
    g_aviso.begin(4, 148, 312, 20, T);

    g_estado = EST_LISTA;
    g_pieDibujado = false;
}

extern "C" void jwplcUIUpdate()
{
    if (g_estado == EST_LISTA)
    {
        g_lista.handleKeys();
        if (JWPLC_Buttons.pressed(BTN_OK))
        {
            abrirEditor();
        }
        else if (JWPLC_Buttons.pressed(BTN_ESC))
        {
            g_pedirIdle = true; // goIdle() se llama desde loop(), fuera del batch
        }
    }
    else if (g_estado == EST_EDITOR)
    {
        const uint8_t r = g_editor.handleKeys();
        if (r == UI_EDIT_ACCEPTED)
        {
            const uint16_t i = g_lista.selected();
            g_mensaje = String(g_param[i].nombre) + " = " + formatear(i, g_editor.value());
            g_estado = EST_DIALOGO;
            g_dialogo.open("Guardar cambio?", g_mensaje.c_str(), true, T);
        }
        else if (r == UI_EDIT_CANCELLED)
        {
            volverALista();
            g_aviso.show("Edicion descartada", 1500, T.warn);
        }
        else
        {
            g_editor.draw();
        }
    }
    else if (g_estado == EST_DIALOGO)
    {
        const uint8_t r = g_dialogo.handleKeys();
        if (r == UI_DIALOG_YES)
        {
            g_param[g_lista.selected()].valor = g_editor.value();
            Serial.println("GUARDADO " + g_mensaje);
            volverALista();
            g_aviso.show("Guardado", 1500, T.ok);
        }
        else if (r == UI_DIALOG_NO)
        {
            volverALista();
            g_aviso.show("Sin cambios", 1500, T.warn);
        }
        else
        {
            g_dialogo.draw();
        }
    }

    if (g_estado == EST_LISTA && g_lista.rowCount())
    {
        g_lista.draw();
    }

    // El pie muestra la ayuda o, por un momento, el aviso.
    if (g_aviso.visible())
    {
        if (g_aviso.draw())
        {
            g_pieDibujado = false;
        }
    }
    if (!g_aviso.visible() && !g_pieDibujado && g_estado != EST_DIALOGO)
    {
        dibujarPie();
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
