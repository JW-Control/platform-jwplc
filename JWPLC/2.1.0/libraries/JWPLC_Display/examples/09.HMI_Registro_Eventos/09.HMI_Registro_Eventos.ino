// 09.HMI_Registro_Eventos: historial de eventos en la TFT
// Diseño generado con el HMI Designer (CLI jwhmi); la lógica es del sketch.
//
// Una estación de bombeo simulada anota sus eventos con JWPLC_Display.logEvent():
// marcha y paro de la bomba, la alarma de nivel alto y el paro por operador.
// El arranque del equipo (código 100) lo anota la librería al iniciar el historial.
// La página «Eventos» los muestra con hora, código, ACT (activo) o CES (cesó)
// y el texto que define el Designer para cada código. Los últimos 16 quedan
// en la FRAM y vuelven tras un corte de luz; con microSD se copian a /EVENTOS.CSV.
//
// Botonera: en el selector de páginas ◀ ▶ cambian de página y OK entra.
//   «Proceso»: OK paro por operador (enclavado: OK otra vez lo rearma).
//   «Eventos»: ▲ ▼ desplazan, OK abre el detalle, ESC vuelve al selector.
#include "JWPLC_HMI_Generated.h"

// Códigos de evento: los mismos que la tabla del Designer (página «Eventos»).
static const uint16_t EV_BOMBA_MARCHA = 201;
static const uint16_t EV_BOMBA_PARADA = 202;
static const uint16_t EV_NIVEL_ALTO = 301;
static const uint16_t EV_PARO_OPERADOR = 302;

static const uint32_t PERIODO_MS = 100;

static bool paro = false;
static float nivel = 50.0f; // %
static uint32_t ultimo = 0;

static bool enContenido(uint8_t p)
{
    return !JWPLC_Display.isIdleMode() &&
           !JWPLC_Display.isUserPageSelection() &&
           JWPLC_Display.userPage() == p;
}

static void anotar(uint16_t codigo, uint8_t transicion)
{
    JWPLC_Display.logEvent(codigo, transicion);
}

static void teclas()
{
    if (!enContenido(PAGE_PROCESO)) return;
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        paro = !paro;
        anotar(EV_PARO_OPERADOR, paro ? JWPLC_EVENT_TRANS_ACTIVE : JWPLC_EVENT_TRANS_NORMAL);
    }
}

static void simular()
{
    // La bomba vacía el tanque entre 80 % y 40 %; sin paro el tanque se llena solo.
    bool marcha = bombaMarcha;
    if (paro) marcha = false;
    else if (nivel > 80.0f) marcha = true;
    else if (nivel < 40.0f) marcha = false;

    nivel += marcha ? -2.0f : 1.5f;
    if (nivel < 0.0f) nivel = 0.0f;
    if (nivel > 100.0f) nivel = 100.0f;

    if (marcha != bombaMarcha)
    {
        bombaMarcha = marcha;
        if (marcha) arranques = arranques + 1;
        anotar(marcha ? EV_BOMBA_MARCHA : EV_BOMBA_PARADA, JWPLC_EVENT_TRANS_ACTIVE);
    }

    // Alarma con histéresis: activa sobre 90 %, cesa bajo 85 %.
    const bool alta = alarmaNivel ? (nivel > 85.0f) : (nivel > 90.0f);
    if (alta != alarmaNivel)
    {
        alarmaNivel = alta;
        anotar(EV_NIVEL_ALTO, alta ? JWPLC_EVENT_TRANS_ACTIVE : JWPLC_EVENT_TRANS_NORMAL);
    }
}

void setup()
{
    jwplcHMISetup(); // páginas, campos e historial del Designer (anota el arranque)
    ultimo = millis();
}

void loop()
{
    teclas();
    const uint32_t ahora = millis();
    if (ahora - ultimo < PERIODO_MS) return;
    ultimo = ahora;
    simular();
    eventos = JWPLC_Display.eventCount();
}
