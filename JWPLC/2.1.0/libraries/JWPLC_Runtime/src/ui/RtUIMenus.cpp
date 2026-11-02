// Menú raíz (6 baldosas + INICIAR/DETENER) y submenús del Runtime.
#include "RtUI.h"
#include "../JWPLC_Runtime.h"

#include <string.h>

namespace rtui
{

// ================================================================ menú raíz

namespace
{
const char *const TILE_NAMES[6] = {"PROGRAMA", "PARAMETROS", "TARJETA SD",
                                   "CONFIG.", "RED", "DIAGNOSTICO"};
const int16_t TILE_W = 102;
const int16_t TILE_H = 52;

void tileRect(uint8_t i, int16_t &x, int16_t &y)
{
    x = (int16_t)(4 + (i % 3) * (TILE_W + 4));
    y = (int16_t)(BODY_Y + 2 + (i / 3) * (TILE_H + 4));
}

// Se dibuja en el sprite de la baldosa; c es un índice de su paleta.
void drawIcon(JWPLC_TFTSprite &g, uint8_t i, int16_t cx, int16_t cy, uint16_t c)
{
    switch (i)
    {
    case 0: // dos bloques unidos
        g.drawRect(cx - 14, cy - 7, 10, 14, c);
        g.drawRect(cx + 4, cy - 7, 10, 14, c);
        g.drawFastHLine(cx - 4, cy - 1, 8, c);
        g.drawFastHLine(cx - 4, cy, 8, c);
        break;
    case 1: // deslizadores
        for (int8_t k = -1; k <= 1; k++)
        {
            g.drawFastHLine(cx - 14, cy + k * 7, 28, c);
            const int16_t kx = (int16_t)(cx + (k == 0 ? 4 : (k < 0 ? -4 : -8)));
            g.fillRect(kx - 2, cy + k * 7 - 2, 5, 5, c);
        }
        break;
    case 2: // tarjeta SD
        g.drawLine(cx - 8, cy - 10, cx + 4, cy - 10, c);
        g.drawLine(cx + 4, cy - 10, cx + 9, cy - 5, c);
        g.drawFastVLine(cx + 9, cy - 5, 16, c);
        g.drawFastHLine(cx - 8, cy + 10, 18, c);
        g.drawFastVLine(cx - 8, cy - 10, 21, c);
        g.drawFastVLine(cx - 4, cy - 7, 4, c);
        g.drawFastVLine(cx - 1, cy - 7, 4, c);
        break;
    case 3: // engranaje
        g.drawCircle(cx, cy, 8, c);
        g.drawCircle(cx, cy, 7, c);
        g.fillCircle(cx, cy, 3, c);
        g.fillRect(cx - 2, cy - 11, 4, 3, c);
        g.fillRect(cx - 2, cy + 9, 4, 3, c);
        g.fillRect(cx - 11, cy - 2, 3, 4, c);
        g.fillRect(cx + 9, cy - 2, 3, 4, c);
        break;
    case 4: // red
        g.drawRect(cx - 4, cy - 11, 9, 7, c);
        g.drawFastVLine(cx, cy - 4, 5, c);
        g.drawFastHLine(cx - 10, cy + 1, 21, c);
        g.drawFastVLine(cx - 10, cy + 1, 4, c);
        g.drawFastVLine(cx + 10, cy + 1, 4, c);
        g.drawRect(cx - 14, cy + 5, 9, 7, c);
        g.drawRect(cx + 6, cy + 5, 9, 7, c);
        break;
    default: // pulso
        g.drawFastHLine(cx - 14, cy, 8, c);
        g.drawLine(cx - 6, cy, cx - 2, cy - 10, c);
        g.drawLine(cx - 2, cy - 10, cx + 2, cy + 10, c);
        g.drawLine(cx + 2, cy + 10, cx + 6, cy, c);
        g.drawFastHLine(cx + 6, cy, 8, c);
        break;
    }
}

class RootMenu : public Screen
{
public:
    uint8_t sel = 0;      // 0..5 baldosas, 6 = INICIAR/DETENER
    uint8_t drawnSel = 0xFF;
    int8_t drawnMode = -1;

    // La baldosa se arma en un sprite de 4 bits y se envía de una vez, para
    // que al mover la selección no se vea el relleno antes del icono.
    void drawTile(uint8_t i, bool selected)
    {
        int16_t x, y;
        tileRect(i, x, y);
        const uint16_t bg = selected ? C_TILE_SEL : T.surface;
        const uint16_t fg = selected ? C_ON : T.text;
        static JWPLC_TFTSprite g;
        if (!g.created() && !g.create(TILE_W, TILE_H, 4))
        {
            return; // sin memoria: la baldosa no se dibuja (no debería pasar)
        }
        // Paleta: 0 fondo de pantalla (esquinas), 1 baldosa, 2 borde, 3 icono y texto.
        g.setPaletteColor(0, T.background);
        g.setPaletteColor(1, bg);
        g.setPaletteColor(2, selected ? C_ON : T.border);
        g.setPaletteColor(3, fg);
        g.fill(0);
        g.fillRoundRect(0, 0, TILE_W, TILE_H, 5, 1);
        g.drawRoundRect(0, 0, TILE_W, TILE_H, 5, 2);
        if (selected)
        {
            g.drawRoundRect(1, 1, TILE_W - 2, TILE_H - 2, 4, 2);
        }
        drawIcon(g, i, TILE_W / 2, 16, 3);
        g.setFont(JWPLC_FONT_16);
        g.setTextSize(1);
        g.setTextDatum(DATUM_TOP_CENTER);
        g.setTextColor(3, 1);
        g.drawString(TILE_NAMES[i], TILE_W / 2, 32);
        g.push(x, y);
    }

