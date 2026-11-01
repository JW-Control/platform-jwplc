/*
  JWPLC Basic - Widgets_QR

  Vinculación del celular con JWPLC_UIQR, como en la maqueta del Runtime
  (vista «Conectar celular»):

    - QR con la dirección del equipo y un código de 6 dígitos.
    - El código expira a los 60 s (barra y «Expira en N s») y se renueva.
    - ◄ ► eligen el rol con el que entra el celular: Operador o Admin.
    - Cuando un celular pide vincularse, el equipo pide confirmación física:
      OK acepta y ESC rechaza. Sin el botón no hay vínculo.

  En este ejemplo la solicitud del celular se simula con ▼; en una
  aplicación real la daría el servidor web del equipo al recibir el código.
  ESC (sin solicitud pendiente) vuelve a IDLE.
*/

#include <JWPLC_UI_Widgets.h>

static JWPLC_UITheme T = JWPLC_UI_THEME_DARK;
static JWPLC_UIQR g_qr;

static const char DIRECCION[] = "http://192.168.1.50"; // en una aplicación real, la IP del equipo
static const uint32_t VIGENCIA_MS = 60000;

enum : uint8_t
{
    EST_ESPERA,    // QR a la vista, esperando al celular
    EST_SOLICITUD, // un celular envió el código: falta OK en el equipo
    EST_VINCULADO,
    EST_RECHAZADO
};

static uint8_t g_estado = EST_ESPERA;
static uint8_t g_rol = 0; // 0 = Operador, 1 = Admin
static String g_codigo;
static uint32_t g_inicioCodigo = 0;
static uint32_t g_inicioMensaje = 0;
static int16_t g_barraDibujada = -1;
static int32_t g_segundosDibujados = -1;
static bool g_pantallaDibujada = false;
static volatile bool g_pedirIdle = false;

// ---------------------------------------------------------------- código

static void nuevoCodigo()
{
    // random() sirve para el ejemplo; en producción usar un número aleatorio
    // del hardware para que el código no se pueda adivinar.
    char c[8];
    snprintf(c, sizeof(c), "%06ld", random(0, 1000000));
    g_codigo = c;
    g_inicioCodigo = millis();
    g_qr.setText((String(DIRECCION) + "/v?c=" + g_codigo).c_str());
    Serial.println("CODIGO " + g_codigo);
}

// ---------------------------------------------------------------- pantalla

static void texto(const char t[], int16_t x, int16_t y, uint16_t color, uint16_t fondo)
{
    JWPLC_TFT.setFont(JWPLC_FONT_16);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(color, fondo);
    JWPLC_TFT.drawString(t, x, y);
}

static void dibujarEncabezado()
{
    JWPLC_TFT.fillRect(0, 0, 320, 22, T.header);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setTextDatum(DATUM_MIDDLE_LEFT);
    JWPLC_TFT.setTextColor(T.headerText, T.header);
    JWPLC_TFT.drawString("Conectar celular", 8, 11);
    JWPLC_TFT.setTextSize(1);
}

static void dibujarPie()
{
    JWPLC_TFT.fillRect(0, 158, 320, 12, T.background);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(T.textMuted, T.background);
    JWPLC_TFT.drawString("ESC cancelar  <> rol:", 4, 160);
    JWPLC_TFT.setTextColor(g_rol == 0 ? T.accent : T.textMuted, T.background);
    JWPLC_TFT.drawString("Operador", 136, 160);
    JWPLC_TFT.setTextColor(T.textMuted, T.background);
    JWPLC_TFT.drawString("/", 190, 160);
    JWPLC_TFT.setTextColor(g_rol == 1 ? T.accent : T.textMuted, T.background);
    JWPLC_TFT.drawString("Admin", 202, 160);
}

// Recuadro de abajo a la derecha: indicación o pedido de confirmación.
static void dibujarRecuadro()
{
    uint16_t borde = T.accent;
    String l1 = "Al conectar, confirma";
    String l2 = "con OK en el equipo";
    if (g_estado == EST_SOLICITUD)
    {
        borde = T.warn;
        l1 = String("Pide acceso: ") + (g_rol == 0 ? "Operador" : "Admin");
        l2 = "OK acepta  ESC rechaza";
    }
    else if (g_estado == EST_VINCULADO)
    {
        borde = T.ok;
        l1 = "Celular vinculado";
        l2 = String("como ") + (g_rol == 0 ? "Operador" : "Admin");
    }
    else if (g_estado == EST_RECHAZADO)
    {
        borde = T.error;
        l1 = "Solicitud rechazada";
        l2 = "Codigo renovado";
    }
    JWPLC_TFT.fillRect(150, 116, 168, 40, T.background);
    JWPLC_TFT.fillRoundRect(150, 116, 168, 40, 5, T.surface);
    JWPLC_TFT.drawRoundRect(150, 116, 168, 40, 5, borde);
    texto(l1.c_str(), 158, 119, T.text, T.surface);
    texto(l2.c_str(), 158, 137, borde, T.surface);
}

