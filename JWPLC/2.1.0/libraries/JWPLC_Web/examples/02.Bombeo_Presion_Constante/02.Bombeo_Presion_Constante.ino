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
    de primer ingreso; después, la presión y las bombas. ESC muestra la
    pantalla de E/S (IDLE) y cualquier tecla vuelve.
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
static int16_t yPresion = 30; // dónde va el valor (lo calcula dibujarTodo)
static int16_t yBombas = 98;

// ------------------------------------------------------------ pantalla

// Escribe una línea en la fuente de 16 y devuelve dónde empieza la siguiente.
// La altura sale de la fuente (fontHeight), no de un número a ojo: así las
// líneas no se pisan aunque cambie la fuente.
static int16_t texto(const char t[], int16_t x, int16_t y, uint16_t color)
{
    JWPLC_TFT.setFont(JWPLC_FONT_16);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(color, JWPLC_TFT_BLACK);
    JWPLC_TFT.drawString(t, x, y);
    return y + JWPLC_TFT.fontHeight() + 2;
}

// Fuente de 26 al doble (52 px): el código de primer ingreso.
static int16_t textoGrande(const char t[], int16_t x, int16_t y, uint16_t color)
{
    JWPLC_TFT.setFont(JWPLC_FONT_26);
    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(color, JWPLC_TFT_BLACK);
    JWPLC_TFT.drawString(t, x, y);
    JWPLC_TFT.setTextSize(1);
    return y + 2 * JWPLC_TFT.fontHeight() + 4;
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
        // Primer ingreso: QR a la izquierda (dirección con el código) y, a la
        // derecha, el código para escribirlo a mano. El bloque de texto se mide
        // con textWidth para que quepa entero; el QR usa lo que sobra.
        const String codigoVisible = codigo.substring(0, 3) + " " + codigo.substring(3);
        JWPLC_TFT.setFont(JWPLC_FONT_16);
        JWPLC_TFT.setTextSize(1);
        int16_t ancho = max(JWPLC_TFT.textWidth("Primer ingreso"), JWPLC_TFT.textWidth("Escanea el QR"));
        const int16_t alto16 = JWPLC_TFT.fontHeight();
        JWPLC_TFT.setFont(JWPLC_FONT_26);
        JWPLC_TFT.setTextSize(1);
        // Medido en tamaño 1 y multiplicado: el código se dibuja al doble.
        ancho = max(ancho, (int16_t)(2 * JWPLC_TFT.textWidth(codigoVisible.c_str())));
        const int16_t altoCodigo = (int16_t)(2 * JWPLC_TFT.fontHeight());
        const int16_t xTexto = 320 - 6 - ancho;
        const String url = JWPLC_Web.pairingURL();
        if (url.length() > 0)
        {
            qr.setText(url.c_str());
            uint8_t escala = (uint8_t)((xTexto - 12) / qr.drawSize(1));
            if (escala == 0)
            {
                escala = 1;
            }
            const int16_t lado = qr.drawSize(escala);
            qr.draw((xTexto - lado) / 2, (170 - lado) / 2, escala);
        }
        else
        {
            int16_t y = 60;
            y = texto("Esperando", 8, y, GRIS);
            texto("la red...", 8, y, GRIS);
        }
        // Bloque centrado en alto: título, código y dos líneas de ayuda.
        const int16_t altoBloque = (alto16 + 2) + 4 + (altoCodigo + 4) + 6 + 2 * (alto16 + 2);
        int16_t y = (170 - altoBloque) / 2;
        y = texto("Primer ingreso", xTexto, y, JWPLC_TFT_WHITE);
        y = textoGrande(codigoVisible.c_str(), xTexto, y + 4, JWPLC_TFT_GREEN);
        y = texto("Escanea el QR", xTexto, y + 6, GRIS);
        texto("o escribelo", xTexto, y, GRIS);
        return;
    }
    int16_t y = texto("Presion de red", 12, 10, GRIS);
    yPresion = y + 2;
    JWPLC_TFT.setFont(JWPLC_FONT_26);
    JWPLC_TFT.setTextSize(1);
    y = yPresion + 2 * JWPLC_TFT.fontHeight() + 10; // el valor va al doble: se lee de lejos
    yBombas = y;
    texto("Bomba 1", 12, y, GRIS);
    y = texto("Bomba 2", 170, y, GRIS);
    texto(urlDibujada.c_str(), 12, y + 12, GRIS);
}

// JWPLC_Display dibuja la IDLE y sus menús en GLCD: al salir de cada dibujo se
// deja la fuente como estaba (si no, la IDLE sale con la fuente de 26 encima).
static void restaurarFuente()
{
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
}

static void actualizar()
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
        JWPLC_TFT.setTextSize(2);
        JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
        JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE, JWPLC_TFT_BLACK);
        JWPLC_TFT.drawString(t, 12, yPresion);
        JWPLC_TFT.setTextSize(1);
    }
    const uint8_t b = (uint8_t)((bomba1 ? 1 : 0) | (bomba2 ? 2 : 0));
    if (b != bombasDibujadas)
    {
        bombasDibujadas = b;
        JWPLC_TFT.setFont(JWPLC_FONT_16);
        JWPLC_TFT.setTextSize(1);
        const int16_t yc = yBombas + JWPLC_TFT.fontHeight() / 2;
        JWPLC_TFT.fillCircle(12 + JWPLC_TFT.textWidth("Bomba 1") + 16, yc, 7, bomba1 ? JWPLC_TFT_GREEN : GRIS_OSCURO);
        JWPLC_TFT.fillCircle(170 + JWPLC_TFT.textWidth("Bomba 2") + 16, yc, 7, bomba2 ? JWPLC_TFT_GREEN : GRIS_OSCURO);
    }
}

extern "C" void jwplcUIEnter()
{
    dibujarTodo();
    restaurarFuente();
}

extern "C" void jwplcUIUpdate()
{
    actualizar();
    restaurarFuente();
}

// ------------------------------------------------------------ programa

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - 02.Bombeo_Presion_Constante");

    // La pantalla del programa queda fija (el QR no se va a los 15 s); ESC va a
    // la IDLE de E/S y cualquier tecla vuelve.
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
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