    void drawAction(bool selected)
    {
        const bool run = core.mode == MODE_RUN;
        const int16_t x = 188, y = BODY_Y + 2 + 2 * (TILE_H + 4) - 2, w = 128, h = 20;
        const uint16_t c = run ? T.error : C_ON;
        const uint16_t bg = selected ? (run ? 0x6000 : C_TILE_SEL) : T.background;
        JWPLC_TFT.fillRoundRect(x, y, w, h, 4, bg);
        JWPLC_TFT.drawRoundRect(x, y, w, h, 4, c);
        if (selected)
        {
            JWPLC_TFT.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 3, c);
        }
        // Triángulo (iniciar) o cuadrado (detener).
        if (run)
        {
            JWPLC_TFT.fillRect(x + 12, y + 6, 8, 8, c);
        }
        else
        {
            JWPLC_TFT.fillTriangle(x + 12, y + 5, x + 12, y + 15, x + 20, y + 10, c);
        }
        textGlcd(run ? "DETENER" : "INICIAR", x + 30, y + 3, c, bg, 2);
        drawnMode = (int8_t)core.mode;
    }

    void enter() override
    {
        JWPLC_TFT.fillRect(0, BAR_H, SCREEN_W, SCREEN_H - BAR_H, T.background);
        drawStatusBar("MENU");
        for (uint8_t i = 0; i < 6; i++)
        {
            drawTile(i, i == sel);
        }
        drawAction(sel == 6);
        drawnSel = sel;
        drawFooter("OK abrir  ESC pantalla RUN");
    }

    void open(uint8_t i)
    {
        switch (i)
        {
        case 0:
            openProgramMenu();
            break;
        case 1:
            openParams();
            break;
        case 2:
            requireAdmin(openSdMenu);
            break;
        case 3:
            requireAdmin(openConfigMenu);
            break;
        case 4:
            openNetMenu();
            break;
        case 5:
            openDiagMenu();
            break;
        default:
            requireAdmin(startStop);
            break;
        }
    }

    static void startStop()
    {
        if (core.mode == MODE_RUN)
        {
            confirm("Detener", "Las salidas Q se apagan.", []() { runCommand(CMD_STOP); });
        }
        else if (core.engine.program().count == 0)
        {
            footerMessage("No hay programa: Programa > Editar", T.warn, 2500);
        }
        else
        {
            confirm("Iniciar", "El programa toma las salidas Q.", []() { runCommand(CMD_RUN); });
        }
    }

    void update() override
    {
        uint8_t s = sel;
        if (key(BTN_RIGHT))
        {
            s = (s == 6) ? 6 : (uint8_t)((s % 3 == 2) ? s - 2 : s + 1);
        }
        else if (key(BTN_LEFT))
        {
            s = (s == 6) ? 6 : (uint8_t)((s % 3 == 0) ? s + 2 : s - 1);
        }
        else if (key(BTN_DOWN))
        {
            s = (s < 3) ? (uint8_t)(s + 3) : (s < 6 ? 6 : 0);
        }
        else if (key(BTN_UP))
        {
            s = (s == 6) ? 5 : (s >= 3 ? (uint8_t)(s - 3) : 6);
        }
        else if (key(BTN_OK))
        {
            open(sel);
            return;
        }
        else if (key(BTN_ESC))
        {
            pop(); // en la raíz: vuelve a la pantalla RUN
            return;
        }
        if (s != sel)
        {
            if (sel < 6)
            {
                drawTile(sel, false);
            }
            else
            {
                drawAction(false);
            }
            sel = s;
            if (sel < 6)
            {
                drawTile(sel, true);
            }
            else
            {
                drawAction(true);
            }
        }
        if (drawnMode != (int8_t)core.mode)
        {
            drawAction(sel == 6);
        }
    }
};

RootMenu g_root;
} // namespace

void pushRootMenu()
{
    g_root.sel = 0;
    push(&g_root);
}

// ================================================================ PROGRAMA

