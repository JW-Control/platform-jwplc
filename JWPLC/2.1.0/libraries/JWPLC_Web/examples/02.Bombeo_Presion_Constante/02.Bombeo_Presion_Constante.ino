/*
  JWPLC Basic - 02.Bombeo_Presion_Constante

  Qué hace: bombeo de presión constante para un edificio (como la
  arquitectura 01 del JWPLC Sim) con su panel web. La presión se simula en el
  programa: sube con las bombas en marcha y baja con el consumo.
  - Bomba 1 arranca bajo la consigna menos 0,3 bar y para sobre la consigna.
  - Bomba 2 ayuda si la presión cae bajo la consigna menos 0,8 bar.
  - Alarmas: presión baja (< 1 bar) y presión alta (> 6 bar).

  Qué conectar: el JWPLC Basic a la red. Q0.0 = bomba 1, Q0.1 = bomba 2.

  Qué mirar:
  - En la pantalla del equipo: mientras no haya usuarios, el QR y el código
    de primer ingreso; después, la presión y las bombas.
  - En el celular: escanear el QR, escribir el código y crear el admin.
    Inicio muestra presión, consigna y bombas; la consigna se cambia desde el
    rol Operador (entre 1 y 6 bar). Tendencias guarda la presión cada 5 s.
*/

#include <JWPLC_UI_Widgets.h>
#include <JWPLC_Web.h>

float presion = 2.0;   // bar
float consigna = 3.5;  // bar
float consumo = 0.25;  // bar por segundo que se pierden con el consumo
bool bomba1 = false;
bool bomba2 = false;
unsigned long arranques = 0;

static const uint16_t GRIS = 0xC618;
static const uint16_t GRIS_OSCURO = 0x4208;

static JWPLC_UIQR qr;
static String codigoDibujado = "-";
static String urlDibujada = "-";
static int32_t presionDibujada = -1;
static uint8_t bombasDibujadas = 255;

// ------------------------------------------------------------ pantalla

static void texto(const char t[], int16_t x, int16_t y, uint16_t color)
{
    JWPLC_TFT.setFont(JWPLC_FONT_16);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(color, JWPLC_TFT_BLACK);
    JWPLC_TFT.drawString(t, x, y);
}

static void dibujarTodo()
{
    JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);
    const String codigo = JWPLC_Web.pairingCode();
    codigoDibujado = codigo;
    urlDibujada = JWPLC_Web.url();
    presionDibujada = -1;
    bombasDibujadas = 255;
    if (codigo.length() > 0)
    {
        // Primer ingreso: QR con la dirección y el código.
        const String url = JWPLC_Web.pairingURL();
        if (url.length() > 0)
        {
            qr.setText(url.c_str());
            uint8_t escala = (uint8_t)(124 / qr.drawSize(1));
            if (escala == 0)
            {
                escala = 1;
            }
            const int16_t lado = qr.drawSize(escala);
            qr.draw(8, (170 - lado) / 2, escala);
        }
        else
        {
            texto("Esperando", 12, 60, GRIS);
            texto("la red...", 12, 80, GRIS);
        }
        texto("Primer ingreso", 140, 18, JWPLC_TFT_WHITE);
        JWPLC_TFT.setFont(JWPLC_FONT_26);
        JWPLC_TFT.setTextColor(JWPLC_TFT_GREEN, JWPLC_TFT_BLACK);
        JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
        JWPLC_TFT.drawString((codigo.substring(0, 3) + " " + codigo.substring(3)).c_str(), 140, 48);
        texto("Escanea el QR", 140, 100, GRIS);
        texto("y escribe el", 140, 120, GRIS);
        texto("codigo.", 140, 140, GRIS);
        return;
    }
    texto("Presion de red", 12, 8, GRIS);
    texto("Bomba 1", 12, 98, GRIS);
    texto("Bomba 2", 170, 98, GRIS);
    texto(urlDibujada.c_str(), 12, 140, GRIS);
}

