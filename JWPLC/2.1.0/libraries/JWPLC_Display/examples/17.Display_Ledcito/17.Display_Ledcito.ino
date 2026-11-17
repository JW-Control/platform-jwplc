/*
  17.Display_Ledcito

  Qué hace:
    Anima el rostro de Ledcito, la mascota de JW Control, en la TFT del frente.
    Toda la expresión está en los ojos y en la cabeza:
      1. Despierta: ojos cerrados, entreabre, se vuelve a dormir y despierta
         de golpe con un estirón y doble parpadeo.
      2. Mira a la derecha y a la izquierda, y vuelve al centro.
      3. Corte: la cabeza aparece a la derecha, girada hacia abajo a la
         izquierda, donde va el Ledcito impreso junto a la placa.
      4. Se asombra (ojos grandes y redondos, salto de cabeza) y se alegra
         (ojos ^ ^ y rebotes). Al final te mira, se apaga y la animación se
         repite.

  Qué conectar: nada. Coloca el Ledcito físico abajo a la izquierda de la placa.

  Teclas:
    OK  = reinicia la animación (en IDLE la vuelve a abrir).
    ESC = vuelve a IDLE.

  Cómo está hecho:
    - Un sprite de 4 bits del tamaño de la pantalla (320x170, ~27 KB) se
      redibuja entero en RAM y se envía de una vez: sin parpadeo.
    - La animación es una línea de tiempo: poseEn(t) calcula la pose
      (cabeza, giro, mirada, apertura y forma de los ojos) para el instante
      t; dibujarLedcito() la pinta. Para cambiar la coreografía basta tocar
      los tiempos de poseEn().
    - El encendido y el apagado son un fundido de la paleta del sprite.
*/

// =====================================================
// Ajustes
// =====================================================

static const uint32_t PERIODO_FRAME_MS = 33;   // ~30 FPS
static const uint32_t DURACION_CICLO_MS = 17000; // la animación se repite

// Momentos clave de la línea de tiempo (ms desde el inicio)
static const uint32_t T_DESPIERTA = 2200;  // abre los ojos de golpe
static const uint32_t T_CORTE = 7200;      // cambia el plano: mira a Ledcito
static const uint32_t T_ASOMBRO = 8400;    // ojos grandes + salto
static const uint32_t T_FELIZ = 10300;     // ojos ^ ^ + rebotes
static const uint32_t T_TE_MIRA = 13600;   // vuelve a mirar al frente
static const uint32_t T_APAGA = 15600;     // fundido a negro

// Colores (RGB565), tomados del modelo impreso
static const uint16_t COLOR_FONDO = 0x0000;
static const uint16_t COLOR_CARA = 0xE73B;   // blanco hueso
static const uint16_t COLOR_SOMBRA = 0x9CD3; // borde inferior de la cara
static const uint16_t COLOR_VISOR = 0x18E3;  // negro grafito
static const uint16_t COLOR_VERDE = 0x3EA8;  // ojos y orejas
static const uint16_t COLOR_VERDE_OSC = 0x2589;
static const uint16_t COLOR_BRILLO = 0xFFFF;

// Índices de la paleta del sprite (4 bits = 16 colores)
enum : uint8_t
{
    P_FONDO = 0,
    P_CARA,
    P_SOMBRA,
    P_VISOR,
    P_VERDE,
    P_VERDE_OSC,
    P_BRILLO
};

// Formas de los ojos
enum : uint8_t
{
    OJO_NORMAL = 0,
    OJO_ASOMBRO,
    OJO_FELIZ
};

// =====================================================
// Pose: todo lo que se anima
// =====================================================

struct Pose
{
    float cabezaX;  // desplazamiento de la cabeza (px)
    float cabezaY;
    float estira;   // 1 = normal, >1 estirada, <1 aplastada
    float giro;     // -1 mira a su izquierda (nuestra izquierda) .. +1 derecha
    float miraX;    // mirada dentro del visor, -1..1
    float miraY;
    float abre;     // 0 = ojos cerrados .. 1 = abiertos
    float tamano;   // escala de los ojos (rebotes)
    uint8_t forma;  // OJO_NORMAL, OJO_ASOMBRO u OJO_FELIZ
    float luz;      // fundido: 0 = negro .. 1 = colores completos
    float brillo;   // pulso del brillo en los ojos de asombro
};