namespace
{
const char *const PROG_ROWS[] = {"Editar", "Monitor (en vivo)", "Lista de bloques", "Nombre",
                                 "Recursos", "Cargar ejemplo", "Borrar programa"};

uint16_t progCount() { return 7; }

void progRow(uint16_t i, JWPLC_UIRow r)
{
    r.left(PROG_ROWS[i]);
    if (i == 0)
    {
        r.right(core.mode == MODE_RUN ? "requiere STOP" : ">");
    }
    else if (i == 3)
    {
        r.right(core.engine.program().name);
    }
    else if (i == 4)
    {
        char t[16];
        snprintf(t, sizeof(t), "%u/%u bloques", core.engine.program().count, MAX_BLOCKS);
        r.right(t);
    }
    else
    {
        r.right(">");
    }
}

void stopThenEdit()
{
    runCommand(CMD_STOP, [](bool) { openEditor(false); });
}

void editAdmin()
{
    if (core.mode == MODE_RUN)
    {
        confirm("Editar", "Hay que detener (STOP).", stopThenEdit);
    }
    else
    {
        openEditor(false);
    }
}

void nameDone(const char t[])
{
    core.draft = core.engine.program();
    strncpy(core.draft.name, t[0] ? t : "Sin nombre", NAME_LEN - 1);
    core.draft.name[NAME_LEN - 1] = 0;
    runCommand(CMD_APPLY_DRAFT);
}

// Lista de bloques
uint16_t blkCount() { return core.engine.program().count; }
void blkRow(uint16_t i, JWPLC_UIRow r)
{
    const Program &p = core.engine.program();
    const Block &b = p.blocks[i];
    const TypeInfo *ti = typeInfo(b.type);
    char l[24];
    snprintf(l, sizeof(l), "B%02u %s", i + 1, ti ? ti->name : "?");
    r.left(l);
    char t[24] = "";
    char x[12];
    if (ti && ti->p1Kind == PK_TIME)
    {
        formatTime(b.p1, (uint8_t)((b.flags & BF_BASE_MASK) >> BF_BASE_SHIFT), x, sizeof(x));
        snprintf(t, sizeof(t), "%s=%s", ti->p1Name, x);
    }
    else if (ti && ti->p1Kind == PK_COUNT)
    {
        snprintf(t, sizeof(t), "%ld/%ld", (long)core.engine.count((uint8_t)i), (long)b.p1);
    }
    else if (ti && ti->p1Kind == PK_CLOCK)
    {
        char y[8];
        formatClock(b.p1, x, sizeof(x));
        formatClock(b.p2, y, sizeof(y));
        snprintf(t, sizeof(t), "%s-%s", x, y);
    }
    else if (b.type == BT_ALARM)
    {
        snprintf(t, sizeof(t), "%s", p.texts[b.aux & 7]);
    }
    if (core.mode == MODE_RUN)
    {
        strncat(t, core.engine.blockOut((uint8_t)i) ? "  [1]" : "  [0]", sizeof(t) - strlen(t) - 1);
    }
    r.right(t);
}

void resourcesInfo()
{
    static char msg[64];
    const Program &p = core.engine.program();
    uint8_t q = 0;
    for (uint8_t i = 0; i < NUM_Q; i++)
    {
        q += p.q[i] != SRC_OPEN;
    }
    snprintf(msg, sizeof(msg), "%u/%u bloques, %u/8 Q. Imagen %u B.", p.count, MAX_BLOCKS, q,
             (unsigned)sizeof(Program));
    info("Recursos", msg);
}

void progOk(uint16_t i)
{
    switch (i)
    {
    case 0:
        requireAdmin(editAdmin);
        break;
    case 1:
        openEditor(true);
        break;
    case 2:
    {
        ListSpec s = {"BLOQUES", "^v elegir  ESC volver", blkCount, blkRow, nullptr, nullptr, nullptr, 500};
        pushList(s);
        break;
    }
    case 3:
        requireAdmin([]() { editText("Nombre del programa", core.engine.program().name, NAME_LEN - 1, nameDone); });
        break;
    case 4:
        resourcesInfo();
        break;
    case 5:
        requireAdmin([]() {
            confirm("Cargar ejemplo", "Reemplaza el programa (STOP).", []() {
                runCommand(CMD_STOP, [](bool) { runCommand(CMD_LOAD_DEMO, [](bool) { refreshList(); }); });
            });
        });
        break;
    case 6:
        requireAdmin([]() {
            confirm("Borrar programa", "Se borra el programa activo.", []() {
                runCommand(CMD_ERASE_PROGRAM, [](bool) { refreshList(); });
            });
        });
        break;
    }
}
} // namespace

void openProgramMenu()
{
    ListSpec s = {"PROGRAMA", "OK abrir  ESC volver", progCount, progRow, progOk, nullptr, nullptr, 0};
    pushList(s);
}

// ================================================================ PARÁMETROS

