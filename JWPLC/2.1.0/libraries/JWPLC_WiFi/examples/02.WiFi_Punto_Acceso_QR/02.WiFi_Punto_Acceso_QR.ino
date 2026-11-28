/*
  02.WiFi_Punto_Acceso_QR

  Qué hace:
  - Abre la red WiFi propia del PLC (punto de acceso) «JWPLC_AP», clave 12345678.
    El PLC queda en 192.168.4.1 dentro de esa red.
  - Muestra en la TFT el QR para unirse a la red con la cámara del celular.
    El texto del QR sale de JWPLC_WiFi.qrText() y tiene el mismo formato que el
    preset WIFI del HMI Designer (WIFI:T:WPA;S:...;P:...;;), así que sirve también
    para un QR VARIABLE generado por el Designer.
  - Abajo indica cuántos celulares están conectados (solo redibuja si cambia).

  Qué conectar:
  - Nada.

  Qué mirar:
  - TFT: QR a la izquierda; red, IP y conectados a la derecha.
  - Escanear el QR con el celular: se une a JWPLC_AP sin escribir la clave.
  - Monitor serie a 115200: texto del QR y estado.

  Nota: JWPLC Sim todavía no simula la radio WiFi; este ejemplo es para la placa.
*/

#include <JWPLC_UI_Widgets.h>
#include <JWPLC_WiFi.h>

const char RED_PLC[] = "JWPLC_AP";
const char CLAVE_PLC[] = "12345678"; // 8 a 63 caracteres; "" = red abierta

static JWPLC_UITheme T = JWPLC_UI_THEME_DARK;
static JWPLC_UIQR g_qr;
static bool g_pantallaDibujada = false;
static int16_t g_conectadosDibujados = -1;

static void texto(const char t[], int16_t x, int16_t y, uint16_t color)
{
    JWPLC_TFT.setFont(JWPLC_FONT_16);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(color, T.background);
    JWPLC_TFT.drawString(t, x, y);
}

static void dibujarTodo()
{
    JWPLC_TFT.fillScreen(T.background);

    JWPLC_TFT.fillRect(0, 0, 320, 22, T.header);
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setTextDatum(DATUM_MIDDLE_LEFT);
    JWPLC_TFT.setTextColor(T.headerText, T.header);
    JWPLC_TFT.drawString("Red del PLC", 8, 11);
    JWPLC_TFT.setTextSize(1);

    // QR a la izquierda, centrado en 146 x 134 px.
    uint8_t escala = 134 / g_qr.drawSize(1);
    if (escala < 1)
        escala = 1;
    const int16_t lado = g_qr.drawSize(escala);
    g_qr.draw((int16_t)(74 - lado / 2), (int16_t)(24 + (134 - lado) / 2), escala);

    texto("Escanea para unirte", 150, 28, T.text);
    texto((String("RED: ") + JWPLC_WiFi.accessPointNetwork()).c_str(), 150, 52, T.textMuted);
    texto((String("IP: ") + JWPLC_WiFi.accessPointIP().toString()).c_str(), 150, 72, T.textMuted);
    g_pantallaDibujada = true;
    g_conectadosDibujados = -1;
}

static void dibujarConectados()
{
    const int16_t n = JWPLC_WiFi.accessPointClients();
    if (n == g_conectadosDibujados)
        return;
    g_conectadosDibujados = n;
    JWPLC_TFT.fillRect(150, 110, 168, 16, T.background);
    texto((String("Conectados: ") + String(n)).c_str(), 150, 110, T.text);
}

void setup()
{
    Serial.begin(115200);

    if (!JWPLC_WiFi.startAccessPoint(RED_PLC, CLAVE_PLC))
        Serial.println("No se pudo abrir el punto de acceso (nombre o clave inválidos)");

    // Mismo texto que pondría el preset WIFI del HMI Designer.
    g_qr.setText(JWPLC_WiFi.qrText().c_str());
    Serial.println(JWPLC_WiFi.qrText());

    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
    JWPLC_Display.clearPendingInput();
}

void loop()
{
    static uint32_t ultimoReporte = 0;
    if (millis() - ultimoReporte >= 5000)
    {
        ultimoReporte = millis();
        JWPLC_WiFi.printStatus(Serial);
    }
}

extern "C" void jwplcUIEnter()
{
    g_pantallaDibujada = false;
}

extern "C" void jwplcUIUpdate()
{
    if (!g_pantallaDibujada)
        dibujarTodo();
    dibujarConectados();
}

extern "C" void jwplcUIExit()
{
}
