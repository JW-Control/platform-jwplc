// Prueba IA 02 · Botonera interactiva
// Diseño generado con la CLI jwhmi (botonera.json); lógica escrita por el agente.
//
// Botonera: en el selector de páginas ◀ ▶ cambian de página y OK entra.
// Dentro de una página las teclas son de este sketch y ESC vuelve al selector.
//   Receta   ▲▼ elige receta · OK arranca/cancela la cuenta atrás del horno
//   Reflejos OK empieza · espera la luz verde · OK lo antes posible
//   Motor    ▲▼ consigna · ◀ ▶ paso x1/x10/x100 · OK marcha/paro (con rampa)
//   Ajustes  MENU del Designer (lo maneja el firmware)
#include "JWPLC_HMI_Generated.h"

// ---------------------------------------------------------------- utilidades

// true sólo si la página p está abierta en modo contenido (teclas del usuario).
static bool enContenido(uint8_t p)
{
    return !JWPLC_Display.isIdleMode() &&
           !JWPLC_Display.isUserPageSelection() &&
           JWPLC_Display.userPage() == p;
}

static bool tecla(uint8_t id)
{
    return JWPLC_Buttons.pressed(id);
}

static void ponerTexto(char *destino, size_t tam, const char *texto)
{
    strncpy(destino, texto, tam - 1);
    destino[tam - 1] = '\0';
}

static float limitar(float v, float minimo, float maximo)
{
    return v < minimo ? minimo : (v > maximo ? maximo : v);
}

// xorshift32: aleatorio sin depender de random().
static uint32_t semilla = 0x2545F491u;
static uint32_t aleatorio(uint32_t minimo, uint32_t maximo)
{
    semilla ^= semilla << 13;
    semilla ^= semilla >> 17;
    semilla ^= semilla << 5;
    return minimo + semilla % (maximo - minimo + 1);
}

// ---------------------------------------------------------------- Receta

struct Receta
{
    const char *nombre;
    uint16_t temperatura; // °C
    uint32_t segundos;
};

static const Receta RECETAS[] = {
    {"Pan integral", 220, 35 * 60},
    {"Pizza", 300, 90},
    {"Galletas", 180, 12 * 60},
    {"Bizcocho", 170, 40 * 60},
    {"Pollo asado", 200, 75 * 60},
};
static const uint8_t NUM_RECETAS = sizeof(RECETAS) / sizeof(RECETAS[0]);

static uint8_t recetaIdx = 0;
static uint32_t ultimoSegundo = 0;

static void mostrarReceta()
{
    const Receta &r = RECETAS[recetaIdx];
    recetaNum = recetaIdx + 1;
    ponerTexto(receta, sizeof(receta), r.nombre);
    recetaTemp = r.temperatura;
    recetaTiempo = r.segundos;
    recetaCargada = false;
}

static void logicaReceta(uint32_t ahora)
{
    if (enContenido(PAGE_RECETA))
    {
        if (tecla(BTN_UP))
        {
            recetaIdx = (recetaIdx + 1) % NUM_RECETAS;
            mostrarReceta(); // cambiar de receta cancela el horneado
        }
        if (tecla(BTN_DOWN))
        {
            recetaIdx = (recetaIdx + NUM_RECETAS - 1) % NUM_RECETAS;
            mostrarReceta();
        }
        if (tecla(BTN_OK))
        {
            if (recetaCargada)
            {
                mostrarReceta(); // cancelar: vuelve al tiempo completo
            }
            else
            {
                if (recetaTiempo == 0)
                    recetaTiempo = RECETAS[recetaIdx].segundos;
                recetaCargada = true;
                ultimoSegundo = ahora;
            }
        }
    }

    // La cuenta atrás sigue aunque el operador salga de la página.
    if (recetaCargada && ahora - ultimoSegundo >= 1000)
    {
        ultimoSegundo += 1000;
        if (recetaTiempo > 0)
            recetaTiempo--;
        if (recetaTiempo == 0)
            recetaCargada = false; // listo: el piloto se apaga en 00:00
    }
}

// ---------------------------------------------------------------- Reflejos

enum EstadoJuego : uint8_t
{
    JUEGO_REPOSO,
    JUEGO_ESPERA,
    JUEGO_YA,
    JUEGO_FIN
};

static EstadoJuego juego = JUEGO_REPOSO;
static uint32_t juegoT0 = 0;
static uint32_t juegoEspera = 0;
static bool trofeoVisible = true; // el PixelMap empieza visible

// setPixelMapVisible redibuja la página entera: sólo se llama si cambia.
static void mostrarTrofeo(bool visible)
{
    if (visible == trofeoVisible)
        return;
    trofeoVisible = visible;
    JWPLC_Display.setPixelMapVisible(PIXEL_TROFEO, visible);
}

