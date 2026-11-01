// Marco de la UI del Runtime: pila de pantallas, barra de estado, pie,
// órdenes a la tarea, listas genéricas y modales.
#include "RtUI.h"

#include <string.h>

namespace rtui
{

JWPLC_UITheme T = JWPLC_UI_THEME_DARK;

namespace
{
const uint8_t STACK_MAX = 10;
Screen *g_stack[STACK_MAX];
uint8_t g_depth = 0;
bool g_needResume = false;
volatile bool g_idleRequest = false;
volatile uint32_t g_lastKey = 0;

// Barra de estado: lo último dibujado.
char g_barTitle[24] = "";
int8_t g_barMode = -1;
char g_barClock[8] = "";

// Pie.
char g_footHelp[64] = "";
char g_footMsg[48] = "";
uint16_t g_footColor = 0;
uint32_t g_footUntil = 0;
bool g_footMsgShown = false;

// Orden en curso.
uint32_t g_cmdSeq = 0;
bool g_cmdPending = false;
void (*g_cmdDone)(bool) = nullptr;
} // namespace

// ---------------------------------------------------------------- pila

void push(Screen *s)
{
    if (g_depth >= STACK_MAX || !s)
    {
        return;
    }
    JWPLC_Buttons.clearPendingInput();
    g_stack[g_depth++] = s;
    g_needResume = false;
    s->enter();
}

void pop()
{
    if (g_depth > 1)
    {
        g_depth--;
        JWPLC_Buttons.clearPendingInput();
        g_needResume = true; // la de abajo se redibuja al final del ciclo
    }
    else
    {
        g_idleRequest = true; // ESC en el menú raíz: pantalla RUN
    }
}

void popToRoot()
{
    if (g_depth > 1)
    {
        g_depth = 1;
        g_needResume = true;
    }
}

void popTo(Screen *s)
{
    bool found = false;
    for (uint8_t d = 0; d < g_depth; d++)
    {
        found = found || g_stack[d] == s;
    }
    if (!found)
    {
        return;
    }
    while (g_depth > 1 && g_stack[g_depth - 1] != s)
    {
        g_depth--;
    }
    JWPLC_Buttons.clearPendingInput();
    g_needResume = true;
}

volatile bool editing = false;

void requestIdle()
{
    g_idleRequest = true;
}

Screen *current()
{
    return g_depth ? g_stack[g_depth - 1] : nullptr;
}

bool takeIdleRequest()
{
    if (g_idleRequest)
    {
        g_idleRequest = false;
        return true;
    }
    return false;
}

uint32_t lastKeyMs()
{
    return g_lastKey;
}

bool key(uint8_t id)
{
    if (JWPLC_Buttons.pressed(id))
    {
        g_lastKey = millis();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- dibujo

void text16(const char t[], int16_t x, int16_t y, uint16_t fg, uint16_t bg)
{
    JWPLC_TFT.setFont(JWPLC_FONT_16);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(fg, bg);
    JWPLC_TFT.drawString(t, x, y);
}

void textGlcd(const char t[], int16_t x, int16_t y, uint16_t fg, uint16_t bg, uint8_t size)
{
    JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
    JWPLC_TFT.setTextSize(size);
    JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    JWPLC_TFT.setTextColor(fg, bg);
    JWPLC_TFT.drawString(t, x, y);
    JWPLC_TFT.setTextSize(1);
}

void clearBody()
{
    JWPLC_TFT.fillRect(0, BAR_H, SCREEN_W, FOOT_Y - BAR_H, T.background);
}

void drawStatusBar(const char title[])
{
    JWPLC_TFT.fillRect(0, 0, SCREEN_W, BAR_H, T.header);
    textGlcd(title, 6, 4, T.headerText, T.header, 2);
    strncpy(g_barTitle, title, sizeof(g_barTitle) - 1);
    refreshStatusBar(true);
}

void refreshStatusBar(bool force)
{
    const int8_t mode = (int8_t)core.mode;
    if (force || mode != g_barMode)
    {
        g_barMode = mode;
        const uint16_t c = mode == MODE_RUN ? C_RUN : C_STOP;
        JWPLC_TFT.fillRoundRect(236, 4, 38, 14, 3, c);
        JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
        JWPLC_TFT.setTextSize(1);
        JWPLC_TFT.setTextDatum(DATUM_MIDDLE_CENTER);
        JWPLC_TFT.setTextColor(0x0000, c);
        JWPLC_TFT.drawString(mode == MODE_RUN ? "RUN" : "STOP", 255, 11);
        JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
    }
    char clk[8];
    clockText(clk, sizeof(clk));
    if (force || strcmp(clk, g_barClock) != 0)
    {
        strcpy(g_barClock, clk);
        textGlcd(clk, 284, 8, T.headerText, T.header, 1);
    }
}

static void paintFooter(const char t[], uint16_t color)
{
    JWPLC_TFT.fillRect(0, FOOT_Y, SCREEN_W, FOOT_H, T.background);
    JWPLC_TFT.drawFastHLine(0, FOOT_Y - 1, SCREEN_W, T.border);
    textGlcd(t, 4, FOOT_Y + 4, color, T.background, 1);
}

void drawFooter(const char help[])
{
    strncpy(g_footHelp, help ? help : "", sizeof(g_footHelp) - 1);
    if (!g_footMsgShown)
    {
        paintFooter(g_footHelp, T.textMuted);
    }
}

void footerMessage(const char text[], uint16_t color, uint32_t ms)
{
    strncpy(g_footMsg, text, sizeof(g_footMsg) - 1);
    g_footColor = color ? color : T.text;
    g_footUntil = millis() + ms;
    g_footMsgShown = true;
    paintFooter(g_footMsg, g_footColor);
}

void serviceFooter()
{
    if (g_footMsgShown && (int32_t)(millis() - g_footUntil) >= 0)
    {
        g_footMsgShown = false;
        paintFooter(g_footHelp, T.textMuted);
    }
}

// ---------------------------------------------------------------- órdenes

void runCommand(Command cmd, const CommandArgs &args, void (*onDone)(bool ok))
{
    g_cmdSeq = post(cmd, args);
    g_cmdPending = true;
    g_cmdDone = onDone;
    footerMessage("Procesando...", T.warn, 60000);
}

void runCommand(Command cmd, void (*onDone)(bool ok))
{
    CommandArgs a;
    memset(&a, 0, sizeof(a));
    runCommand(cmd, a, onDone);
}

bool commandPending()
{
    return g_cmdPending;
}

void serviceCommand()
{
    if (!g_cmdPending || !done(g_cmdSeq))
    {
        return;
    }
    g_cmdPending = false;
    const bool ok = core.result == RESULT_OK;
    if (core.resultText[0])
    {
        footerMessage(core.resultText, ok ? T.ok : T.error, 2500);
    }
    else
    {
        g_footUntil = millis(); // quita "Procesando..."
    }
    JWPLC_Buttons.clearPendingInput();
    void (*cb)(bool) = g_cmdDone;
    g_cmdDone = nullptr;
    if (cb)
    {
        cb(ok);
    }
}

// ---------------------------------------------------------------- listas

namespace
{
class ListScreen : public Screen
{
public:
    ListSpec spec;
    JWPLC_UIList list;
    uint16_t initial = 0;
    uint32_t lastRefresh = 0;

    void enter() override
    {
        // La lista pinta todo su área (filas, resto y barra): no se borra antes
        // para que no se vea un barrido. Solo las 2 líneas sobre la lista.
        JWPLC_TFT.fillRect(0, BAR_H, SCREEN_W, BODY_Y + 1 - BAR_H, T.background);
        JWPLC_TFT.fillRect(0, BODY_Y + BODY_H, SCREEN_W, FOOT_Y - BODY_Y - BODY_H, T.background);
        drawStatusBar(spec.title);
        list.begin(0, BODY_Y + 1, SCREEN_W, BODY_H - 1, 20, T);
        list.setRows(spec.count ? spec.count() : 0, spec.row);
        if (initial < list.rowCount())
        {
            list.setSelected(initial);
        }
        list.invalidate();
        list.draw();
        drawFooter(spec.help);
        lastRefresh = millis();
    }

    void resume() override
    {
        initial = list.selected();
        enter();
    }

    void update() override
    {
        list.handleKeys();
        if (spec.tick)
        {
            spec.tick();
        }
        if (current() != this)
        {
            return;
        }
        if (key(BTN_OK) && spec.ok && list.rowCount())
        {
            spec.ok(list.selected());
        }
        else if (key(BTN_ESC))
        {
            pop();
            return;
        }
        else if (spec.side && list.rowCount())
        {
            if (key(BTN_LEFT))
            {
                spec.side(list.selected(), -1);
            }
            else if (key(BTN_RIGHT))
            {
                spec.side(list.selected(), 1);
            }
        }
        if (current() != this)
        {
            return;
        }
        if (spec.refreshMs && millis() - lastRefresh >= spec.refreshMs)
        {
            lastRefresh = millis();
            refresh(false);
        }
        list.draw();
    }

    void refresh(bool all)
    {
        const uint16_t n = spec.count ? spec.count() : 0;
        if (n != list.rowCount() || all)
        {
            const uint16_t sel = list.selected();
            list.setRows(n, spec.row);
            if (n)
            {
                list.setSelected(sel < n ? sel : (uint16_t)(n - 1));
            }
            list.invalidate();
        }
        else
        {
            list.refreshChanged();
        }
    }
};

ListScreen g_lists[6];
uint8_t g_listNext = 0;
} // namespace

void pushList(const ListSpec &spec, uint16_t selected)
{
    // Busca una lista libre (que no esté en la pila).
    ListScreen *ls = nullptr;
    for (uint8_t k = 0; k < 6 && !ls; k++)
    {
        ListScreen *cand = &g_lists[(g_listNext + k) % 6];
        bool used = false;
        for (uint8_t d = 0; d < g_depth; d++)
        {
            used = used || g_stack[d] == cand;
        }
        if (!used)
        {
            ls = cand;
            g_listNext = (uint8_t)((g_listNext + k + 1) % 6);
        }
    }
    if (!ls)
    {
        return;
    }
    ls->spec = spec;
    ls->initial = selected;
    push(ls);
}

void refreshList()
{
    for (uint8_t k = 0; k < 6; k++)
    {
        if (current() == &g_lists[k])
        {
            g_lists[k].refresh(true);
        }
    }
}

uint16_t listSelected()
{
    for (uint8_t k = 0; k < 6; k++)
    {
        if (current() == &g_lists[k])
        {
            return g_lists[k].list.selected();
        }
    }
    return 0;
}

// ---------------------------------------------------------------- modales

namespace
{
class ConfirmScreen : public Screen
{
public:
    JWPLC_UIDialog dialog;
    char title[32];
    char message[64];
    bool yesNo = true;
    void (*onYes)() = nullptr;
    void (*onNo)() = nullptr;

    bool modal() const override { return true; }
    void enter() override
    {
        dialog.open(title, message, yesNo, T);
        if (yesNo)
        {
            dialog.selectNo(); // como el LOGO!: nada se confirma con un OK de más
        }
        dialog.draw();
    }
    void update() override
    {
        const uint8_t r = dialog.handleKeys();
        if (r == UI_DIALOG_YES || r == UI_DIALOG_NO)
        {
            void (*cb)() = (r == UI_DIALOG_YES) ? onYes : onNo;
            pop();
            if (cb)
            {
                cb();
            }
            return;
        }
        dialog.draw();
    }
};
ConfirmScreen g_confirm;

class ValueScreen : public Screen
{
public:
    JWPLC_UIDigitEditor editor;
    char title[32];
    char unit[6];
    int32_t value = 0, minV = 0, maxV = 0;
    uint8_t digits = 4, decimals = 0;
    void (*onDone)(int32_t) = nullptr;

    bool modal() const override { return true; }
    void enter() override
    {
        JWPLC_TFT.fillRoundRect(30, 34, 260, 112, 6, T.surface);
        JWPLC_TFT.drawRoundRect(30, 34, 260, 112, 6, T.accent);
        text16(title, 42, 42, T.text, T.surface);
        editor.begin(0, 70, digits, decimals, unit[0] ? unit : nullptr, T);
        editor.setRange(minV, maxV);
        const int16_t x = (int16_t)(160 - editor.width() / 2);
        editor.begin(x, 70, digits, decimals, unit[0] ? unit : nullptr, T);
        editor.setRange(minV, maxV);
        editor.setValue(value);
        editor.invalidate();
        editor.draw();
        textGlcd("<> digito  ^v valor  OK  ESC", 42, 128, T.textMuted, T.surface, 1);
    }
    void update() override
    {
        const uint8_t r = editor.handleKeys();
        if (r == UI_EDIT_ACCEPTED)
        {
            const int32_t v = editor.value();
            void (*cb)(int32_t) = onDone;
            pop();
            if (cb)
            {
                cb(v);
            }
            return;
        }
        if (r == UI_EDIT_CANCELLED)
        {
            pop();
            return;
        }
        editor.draw();
    }
};
ValueScreen g_value;

class TextScreen : public Screen
{
public:
    JWPLC_UIKeyboard kb;
    char title[32];
    char initial[24];
    uint8_t maxLen = 16;
    void (*onDone)(const char[]) = nullptr;

    void enter() override
    {
        JWPLC_TFT.fillRect(0, 0, SCREEN_W, SCREEN_H, T.background);
        kb.begin(4, T);
        kb.open(title, initial, maxLen, false);
        kb.draw();
    }
    void update() override
    {
        const uint8_t r = kb.handleKeys();
        if (r == UI_EDIT_ACCEPTED)
        {
            char out[24];
            strncpy(out, kb.text(), sizeof(out) - 1);
            out[sizeof(out) - 1] = 0;
            void (*cb)(const char[]) = onDone;
            pop();
            if (cb)
            {
                cb(out);
            }
            return;
        }
        if (r == UI_EDIT_CANCELLED)
        {
            pop();
            return;
        }
        kb.draw();
    }
};
TextScreen g_text;

void (*g_adminOk)() = nullptr;

void pinEntered(int32_t pin)
{
    if ((uint16_t)pin == core.store.config().pin)
    {
        core.admin = true;
        if (g_adminOk)
        {
            g_adminOk();
        }
    }
    else
    {
        CommandArgs a;
        memset(&a, 0, sizeof(a));
        a.block = EV_PIN_FAIL;
        post(CMD_EVENT, a);
        footerMessage("PIN incorrecto", T.error, 2500);
    }
}
} // namespace

void confirm(const char title[], const char message[], void (*onYes)(), void (*onNo)())
{
    strncpy(g_confirm.title, title, sizeof(g_confirm.title) - 1);
    strncpy(g_confirm.message, message, sizeof(g_confirm.message) - 1);
    g_confirm.title[sizeof(g_confirm.title) - 1] = 0;
    g_confirm.message[sizeof(g_confirm.message) - 1] = 0;
    g_confirm.yesNo = true;
    g_confirm.onYes = onYes;
    g_confirm.onNo = onNo;
    push(&g_confirm);
}

void info(const char title[], const char message[])
{
    confirm(title, message, nullptr, nullptr);
    g_confirm.yesNo = false;
    g_confirm.enter();
}

void editValue(const char title[], int32_t value, uint8_t digits, uint8_t decimals,
               const char unit[], int32_t minValue, int32_t maxValue, void (*onDone)(int32_t))
{
    strncpy(g_value.title, title, sizeof(g_value.title) - 1);
    g_value.title[sizeof(g_value.title) - 1] = 0;
    strncpy(g_value.unit, unit ? unit : "", sizeof(g_value.unit) - 1);
    g_value.unit[sizeof(g_value.unit) - 1] = 0;
    g_value.value = value;
    g_value.digits = digits;
    g_value.decimals = decimals;
    g_value.minV = minValue;
    g_value.maxV = maxValue;
    g_value.onDone = onDone;
    push(&g_value);
}

void editText(const char title[], const char initial[], uint8_t maxLen, void (*onDone)(const char[]))
{
    strncpy(g_text.title, title, sizeof(g_text.title) - 1);
    g_text.title[sizeof(g_text.title) - 1] = 0;
    strncpy(g_text.initial, initial ? initial : "", sizeof(g_text.initial) - 1);
    g_text.initial[sizeof(g_text.initial) - 1] = 0;
    g_text.maxLen = maxLen;
    g_text.onDone = onDone;
    push(&g_text);
}

void requireAdmin(void (*onOk)())
{
    if (!core.store.config().pinEnabled || core.admin)
    {
        if (onOk)
        {
            onOk();
        }
        return;
    }
    g_adminOk = onOk;
    editValue("PIN de Admin", 0, 4, 0, nullptr, 0, 9999, pinEntered);
}

// ---------------------------------------------------------------- arranque

void begin()
{
    g_depth = 0;
}

} // namespace rtui

// ---------------------------------------------------------------- callbacks de JWPLC_Display

using namespace rtui;

extern "C" void jwplcUIEnter(void)
{
    // Teclas pulsadas en la pantalla RUN no deben actuar sobre el menú.
    JWPLC_Display.clearPendingInput();
    g_depth = 0;
    core.admin = false;
    g_lastKey = millis();
    releaseHMI();
    // ◄ ► en la pantalla RUN: páginas HMI (la última o la primera); el resto, menú.
    const bool right = JWPLC_Buttons.isDown(BTN_RIGHT);
    const bool left = JWPLC_Buttons.isDown(BTN_LEFT);
    pushRootMenu();
    if (hasHMI() && (right || left))
    {
        openHMI(right ? 0 : (uint8_t)(hmiPageCount() - 1));
    }
}

extern "C" void jwplcUIUpdate(void)
{
    // Actividad: cualquier tecla mantenida.
    for (uint8_t b = 0; b < BTN_COUNT; b++)
    {
        if (JWPLC_Buttons.isDown(b))
        {
            g_lastKey = millis();
        }
    }
    serviceCommand();
    Screen *s = current();
    if (s && !commandPending())
    {
        s->update();
    }
    if (g_needResume && current())
    {
        g_needResume = false;
        // Si debajo de un modal quedó otro modal, se redibuja desde la primera
        // pantalla no modal.
        uint8_t first = g_depth - 1;
        while (first > 0 && g_stack[first]->modal())
        {
            first--;
        }
        for (uint8_t d = first; d < g_depth; d++)
        {
            g_stack[d]->resume();
        }
    }
    if (!hmiActive()) // la página HMI ocupa toda la pantalla
    {
        refreshStatusBar(false);
        serviceFooter();
    }
}

extern "C" void jwplcUIExit(void)
{
    releaseHMI();
    g_depth = 0;
    core.admin = false;
}