namespace
{
// Cada fila es (bloque, parámetro 1 o 2) de un bloque con "+".
uint8_t g_parBlock[MAX_BLOCKS * 2];
uint8_t g_parWhich[MAX_BLOCKS * 2];
uint8_t g_parCount = 0;
uint8_t g_parEdit = 0;

void buildParams()
{
    g_parCount = 0;
    const Program &p = core.engine.program();
    for (uint8_t b = 0; b < p.count; b++)
    {
        const TypeInfo *ti = typeInfo(p.blocks[b].type);
        if (!ti || !(p.blocks[b].flags & BF_PARAM))
        {
            continue;
        }
        if (ti->p1Kind != PK_NONE)
        {
            g_parBlock[g_parCount] = b;
            g_parWhich[g_parCount++] = 1;
        }
        if (ti->p2Kind != PK_NONE)
        {
            g_parBlock[g_parCount] = b;
            g_parWhich[g_parCount++] = 2;
        }
    }
}

uint16_t parCount() { return g_parCount; }

void paramValueText(const Block &b, uint8_t which, char out[], size_t size)
{
    const TypeInfo *ti = typeInfo(b.type);
    const uint8_t kind = which == 1 ? ti->p1Kind : ti->p2Kind;
    const int32_t v = which == 1 ? b.p1 : b.p2;
    if (kind == PK_TIME)
    {
        formatTime(v, (uint8_t)((b.flags & BF_BASE_MASK) >> BF_BASE_SHIFT), out, size);
    }
    else if (kind == PK_CLOCK)
    {
        formatClock(v, out, size);
    }
    else
    {
        snprintf(out, size, "%ld", (long)v);
    }
}

void parRow(uint16_t i, JWPLC_UIRow r)
{
    const Program &p = core.engine.program();
    const uint8_t b = g_parBlock[i];
    const Block &blk = p.blocks[b];
    const TypeInfo *ti = typeInfo(blk.type);
    char l[24];
    snprintf(l, sizeof(l), "B%02u %s %s", b + 1, ti->name, g_parWhich[i] == 1 ? ti->p1Name : ti->p2Name);
    r.left(l);
    char v[16];
    paramValueText(blk, g_parWhich[i], v, sizeof(v));
    char t[32];
    if (core.mode == MODE_RUN && g_parWhich[i] == 1 && ti->p1Kind == PK_TIME)
    {
        // Valor actual, como "Ta" del LOGO!.
        const uint32_t ms = core.engine.elapsedMs(b, millis());
        snprintf(t, sizeof(t), "%lu.%lus  %s", (unsigned long)(ms / 1000), (unsigned long)((ms % 1000) / 100), v);
    }
    else if (core.mode == MODE_RUN && ti->p1Kind == PK_COUNT)
    {
        snprintf(t, sizeof(t), "%ld  %s", (long)core.engine.count(b), v);
    }
    else
    {
        snprintf(t, sizeof(t), "%s", v);
    }
    r.right(t);
}

void parDone(int32_t v)
{
    const uint8_t i = g_parEdit;
    const uint8_t b = g_parBlock[i];
    const Block &blk = core.engine.program().blocks[b];
    const TypeInfo *ti = typeInfo(blk.type);
    const uint8_t kind = g_parWhich[i] == 1 ? ti->p1Kind : ti->p2Kind;
    if (kind == PK_CLOCK)
    {
        v = (v / 100) * 60 + (v % 100); // HHMM -> minutos
        if (v > 1439)
        {
            v = 1439;
        }
    }
    else if (kind == PK_TIME && ((blk.flags & BF_BASE_MASK) >> BF_BASE_SHIFT) != 0 && v % 100 > 59)
    {
        v = (v / 100) * 100 + 59;
    }
    CommandArgs a;
    memset(&a, 0, sizeof(a));
    a.block = b;
    a.p1 = g_parWhich[i] == 1 ? v : blk.p1;
    a.p2 = g_parWhich[i] == 2 ? v : blk.p2;
    a.aux = blk.aux;
    runCommand(CMD_SET_PARAM, a, [](bool) { refreshList(); });
}

void parOk(uint16_t i)
{
    g_parEdit = (uint8_t)i;
    const uint8_t b = g_parBlock[i];
    const Block &blk = core.engine.program().blocks[b];
    const TypeInfo *ti = typeInfo(blk.type);
    const uint8_t kind = g_parWhich[i] == 1 ? ti->p1Kind : ti->p2Kind;
    const int32_t v = g_parWhich[i] == 1 ? blk.p1 : blk.p2;
    static char title[32];
    snprintf(title, sizeof(title), "B%02u %s %s", b + 1, ti->name, g_parWhich[i] == 1 ? ti->p1Name : ti->p2Name);
    if (kind == PK_TIME)
    {
        static const char *const U[3] = {"s", "m", "h"};
        editValue(title, v, 4, 2, U[(blk.flags & BF_BASE_MASK) >> BF_BASE_SHIFT], 0, 9999, parDone);
    }
    else if (kind == PK_CLOCK)
    {
        editValue(title, (v / 60) * 100 + v % 60, 4, 2, "h", 0, 2359, parDone);
    }
    else
    {
        editValue(title, v, 6, 0, nullptr, 0, 999999, parDone);
    }
}
} // namespace

