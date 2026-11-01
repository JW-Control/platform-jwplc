// Página HMI del Runtime: las páginas de un proyecto «Runtime» del HMI Designer.
//
// El header generado registra los campos con JWPLC_Runtime.setHMI() dentro de
// jwplcRuntimeHMISetup() y los llena en jwplcRuntimeHMIUpdate(page) con los
// valores del programa (JWPLC_Runtime.input(1), .output(1), .block(5)...).
// Desde la pantalla RUN, ◄ ► entran a las páginas; dentro, ◄ ► pasan de página
// (y desde la primera o la última vuelven a RUN) y ESC abre el menú.
//
// La página ocupa toda la TFT: mientras está en pantalla, JWPLC_Display tiene
// los campos del header y su dirty refresh dibuja solo los valores que
// cambiaron. Al salir se quitan, para que el menú del Runtime no los vea.
#include "../JWPLC_Runtime.h"
#include "RtUI.h"

// Sin header generado, el Runtime no tiene página HMI.
extern "C" void __attribute__((weak)) jwplcRuntimeHMISetup(void) {}
extern "C" void __attribute__((weak)) jwplcRuntimeHMIPageEnter(uint8_t page) { (void)page; }
extern "C" void __attribute__((weak)) jwplcRuntimeHMIUpdate(uint8_t page) { (void)page; }

namespace rtui
{

namespace
{
const JWPLC_UIField *g_fields = nullptr;
uint8_t g_count = 0;
uint8_t g_pages = 0;
bool g_active = false;
JWPLC_UIRefreshMode g_prevMode = USER_REFRESH_ON_DEMAND;

class HMIScreen : public Screen
{
public:
    uint8_t page = 0;

    void enter() override
    {
        show(page);
    }

    void show(uint8_t p)
    {
        page = p;
        if (!g_active)
        {
            g_prevMode = JWPLC_Display.userRefreshMode();
            JWPLC_Display.setUserPage(p); // antes de setFields: un solo redibujo
            JWPLC_Display.setFields(g_fields, g_count);
            // Periódico: el ciclo de la UI sigue corriendo aunque ningún campo cambie.
            JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);
            g_active = true;
        }
        JWPLC_Display.setUserPage(p);
        // Estamos dentro del batch de la pantalla (bus tomado): se dibuja aquí
        // y JWPLC_Display no repite el borrado en el ciclo siguiente.
        JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);
        jwplcRuntimeHMIPageEnter(p);
        JWPLCUI::drawStatic(JWPLC_TFT);
        JWPLCUI::invalidateAll(false); // los valores, en drawDirty de este ciclo
        JWPLCUI::consumePageRedrawPending();
        jwplcRuntimeHMIUpdate(p);
    }

    void update() override
    {
        if (key(BTN_ESC))
        {
            releaseHMI();
            pop(); // menú raíz
            return;
        }
        if (key(BTN_RIGHT))
        {
            if (page + 1 < g_pages)
            {
                show((uint8_t)(page + 1));
            }
            else
            {
                releaseHMI();
                requestIdle(); // después de la última: pantalla RUN
            }
            return;
        }
        if (key(BTN_LEFT))
        {
            if (page > 0)
            {
                show((uint8_t)(page - 1));
            }
            else
            {
                releaseHMI();
                requestIdle();
            }
            return;
        }
        jwplcRuntimeHMIUpdate(page);
    }
};

HMIScreen g_hmi;
} // namespace

void setHMI(const JWPLC_UIField fields[], uint8_t count, uint8_t pages)
{
    releaseHMI();
    g_fields = fields;
    g_count = fields ? count : 0;
    g_pages = (fields && count) ? (pages ? pages : 1) : 0;
}

bool hasHMI()
{
    return g_pages > 0;
}

uint8_t hmiPageCount()
{
    return g_pages;
}

bool hmiActive()
{
    return g_active;
}

void openHMI(uint8_t page)
{
    if (!hasHMI())
    {
        return;
    }
    g_hmi.page = page < g_pages ? page : 0;
    push(&g_hmi);
}

void releaseHMI()
{
    if (!g_active)
    {
        return;
    }
    g_active = false;
    JWPLC_Display.setUserPage(0);
    JWPLC_Display.setFields(nullptr, 0);
    JWPLC_Display.setUserRefreshMode(g_prevMode);
    // La pantalla que vuelve se dibuja sola: sin borrado extra de JWPLC_Display.
    JWPLCUI::consumePageRedrawPending();
}

} // namespace rtui