// uint8_t y no EstadoJuego: el IDE de Arduino coloca los prototipos de las
// funciones antes que el enum y no compilaría.
static void terminar(uint8_t estado, const char *texto)
{
    juego = (EstadoJuego)estado;
    luz = false;
    ponerTexto(estadoJuego, sizeof(estadoJuego), texto);
}

static void logicaReflejos(uint32_t ahora)
{
    if (!enContenido(PAGE_REFLEJOS))
    {
        // Salir de la página a mitad de partida la anula.
        if (juego == JUEGO_ESPERA || juego == JUEGO_YA)
            terminar(JUEGO_REPOSO, "PULSA OK");
        return;
    }

    const bool ok = tecla(BTN_OK);
    semilla ^= ahora; // el momento de cada pulsación mezcla el aleatorio

    switch (juego)
    {
    case JUEGO_REPOSO:
    case JUEGO_FIN:
        if (ok)
        {
            juego = JUEGO_ESPERA;
            juegoT0 = ahora;
            juegoEspera = aleatorio(1500, esperaMaxMs > 1500 ? (uint32_t)esperaMaxMs : 1500);
            luz = false;
            ponerTexto(estadoJuego, sizeof(estadoJuego), "ESPERA...");
        }
        break;

    case JUEGO_ESPERA:
        if (ok)
        {
            terminar(JUEGO_FIN, "TEMPRANO!");
        }
        else if (ahora - juegoT0 >= juegoEspera)
        {
            juego = JUEGO_YA;
            juegoT0 = ahora;
            luz = true;
            ponerTexto(estadoJuego, sizeof(estadoJuego), "YA!");
        }
        break;

    case JUEGO_YA:
        if (ok)
        {
            tiempoReaccion = ahora - juegoT0;
            partidas++;
            if (record == 0 || tiempoReaccion < record)
            {
                record = tiempoReaccion;
                terminar(JUEGO_FIN, "RECORD!");
                mostrarTrofeo(true);
            }
            else
            {
                terminar(JUEGO_FIN, "BIEN");
            }
        }
        else if (ahora - juegoT0 > 2000)
        {
            terminar(JUEGO_FIN, "LENTO...");
        }
        break;
    }
}

// ---------------------------------------------------------------- Motor

static const uint16_t PASOS[] = {1, 10, 100};
static uint8_t pasoIdx = 2;
static uint32_t motorUltimo = 0;

static void mostrarPaso()
{
    static const char *const TEXTOS[] = {"x1", "x10", "x100"};
    ponerTexto(paso, sizeof(paso), TEXTOS[pasoIdx]);
}

static void logicaMotor(uint32_t ahora)
{
    if (enContenido(PAGE_MOTOR))
    {
        if (tecla(BTN_UP))
            consigna = limitar(consigna + PASOS[pasoIdx], 0.0f, 3000.0f);
        if (tecla(BTN_DOWN))
            consigna = limitar(consigna - PASOS[pasoIdx], 0.0f, 3000.0f);
        if (tecla(BTN_RIGHT) && pasoIdx < 2)
        {
            pasoIdx++;
            mostrarPaso();
        }
        if (tecla(BTN_LEFT) && pasoIdx > 0)
        {
            pasoIdx--;
            mostrarPaso();
        }
        if (tecla(BTN_OK))
            marcha = !marcha;
    }
    barConsigna = consigna;

    // Rampa: la velocidad sigue a la consigna (o a 0 en paro) a «rampa» rpm/s.
    const float dt = (ahora - motorUltimo) / 1000.0f;
    motorUltimo = ahora;
    const float objetivo = marcha ? consigna : 0.0f;
    const float delta = rampa * dt;
    if (velocidad < objetivo)
        velocidad = limitar(velocidad + delta, 0.0f, objetivo);
    else if (velocidad > objetivo)
        velocidad = limitar(velocidad - delta, objetivo, 3000.0f);
}

// ---------------------------------------------------------------- MENU

static void logicaMenu()
{
    uint8_t nodo;
    if (!JWPLC_Display.menuChanged(MENU_AJUSTES, &nodo))
        return;
    if (nodo == MENU_AJUSTES_BORRAR_RECORD && borrarRecord)
    {
        record = 0;
        tiempoReaccion = 0;
        partidas = 0;
        borrarRecord = false; // acción de un solo disparo: vuelve a NO
        mostrarTrofeo(false);
    }
    // rampa y esperaMaxMs se leen directamente en cada ciclo.
}

// ---------------------------------------------------------------- Arduino

void setup()
{
    jwplcHMISetup(); // páginas, campos, MENU e IDLE del Designer
    mostrarReceta();
    mostrarPaso();
    consigna = 1500;
    terminar(JUEGO_REPOSO, "PULSA OK");
    mostrarTrofeo(false); // sin récord todavía
    motorUltimo = millis();
}

void loop()
{
    const uint32_t ahora = millis();
    logicaReceta(ahora);
    logicaReflejos(ahora);
    logicaMotor(ahora);
    logicaMenu();
}