static JWPLC_TFTSprite g_lienzo;
static bool g_lienzoOk = false;
static bool g_entro = false;
static uint32_t g_inicioMs = 0;
static float g_luzAplicada = -1.0f;
static uint8_t g_fase = 0xFF;

// =====================================================
// Utilidades de animación
// =====================================================

static float limitar(float v, float a, float b)
{
    if (v < a) return a;
    if (v > b) return b;
    return v;
}

static float mezclar(float a, float b, float k)
{
    return a + (b - a) * k;
}

// 0..1 lineal entre a y b
static float tramo(uint32_t t, uint32_t a, uint32_t b)
{
    if (t <= a) return 0.0f;
    if (t >= b) return 1.0f;
    return (float)(t - a) / (float)(b - a);
}

// Entrada y salida suaves
static float suave(float x)
{
    x = limitar(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

// Se pasa un poco y vuelve (rebote elástico corto)
static float pasarse(float x)
{
    x = limitar(x, 0.0f, 1.0f);
    const float c = 2.2f;
    const float y = x - 1.0f;
    return 1.0f + (c + 1.0f) * y * y * y + c * y * y;
}

// Medio seno: 0 -> 1 -> 0 entre a y b
static float campana(uint32_t t, uint32_t a, uint32_t b)
{
    return sinf(PI * tramo(t, a, b));
}

// Parpadeo de 180 ms que empieza en `a`: devuelve cuánto se cierra (0..1)
static float parpadeo(uint32_t t, uint32_t a)
{
    return campana(t, a, a + 180);
}

static uint16_t atenuar(uint16_t c, float k)
{
    const uint16_t r = (uint16_t)((c >> 11) * k);
    const uint16_t g = (uint16_t)(((c >> 5) % 64) * k);
    const uint16_t b = (uint16_t)((c % 32) * k);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

// =====================================================
// Coreografía
// =====================================================

static Pose poseEn(uint32_t t)
{
    Pose p;
    p.cabezaX = 0.0f;
    p.cabezaY = 0.0f;
    p.estira = 1.0f;
    p.giro = 0.0f;
    p.miraX = 0.0f;
    p.miraY = 0.0f;
    p.abre = 1.0f;
    p.tamano = 1.0f;
    p.forma = OJO_NORMAL;
    p.luz = 1.0f;
    p.brillo = 0.0f;

    if (t < T_CORTE)
    {
        // ---------- Escena 1: despierta y mira a los lados ----------
        p.luz = suave(tramo(t, 0, 1000));

        // Dormido: cabeza baja que respira despacio
        const float dormido = 1.0f - suave(tramo(t, T_DESPIERTA, T_DESPIERTA + 400));
        p.cabezaY = 9.0f * dormido + 2.0f * sinf(t * 0.0035f) * dormido;
        p.miraY = 0.5f * dormido;

        // Entreabre, se vuelve a dormir y despierta de golpe
        if (t < 1100)
        {
            p.abre = 0.0f;
        }
        else if (t < T_DESPIERTA)
        {
            const float entreabre = suave(tramo(t, 1100, 1500)) - suave(tramo(t, 1750, 2000));
            p.abre = 0.32f * entreabre;
        }
        else
        {
            p.abre = suave(tramo(t, T_DESPIERTA, T_DESPIERTA + 160));
            p.tamano = mezclar(0.8f, 1.0f, pasarse(tramo(t, T_DESPIERTA, T_DESPIERTA + 420)));
            // Estirón al despertar
            p.estira = 1.0f + 0.07f * campana(t, T_DESPIERTA, T_DESPIERTA + 500);
            // Doble parpadeo
            p.abre *= 1.0f - parpadeo(t, T_DESPIERTA + 700);
            p.abre *= 1.0f - parpadeo(t, T_DESPIERTA + 950);
        }

        // Mira a la derecha, luego a la izquierda, luego al centro
        const float aDerecha = suave(tramo(t, 3500, 3900));
        const float aIzquierda = suave(tramo(t, 4700, 5300));
        const float alCentro = suave(tramo(t, 6100, 6500));
        float lado = 0.0f;
        lado = mezclar(lado, 1.0f, aDerecha);
        lado = mezclar(lado, -1.0f, aIzquierda);
        lado = mezclar(lado, 0.0f, alCentro);
        p.giro = 0.7f * lado;
        p.miraX = lado;
        // La cabeza se eleva un poco al girar
        p.cabezaY -= 3.0f * campana(t, 3500, 3900);
        p.cabezaY -= 3.0f * campana(t, 4700, 5300);
        p.cabezaY -= 2.0f * campana(t, 6100, 6500);
        p.abre *= 1.0f - parpadeo(t, 5600);
        p.abre *= 1.0f - parpadeo(t, 6750);
        return p;
    }

    // ---------- Escena 2 (corte): ve al Ledcito impreso ----------
    // Cabeza a la derecha de la pantalla, girada hacia abajo a la izquierda.
    p.cabezaX = 46.0f;
    p.cabezaY = 4.0f;
    p.giro = -0.85f;
    p.miraX = mezclar(-0.3f, -1.0f, suave(tramo(t, T_CORTE + 250, T_CORTE + 550)));
    p.miraY = mezclar(0.1f, 0.65f, suave(tramo(t, T_CORTE + 250, T_CORTE + 550)));
    p.abre = 0.85f;
    // Doble toma: parpadea como si no lo creyera
    p.abre *= 1.0f - parpadeo(t, T_CORTE + 750);
    p.abre *= 1.0f - parpadeo(t, T_CORTE + 980);

    if (t >= T_ASOMBRO - 120 && t < T_ASOMBRO)
    {
        // Anticipación: se agacha un poco antes del salto
        p.estira = 1.0f - 0.07f * tramo(t, T_ASOMBRO - 120, T_ASOMBRO);
        p.cabezaY += 4.0f * tramo(t, T_ASOMBRO - 120, T_ASOMBRO);
    }

    if (t >= T_ASOMBRO && t < T_FELIZ - 200)
    {
        // ---------- ¡Asombro! ----------
        p.forma = OJO_ASOMBRO;
        p.abre = 1.0f;
        p.tamano = mezclar(0.55f, 1.0f, pasarse(tramo(t, T_ASOMBRO, T_ASOMBRO + 380)));
        p.giro = -0.95f;
        // Salto
        p.cabezaY -= 16.0f * campana(t, T_ASOMBRO, T_ASOMBRO + 420);
        p.estira = 1.0f + 0.08f * campana(t, T_ASOMBRO, T_ASOMBRO + 300)
                   - 0.06f * campana(t, T_ASOMBRO + 400, T_ASOMBRO + 560);
        // Tiembla de emoción y los ojos brillan
        const float tiembla = tramo(t, T_ASOMBRO + 600, T_ASOMBRO + 700)
                              * (1.0f - tramo(t, T_FELIZ - 500, T_FELIZ - 300));
        p.cabezaX += 1.5f * sinf(t * 0.09f) * tiembla;
        p.brillo = 0.5f + 0.5f * sinf(t * 0.012f);
    }
    else if (t >= T_FELIZ - 200 && t < T_FELIZ)
    {
        // Transición: aprieta los ojos antes de sonreír
        p.abre = 0.15f;
        p.giro = -0.9f;
        p.estira = 0.95f;
    }
    else if (t >= T_FELIZ)
    {
        // ---------- ¡Feliz! ----------
        p.forma = OJO_FELIZ;
        p.tamano = mezclar(0.6f, 1.0f, pasarse(tramo(t, T_FELIZ, T_FELIZ + 350)));
        p.miraY = 0.25f;

        // Tres rebotes, con aplastado al tocar el suelo
        if (t < T_FELIZ + 1260)
        {
            const uint32_t salto = (t - T_FELIZ) % 420;
            const float fase = salto / 420.0f;
            p.cabezaY -= 11.0f * sinf(PI * fase);
            p.estira = 1.0f + 0.06f * sinf(PI * fase) - 0.07f * (fase < 0.12f ? (1.0f - fase / 0.12f) : 0.0f);
        }
        else
        {
            // Vaivén contento de lado a lado
            const float vaiven = sinf((t - T_FELIZ - 1260) * 0.007f);
            p.cabezaY -= 2.0f * fabsf(vaiven);
            p.giro = -0.85f + 0.12f * vaiven;
        }

        // Al final te mira a ti, todavía feliz
        const float aTi = suave(tramo(t, T_TE_MIRA, T_TE_MIRA + 600));
        p.cabezaX = mezclar(p.cabezaX, 0.0f, aTi);
        p.giro = mezclar(p.giro, 0.0f, aTi);
        p.miraX = mezclar(p.miraX, 0.0f, aTi);
        p.miraY = mezclar(p.miraY, 0.0f, aTi);
        p.cabezaY = mezclar(p.cabezaY, -2.0f * fabsf(sinf(t * 0.006f)), aTi);
    }

    p.luz = 1.0f - suave(tramo(t, T_APAGA, T_APAGA + 700));
    return p;
}

// Nombre de la fase para el monitor serie
static uint8_t faseEn(uint32_t t)
{
    if (t < T_DESPIERTA) return 0;
    if (t < 3500) return 1;
    if (t < T_CORTE) return 2;
    if (t < T_ASOMBRO) return 3;
    if (t < T_FELIZ) return 4;
    if (t < T_APAGA) return 5;
    return 6;
}

static const char FASE_TEXTO[7][40] = {
    "durmiendo",
    "despierta",
    "mira a ambos lados",
    "corte: ve a Ledcito",
    "asombro",
    "feliz",
    "se apaga",
};

// =====================================================
// Dibujo
// =====================================================

static void aplicarLuz(float k)
{
    if (k == g_luzAplicada) return;
    g_luzAplicada = k;
    g_lienzo.setPaletteColor(P_FONDO, atenuar(COLOR_FONDO, k));
    g_lienzo.setPaletteColor(P_CARA, atenuar(COLOR_CARA, k));
    g_lienzo.setPaletteColor(P_SOMBRA, atenuar(COLOR_SOMBRA, k));
    g_lienzo.setPaletteColor(P_VISOR, atenuar(COLOR_VISOR, k));
    g_lienzo.setPaletteColor(P_VERDE, atenuar(COLOR_VERDE, k));
    g_lienzo.setPaletteColor(P_VERDE_OSC, atenuar(COLOR_VERDE_OSC, k));
    g_lienzo.setPaletteColor(P_BRILLO, atenuar(COLOR_BRILLO, k));
}

// Un ojo centrado en (x, y). `escala` aplica la perspectiva del giro.
static void dibujarOjo(float x, float y, float escala, Pose p)
{
    const int16_t cx = (int16_t)x;
    const int16_t cy = (int16_t)y;

    if (p.forma == OJO_ASOMBRO)
    {
        // Ojos grandes y redondos con brillo
        const int16_t r = (int16_t)(26.0f * escala * p.tamano);
        g_lienzo.fillCircle(cx, cy, r, P_VERDE);
        const int16_t rb = (int16_t)(r * (0.26f + 0.06f * p.brillo));
        g_lienzo.fillCircle(cx - r * 0.36f, cy - r * 0.36f, rb, P_BRILLO);
        g_lienzo.fillCircle(cx + r * 0.32f, cy + r * 0.30f, r * 0.11f, P_BRILLO);
        return;
    }

    if (p.forma == OJO_FELIZ)
    {
        // Arco ^ como en el modelo: medio anillo con puntas redondas
        const int16_t r = (int16_t)(24.0f * escala * p.tamano);
        const int16_t grosor = (int16_t)(10.0f * escala * p.tamano);
        const int16_t ay = cy + r / 3;
        g_lienzo.fillCircle(cx, ay, r, P_VERDE);
        g_lienzo.fillCircle(cx, ay, r - grosor, P_VISOR);
        g_lienzo.fillRect(cx - r - 1, ay + 1, 2 * r + 3, r + 1, P_VISOR);
        g_lienzo.fillCircle(cx - r + grosor / 2, ay, grosor / 2, P_VERDE);
        g_lienzo.fillCircle(cx + r - grosor / 2, ay, grosor / 2, P_VERDE);
        return;
    }

    // Normal: píldora vertical; cerrada es una rayita
    const float ancho = 22.0f * escala * p.tamano;
    float alto = 44.0f * escala * p.tamano * p.abre;
    if (alto < 4.0f) alto = 4.0f;
    const int16_t w = (int16_t)ancho;
    const int16_t h = (int16_t)alto;
    const int16_t radio = (w < h ? w : h) / 2;
    g_lienzo.fillRoundRect(cx - w / 2, cy - h / 2, w, h, radio, P_VERDE);
}

static void dibujarLedcito(Pose p)
{
    g_lienzo.fill(P_FONDO);

    const float giroAbs = fabsf(p.giro);
    const float cx = 160.0f + p.cabezaX;
    const float cy = 87.0f + p.cabezaY;

    // Cabeza: al estirarse se afina y al aplastarse se ensancha
    const float hh = 142.0f * p.estira;
    const float hw = (214.0f - giroAbs * 14.0f) * (1.0f + (1.0f - p.estira) * 0.8f);
    const int16_t hx = (int16_t)(cx - hw / 2.0f);
    const int16_t hy = (int16_t)(cy - hh / 2.0f);
    const int16_t radioCabeza = (int16_t)(hh * 0.36f);

    // Orejas verdes: la del lado al que gira se esconde y la otra asoma
    const float orejaAlto = 64.0f * p.estira;
    const float orejaIzq = 15.0f * (1.0f + p.giro * 0.75f);
    const float orejaDer = 15.0f * (1.0f - p.giro * 0.75f);
    const int16_t oy = (int16_t)(cy - 4.0f * p.estira - orejaAlto / 2.0f);
    if (orejaIzq > 2.0f)
    {
        const int16_t ow = (int16_t)(orejaIzq + 10.0f);
        g_lienzo.fillRoundRect(hx - (int16_t)orejaIzq, oy, ow, (int16_t)orejaAlto, 8, P_VERDE);
        g_lienzo.fillRoundRect(hx - (int16_t)orejaIzq, oy + (int16_t)(orejaAlto * 0.62f), ow,
                               (int16_t)(orejaAlto * 0.38f), 8, P_VERDE_OSC);
    }
    if (orejaDer > 2.0f)
    {
        const int16_t ow = (int16_t)(orejaDer + 10.0f);
        const int16_t ox = (int16_t)(hx + hw) - 10;
        g_lienzo.fillRoundRect(ox, oy, ow, (int16_t)orejaAlto, 8, P_VERDE);
        g_lienzo.fillRoundRect(ox, oy + (int16_t)(orejaAlto * 0.62f), ow,
                               (int16_t)(orejaAlto * 0.38f), 8, P_VERDE_OSC);
    }

    // Cara blanca con borde inferior en sombra (relieve)
    g_lienzo.fillRoundRect(hx, hy, (int16_t)hw, (int16_t)hh, radioCabeza, P_SOMBRA);
    g_lienzo.fillRoundRect(hx, hy, (int16_t)hw, (int16_t)hh - 6, radioCabeza, P_CARA);

    // Visor: se corre hacia donde gira la cabeza
    const float vw = 166.0f - giroAbs * 16.0f;
    const float vh = 94.0f * p.estira;
    const float vx = cx + p.giro * 22.0f;
    const float vy = cy - 5.0f * p.estira;
    g_lienzo.fillRoundRect((int16_t)(vx - vw / 2.0f), (int16_t)(vy - vh / 2.0f), (int16_t)vw, (int16_t)vh,
                           (int16_t)(vh * 0.42f), P_VISOR);

    // Ojos: separación y tamaño según el giro (perspectiva)
    const float separacion = 39.0f * (1.0f - giroAbs * 0.15f);
    const float escalaIzq = 1.0f - (p.giro < 0.0f ? giroAbs * 0.12f : -giroAbs * 0.05f);
    const float escalaDer = 1.0f - (p.giro > 0.0f ? giroAbs * 0.12f : -giroAbs * 0.05f);
    float ex = vx + p.miraX * 16.0f;
    float ey = vy + p.miraY * 10.0f;

    // Los ojos nunca salen del visor
    float medioAncho = 12.0f;
    float medioAlto = 23.0f;
    if (p.forma == OJO_ASOMBRO)
    {
        medioAncho = 27.0f * p.tamano;
        medioAlto = medioAncho;
    }
    if (p.forma == OJO_FELIZ)
    {
        medioAncho = 25.0f * p.tamano;
        medioAlto = 18.0f * p.tamano;
    }
    const float margenX = vw / 2.0f - 6.0f - separacion - medioAncho;
    const float margenY = vh / 2.0f - 6.0f - medioAlto;
    ex = limitar(ex, vx - (margenX > 0.0f ? margenX : 0.0f), vx + (margenX > 0.0f ? margenX : 0.0f));
    ey = limitar(ey, vy - (margenY > 0.0f ? margenY : 0.0f), vy + (margenY > 0.0f ? margenY : 0.0f));

    dibujarOjo(ex - separacion, ey, escalaIzq, p);
    dibujarOjo(ex + separacion, ey, escalaDer, p);
}

// =====================================================
// Pantalla USER
// =====================================================

static void reiniciarAnimacion()
{
    g_inicioMs = millis();
    g_fase = 0xFF;
    g_luzAplicada = -1.0f;
}

extern "C" void jwplcUIEnter()
{
    // Las teclas pulsadas en IDLE no deben reiniciar la animación al entrar.
    JWPLC_Display.clearPendingInput();
    JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);

    if (!g_lienzoOk)
    {
        g_lienzoOk = g_lienzo.create(320, 170, 4);
    }
    if (!g_lienzoOk)
    {
        JWPLC_TFT.setFont(JWPLC_FONT_16);
        JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE, JWPLC_TFT_BLACK);
        JWPLC_TFT.setCursor(10, 76);
        JWPLC_TFT.print("Sin RAM para el sprite");
        Serial.println("Ledcito: sin RAM para el sprite");
        return;
    }
    reiniciarAnimacion();
}

extern "C" void jwplcUIUpdate()
{
    if (!g_lienzoOk) return;

    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        reiniciarAnimacion();
    }

    uint32_t t = millis() - g_inicioMs;
    if (t >= DURACION_CICLO_MS)
    {
        reiniciarAnimacion();
        t = 0;
    }

    const uint8_t fase = faseEn(t);
    if (fase != g_fase)
    {
        g_fase = fase;
        Serial.print("Ledcito: ");
        Serial.println(FASE_TEXTO[fase]);
    }

    const Pose p = poseEn(t);
    aplicarLuz(p.luz);
    dibujarLedcito(p);
    g_lienzo.push(0, 0);
}

extern "C" void jwplcUIExit()
{
    // La RAM del sprite se conserva para la próxima entrada.
}

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - Ledcito");
    Serial.println("OK = reiniciar | ESC = IDLE");

    JWPLC_Display.setIdleWakeButton(BTN_OK);
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
    JWPLC_Display.setUserRefreshPeriodMs(PERIODO_FRAME_MS);
    JWPLC_Display.clearPendingInput();
}

void loop()
{
    // Al arrancar entra directo a la animación, sin esperar una tecla.
    if (!g_entro && JWPLC_Display.isReady())
    {
        g_entro = true;
        JWPLC_Display.enterUserUI();
    }
}