extern "C" void jwplcUIEnter() { dibujarTodo(); }

extern "C" void jwplcUIUpdate()
{
    if (JWPLC_Web.pairingCode() != codigoDibujado || JWPLC_Web.url() != urlDibujada)
    {
        dibujarTodo(); // cambió el código, la IP o el equipo ya se reclamó
        return;
    }
    if (codigoDibujado.length() > 0)
    {
        return;
    }
    const int32_t p = (int32_t)(presion * 100);
    if (p != presionDibujada)
    {
        presionDibujada = p;
        char t[16];
        snprintf(t, sizeof(t), "%.2f bar ", presion);
        JWPLC_TFT.setFont(JWPLC_FONT_26);
        JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
        JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE, JWPLC_TFT_BLACK);
        JWPLC_TFT.drawString(t, 12, 34);
    }
    const uint8_t b = (uint8_t)((bomba1 ? 1 : 0) | (bomba2 ? 2 : 0));
    if (b != bombasDibujadas)
    {
        bombasDibujadas = b;
        JWPLC_TFT.fillCircle(134, 108, 8, bomba1 ? JWPLC_TFT_GREEN : GRIS_OSCURO);
        JWPLC_TFT.fillCircle(292, 108, 8, bomba2 ? JWPLC_TFT_GREEN : GRIS_OSCURO);
    }
}

// ------------------------------------------------------------ programa

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - 02.Bombeo_Presion_Constante");

    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_DISABLED);
    JWPLC_Display.setUserRefreshPeriodMs(200);
    JWPLC_Display.enterUserUI();

    JWPLC_Web.begin("Bombeo Edificio A");
    JWPLC_Web.publish("presion", presion).label("Presión de red").unit("bar").range(0, 8).decimals(2).trend();
    JWPLC_Web.publish("consigna", consigna).label("Consigna").unit("bar").range(1, 6).decimals(2).writable(WEB_OPERADOR);
    JWPLC_Web.publish("consumo", consumo).label("Consumo simulado").unit("bar/s").range(0, 1).decimals(2).writable(WEB_TECNICO).group("Simulación");
    JWPLC_Web.publish("bomba1", bomba1).label("Bomba 1").group("Motores");
    JWPLC_Web.publish("bomba2", bomba2).label("Bomba 2").group("Motores");
    JWPLC_Web.publish("arranques", arranques).label("Arranques B1").group("Motores");
    JWPLC_Web.addAlarm("baja", "Presión baja (< 1 bar)", WEB_ALARMA_ALTA);
    JWPLC_Web.addAlarm("alta", "Presión alta (> 6 bar)", WEB_ALARMA_MEDIA);
    JWPLC_Web.trendEvery(5);
}

void loop()
{
    static uint32_t ultimo = millis();
    const uint32_t ahora = millis();
    if (ahora - ultimo < 100)
    {
        return;
    }
    const float dt = (ahora - ultimo) / 1000.0f;
    ultimo = ahora;

    // Control de dos bombas con histéresis.
    if (!bomba1 && presion < consigna - 0.3f)
    {
        bomba1 = true;
        arranques++;
    }
    else if (bomba1 && presion > consigna)
    {
        bomba1 = false;
    }
    bomba2 = presion < consigna - 0.8f || (bomba2 && presion < consigna - 0.2f);
    digitalWrite(Q0_0, bomba1 ? HIGH : LOW);
    digitalWrite(Q0_1, bomba2 ? HIGH : LOW);

    // Proceso simulado.
    presion += ((bomba1 ? 0.35f : 0) + (bomba2 ? 0.35f : 0) - consumo) * dt;
    presion = constrain(presion, 0.0f, 8.0f);

    JWPLC_Web.setAlarm("baja", presion < 1.0f);
    JWPLC_Web.setAlarm("alta", presion > 6.0f);
}