void openParams()
{
    buildParams();
    ListSpec s = {"PARAMETROS", "OK cambiar  ESC volver   (en RUN)", parCount, parRow, parOk, nullptr, nullptr, 500};
    pushList(s);
    if (g_parCount == 0)
    {
        footerMessage("Sin parametros: marca '+' en el bloque", T.warn, 3000);
    }
}

// ================================================================ TARJETA SD

namespace
{
const char *const SD_ROWS[] = {"Guardar programa en SD", "Cargar programa de SD", "Exportar eventos (CSV)", "Estado"};
uint16_t sdCount() { return 4; }
void sdRow(uint16_t i, JWPLC_UIRow r)
{
    r.left(SD_ROWS[i]);
    if (i == 3)
    {
        r.right(JWPLC_SD.isCardPresent() ? (JWPLC_SD.isReady() ? "lista" : "sin montar") : "sin tarjeta");
    }
    else
    {
        r.right(">");
    }
}

uint16_t fileCount() { return core.sdFileCount; }
void fileRow(uint16_t i, JWPLC_UIRow r)
{
    r.left(core.sdFiles[i]);
    r.right("cargar");
}
uint16_t g_fileSel = 0;
void fileLoad()
{
    CommandArgs a;
    memset(&a, 0, sizeof(a));
    strncpy(a.text, core.sdFiles[g_fileSel], sizeof(a.text) - 1);
    runCommand(CMD_SD_LOAD, a);
}
void fileOk(uint16_t i)
{
    g_fileSel = i;
    confirm("Cargar programa", "Reemplaza el activo (STOP).", fileLoad);
}

void sdOk(uint16_t i)
{
    switch (i)
    {
    case 0:
        runCommand(CMD_SD_SAVE);
        break;
    case 1:
        runCommand(CMD_SD_LIST, [](bool ok) {
            if (!ok)
            {
                return;
            }
            if (core.sdFileCount == 0)
            {
                footerMessage("No hay programas en /jwplc", T.warn, 2500);
                return;
            }
            ListSpec s = {"PROGRAMAS EN SD", "OK cargar  ESC volver", fileCount, fileRow, fileOk, nullptr, nullptr, 0};
            pushList(s);
        });
        break;
    case 2:
        runCommand(CMD_SD_EXPORT);
        break;
    default:
        refreshList();
        break;
    }
}
} // namespace

void openSdMenu()
{
    ListSpec s = {"TARJETA SD", "OK elegir  ESC volver", sdCount, sdRow, sdOk, nullptr, nullptr, 1000};
    pushList(s);
}

// ================================================================ CONFIGURACIÓN

