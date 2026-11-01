/*
  JWPLC Basic - Widgets_Teclado

  Teclado en pantalla con JWPLC_UIKeyboard: pide un nombre y luego una
  contraseña (se muestra con «*») y enseña el resultado.

  Uso: OK sale de IDLE. En el teclado, ◄ ► ▲ ▼ mueven la tecla marcada y OK
  la pulsa. La fila de abajo tiene ABC/abc (mayúsculas), #+= (símbolos),
  espacio, <- (borrar) y OK (aceptar). ESC cancela.
  En el resumen, OK vuelve a empezar y ESC vuelve a IDLE.

  JWPLC_TFT es la pantalla. Los callbacks jwplcUIEnter/jwplcUIUpdate ya se
  ejecutan con el bus de la TFT adquirido por JWPLC_Display.
*/

#include <JWPLC_UI_Widgets.h>

static JWPLC_UITheme T = JWPLC_UI_THEME_DARK;
static JWPLC_UIKeyboard g_teclado;

enum : uint8_t
{
    PASO_NOMBRE,
    PASO_CLAVE,
    PASO_RESUMEN
};

static uint8_t g_paso = PASO_NOMBRE;
static String g_nombre = "Bomba 1";
static String g_clave = "";
static bool g_resumenDibujado = false;
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

static void pedirNombre()
{
    g_paso = PASO_NOMBRE;
    JWPLC_TFT.fillRect(0, 22, 320, 148, T.background);
    dibujarEncabezado("Equipo");
    g_teclado.open("Nombre", g_nombre.c_str(), 16);
}

static void pedirClave()
{
    g_paso = PASO_CLAVE;
    JWPLC_TFT.fillRect(0, 22, 320, 148, T.background);
    dibujarEncabezado("Acceso");
    g_teclado.open("Clave", "", 8, true);
}

static void dibujarResumen()
{
    JWPLC_TFT.fillRect(0, 22, 320, 148, T.background);
    dibujarEncabezado("Resumen");

    JWPLC_TFT.setFont(JWPLC_FONT_16);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(T.textMuted, T.background);
    JWPLC_TFT.drawString("Nombre", 12, 40);
    JWPLC_TFT.drawString("Clave", 12, 70);
    JWPLC_TFT.setTextColor(T.text, T.background);
    JWPLC_TFT.drawString(g_nombre.c_str(), 100, 40);
    String oculta = "";
    for (unsigned int i = 0; i < g_clave.length(); i++)
    {
        oculta += "*";
    }
    JWPLC_TFT.drawString(oculta.c_str(), 100, 70);

    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextColor(T.textMuted, T.background);
    JWPLC_TFT.setCursor(8, 156);
    JWPLC_TFT.print("OK repetir   ESC salir");
    g_resumenDibujado = true;
}

extern "C" void jwplcUIEnter()
{
    // Teclas pulsadas en IDLE: no deben actuar sobre esta pantalla.
    JWPLC_Display.clearPendingInput();

    JWPLC_TFT.fillScreen(T.background);
    g_teclado.begin(26, T);
    pedirNombre();
}

extern "C" void jwplcUIUpdate()
{
    if (g_paso == PASO_RESUMEN)
    {
        if (!g_resumenDibujado)
        {
            dibujarResumen();
        }
        if (JWPLC_Buttons.pressed(BTN_OK))
        {
            g_resumenDibujado = false;
            pedirNombre();
        }
        else if (JWPLC_Buttons.pressed(BTN_ESC))
        {
            g_pedirIdle = true; // goIdle() se llama desde loop(), fuera del batch
        }
        return;
    }

    const uint8_t r = g_teclado.handleKeys();
    if (r == UI_EDIT_ACCEPTED && g_paso == PASO_NOMBRE)
    {
        g_nombre = g_teclado.text();
        Serial.println("NOMBRE=" + g_nombre);
        pedirClave();
    }
    else if (r == UI_EDIT_ACCEPTED && g_paso == PASO_CLAVE)
    {
        g_clave = g_teclado.text();
        Serial.println("CLAVE=" + g_clave);
        g_paso = PASO_RESUMEN;
        g_resumenDibujado = false;
        return;
    }
    else if (r == UI_EDIT_CANCELLED)
    {
        Serial.println("CANCELADO");
        g_paso = PASO_RESUMEN;
        g_resumenDibujado = false;
        return;
    }
    g_teclado.draw();
}

extern "C" void jwplcUIExit()
{
    g_teclado.close();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - Widgets_Teclado");

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
