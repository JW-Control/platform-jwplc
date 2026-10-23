// Prueba IA 03 · Iconos con estado
// Diseño generado con la CLI jwhmi (planta_iconos.json); lógica escrita por el agente.
//
// Una planta de agua simulada: el tanque se llena por la válvula de entrada y
// la bomba lo vacía. Cada equipo es un ICON del catálogo y su variable es el
// estado (0..N-1 o bool, ver los comentarios de JWPLC_HMI_Generated.h).
//
// Botonera: en el selector de páginas ◀ ▶ cambian de página y OK entra.
// Dentro de «Proceso» las teclas son de este sketch y ESC vuelve al selector:
//   OK  paro de emergencia (enclavado: OK otra vez lo rearma)
//   ▲   automático / manual
//   ▼   simula una falla de la bomba (▼ otra vez la repara)
#include "JWPLC_HMI_Generated.h"

// Estados del catálogo (jwhmi icons).
enum : uint8_t { PARADO = 0, MARCHA = 1, FALLA = 2 };
enum : uint8_t { CERRADA = 0, ABIERTA = 1 };
enum : uint8_t { LUZ_APAGADA = 0, LUZ_VERDE = 1, LUZ_AMARILLA = 2, LUZ_ROJA = 3 };
enum : uint8_t { TANQUE_VACIO = 0, TANQUE_BAJO = 1, TANQUE_MEDIO = 2, TANQUE_LLENO = 3 };
enum : uint8_t { BAJA = 0, NORMAL = 1, ALTA = 2 };

static const uint32_t PERIODO_MS = 100;

static bool enAuto = true;
static bool fallaBomba = false;
static bool llenando = false;
static bool vaciando = false;
static float nivelSim = 35.0f;    // %
static float temperaturaSim = 30.0f; // °C
static uint32_t ultimo = 0;
static uint32_t ciclos = 0;

// true sólo si la página p está abierta en modo contenido (teclas del usuario).
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
    if (JWPLC_Buttons.pressed(BTN_UP)) enAuto = !enAuto;
    if (JWPLC_Buttons.pressed(BTN_DOWN)) fallaBomba = !fallaBomba;
}

static uint8_t estadoTanque(float n)
{
    if (n < 10.0f) return TANQUE_VACIO;
    if (n < 40.0f) return TANQUE_BAJO;
    if (n < 80.0f) return TANQUE_MEDIO;
    return TANQUE_LLENO;
}

static void simular()
{
    const bool habilitado = !paro && enAuto;

    // Histéresis: llena por debajo de 20 % hasta 90 %; vacía por encima de 60 % hasta 25 %.
    if (!habilitado) llenando = false;
    else if (nivelSim < 20.0f) llenando = true;
    else if (nivelSim >= 90.0f) llenando = false;

    if (!habilitado || fallaBomba) vaciando = false;
    else if (nivelSim > 60.0f) vaciando = true;
    else if (nivelSim < 25.0f) vaciando = false;

    if (llenando) nivelSim += 0.8f;
    if (vaciando) nivelSim -= 0.6f;
    nivelSim = nivelSim < 0.0f ? 0.0f : (nivelSim > 100.0f ? 100.0f : nivelSim);

    // La calefacción mantiene el agua entre 35 y 45 °C.
    resistencia = !paro && temperaturaSim < (resistencia ? 45.0f : 35.0f);
    temperaturaSim += resistencia ? 0.15f : -0.05f;
}

static void mostrar()
{
    const bool agitando = !paro && nivelSim > 30.0f;

    // Página «Proceso»
    nivel = nivelSim;
    tanque = estadoTanque(nivelSim);
    valvulaEntrada = llenando ? ABIERTA : CERRADA;
    bomba = fallaBomba ? FALLA : (vaciando ? MARCHA : PARADO);
    agitador = agitando ? MARCHA : PARADO;
    alarma = paro || fallaBomba;
    modoAuto = paro ? LUZ_ROJA : (enAuto ? LUZ_VERDE : LUZ_AMARILLA);
    caudal = vaciando ? 118.0f + (float)(ciclos % 25) * 0.2f : 0.0f;

    // Página «Equipos»
    cinta = paro ? FALLA : (vaciando ? MARCHA : PARADO);
    silo = paro ? FALLA : (nivelSim > 10.0f ? 1 : 0);
    ventilador = (!paro && (ciclos / 200) % 2 == 0) ? MARCHA : PARADO; // 20 s sí, 20 s no
    compresor = (!paro && llenando) ? MARCHA : PARADO;
    temperatura = temperaturaSim < 35.0f ? BAJA : (temperaturaSim > 45.0f ? ALTA : NORMAL);
    presion = fallaBomba ? ALTA : (vaciando ? NORMAL : BAJA);
    baliza = paro ? LUZ_ROJA : (fallaBomba ? LUZ_AMARILLA : (vaciando || llenando ? LUZ_VERDE : LUZ_APAGADA));
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
    ++ciclos;
    simular();
    mostrar();
}