namespace
{
const char *const CFG_ROWS[] = {"Fecha", "Hora", "Arranque", "PIN de Admin", "Volver a RUN sin teclas",
                                "Borrar retentivos", "Expansion", "Version"};
uint16_t cfgCount() { return 8; }
int32_t g_newDate = 0;

void cfgRow(uint16_t i, JWPLC_UIRow r)
{
    r.left(CFG_ROWS[i]);
    const Config &c = core.store.config();
    char t[24];
    const JWPLC_RTCState *rtc = jwplcGetRTCState();
    switch (i)
    {
    case 0:
        dateText(t, sizeof(t));
        r.right(t);
        break;
    case 1:
        if (rtc && rtc->valid)
        {
            snprintf(t, sizeof(t), "%02u:%02u:%02u", rtc->hour, rtc->minute, rtc->second);
        }
        else
        {
            snprintf(t, sizeof(t), "--:--:--");
        }
        r.right(t);
        break;
    case 2:
        r.right(c.startMode == START_RUN ? "< RUN >" : (c.startMode == START_STOP ? "< STOP >" : "< ultimo estado >"));
        break;
    case 3:
        r.right(c.pinEnabled ? "activo" : "sin PIN");
        break;
    case 4:
        if (c.idleMinutes)
        {
            snprintf(t, sizeof(t), "%u min", c.idleMinutes);
        }
        else
        {
            snprintf(t, sizeof(t), "nunca");
        }
        r.right(t);
        break;
    case 5:
        r.right(">");
        break;
    case 6:
        snprintf(t, sizeof(t), "%u modulo(s) >", core.expansion.count());
        r.right(t);
        break;
    default:
        snprintf(t, sizeof(t), "Runtime %s", JWPLC_RUNTIME_VERSION);
        r.right(t);
        break;
    }
}

void setClock(int32_t date, int32_t time)
{
    CommandArgs a;
    memset(&a, 0, sizeof(a));
    a.p1 = date;
    a.p2 = time;
    runCommand(CMD_SET_CLOCK, a, [](bool) { refreshList(); });
}

int32_t currentDate()
{
    const JWPLC_RTCState *rtc = jwplcGetRTCState();
    if (!rtc || !rtc->valid || rtc->year < 2000)
    {
        return 20260101;
    }
    return (int32_t)rtc->year * 10000 + rtc->month * 100 + rtc->day;
}

int32_t currentTime()
{
    const JWPLC_RTCState *rtc = jwplcGetRTCState();
    if (!rtc || !rtc->valid)
    {
        return 0;
    }
    return (int32_t)rtc->hour * 10000 + rtc->minute * 100 + rtc->second;
}

void cfgSide(uint16_t i, int8_t dir)
{
    Config &c = core.store.config();
    if (i == 2)
    {
        c.startMode = (uint8_t)((c.startMode + 3 + dir) % 3);
        runCommand(CMD_SAVE_CONFIG, [](bool) { refreshList(); });
    }
}

void cfgOk(uint16_t i)
{
    Config &c = core.store.config();
    switch (i)
    {
    case 0:
        editValue("Fecha AAAAMMDD", currentDate(), 8, 0, nullptr, 20000101, 20991231, [](int32_t v) {
            const int32_t m = (v / 100) % 100, d = v % 100;
            if (m < 1 || m > 12 || d < 1 || d > 31)
            {
                footerMessage("Fecha invalida", T.error, 2500);
                return;
            }
            setClock(v, currentTime());
        });
        break;
    case 1:
        editValue("Hora HHMMSS", currentTime(), 6, 0, nullptr, 0, 235959, [](int32_t v) {
            if ((v / 100) % 100 > 59 || v % 100 > 59)
            {
                footerMessage("Hora invalida", T.error, 2500);
                return;
            }
            setClock(currentDate(), v);
        });
        break;
    case 2:
        cfgSide(i, 1);
        break;
    case 3:
        editValue("PIN nuevo (0000 = sin PIN)", c.pin, 4, 0, nullptr, 0, 9999, [](int32_t v) {
            Config &cc = core.store.config();
            cc.pin = (uint16_t)v;
            cc.pinEnabled = v != 0;
            core.admin = true;
            runCommand(CMD_SAVE_CONFIG, [](bool) { refreshList(); });
        });
        break;
    case 4:
        editValue("Minutos (0 = nunca)", c.idleMinutes, 2, 0, "min", 0, 60, [](int32_t v) {
            core.store.config().idleMinutes = (uint8_t)v;
            runCommand(CMD_SAVE_CONFIG, [](bool) { refreshList(); });
        });
        break;
    case 5:
        confirm("Retentivos", "Contadores y TONR a cero.", []() { runCommand(CMD_CLEAR_RETAIN); });
        break;
    case 6:
        openExpansionMenu();
        break;
    default:
        break;
    }
}
} // namespace

void openConfigMenu()
{
    ListSpec s = {"CONFIGURACION", "OK cambiar  <> opcion  ESC volver", cfgCount, cfgRow, cfgOk, cfgSide, nullptr, 1000};
    pushList(s);
}

// ================================================================ RED

// Puerto de JWPLC_WebScreen si el sketch la usa (la librería la redefine); 0 = no activa.
extern "C" uint16_t __attribute__((weak)) jwplcWebScreenPort() { return 0; }

namespace
{
const char *const NET_ROWS[] = {"Enlace", "IP", "Mascara", "Puerta de enlace", "Pantalla web", "Conectar celular"};
uint16_t netCount() { return 6; }
void netRow(uint16_t i, JWPLC_UIRow r)
{
    r.left(NET_ROWS[i]);
    switch (i)
    {
    case 0:
        r.right(core.linkUp ? "conectado" : "sin cable");
        break;
    case 1:
        r.right(core.ip);
        break;
    case 2:
        r.right(core.mask);
        break;
    case 3:
        r.right(core.gateway);
        break;
    case 4:
    {
        // JWPLC_WebScreen en el sketch: dirección para el navegador.
        const uint16_t port = jwplcWebScreenPort();
        static char url[32];
        if (!port)
        {
            r.right("no activa");
        }
        else if (core.ip[0] == '-')
        {
            r.right("sin IP");
        }
        else
        {
            if (port == 80)
            {
                snprintf(url, sizeof(url), "http://%s/", core.ip);
            }
            else
            {
                snprintf(url, sizeof(url), "http://%s:%u/", core.ip, (unsigned)port);
            }
            r.right(url);
        }
        break;
    }
    default:
        r.right("vista previa >");
        break;
    }
}

// Vinculación del celular: vista previa del hito F9 (todavía sin servidor web).
class PhoneScreen : public Screen
{
public:
    JWPLC_UIQR qr;
    char code[8];
    uint32_t start = 0;
    int32_t drawnS = -1;

