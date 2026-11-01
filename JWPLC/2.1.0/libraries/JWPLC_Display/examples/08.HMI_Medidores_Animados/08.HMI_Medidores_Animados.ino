// 08.HMI_Medidores_Animados: medidor circular e iconos animados
// Diseño generado con el HMI Designer (CLI jwhmi); la lógica es del sketch.
//
// Dos medidores circulares (nivel en 270° y presión en 180° por segmentos),
// una barra lineal de caudal y tres iconos que se animan solos cuando su
// estado es «En marcha» (bomba, motor del agitador y ventilador de la torre).
// El sketch solo escribe las variables: la animación la hace la librería.
//
// Botonera: en el selector de páginas OK entra a «Proceso».
// Dentro de la página las teclas son de este sketch y ESC vuelve al selector:
//   OK  paro general (enclavado: OK otra vez lo rearma)
//   ▼   simula una falla de la bomba (▼ otra vez la repara)
#include "JWPLC_HMI_Generated.h"

// Estados de los iconos (los comenta JWPLC_HMI_Generated.h).
enum : uint8_t { PARADO = 0, MARCHA = 1, FALLA = 2 };

static const uint32_t PERIODO_MS = 100;

static bool paro = false;
static bool fallaBomba = false;
static bool vaciando = false;
static float nivelSim = 66.0f;   // %
static float presionSim = 0.0f;  // bar
static uint32_t ultimo = 0;

// true solo si la página p está abierta en modo contenido (teclas del usuario).
static bool enContenido(uint8_t p)
{
    return !JWPLC_Display.isIdleMode() &&
           !JWPLC_Display.isUserPageSelection() &&
           JWPLC_Display.userPage() == p;
}

static void teclas()
{
    if (!enContenido(PAGE_PROCESO)) return;
    if (JWPLC_Buttons.pressed(BTN_OK)) paro = !paro;
    if (JWPLC_Buttons.pressed(BTN_DOWN)) fallaBomba = !fallaBomba;
}

static void simular()
{
    // El tanque entra siempre (0.5 %/ciclo); la bomba lo vacía entre 70 % y 30 %.
    const bool bombaOk = !paro && !fallaBomba;
    if (!bombaOk) vaciando = false;
    else if (nivelSim > 70.0f) vaciando = true;
    else if (nivelSim < 30.0f) vaciando = false;

    if (!paro) nivelSim += 0.5f;
    if (vaciando) nivelSim -= 1.2f;
    nivelSim = nivelSim < 0.0f ? 0.0f : (nivelSim > 100.0f ? 100.0f : nivelSim);

    // La presión sube con la bomba en marcha y baja sola.
    const float objetivo = vaciando ? 6.5f : 0.0f;
    presionSim += (objetivo - presionSim) * 0.15f;
}

static void mostrar()
{
    nivel = nivelSim;
    presion = presionSim;
    caudal = vaciando ? 80.0f : 0.0f;
    bomba = fallaBomba ? FALLA : (vaciando ? MARCHA : PARADO);
    motor = paro ? PARADO : MARCHA;                        // el agitador gira siempre
    ventilador = (!paro && nivelSim > 50.0f) ? MARCHA : PARADO;
}

void setup()
{
    jwplcHMISetup(); // páginas, campos e iconos del Designer
    mostrar();
    ultimo = millis();
}

void loop()
{
    teclas();
    const uint32_t ahora = millis();
    if (ahora - ultimo < PERIODO_MS) return;
    ultimo = ahora;
    simular();
    mostrar();
}
