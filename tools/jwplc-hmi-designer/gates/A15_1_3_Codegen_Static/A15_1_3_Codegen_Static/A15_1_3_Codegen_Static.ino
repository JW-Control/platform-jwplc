#include <JWPLC_Display.h>
#include "JWPLC_HMI_Generated.h"

// =====================================================
// Alpha15 A15-1..A15-3 - Gate físico del codegen
// =====================================================
// Header generado por el HMI Designer desde A15_1_3_Codegen_Static.jwhmi.
//
// Verifica en la TFT:
//   - figuras (rect, elipse, triángulo, hexágono, línea), texto RAW y
//     barra de navegación dibujados una sola vez al entrar a cada página;
//   - TEXT / VALUE / BOOL / BAR actualizándose sin parpadeo de la estática;
//   - ids de página contiguos (3 páginas).
//
// Navegación (Alpha11):
//   PAGE_SELECT : LEFT/RIGHT cambia página, OK entra
//   PAGE_CONTENT: UP/DOWN cambian el nivel (página 3), ESC vuelve
//
// Serial 115200: una línea por cada jwplcUIPageEnter(). Si aparecen líneas
// repetidas sin cambiar de página, la estática se está redibujando de más.
// =====================================================

static uint32_t g_pageEnterCount = 0;

// Hook opcional que llama el jwplcUIPageEnter() generado.
extern "C" void jwplcHMIUserPageEnter(uint8_t page)
{
    g_pageEnterCount++;
    Serial.printf("[A15-GATE] pageEnter page=%u count=%lu t=%lu ms\n",
                  (unsigned)page,
                  (unsigned long)g_pageEnterCount,
                  (unsigned long)millis());
}

void setup()
{
    Serial.begin(115200);

    jwplcHMISetup();
    snprintf(estado, sizeof(estado), "READY");
    nivel = 50.0f;

    JWPLC_Display.enterUserUI();
    Serial.println("[A15-GATE] listo: LEFT/RIGHT páginas, OK entra, ESC sale");
}

void loop()
{
    const uint32_t now = millis();

    // Página 1: temperatura oscilante y estado cíclico.
    temperatura = 25.0f + 10.0f * sinf((float)now / 2000.0f);
    static uint32_t lastState = 0;
    static uint8_t stateIndex = 0;
    if (now - lastState >= 1500)
    {
        static const char *const STATES[] = {"READY", "RUN", "WAIT", "ALARM"};
        lastState = now;
        stateIndex = (uint8_t)((stateIndex + 1) % 4);
        snprintf(estado, sizeof(estado), "%s", STATES[stateIndex]);
    }

    // Página 2: motor alterna cada 2 s.
    motor = ((now / 2000) % 2) != 0;

    // Página 3: el nivel se ajusta con UP/DOWN dentro de la página.
    if (!JWPLC_Display.isUserPageSelection() && JWPLC_Display.userPage() == PAGE_NIVEL)
    {
        if (JWPLC_Buttons.pressed(BTN_UP) && nivel < 100.0f)
            nivel += 5.0f;
        if (JWPLC_Buttons.pressed(BTN_DOWN) && nivel > 0.0f)
            nivel -= 5.0f;
    }
}