    void newCode()
    {
        snprintf(code, sizeof(code), "%06ld", random(0, 1000000));
        char url[48];
        snprintf(url, sizeof(url), "http://%s/v?c=%s", core.ip[0] == '-' ? "192.168.1.50" : core.ip, code);
        qr.setText(url);
        start = millis();
        drawnS = -1;
    }
    void enter() override
    {
        clearBody();
        drawStatusBar("CELULAR");
        newCode();
        const uint8_t sc = (uint8_t)(128 / qr.drawSize(1));
        const int16_t side = qr.drawSize(sc ? sc : 1);
        qr.draw((int16_t)(72 - side / 2), (int16_t)(BODY_Y + 2 + (130 - side) / 2), sc ? sc : 1);
        text16("Escanea con la camara", 148, 28, T.text, T.background);
        JWPLC_TFT.setFont(JWPLC_FONT_26);
        JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
        JWPLC_TFT.setTextColor(T.accent, T.background);
        char g[10];
        snprintf(g, sizeof(g), "%.3s %.3s", code, code + 3);
        JWPLC_TFT.drawString(g, 148, 50);
        JWPLC_TFT.fillRoundRect(148, 104, 168, 46, 5, T.surface);
        JWPLC_TFT.drawRoundRect(148, 104, 168, 46, 5, T.warn);
        text16("Vista previa (F9):", 156, 108, T.text, T.surface);
        text16("falta el servidor web", 156, 127, T.warn, T.surface);
        drawFooter("ESC volver");
    }
    void update() override
    {
        const uint32_t left = 60000 - min<uint32_t>(60000, millis() - start);
        const int32_t s = (int32_t)((left + 999) / 1000);
        if (s != drawnS)
        {
            drawnS = s;
            JWPLC_TFT.fillRect(148, 80, 168, 4, T.surfaceAlt);
            JWPLC_TFT.fillRect(148, 80, (int16_t)(168 * left / 60000), 4, T.accent);
            char t[24];
            snprintf(t, sizeof(t), "Expira en %ld s ", (long)s);
            textGlcd(t, 148, 88, T.textMuted, T.background, 1);
        }
        if (left == 0)
        {
            enter();
        }
        if (key(BTN_ESC))
        {
            pop();
        }
    }
};
PhoneScreen g_phone;

void netOk(uint16_t i)
{
    if (i == 5)
    {
        push(&g_phone);
    }
    else
    {
        runCommand(CMD_NET_REFRESH, [](bool) { refreshList(); });
    }
}
} // namespace

void openNetMenu()
{
    runCommand(CMD_NET_REFRESH, [](bool) {
        ListSpec s = {"RED", "OK actualizar  ESC volver", netCount, netRow, netOk, nullptr, nullptr, 0};
        pushList(s);
    });
}

// ================================================================ DIAGNÓSTICO

namespace
{
const char *const DIAG_ROWS[] = {"Resumen", "Eventos", "Avisos activos", "Prueba de salidas Q", "Borrar eventos"};
uint16_t diagCount() { return 5; }
void diagRow(uint16_t i, JWPLC_UIRow r)
{
    r.left(DIAG_ROWS[i]);
    char t[16];
    if (i == 1)
    {
        snprintf(t, sizeof(t), "%u", core.store.eventCount());
        r.right(t);
    }
    else if (i == 2)
    {
        uint8_t n = 0;
        const uint64_t a = core.engine.alarms();
        for (uint8_t b = 0; b < MAX_BLOCKS; b++)
        {
            n += (a >> b) & 1;
        }
        snprintf(t, sizeof(t), "%u", n);
        r.right(t);
    }
    else if (i == 3)
    {
        r.right(core.mode == MODE_RUN ? "requiere STOP" : ">");
    }
    else
    {
        r.right(">");
    }
}

// Resumen
const char *const SUM_ROWS[] = {"Estado", "Programa", "Scan (prom/max)", "Scans", "FRAM", "microSD",
                                "Reloj", "Memoria libre", "Encendido hace"};
uint16_t sumCount() { return 9; }
void sumRow(uint16_t i, JWPLC_UIRow r)
{
    r.left(SUM_ROWS[i]);
    char t[28];
    switch (i)
    {
    case 0:
        r.right(core.mode == MODE_RUN ? "RUN" : "STOP");
        break;
    case 1:
        snprintf(t, sizeof(t), "%s (%u)", core.engine.program().name, core.engine.program().count);
        r.right(t);
        break;
    case 2:
        snprintf(t, sizeof(t), "%lu / %lu us", (unsigned long)core.scanUsAvg, (unsigned long)core.scanUsMax);
        r.right(t);
        break;
    case 3:
        snprintf(t, sizeof(t), "%lu", (unsigned long)core.scanCount);
        r.right(t);
        break;
    case 4:
        r.right(core.framOk ? "OK" : "sin respuesta");
        break;
    case 5:
        r.right(JWPLC_SD.isCardPresent() ? (JWPLC_SD.isReady() ? "OK" : "sin montar") : "sin tarjeta");
        break;
    case 6:
        dateText(t, sizeof(t));
        r.right(t);
        break;
    case 7:
        snprintf(t, sizeof(t), "%lu B", (unsigned long)ESP.getFreeHeap());
        r.right(t);
        break;
    default:
    {
        const uint32_t s = millis() / 1000;
        snprintf(t, sizeof(t), "%luh %02lum %02lus", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60),
                 (unsigned long)(s % 60));
        r.right(t);
        break;
    }
    }
}