static void dibujarQRyCodigo()
{
    // QR a la izquierda, centrado en el área de 146 x 134 px.
    JWPLC_TFT.fillRect(0, 22, 148, 136, T.background);
    uint8_t escala = 134 / g_qr.drawSize(1);
    if (escala < 1)
    {
        escala = 1;
    }
    const int16_t lado = g_qr.drawSize(escala);
    g_qr.draw((int16_t)(74 - lado / 2), (int16_t)(24 + (134 - lado) / 2), escala);

    // Código en dos grupos de 3, como en la maqueta.
    JWPLC_TFT.fillRect(150, 66, 170, 28, T.background);
    JWPLC_TFT.setFont(JWPLC_FONT_26);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(T.accent, T.background);
    const String grupos = g_codigo.substring(0, 3) + " " + g_codigo.substring(3);
    JWPLC_TFT.drawString(grupos.c_str(), 150, 66);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    g_barraDibujada = -1;
    g_segundosDibujados = -1;
}

// Barra de vigencia y «Expira en N s»: solo el tramo y el número que cambian.
static void dibujarVigencia()
{
    const uint32_t pasado = millis() - g_inicioCodigo;
    const uint32_t resta = pasado < VIGENCIA_MS ? VIGENCIA_MS - pasado : 0;
    const int16_t ancho = 164;
    const int16_t lleno = (int16_t)((uint32_t)ancho * resta / VIGENCIA_MS);
    if (g_barraDibujada < 0)
    {
        JWPLC_TFT.fillRoundRect(150, 96, ancho + 4, 8, 3, T.surfaceAlt);
        JWPLC_TFT.fillRect(152, 98, lleno, 4, T.accent);
    }
    else if (lleno < g_barraDibujada)
    {
        JWPLC_TFT.fillRect(152 + lleno, 98, g_barraDibujada - lleno, 4, T.surfaceAlt);
    }
    g_barraDibujada = lleno;

    const int32_t s = (int32_t)((resta + 999) / 1000);
    if (s != g_segundosDibujados)
    {
        g_segundosDibujados = s;
        JWPLC_TFT.fillRect(150, 104, 168, 12, T.background);
        texto(("Expira en " + String(s) + " s").c_str(), 150, 102, T.textMuted, T.background);
    }
}

static void dibujarTodo()
{
    JWPLC_TFT.fillScreen(T.background);
    dibujarEncabezado();
    texto("Escanea con la camara", 150, 26, T.text, T.background);
    texto(DIRECCION, 150, 42, T.textMuted, T.background);
    dibujarQRyCodigo();
    dibujarVigencia();
    dibujarRecuadro();
    dibujarPie();
    g_pantallaDibujada = true;
}

// ---------------------------------------------------------------- USER

extern "C" void jwplcUIEnter()
{
    // Teclas pulsadas en IDLE: no deben actuar sobre esta pantalla.
    JWPLC_Display.clearPendingInput();
    g_estado = EST_ESPERA;
    nuevoCodigo();
    g_pantallaDibujada = false;
}

extern "C" void jwplcUIUpdate()
{
    if (!g_pantallaDibujada)
    {
        dibujarTodo();
    }

    const uint32_t ahora = millis();
    if (g_estado == EST_ESPERA)
    {
        if (JWPLC_Buttons.pressed(BTN_LEFT) || JWPLC_Buttons.pressed(BTN_RIGHT))
        {
            g_rol = (uint8_t)(1 - g_rol);
            dibujarPie();
        }
        if (JWPLC_Buttons.pressed(BTN_DOWN)) // simula el pedido del celular
        {
            g_estado = EST_SOLICITUD;
            Serial.println(String("SOLICITUD rol=") + (g_rol == 0 ? "Operador" : "Admin"));
            dibujarRecuadro();
        }
        else if (JWPLC_Buttons.pressed(BTN_ESC))
        {
            g_pedirIdle = true; // goIdle() se llama desde loop(), fuera del batch
        }
        else if (ahora - g_inicioCodigo >= VIGENCIA_MS)
        {
            nuevoCodigo();
            dibujarQRyCodigo();
        }
    }
    else if (g_estado == EST_SOLICITUD)
    {
        // Confirmación física: solo OK en el equipo crea el vínculo.
        if (JWPLC_Buttons.pressed(BTN_OK))
        {
            g_estado = EST_VINCULADO;
            g_inicioMensaje = ahora;
            Serial.println(String("VINCULADO rol=") + (g_rol == 0 ? "Operador" : "Admin"));
            dibujarRecuadro();
        }
        else if (JWPLC_Buttons.pressed(BTN_ESC))
        {
            g_estado = EST_RECHAZADO;
            g_inicioMensaje = ahora;
            Serial.println("RECHAZADO");
            nuevoCodigo(); // el código usado ya no sirve
            dibujarQRyCodigo();
            dibujarRecuadro();
        }
        else if (ahora - g_inicioCodigo >= VIGENCIA_MS)
        {
            g_estado = EST_ESPERA; // venció sin confirmar
            nuevoCodigo();
            dibujarQRyCodigo();
            dibujarRecuadro();
        }
    }
    else if (ahora - g_inicioMensaje >= 3000)
    {
        // Tras el mensaje, otro código para el siguiente celular.
        g_estado = EST_ESPERA;
        nuevoCodigo();
        dibujarQRyCodigo();
        dibujarRecuadro();
    }

    dibujarVigencia();
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
