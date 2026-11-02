// Configuración > Expansión: bus RS-485, módulos vinculados, alta de un
// módulo de fábrica, búsqueda y estado seguro.
//
// Toda operación con el bus la hace la tarea del Runtime (runCommand): la
// pantalla solo lee el estado que la tarea publica.
#include "RtUI.h"

#include <JWPLC_ExpansionProtocol.h>
#include <string.h>

namespace rtui
{

namespace
{
Expansion &X() { return core.expansion; }

const char *stateText(uint8_t s)
{
    switch (s)
    {
    case XS_ONLINE:
        return "en linea";
    case XS_OFFLINE:
        return "sin respuesta";
    case XS_WRONG:
        return "otro equipo";
    case XS_CHECK:
        return "verificando";
    default:
        return "-";
    }
}

const char *safeText(uint8_t value, uint8_t hold, uint8_t q)
{
    if ((hold >> q) & 1)
    {
        return "mantener";
    }
    return ((value >> q) & 1) ? "ON" : "OFF";
}

// OFF -> ON -> mantener
void cycleSafe(uint8_t &value, uint8_t &hold, uint8_t q, int8_t dir)
{
    const uint8_t bit = (uint8_t)(1u << q);
    uint8_t mode = (hold & bit) ? 2 : ((value & bit) ? 1 : 0);
    mode = (uint8_t)((mode + 3 + dir) % 3);
    value = (uint8_t)((value & ~bit) | (mode == 1 ? bit : 0));
    hold = (uint8_t)((hold & ~bit) | (mode == 2 ? bit : 0));
}

void bitsText(uint8_t bits, char out[], size_t size)
{
    // Q1/I1 a la izquierda.
    size_t n = 0;
    for (uint8_t i = 0; i < 8 && n + 1 < size; i++)
    {
        out[n++] = ((bits >> i) & 1) ? '1' : '0';
    }
    out[n] = 0;
}

int8_t moduleAtRow(uint16_t row) // filas 1..n del menú = módulos vinculados
{
    uint16_t k = 1;
    for (uint8_t x = 0; x < NUM_X; x++)
    {
        if (X().used(x))
        {
            if (k == row)
            {
                return (int8_t)x;
            }
            k++;
        }
    }
    return -1;
}

uint32_t packSafe(uint8_t value, uint8_t hold, uint8_t wd)
{
    return (uint32_t)value | ((uint32_t)hold << 8) | ((uint32_t)wd << 16);
}

bool requireStop()
{
    if (core.mode == MODE_RUN)
    {
        footerMessage("Detenga el programa primero", T.warn, 2500);
        return false;
    }
    return true;
}

void openModule(uint8_t module);
void openAdd();
void openFound();

// ================================================================ EXPANSIÓN

uint16_t expCount() { return (uint16_t)(1 + X().count() + 2); }

void expRow(uint16_t i, JWPLC_UIRow r)
{
    char t[28];
    const uint16_t n = X().count();
    if (i == 0)
    {
        r.left("Bus RS-485");
        snprintf(t, sizeof(t), n ? "%lu %s" : "< %lu %s >", (unsigned long)JWX_BAUDS[X().busBaud()],
                 jwxFrameName(X().busFrame()));
        r.right(t);
    }
    else if (i <= n)
    {
        const int8_t m = moduleAtRow(i);
        snprintf(t, sizeof(t), "Modulo %d  ID %u", m + 1, X().link((uint8_t)m).id);
        r.left(t);
        r.right(stateText(X().state((uint8_t)m)));
    }
    else if (i == n + 1)
    {
        r.left("+ Agregar modulo");
        r.right("solo el nuevo >");
    }
    else
    {
        r.left("Buscar modulos");
        r.right("1..247 >");
    }
}

void setBus(uint8_t baud, uint8_t frame)
{
    CommandArgs a;
    memset(&a, 0, sizeof(a));
    a.p1 = baud;
    a.p2 = frame;
    runCommand(CMD_X_BUS, a, [](bool) { refreshList(); });
}

void expSide(uint16_t i, int8_t dir)
{
    if (i != 0)
    {
        return;
    }
    if (X().count())
    {
        footerMessage("Quite los modulos para cambiar el bus", T.warn, 2500);
        return;
    }
    setBus((uint8_t)((X().busBaud() + JWX_BAUD_CODES + dir) % JWX_BAUD_CODES), X().busFrame());
}

void expOk(uint16_t i)
{
    const uint16_t n = X().count();
    if (i == 0)
    {
        if (X().count())
        {
            footerMessage("Quite los modulos para cambiar el bus", T.warn, 2500);
            return;
        }
        // OK cambia la trama; < > el baudrate.
        setBus(X().busBaud(), (uint8_t)((X().busFrame() + 1) % JWX_FRAME_CODES));
    }
    else if (i <= n)
    {
        openModule((uint8_t)moduleAtRow(i));
    }
    else if (i == n + 1)
    {
        if (!requireStop())
        {
            return;
        }
        if (n >= NUM_X)
        {
            footerMessage("Ya hay 7 modulos", T.warn, 2500);
            return;
        }
        confirm("Agregar modulo", "Conecte solo el modulo nuevo.", []() {
            CommandArgs a;
            memset(&a, 0, sizeof(a));
            a.aux = JWX_FACTORY_ID;
            a.flags = 1; // trama de fábrica
            runCommand(CMD_X_PROBE, a, [](bool ok) {
                if (ok)
                {
                    openAdd();
                }
            });
        });
    }
    else
    {
        if (!requireStop())
        {
            return;
        }
        runCommand(CMD_X_SCAN, [](bool) { openFound(); });
    }
}

// ================================================================ AGREGAR

uint8_t g_newId = 1;
uint8_t g_safeValue = 0;
uint8_t g_safeHold = 0;
uint8_t g_watchdog = JWX_FACTORY_WATCHDOG;

const uint16_t ADD_Q0 = 4;                 // filas Q1..Q8
const uint16_t ADD_WD = ADD_Q0 + 8;
const uint16_t ADD_SAVE = ADD_WD + 1;

uint16_t addCount() { return ADD_SAVE + 1; }

void addRow(uint16_t i, JWPLC_UIRow r)
{
    const XInfo &p = X().lastProbe;
    char t[28];
    if (i == 0)
    {
        r.left("Modelo");
        snprintf(t, sizeof(t), "%s %u/%u", p.product == JWX_PRODUCT_BASIC ? "JWPLC Basic" : "Modulo",
                 p.io >> 8, p.io & 0xFF);
        r.right(t);
    }
    else if (i == 1)
    {
        r.left("Firmware / MAC");
        snprintf(t, sizeof(t), "%u.%u  %02X%02X%02X", p.firmware >> 8, p.firmware & 0xFF, p.mac[3], p.mac[4],
                 p.mac[5]);
        r.right(t);
    }
    else if (i == 2)
    {
        r.left("ID nuevo");
        snprintf(t, sizeof(t), "< %u >", g_newId);
        r.right(t);
    }
    else if (i == 3)
    {
        r.left("Bus");
        snprintf(t, sizeof(t), "%lu %s", (unsigned long)JWX_BAUDS[X().busBaud()], jwxFrameName(X().busFrame()));
        r.right(t);
    }
    else if (i < ADD_WD)
    {
        snprintf(t, sizeof(t), "Q%u sin maestro", (unsigned)(i - ADD_Q0 + 1));
        r.left(t);
        snprintf(t, sizeof(t), "< %s >", safeText(g_safeValue, g_safeHold, (uint8_t)(i - ADD_Q0)));
        r.right(t);
    }
    else if (i == ADD_WD)
    {
        r.left("Vigilancia");
        snprintf(t, sizeof(t), "< %u,%u s >", g_watchdog / 10, g_watchdog % 10);
        r.right(t);
    }
    else
    {
        r.left("Guardar y vincular");
        r.right(">");
    }
}

void addSide(uint16_t i, int8_t dir)
{
    if (i == 2)
    {
        int16_t id = g_newId;
        id = (int16_t)(id + dir);
        g_newId = (uint8_t)(id < 1 ? JWX_MAX_ID : (id > JWX_MAX_ID ? 1 : id));
    }
    else if (i >= ADD_Q0 && i < ADD_WD)
    {
        cycleSafe(g_safeValue, g_safeHold, (uint8_t)(i - ADD_Q0), dir);
    }
    else if (i == ADD_WD)
    {
        int16_t w = (int16_t)(g_watchdog + dir * 5);
        g_watchdog = (uint8_t)(w < JWX_MIN_WATCHDOG ? JWX_MIN_WATCHDOG : (w > JWX_MAX_WATCHDOG ? JWX_MAX_WATCHDOG : w));
    }
    else
    {
        return;
    }
    refreshList();
}

void addOk(uint16_t i)
{
    if (i == 2)
    {
        editValue("ID nuevo (1..246)", g_newId, 3, 0, nullptr, 1, JWX_MAX_ID, [](int32_t v) {
            g_newId = (uint8_t)v;
            refreshList();
        });
    }
    else if (i == ADD_SAVE)
    {
        CommandArgs a;
        memset(&a, 0, sizeof(a));
        a.aux = g_newId;
        a.p1 = (int32_t)packSafe(g_safeValue, g_safeHold, g_watchdog);
        runCommand(CMD_X_ADD, a, [](bool ok) {
            if (ok)
            {
                pop(); // vuelve a EXPANSION con el módulo en la lista
                refreshList();
            }
        });
    }
    else
    {
        addSide(i, 1);
    }
}

void openAdd()
{
    g_newId = X().freeId();
    g_safeValue = 0;
    g_safeHold = 0;
    g_watchdog = JWX_FACTORY_WATCHDOG;
    ListSpec s = {"AGREGAR MODULO", "<> cambiar  OK elegir  ESC cancelar", addCount, addRow, addOk, addSide,
                  nullptr, 0};
    pushList(s, ADD_SAVE);
}

// ================================================================ MÓDULO

uint8_t g_mod = 0;
bool g_safeDirty = false;

const uint16_t MOD_Q0 = 6;
const uint16_t MOD_WD = MOD_Q0 + 8;
const uint16_t MOD_SAVE = MOD_WD + 1;
const uint16_t MOD_REMOVE = MOD_SAVE + 1;

uint16_t modCount() { return MOD_REMOVE + 1; }

void modRow(uint16_t i, JWPLC_UIRow r)
{
    const XLink &l = X().link(g_mod);
    const bool locked = l.flags & XL_LOCKED;
    char t[28];
    switch (i)
    {
    case 0:
        r.left("Estado");
        snprintf(t, sizeof(t), "ID %u  %s", l.id, stateText(X().state(g_mod)));
        r.right(t);
        return;
    case 1:
        r.left("Modelo");
        snprintf(t, sizeof(t), "%s %u/%u  v%u.%u", l.product == JWX_PRODUCT_BASIC ? "JWPLC Basic" : "Modulo",
                 l.io >> 4, l.io & 0x0F, l.firmware >> 4, l.firmware & 0x0F);
        r.right(t);
        return;
    case 2:
        r.left("Direcciones");
        snprintf(t, sizeof(t), "%u.I1-%u.I8 %u.Q1-%u.Q8", g_mod + 1, g_mod + 1, g_mod + 1, g_mod + 1);
        r.right(t);
        return;
    case 3:
    {
        r.left("Entradas I1..I8");
        bitsText(X().inputs(g_mod), t, sizeof(t));
        r.right(t);
        return;
    }
    case 4:
    {
        r.left("Salidas Q1..Q8");
        bitsText(core.mode == MODE_RUN ? core.engine.remoteOutputs(g_mod) : 0, t, sizeof(t));
        r.right(t);
        return;
    }
    case 5:
        r.left("Configuracion");
        r.right(locked ? "fija en su sketch" : "desde este maestro");
        return;
    default:
        break;
    }
    if (i < MOD_WD)
    {
        snprintf(t, sizeof(t), "Q%u sin maestro", (unsigned)(i - MOD_Q0 + 1));
        r.left(t);
        const char *m = safeText(g_safeValue, g_safeHold, (uint8_t)(i - MOD_Q0));
        if (locked)
        {
            r.right(m);
        }
        else
        {
            snprintf(t, sizeof(t), "< %s >", m);
            r.right(t);
        }
    }
    else if (i == MOD_WD)
    {
        r.left("Vigilancia");
        snprintf(t, sizeof(t), locked ? "%u,%u s" : "< %u,%u s >", g_watchdog / 10, g_watchdog % 10);
        r.right(t);
    }
    else if (i == MOD_SAVE)
    {
        r.left("Guardar estado seguro");
        r.right(locked ? "no (sketch)" : (g_safeDirty ? "pendiente >" : ">"));
    }
    else
    {
        r.left("Quitar modulo");
        r.right(">");
    }
}

void modSide(uint16_t i, int8_t dir)
{
    if (X().link(g_mod).flags & XL_LOCKED)
    {
        return;
    }
    if (i >= MOD_Q0 && i < MOD_WD)
    {
        cycleSafe(g_safeValue, g_safeHold, (uint8_t)(i - MOD_Q0), dir);
    }
    else if (i == MOD_WD)
    {
        int16_t w = (int16_t)(g_watchdog + dir * 5);
        g_watchdog = (uint8_t)(w < JWX_MIN_WATCHDOG ? JWX_MIN_WATCHDOG : (w > JWX_MAX_WATCHDOG ? JWX_MAX_WATCHDOG : w));
    }
    else
    {
        return;
    }
    g_safeDirty = true;
    refreshList();
}

void modOk(uint16_t i)
{
    if (i == MOD_SAVE)
    {
        if (X().link(g_mod).flags & XL_LOCKED)
        {
            footerMessage("Se cambia en el sketch del modulo", T.warn, 2500);
            return;
        }
        if (!requireStop())
        {
            return;
        }
        CommandArgs a;
        memset(&a, 0, sizeof(a));
        a.block = g_mod;
        a.p1 = (int32_t)packSafe(g_safeValue, g_safeHold, g_watchdog);
        runCommand(CMD_X_SAFE, a, [](bool ok) {
            g_safeDirty = !ok;
            refreshList();
        });
    }
    else if (i == MOD_REMOVE)
    {
        if (!requireStop())
        {
            return;
        }
        confirm("Quitar modulo", "Sus I y Q dejan de leerse.", []() {
            CommandArgs a;
            memset(&a, 0, sizeof(a));
            a.block = g_mod;
            runCommand(CMD_X_REMOVE, a, [](bool ok) {
                if (ok)
                {
                    pop();
                    refreshList();
                }
            });
        });
    }
    else
    {
        modSide(i, 1);
    }
}

void openModule(uint8_t module)
{
    g_mod = module;
    const XLink &l = X().link(module);
    g_safeValue = l.safeValue;
    g_safeHold = l.safeHold;
    g_watchdog = l.watchdog ? l.watchdog : JWX_FACTORY_WATCHDOG;
    g_safeDirty = false;
    static char title[16];
    snprintf(title, sizeof(title), "MODULO %u", module + 1);
    ListSpec s = {title, "<> cambiar  OK elegir  ESC volver", modCount, modRow, modOk, modSide, nullptr, 500};
    pushList(s);
}

// ================================================================ BÚSQUEDA

uint16_t foundCount() { return core.xFoundCount ? core.xFoundCount : 1; }

void foundRow(uint16_t i, JWPLC_UIRow r)
{
    if (!core.xFoundCount)
    {
        r.left("Ningun modulo responde");
        r.right("");
        return;
    }
    const XFound &f = core.xFound[i];
    char t[28];
    snprintf(t, sizeof(t), "ID %u", f.id);
    r.left(t);
    if (f.module >= 0)
    {
        snprintf(t, sizeof(t), "modulo %d", f.module + 1);
        r.right(t);
    }
    else if (f.status == JWX_STATUS_FACTORY)
    {
        r.right("de fabrica: Agregar");
    }
    else if (f.linked == 2)
    {
        r.right(f.status == JWX_STATUS_LOCKED ? "sketch, otro maestro" : "de otro maestro >");
    }
    else
    {
        r.right(f.status == JWX_STATUS_LOCKED ? "fijo en sketch >" : "sin vincular >");
    }
}

uint8_t g_linkId = 0;

void foundOk(uint16_t i)
{
    if (!core.xFoundCount)
    {
        return;
    }
    const XFound &f = core.xFound[i];
    if (f.module >= 0)
    {
        openModule((uint8_t)f.module);
        return;
    }
    if (f.status == JWX_STATUS_FACTORY)
    {
        footerMessage("Use + Agregar modulo", T.warn, 2500);
        return;
    }
    g_linkId = f.id;
    confirm("Vincular", f.linked == 2 ? "Es de otro maestro. Vincular aqui?" : "Usar como modulo de este equipo?",
            []() {
                CommandArgs a;
                memset(&a, 0, sizeof(a));
                a.aux = g_linkId;
                runCommand(CMD_X_LINK, a, [](bool ok) {
                    if (ok)
                    {
                        pop();
                        refreshList();
                    }
                });
            });
}

void openFound()
{
    ListSpec s = {"MODULOS EN EL BUS", "OK vincular/ver  ESC volver", foundCount, foundRow, foundOk, nullptr,
                  nullptr, 0};
    pushList(s);
}

} // namespace

void openExpansionMenu()
{
    ListSpec s = {"EXPANSION", "OK elegir  <> bus  ESC volver", expCount, expRow, expOk, expSide, nullptr, 500};
    pushList(s);
}

} // namespace rtui