// Eventos (más reciente arriba)
uint16_t evCount() { return core.store.eventCount(); }
void evRow(uint16_t i, JWPLC_UIRow r)
{
    Event e;
    if (!core.store.event((uint8_t)i, e))
    {
        return;
    }
    char l[32];
    if (e.code == EV_ALARM_ON || e.code == EV_ALARM_OFF)
    {
        const Program &p = core.engine.program();
        const char *txt = (e.arg >= 1 && e.arg <= p.count) ? p.texts[p.blocks[e.arg - 1].aux & 7] : "";
        snprintf(l, sizeof(l), "%s %s", e.code == EV_ALARM_ON ? "+" : "-", txt[0] ? txt : eventText(e.code));
    }
    else
    {
        snprintf(l, sizeof(l), "%s", eventText(e));
    }
    r.left(l);
    char t[24];
    if (e.year)
    {
        snprintf(t, sizeof(t), "%02u/%02u %02u:%02u:%02u", e.day, e.month, e.hour, e.minute, e.second);
    }
    else
    {
        snprintf(t, sizeof(t), "+%lus", (unsigned long)e.uptimeS);
    }
    r.right(t);
}

// Avisos activos
uint8_t g_alarmList[MAX_BLOCKS];
uint8_t g_alarmCount = 0;
void buildAlarms()
{
    g_alarmCount = 0;
    const uint64_t a = core.engine.alarms();
    for (uint8_t b = 0; b < MAX_BLOCKS; b++)
    {
        if ((a >> b) & 1)
        {
            g_alarmList[g_alarmCount++] = b;
        }
    }
}
uint16_t alCount()
{
    buildAlarms();
    return g_alarmCount;
}
void alRow(uint16_t i, JWPLC_UIRow r)
{
    const Program &p = core.engine.program();
    const uint8_t b = g_alarmList[i];
    r.left(p.texts[p.blocks[b].aux & 7]);
    static const char *const SEV[3] = {"info", "aviso", "critico"};
    char t[20];
    snprintf(t, sizeof(t), "B%02u %s", b + 1, SEV[((p.blocks[b].aux >> 4) & 3) % 3]);
    r.right(t);
}

// Prueba de salidas (solo STOP)
uint16_t testCount() { return NUM_Q; }
void testRow(uint16_t i, JWPLC_UIRow r)
{
    char l[8];
    snprintf(l, sizeof(l), "Q%u", i + 1);
    r.left(l);
    r.right(((core.testQ >> i) & 1) ? "[ 1 ]  ON" : "[ 0 ]  off");
}
void testOk(uint16_t i)
{
    CommandArgs a;
    memset(&a, 0, sizeof(a));
    a.aux = (uint8_t)(core.testQ ^ (1 << i));
    runCommand(CMD_TEST_Q, a, [](bool) { refreshList(); });
}
void testTick()
{
    // Al salir de la lista se apagan las salidas de prueba.
}

class TestGuard : public Screen
{
public:
    void enter() override
    {
        ListSpec s = {"PRUEBA DE Q", "OK conmutar  ESC salir (apaga Q)", testCount, testRow, testOk, nullptr, testTick, 0};
        pushList(s);
    }
    void resume() override
    {
        // Se volvió de la lista: fin de la prueba.
        pop();
        runCommand(CMD_TEST_END);
    }
    void update() override {}
};
TestGuard g_testGuard;

void diagOk(uint16_t i)
{
    switch (i)
    {
    case 0:
    {
        ListSpec s = {"RESUMEN", "ESC volver", sumCount, sumRow, nullptr, nullptr, nullptr, 1000};
        pushList(s);
        break;
    }
    case 1:
    {
        ListSpec s = {"EVENTOS", "^v recorrer  ESC volver", evCount, evRow, nullptr, nullptr, nullptr, 1000};
        pushList(s);
        break;
    }
    case 2:
    {
        ListSpec s = {"AVISOS ACTIVOS", "ESC volver", alCount, alRow, nullptr, nullptr, nullptr, 1000};
        pushList(s);
        break;
    }
    case 3:
        if (core.mode == MODE_RUN)
        {
            footerMessage("Requiere STOP", T.warn, 2000);
        }
        else
        {
            requireAdmin([]() { push(&g_testGuard); });
        }
        break;
    default:
        requireAdmin([]() { confirm("Eventos", "Borrar todos los eventos?", []() { runCommand(CMD_CLEAR_EVENTS, [](bool) { refreshList(); }); }); });
        break;
    }
}
} // namespace

void openDiagMenu()
{
    ListSpec s = {"DIAGNOSTICO", "OK abrir  ESC volver", diagCount, diagRow, diagOk, nullptr, nullptr, 1000};
    pushList(s);
}

} // namespace rtui
