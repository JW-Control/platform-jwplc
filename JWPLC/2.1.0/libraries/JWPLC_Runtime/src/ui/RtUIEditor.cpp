// Editor FBD del Runtime: mapa del programa, detalle del bloque, elección de
// fuentes y paleta de bloques con ayuda.
//
// El mapa se dibuja entero en un sprite de 4 bits (21 KB) y se envía de una
// vez: mover el cursor o refrescar valores en vivo no deja barridos negros.
#include "RtUI.h"

#include <string.h>

namespace rtui
{

namespace
{
// ---------------------------------------------------------------- paleta del sprite
enum : uint8_t
{
    P_BG = 0,
    P_SURF,
    P_BORDER,
    P_TEXT,
    P_MUTED,
    P_ON,
    P_SEL,
    P_WARN,
    P_ERR,
    P_TAG,
    P_ONBG,
    P_ACCENT,
    P_DOT,
};

const int16_t CH = 34;      // canal entre columnas (cables y etiquetas)
const int16_t BW = 66;      // ancho del bloque
const int16_t PITCH = BW + CH;
const int16_t LABEL_H = 9;  // "B02" encima del bloque
const int16_t PIN_DY = 12;
const int16_t QW = 30;      // etiqueta Q
const int16_t QH = 13;
const int16_t Q_PITCH = 16;
const int16_t MAP_H = BODY_H; // alto del sprite

const uint8_t ITEM_Q = 100;  // índice de selección de Q1..Q8 = 100..107

// ---------------------------------------------------------------- estado del editor
bool g_monitor = false;
bool g_dirty = false;
Program g_undo[4];
uint8_t g_undoCount = 0;

const Program &prog()
{
    return g_monitor ? core.engine.program() : core.draft;
}

void pushUndo()
{
    if (g_undoCount == 4)
    {
        memmove(&g_undo[0], &g_undo[1], sizeof(Program) * 3);
        g_undoCount = 3;
    }
    g_undo[g_undoCount++] = core.draft;
    g_dirty = true;
}

bool undo()
{
    if (!g_undoCount)
    {
        return false;
    }
    core.draft = g_undo[--g_undoCount];
    g_dirty = true;
    return true;
}

// ---------------------------------------------------------------- disposición
struct Layout
{
    uint8_t col[MAX_BLOCKS];
    int16_t y[MAX_BLOCKS];      // arriba de la etiqueta, en coordenadas del contenido
    uint8_t slots[MAX_BLOCKS][MAX_PINS]; // pines visibles (índice de entrada)
    uint8_t nslots[MAX_BLOCKS];
    uint8_t rowInCol[MAX_BLOCKS];
    uint8_t numCols = 0;
    int16_t height = 0;
} L;

bool hasParamLine(const Block &b)
{
    const TypeInfo *ti = typeInfo(b.type);
    return ti && (ti->p1Kind != PK_NONE || b.type == BT_ALARM);
}

int16_t bodyH(uint8_t b)
{
    const uint8_t n = L.nslots[b] < 2 ? 2 : L.nslots[b];
    return (int16_t)(n * PIN_DY + 6 + (hasParamLine(prog().blocks[b]) ? 10 : 0));
}

int16_t colX(uint8_t c) { return (int16_t)(CH + c * PITCH); }
int16_t bodyY(uint8_t b) { return (int16_t)(L.y[b] + LABEL_H); }
int16_t pinY(uint8_t b, uint8_t slot) { return (int16_t)(bodyY(b) + 9 + slot * PIN_DY); }
int16_t outY(uint8_t b)
{
    const uint8_t n = L.nslots[b] < 2 ? 2 : L.nslots[b];
    return (int16_t)(bodyY(b) + 9 + ((n - 1) * PIN_DY) / 2);
}
int16_t qX() { return colX(L.numCols); }
int16_t qY(uint8_t i) { return (int16_t)(4 + i * Q_PITCH); }

void computeLayout()
{
    const Program &p = prog();
    memset(&L, 0, sizeof(L));
    // Columna = 1 + columna de la fuente más a la derecha (lazos: se corta).
    for (uint8_t pass = 0; pass < p.count + 1; pass++)
    {
        bool changed = false;
        for (uint8_t b = 0; b < p.count; b++)
        {
            uint8_t c = 0;
            for (uint8_t k = 0; k < MAX_PINS; k++)
            {
                const uint8_t s = p.blocks[b].in[k];
                if (srcIsBlock(s) && srcBlock(s) < p.count && srcBlock(s) != b)
                {
                    const uint8_t sc = (uint8_t)(L.col[srcBlock(s)] + 1);
                    if (sc > c && sc < 12)
                    {
                        c = sc;
                    }
                }
            }
            if (c != L.col[b])
            {
                L.col[b] = c;
                changed = true;
            }
        }
        if (!changed)
        {
            break;
        }
    }
    // Pines visibles.
    for (uint8_t b = 0; b < p.count; b++)
    {
        const TypeInfo *ti = typeInfo(p.blocks[b].type);
        const uint8_t pins = ti ? ti->pins : 0;
        const bool gate = ti && ti->pinNames[0] == nullptr && pins == 4;
        uint8_t n = 0;
        if (gate)
        {
            for (uint8_t k = 0; k < pins; k++)
            {
                if (p.blocks[b].in[k] != SRC_OPEN)
                {
                    L.slots[b][n++] = k;
                }
            }
            // Siempre se ve al menos una entrada libre para conectar.
            for (uint8_t k = 0; k < pins && n < 2; k++)
            {
                bool used = false;
                for (uint8_t j = 0; j < n; j++)
                {
                    used = used || L.slots[b][j] == k;
                }
                if (!used)
                {
                    L.slots[b][n++] = k;
                }
            }
            // Orden por índice de entrada.
            for (uint8_t i = 1; i < n; i++)
            {
                for (uint8_t j = i; j > 0 && L.slots[b][j - 1] > L.slots[b][j]; j--)
                {
                    const uint8_t t = L.slots[b][j];
                    L.slots[b][j] = L.slots[b][j - 1];
                    L.slots[b][j - 1] = t;
                }
            }
        }
        else
        {
            for (uint8_t k = 0; k < pins; k++)
            {
                L.slots[b][n++] = k;
            }
        }
        L.nslots[b] = n;
        if (L.col[b] + 1 > L.numCols)
        {
            L.numCols = (uint8_t)(L.col[b] + 1);
        }
    }
    // Apilado por columna, en el orden de la primera fuente (menos cruces).
    int16_t colBottom[12];
    for (uint8_t c = 0; c < 12; c++)
    {
        colBottom[c] = 4;
    }
    uint8_t rows[12] = {0};
    for (uint8_t c = 0; c < L.numCols; c++)
    {
        // Selección por clave: y de la primera fuente bloque, o índice.
        uint8_t order[MAX_BLOCKS];
        int16_t keyv[MAX_BLOCKS];
        uint8_t n = 0;
        for (uint8_t b = 0; b < p.count; b++)
        {
            if (L.col[b] != c)
            {
                continue;
            }
            int16_t kv = (int16_t)(b * 2);
            for (uint8_t k = 0; k < MAX_PINS; k++)
            {
                const uint8_t s = p.blocks[b].in[k];
                if (srcIsBlock(s) && srcBlock(s) < p.count && L.col[srcBlock(s)] + 1 == c)
                {
                    kv = (int16_t)(L.y[srcBlock(s)] + 1);
                    break;
                }
            }
            uint8_t j = n++;
            while (j > 0 && keyv[j - 1] > kv)
            {
                keyv[j] = keyv[j - 1];
                order[j] = order[j - 1];
                j--;
            }
            keyv[j] = kv;
            order[j] = b;
        }
        for (uint8_t i = 0; i < n; i++)
        {
            const uint8_t b = order[i];
            L.y[b] = colBottom[c];
            L.rowInCol[b] = rows[c]++;
            colBottom[c] = (int16_t)(bodyY(b) + bodyH(b) + 8);
        }
    }
    int16_t h = qY(NUM_Q - 1) + QH + 4;
    for (uint8_t c = 0; c < L.numCols; c++)
    {
        if (colBottom[c] > h)
        {
            h = colBottom[c];
        }
    }
    L.height = h;
}

// ---------------------------------------------------------------- dibujo en el sprite
JWPLC_TFTSprite g_spr;
bool g_sprOk = false;
int16_t g_sx = 0, g_sy = 0; // desplazamiento de la vista
uint8_t g_sel = ITEM_Q;
bool g_live = false; // valores en vivo (monitor en RUN)

void sprText(const char t[], int16_t x, int16_t y, uint8_t fg, uint8_t size = 1, uint8_t bg = 0xFF)
{
    g_spr.setFont(JWPLC_FONT_GLCD);
    g_spr.setTextSize(size);
    g_spr.setTextDatum(DATUM_TOP_LEFT);
    if (bg == 0xFF)
    {
        g_spr.setTextColor(fg);
    }
    else
    {
        g_spr.setTextColor(fg, bg);
    }
    g_spr.drawString(t, x, y);
}

bool srcValue(uint8_t src)
{
    return g_live && core.engine.sourceValue(src);
}

// Etiqueta de una fuente que no llega por cable (I1, hi, B03...).
void drawTag(uint8_t src, int16_t rightX, int16_t cy, bool neg)
{
    char name[8];
    sourceName(src, name, sizeof(name));
    const int16_t w = (int16_t)(strlen(name) * 6 + 4);
    const int16_t x = (int16_t)(rightX - w - (neg ? 4 : 0) - 3);
    const bool on = srcValue(src);
    if (src == SRC_OPEN)
    {
        sprText("x", (int16_t)(rightX - 9), (int16_t)(cy - 3), P_MUTED);
        g_spr.drawFastHLine((int16_t)(rightX - 3), cy, 3, P_BORDER);
        return;
    }
    g_spr.fillRect(x, (int16_t)(cy - 5), w, 11, on ? P_ONBG : P_TAG);
    g_spr.drawRect(x, (int16_t)(cy - 5), w, 11, on ? P_ON : P_BORDER);
    sprText(name, (int16_t)(x + 2), (int16_t)(cy - 3), on ? P_ON : P_TEXT);
    g_spr.drawFastHLine((int16_t)(x + w), cy, (int16_t)(rightX - x - w), on ? P_ON : P_BORDER);
}

void drawNeg(int16_t x, int16_t cy)
{
    g_spr.fillCircle((int16_t)(x - 3), cy, 2, P_BG);
    g_spr.drawCircle((int16_t)(x - 3), cy, 2, P_TEXT);
}

void blockParamText(uint8_t b, char out[], size_t size, bool &yellow)
{
    const Program &p = prog();
    const Block &blk = p.blocks[b];
    const TypeInfo *ti = typeInfo(blk.type);
    yellow = false;
    out[0] = 0;
    if (!ti)
    {
        return;
    }
    const uint8_t base = (uint8_t)((blk.flags & BF_BASE_MASK) >> BF_BASE_SHIFT);
    if (ti->p1Kind == PK_TIME)
    {
        const uint32_t el = g_live ? core.engine.elapsedMs(b, millis()) : 0;
        if (g_live && el > 0 && blk.type != BT_GEN)
        {
            // Ta en la misma base que T (como el LOGO!).
            int32_t v;
            if (base == 0)
            {
                v = (int32_t)((el / 1000) * 100 + (el % 1000) / 10);
            }
            else if (base == 1)
            {
                v = (int32_t)((el / 60000) * 100 + (el / 1000) % 60);
            }
            else
            {
                v = (int32_t)((el / 3600000UL) * 100 + (el / 60000) % 60);
            }
            char t[10];
            formatTime(v, base, t, sizeof(t));
            snprintf(out, size, "Ta=%s", t);
            yellow = true;
        }
        else
        {
            char t[10];
            formatTime(blk.p1, base, t, sizeof(t));
            snprintf(out, size, "%s%s", ti->p1Name[1] ? "" : "T=", t);
            if (ti->p1Name[1])
            {
                snprintf(out, size, "%s", t);
            }
        }
    }
    else if (ti->p1Kind == PK_COUNT)
    {
        snprintf(out, size, "%ld/%ld", (long)(g_live ? core.engine.count(b) : 0), (long)blk.p1);
        yellow = g_live;
    }
    else if (ti->p1Kind == PK_CLOCK)
    {
        snprintf(out, size, "%02ld%02ld-%02ld%02ld", (long)(blk.p1 / 60), (long)(blk.p1 % 60),
                 (long)(blk.p2 / 60), (long)(blk.p2 % 60));
    }
    else if (blk.type == BT_ALARM)
    {
        snprintf(out, size, "%.9s", p.texts[blk.aux & 7]);
    }
}

void drawBlock(uint8_t b)
{
    const Program &p = prog();
    const Block &blk = p.blocks[b];
    const TypeInfo *ti = typeInfo(blk.type);
    const int16_t x = (int16_t)(colX(L.col[b]) - g_sx);
    const int16_t ty = (int16_t)(L.y[b] - g_sy);
    const int16_t by = (int16_t)(ty + LABEL_H);
    const int16_t h = bodyH(b);
    if (x > SCREEN_W || x + BW + CH < -CH || by > MAP_H || by + h < -LABEL_H)
    {
        return;
    }
    const bool out = g_live && core.engine.blockOut(b);

    // Número del bloque y marca "+" (parámetro visible) arriba.
    char num[8];
    snprintf(num, sizeof(num), "B%02u", b + 1);
    sprText(num, x, ty, P_MUTED);
    if (blk.flags & BF_PARAM)
    {
        sprText("+", (int16_t)(x + 22), ty, P_ACCENT);
    }
    if (blk.flags & BF_RETAIN)
    {
        sprText("Rem", (int16_t)(x + BW - 18), ty, P_WARN);
    }

    g_spr.fillRoundRect(x, by, BW, h, 3, P_SURF);
    g_spr.drawRoundRect(x, by, BW, h, 3, b == g_sel ? P_SEL : P_BORDER);
    if (b == g_sel)
    {
        g_spr.drawRoundRect((int16_t)(x + 1), (int16_t)(by + 1), BW - 2, (int16_t)(h - 2), 2, P_SEL);
    }

    // Símbolo: GLCD x2 si entra, si no x1.
    const bool named = ti && ti->pinNames[0] != nullptr;
    const int16_t sx0 = (int16_t)(x + (named ? 22 : 2));
    const int16_t sw = (int16_t)(x + BW - 2 - sx0);
    const char *sym = ti ? ti->symbol : "?";
    const int16_t len = (int16_t)strlen(sym);
    const uint8_t size = (len * 12 <= sw) ? 2 : 1;
    const int16_t tw = (int16_t)(len * 6 * size);
    const int16_t pinsH = (int16_t)((L.nslots[b] < 2 ? 2 : L.nslots[b]) * PIN_DY);
    sprText(sym, (int16_t)(sx0 + (sw - tw) / 2), (int16_t)(by + 3 + (pinsH - 8 * size) / 2), out ? P_ON : P_TEXT, size);
    if (blk.type == BT_NAND || blk.type == BT_NOR || blk.type == BT_NOT)
    {
        // Salida negada
        g_spr.drawCircle((int16_t)(x + BW + 2), outY(b) - g_sy, 2, P_TEXT);
    }

    // Pines: nombre, negación, cable o etiqueta.
    for (uint8_t i = 0; i < L.nslots[b]; i++)
    {
        const uint8_t k = L.slots[b][i];
        const int16_t py = (int16_t)(pinY(b, i) - g_sy);
        if (named && ti->pinNames[k])
        {
            sprText(ti->pinNames[k], (int16_t)(x + 3), (int16_t)(py - 3), P_MUTED);
        }
        const uint8_t s = blk.in[k];
        const bool neg = (blk.neg >> k) & 1;
        if (srcIsBlock(s) && srcBlock(s) < p.count && L.col[srcBlock(s)] + 1 == L.col[b])
        {
            // Cable desde la columna anterior, por un carril del canal.
            const uint8_t src = srcBlock(s);
            const bool on = srcValue(s);
            const uint8_t c = on ? P_ON : P_BORDER;
            const int16_t ox = (int16_t)(colX(L.col[src]) + BW + (blk.type == BT_NAND || blk.type == BT_NOR ? 0 : 0) - g_sx);
            const int16_t oy = (int16_t)(outY(src) - g_sy);
            const int16_t lane = (int16_t)(ox + 6 + (L.rowInCol[src] % 4) * 5);
            const int16_t endX = (int16_t)(x - (neg ? 5 : 0));
            g_spr.drawFastHLine(ox, oy, (int16_t)(lane - ox + 1), c);
            if (py != oy)
            {
                g_spr.drawFastVLine(lane, oy < py ? oy : py, (int16_t)((oy < py ? py - oy : oy - py) + 1), c);
            }
            g_spr.drawFastHLine(lane, py, (int16_t)(endX - lane), c);
            if (on)
            {
                g_spr.drawFastHLine(ox, (int16_t)(oy + 1), (int16_t)(lane - ox + 1), c);
                g_spr.drawFastHLine(lane, (int16_t)(py + 1), (int16_t)(endX - lane), c);
                g_spr.drawFastVLine((int16_t)(lane + 1), oy < py ? oy : py, (int16_t)((oy < py ? py - oy : oy - py) + 2), c);
            }
        }
        else
        {
            drawTag(s, x, py, neg);
        }
        if (neg)
        {
            drawNeg(x, py);
        }
    }

    // Salida: trazo corto.
    const int16_t oy = (int16_t)(outY(b) - g_sy);
    g_spr.drawFastHLine((int16_t)(x + BW), oy, 5, out ? P_ON : P_BORDER);

    // Línea de parámetro.
    if (hasParamLine(blk))
    {
        char t[16];
        bool yellow;
        blockParamText(b, t, sizeof(t), yellow);
        const int16_t ly = (int16_t)(by + pinsH + 5);
        g_spr.drawFastHLine((int16_t)(x + 3), (int16_t)(ly - 2), BW - 6, P_BORDER);
        sprText(t, (int16_t)(x + 4), ly, yellow ? P_SEL : P_TEXT);
    }
}

void drawOutputs()
{
    const Program &p = prog();
    const int16_t x = (int16_t)(qX() - g_sx);
    for (uint8_t i = 0; i < NUM_Q; i++)
    {
        const int16_t y = (int16_t)(qY(i) - g_sy);
        if (y > MAP_H || y + QH < 0 || x > SCREEN_W)
        {
            continue;
        }
        const uint8_t s = p.q[i];
        const bool connected = s != SRC_OPEN;
        const bool on = g_live && ((core.engine.outputs() >> i) & 1);
        const int16_t cy = (int16_t)(y + QH / 2);
        if (connected)
        {
            if (srcIsBlock(s) && srcBlock(s) < p.count && L.col[srcBlock(s)] + 1 == L.numCols)
            {
                const uint8_t src = srcBlock(s);
                const bool sv = srcValue(s);
                const uint8_t c = sv ? P_ON : P_BORDER;
                const int16_t ox = (int16_t)(colX(L.col[src]) + BW - g_sx);
                const int16_t oy = (int16_t)(outY(src) - g_sy);
                const int16_t lane = (int16_t)(ox + 6 + (L.rowInCol[src] % 4) * 5);
                g_spr.drawFastHLine(ox, oy, (int16_t)(lane - ox + 1), c);
                g_spr.drawFastVLine(lane, oy < cy ? oy : cy, (int16_t)((oy < cy ? cy - oy : oy - cy) + 1), c);
                g_spr.drawFastHLine(lane, cy, (int16_t)(x - lane), c);
            }
            else
            {
                drawTag(s, x, cy, (p.qNeg >> i) & 1);
            }
            if ((p.qNeg >> i) & 1)
            {
                drawNeg(x, cy);
            }
        }
        const bool sel = g_sel == ITEM_Q + i;
        g_spr.fillRect(x, y, QW, QH, on ? P_ON : (connected ? P_SURF : P_BG));
        g_spr.drawRect(x, y, QW, QH, sel ? P_SEL : (connected ? P_TEXT : P_BORDER));
        if (sel)
        {
            g_spr.drawRect((int16_t)(x + 1), (int16_t)(y + 1), QW - 2, QH - 2, P_SEL);
        }
        char t[4];
        snprintf(t, sizeof(t), "Q%u", i + 1);
        sprText(t, (int16_t)(x + 9), (int16_t)(y + 3), on ? P_BG : (connected ? P_TEXT : P_MUTED));
    }
}

void render()
{
    if (!g_sprOk)
    {
        return;
    }
    g_spr.fill(P_BG);
    // Puntos de rejilla, como en un editor FBD.
    for (int16_t yy = (int16_t)(8 - (g_sy % 16)); yy < MAP_H; yy += 16)
    {
        for (int16_t xx = (int16_t)(8 - (g_sx % 16)); xx < SCREEN_W; xx += 16)
        {
            g_spr.drawPixel(xx, yy, P_DOT);
        }
    }
    const Program &p = prog();
    for (uint8_t b = 0; b < p.count; b++)
    {
        drawBlock(b);
    }
    drawOutputs();
    if (p.count == 0)
    {
        // A la derecha de la columna Q (que queda en x = CH).
        sprText("Programa vacio", 96, 30, P_TEXT, 2);
        sprText("Elige una salida Q y pulsa OK", 96, 56, P_MUTED, 1);
        sprText("para conectarle un bloque o", 96, 68, P_MUTED, 1);
        sprText("una entrada. Se construye", 96, 80, P_MUTED, 1);
        sprText("de la salida hacia atras.", 96, 92, P_MUTED, 1);
    }
    g_spr.push(0, BODY_Y);
}

// ---------------------------------------------------------------- navegación
void itemRect(uint8_t item, int16_t &x, int16_t &y, int16_t &w, int16_t &h)
{
    if (item >= ITEM_Q)
    {
        x = qX();
        y = qY((uint8_t)(item - ITEM_Q));
        w = QW;
        h = QH;
    }
    else
    {
        x = colX(L.col[item]);
        y = L.y[item];
        w = BW;
        h = (int16_t)(LABEL_H + bodyH(item));
    }
}

int16_t itemCenterY(uint8_t item)
{
    int16_t x, y, w, h;
    itemRect(item, x, y, w, h);
    return (int16_t)(y + h / 2);
}

uint8_t itemCol(uint8_t item)
{
    return item >= ITEM_Q ? L.numCols : L.col[item];
}

bool ensureVisible()
{
    int16_t x, y, w, h;
    itemRect(g_sel, x, y, w, h);
    int16_t sx = g_sx, sy = g_sy;
    if (x - CH < sx)
    {
        sx = (int16_t)(x - CH);
    }
    if (x + w + 6 > sx + SCREEN_W)
    {
        sx = (int16_t)(x + w + 6 - SCREEN_W);
    }
    if (y - 2 < sy)
    {
        sy = (int16_t)(y - 2);
    }
    if (y + h + 2 > sy + MAP_H)
    {
        sy = (int16_t)(y + h + 2 - MAP_H);
    }
    if (sx < 0)
    {
        sx = 0;
    }
    if (sy < 0)
    {
        sy = 0;
    }
    const bool changed = sx != g_sx || sy != g_sy;
    g_sx = sx;
    g_sy = sy;
    return changed;
}

// Siguiente elemento en la misma columna (dir = +1 abajo, -1 arriba).
uint8_t moveVertical(int8_t dir)
{
    const uint8_t col = itemCol(g_sel);
    const int16_t cy = itemCenterY(g_sel);
    uint8_t best = g_sel;
    int16_t bestD = 32767;
    const Program &p = prog();
    auto consider = [&](uint8_t it) {
        if (it == g_sel || itemCol(it) != col)
        {
            return;
        }
        const int16_t d = (int16_t)((itemCenterY(it) - cy) * dir);
        if (d > 0 && d < bestD)
        {
            bestD = d;
            best = it;
        }
    };
    for (uint8_t b = 0; b < p.count; b++)
    {
        consider(b);
    }
    for (uint8_t i = 0; i < NUM_Q; i++)
    {
        consider((uint8_t)(ITEM_Q + i));
    }
    return best;
}

// Elemento más cercano en altura de la columna vecina con elementos.
uint8_t moveHorizontal(int8_t dir)
{
    const int16_t cy = itemCenterY(g_sel);
    int16_t col = (int16_t)itemCol(g_sel);
    const Program &p = prog();
    for (col += dir; col >= 0 && col <= L.numCols; col += dir)
    {
        uint8_t best = 0xFF;
        int16_t bestD = 32767;
        for (uint8_t b = 0; b < p.count; b++)
        {
            if (L.col[b] == col)
            {
                const int16_t d = (int16_t)abs(itemCenterY(b) - cy);
                if (d < bestD)
                {
                    bestD = d;
                    best = b;
                }
            }
        }
        if (col == L.numCols)
        {
            for (uint8_t i = 0; i < NUM_Q; i++)
            {
                const int16_t d = (int16_t)abs(itemCenterY((uint8_t)(ITEM_Q + i)) - cy);
                if (d < bestD)
                {
                    bestD = d;
                    best = (uint8_t)(ITEM_Q + i);
                }
            }
        }
        if (best != 0xFF)
        {
            return best;
        }
    }
    return g_sel;
}

// ---------------------------------------------------------------- pantallas
void openBlock(uint8_t b);
void openSourcePicker(uint8_t block, uint8_t pin); // block = ITEM_Q + i para una salida
void openPalette(void (*onPick)(uint8_t type));

class MapScreen : public Screen
{
public:
    uint32_t lastLive = 0;
    uint32_t sig = 0;

    void titleInfo()
    {
        char t[28] = "";
        const Program &p = prog();
        if (g_sel >= ITEM_Q)
        {
            char s[8];
            sourceName(p.q[g_sel - ITEM_Q], s, sizeof(s));
            snprintf(t, sizeof(t), "Q%u <- %s", g_sel - ITEM_Q + 1, s);
        }
        else if (g_sel < p.count)
        {
            const TypeInfo *ti = typeInfo(p.blocks[g_sel].type);
            snprintf(t, sizeof(t), "B%02u %s", g_sel + 1, ti ? ti->name : "?");
        }
        // Texto de ancho fijo para no dejar restos.
        char pad[20];
        snprintf(pad, sizeof(pad), "%-16.16s", t);
        textGlcd(pad, 112, 8, T.headerText, T.header, 1);
    }

    void enter() override
    {
        if (!g_sprOk)
        {
            g_sprOk = g_spr.create(SCREEN_W, MAP_H, 4);
            if (g_sprOk)
            {
                const uint16_t pal[13] = {0x0000, 0x18E3, 0x4A69, 0xFFFF, 0x9CF3, 0x07E0, 0xFFE0,
                                          0xFD20, 0xF800, 0x2124, 0x01A0, 0x2E8B, 0x2945};
                for (uint8_t i = 0; i < 13; i++)
                {
                    g_spr.setPaletteColor(i, pal[i]);
                }
            }
        }
        computeLayout();
        if (g_sel < ITEM_Q && g_sel >= prog().count)
        {
            g_sel = prog().count ? (uint8_t)(prog().count - 1) : ITEM_Q;
        }
        ensureVisible();
        drawStatusBar(g_monitor ? "MONITOR" : "EDITAR");
        titleInfo();
        JWPLC_TFT.drawFastHLine(0, BAR_H, SCREEN_W, T.background);
        g_live = g_monitor && core.mode == MODE_RUN;
        if (!g_sprOk)
        {
            clearBody();
            text16("Sin memoria para el mapa", 10, 60, T.error, T.background);
        }
        render();
        drawFooter(g_monitor ? "^v<> mover  OK detalle  ESC salir"
                             : "^v<> mover  OK editar  ESC guardar/salir");
    }

    void update() override
    {
        uint8_t ns = g_sel;
        if (key(BTN_UP))
        {
            ns = moveVertical(-1);
        }
        else if (key(BTN_DOWN))
        {
            ns = moveVertical(1);
        }
        else if (key(BTN_LEFT))
        {
            ns = moveHorizontal(-1);
        }
        else if (key(BTN_RIGHT))
        {
            ns = moveHorizontal(1);
        }
        else if (key(BTN_OK))
        {
            if (g_sel >= ITEM_Q)
            {
                if (!g_monitor)
                {
                    openSourcePicker(g_sel, 0);
                }
            }
            else
            {
                openBlock(g_sel);
            }
            return;
        }
        else if (key(BTN_ESC))
        {
            exitEditor();
            return;
        }
        if (ns != g_sel)
        {
            g_sel = ns;
            ensureVisible();
            titleInfo();
            render();
        }
        // En vivo: redibujo si cambió algo (como mucho cada 150 ms).
        const bool live = g_monitor && core.mode == MODE_RUN;
        if (live != g_live)
        {
            g_live = live;
            render();
        }
        if (g_live && millis() - lastLive >= 150)
        {
            lastLive = millis();
            uint32_t s = core.engine.outputs() | ((uint32_t)core.engine.inputs() << 8) |
                         ((uint32_t)core.engine.markers() << 16);
            const Program &p = prog();
            for (uint8_t b = 0; b < p.count; b++)
            {
                s = s * 31 + core.engine.blockOut(b);
                const TypeInfo *ti = typeInfo(p.blocks[b].type);
                if (ti && (ti->p1Kind == PK_TIME || ti->p1Kind == PK_COUNT))
                {
                    s = s * 31 + (uint32_t)(core.engine.elapsedMs(b, millis()) / 10) + (uint32_t)core.engine.count(b);
                }
            }
            if (s != sig)
            {
                sig = s;
                render();
            }
        }
    }

    void exitEditor();
};
MapScreen g_map;

// ---- salida del editor: guardar, deshacer o descartar
const char *const EXIT_ROWS[] = {"Guardar en FRAM y salir", "Seguir editando", "Deshacer ultimo cambio",
                                 "Salir sin guardar"};
uint16_t exitCount() { return 4; }
void exitRow(uint16_t i, JWPLC_UIRow r)
{
    r.left(EXIT_ROWS[i]);
    if (i == 2)
    {
        char t[12];
        snprintf(t, sizeof(t), "%u", g_undoCount);
        r.right(t);
    }
}
void leaveEditor()
{
    editing = false;
    if (g_sprOk)
    {
        g_spr.destroy();
        g_sprOk = false;
    }
}
void exitOk(uint16_t i)
{
    switch (i)
    {
    case 0:
        runCommand(CMD_APPLY_DRAFT, [](bool ok) {
            if (ok)
            {
                g_dirty = false;
                popTo(&g_map);
                pop();
                leaveEditor();
            }
        });
        break;
    case 1:
        pop();
        break;
    case 2:
        if (undo())
        {
            pop();
            footerMessage("Cambio deshecho", T.ok, 1500);
        }
        else
        {
            footerMessage("Nada para deshacer", T.warn, 1500);
        }
        break;
    default:
        core.draft = core.engine.program();
        g_dirty = false;
        popTo(&g_map);
        pop();
        leaveEditor();
        break;
    }
}

void MapScreen::exitEditor()
{
    if (g_monitor || !g_dirty)
    {
        pop();
        leaveEditor();
        return;
    }
    ListSpec s = {"SALIR DEL EDITOR", "OK elegir  ESC seguir editando", exitCount, exitRow, exitOk, nullptr, nullptr, 0};
    pushList(s);
}

// ---------------------------------------------------------------- detalle del bloque
enum RowKind : uint8_t
{
    R_PIN = 0,
    R_P1,
    R_P2,
    R_BASE,
    R_DAY,
    R_TEXT,
    R_SEV,
    R_PARAMFLAG,
    R_RETAIN,
    R_NEW_AFTER,
    R_HELP,
    R_DELETE,
};
uint8_t g_blk = 0;
uint8_t g_rowKind[24];
uint8_t g_rowArg[24];
uint8_t g_rowCount = 0;

void buildBlockRows()
{
    const Block &b = prog().blocks[g_blk];
    const TypeInfo *ti = typeInfo(b.type);
    g_rowCount = 0;
    auto add = [](uint8_t k, uint8_t a) {
        if (g_rowCount < 24)
        {
            g_rowKind[g_rowCount] = k;
            g_rowArg[g_rowCount++] = a;
        }
    };
    if (!ti)
    {
        return;
    }
    for (uint8_t k = 0; k < ti->pins; k++)
    {
        add(R_PIN, k);
    }
    if (ti->p1Kind != PK_NONE)
    {
        add(R_P1, 0);
    }
    if (ti->p2Kind != PK_NONE)
    {
        add(R_P2, 0);
    }
    if (ti->p1Kind == PK_TIME)
    {
        add(R_BASE, 0);
    }
    if (b.type == BT_WEEK)
    {
        for (uint8_t d = 0; d < 7; d++)
        {
            add(R_DAY, d);
        }
    }
    if (b.type == BT_ALARM)
    {
        add(R_TEXT, 0);
        add(R_SEV, 0);
    }
    if (ti->p1Kind != PK_NONE)
    {
        add(R_PARAMFLAG, 0);
    }
    if (ti->canRetain)
    {
        add(R_RETAIN, 0);
    }
    if (!g_monitor)
    {
        add(R_NEW_AFTER, 0);
    }
    add(R_HELP, 0);
    if (!g_monitor)
    {
        add(R_DELETE, 0);
    }
}

uint16_t blockRowCount() { return g_rowCount; }

void blockRow(uint16_t i, JWPLC_UIRow r)
{
    const Program &p = prog();
    const Block &b = p.blocks[g_blk];
    const TypeInfo *ti = typeInfo(b.type);
    char l[28], t[28];
    l[0] = t[0] = 0;
    const uint8_t base = (uint8_t)((b.flags & BF_BASE_MASK) >> BF_BASE_SHIFT);
    switch (g_rowKind[i])
    {
    case R_PIN:
    {
        const uint8_t k = g_rowArg[i];
        if (ti->pinNames[k])
        {
            snprintf(l, sizeof(l), "Entrada %s", ti->pinNames[k]);
        }
        else
        {
            snprintf(l, sizeof(l), "Entrada %u", k + 1);
        }
        char s[8];
        sourceName(b.in[k], s, sizeof(s));
        const bool neg = (b.neg >> k) & 1;
        if (srcIsBlock(b.in[k]) && srcBlock(b.in[k]) < p.count)
        {
            const TypeInfo *si = typeInfo(p.blocks[srcBlock(b.in[k])].type);
            snprintf(t, sizeof(t), "%s%s %s", neg ? "neg " : "", s, si ? si->name : "");
        }
        else
        {
            snprintf(t, sizeof(t), "%s%s", neg ? "neg " : "", s);
        }
        break;
    }
    case R_P1:
    case R_P2:
    {
        const bool one = g_rowKind[i] == R_P1;
        const uint8_t kind = one ? ti->p1Kind : ti->p2Kind;
        const int32_t v = one ? b.p1 : b.p2;
        snprintf(l, sizeof(l), "Parametro %s", one ? ti->p1Name : ti->p2Name);
        if (kind == PK_TIME)
        {
            formatTime(v, base, t, sizeof(t));
        }
        else if (kind == PK_CLOCK)
        {
            formatClock(v, t, sizeof(t));
        }
        else
        {
            snprintf(t, sizeof(t), "%ld", (long)v);
        }
        break;
    }
    case R_BASE:
        snprintf(l, sizeof(l), "Base de tiempo");
        snprintf(t, sizeof(t), "< %s >", base == 0 ? "s (SS:cc)" : (base == 1 ? "m (MM:SS)" : "h (HH:MM)"));
        break;
    case R_DAY:
    {
        static const char *const DAYS[7] = {"Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado", "Domingo"};
        snprintf(l, sizeof(l), "%s", DAYS[g_rowArg[i]]);
        snprintf(t, sizeof(t), "%s", ((b.aux >> g_rowArg[i]) & 1) ? "[x] activo" : "[ ]");
        break;
    }
    case R_TEXT:
        snprintf(l, sizeof(l), "Texto");
        snprintf(t, sizeof(t), "%s", p.texts[b.aux & 7]);
        break;
    case R_SEV:
    {
        static const char *const SEV[3] = {"info", "aviso", "critico"};
        snprintf(l, sizeof(l), "Severidad");
        snprintf(t, sizeof(t), "< %s >", SEV[((b.aux >> 4) & 3) % 3]);
        break;
    }
    case R_PARAMFLAG:
        snprintf(l, sizeof(l), "En menu Parametros (+)");
        snprintf(t, sizeof(t), "%s", (b.flags & BF_PARAM) ? "Si" : "No");
        break;
    case R_RETAIN:
        snprintf(l, sizeof(l), "Retentivo (FRAM)");
        snprintf(t, sizeof(t), "%s", (b.flags & BF_RETAIN) ? "Si" : "No");
        break;
    case R_NEW_AFTER:
        snprintf(l, sizeof(l), "+ Bloque a su salida");
        snprintf(t, sizeof(t), ">");
        break;
    case R_HELP:
        snprintf(l, sizeof(l), "Ayuda: %s", ti->desc);
        snprintf(t, sizeof(t), ">");
        break;
    default:
        snprintf(l, sizeof(l), "Borrar bloque");
        snprintf(t, sizeof(t), "!");
        break;
    }
    r.left(l);
    r.right(t);
}

Block &draftBlock() { return core.draft.blocks[g_blk]; }

void paramDone(int32_t v);
uint8_t g_editWhich = 1;

void blockSide(uint16_t i, int8_t dir)
{
    if (g_monitor)
    {
        return;
    }
    Block &b = draftBlock();
    switch (g_rowKind[i])
    {
    case R_PIN:
    {
        pushUndo();
        b.neg ^= (uint8_t)(1 << g_rowArg[i]);
        break;
    }
    case R_BASE:
    {
        pushUndo();
        uint8_t base = (uint8_t)((b.flags & BF_BASE_MASK) >> BF_BASE_SHIFT);
        base = (uint8_t)((base + 3 + dir) % 3);
        b.flags = (uint8_t)((b.flags & ~BF_BASE_MASK) | (base << BF_BASE_SHIFT));
        break;
    }
    case R_SEV:
    {
        pushUndo();
        uint8_t sev = (uint8_t)(((b.aux >> 4) & 3) % 3);
        sev = (uint8_t)((sev + 3 + dir) % 3);
        b.aux = (uint8_t)((b.aux & 0x0F) | (sev << 4));
        break;
    }
    case R_DAY:
    case R_PARAMFLAG:
    case R_RETAIN:
        break;
    default:
        return;
    }
    refreshList();
}

uint8_t g_newAfterFrom = 0;

void newAfterPicked(uint8_t type)
{
    pushUndo();
    const uint8_t nb = addBlock(core.draft, type);
    if (nb == 0xFF)
    {
        undo();
        footerMessage("Programa lleno (64 bloques)", T.error, 2500);
        return;
    }
    core.draft.blocks[nb].in[0] = (uint8_t)(SRC_B + g_newAfterFrom);
    popTo(&g_map);
    g_sel = nb;
    computeLayout();
    openBlock(nb);
}

void textDone(const char t[])
{
    pushUndo();
    Block &b = draftBlock();
    // Cada AVISO usa su propio texto: busca uno libre si todavía no tiene.
    uint8_t idx = b.aux & 7;
    bool shared = false;
    for (uint8_t o = 0; o < core.draft.count; o++)
    {
        if (o != g_blk && core.draft.blocks[o].type == BT_ALARM && (core.draft.blocks[o].aux & 7) == idx)
        {
            shared = true;
        }
    }
    if (shared)
    {
        for (uint8_t cand = 0; cand < NUM_TEXTS; cand++)
        {
            bool used = false;
            for (uint8_t o = 0; o < core.draft.count; o++)
            {
                used = used || (core.draft.blocks[o].type == BT_ALARM && (core.draft.blocks[o].aux & 7) == cand);
            }
            if (!used)
            {
                idx = cand;
                break;
            }
        }
        b.aux = (uint8_t)((b.aux & 0xF8) | idx);
    }
    strncpy(core.draft.texts[idx], t, TEXT_LEN - 1);
    core.draft.texts[idx][TEXT_LEN - 1] = 0;
    refreshList();
}

void showHelp();

void blockOk(uint16_t i)
{
    if (g_monitor && g_rowKind[i] != R_HELP)
    {
        return;
    }
    Block &b = draftBlock();
    const TypeInfo *ti = typeInfo(b.type);
    switch (g_rowKind[i])
    {
    case R_PIN:
        openSourcePicker(g_blk, g_rowArg[i]);
        break;
    case R_P1:
    case R_P2:
    {
        g_editWhich = g_rowKind[i] == R_P1 ? 1 : 2;
        const uint8_t kind = g_editWhich == 1 ? ti->p1Kind : ti->p2Kind;
        const int32_t v = g_editWhich == 1 ? b.p1 : b.p2;
        static char title[28];
        snprintf(title, sizeof(title), "B%02u %s %s", g_blk + 1, ti->name, g_editWhich == 1 ? ti->p1Name : ti->p2Name);
        if (kind == PK_TIME)
        {
            static const char *const U[3] = {"s", "m", "h"};
            editValue(title, v, 4, 2, U[(b.flags & BF_BASE_MASK) >> BF_BASE_SHIFT], 0, 9999, paramDone);
        }
        else if (kind == PK_CLOCK)
        {
            editValue(title, (v / 60) * 100 + v % 60, 4, 2, "h", 0, 2359, paramDone);
        }
        else
        {
            editValue(title, v, 6, 0, nullptr, 0, 999999, paramDone);
        }
        break;
    }
    case R_BASE:
    case R_SEV:
        blockSide(i, 1);
        break;
    case R_DAY:
        pushUndo();
        b.aux ^= (uint8_t)(1 << g_rowArg[i]);
        refreshList();
        break;
    case R_TEXT:
        editText("Texto del aviso", core.draft.texts[b.aux & 7], TEXT_LEN - 1, textDone);
        break;
    case R_PARAMFLAG:
        pushUndo();
        b.flags ^= BF_PARAM;
        refreshList();
        break;
    case R_RETAIN:
        pushUndo();
        b.flags ^= BF_RETAIN;
        refreshList();
        break;
    case R_NEW_AFTER:
        g_newAfterFrom = g_blk;
        openPalette(newAfterPicked);
        break;
    case R_HELP:
        showHelp();
        break;
    default:
    {
        static char msg[40];
        snprintf(msg, sizeof(msg), "B%02u se borra; sus enlaces quedan en x.", g_blk + 1);
        confirm("Borrar bloque", msg, []() {
            pushUndo();
            deleteBlock(core.draft, g_blk);
            popTo(&g_map);
            g_sel = core.draft.count ? (uint8_t)(g_blk ? g_blk - 1 : 0) : ITEM_Q;
            footerMessage("Bloque borrado (ESC > Deshacer)", T.ok, 2000);
        });
        break;
    }
    }
}

void paramDone(int32_t v)
{
    Block &b = draftBlock();
    const TypeInfo *ti = typeInfo(b.type);
    const uint8_t kind = g_editWhich == 1 ? ti->p1Kind : ti->p2Kind;
    if (kind == PK_CLOCK)
    {
        v = (v / 100) * 60 + (v % 100 > 59 ? 59 : v % 100);
        if (v > 1439)
        {
            v = 1439;
        }
    }
    else if (kind == PK_TIME && ((b.flags & BF_BASE_MASK) >> BF_BASE_SHIFT) != 0 && v % 100 > 59)
    {
        v = (v / 100) * 100 + 59;
    }
    pushUndo();
    if (g_editWhich == 1)
    {
        b.p1 = v;
    }
    else
    {
        b.p2 = v;
    }
    refreshList();
}

void openBlock(uint8_t b)
{
    g_blk = b;
    buildBlockRows();
    static char title[20];
    const TypeInfo *ti = typeInfo(prog().blocks[b].type);
    snprintf(title, sizeof(title), "B%02u %s", b + 1, ti ? ti->name : "?");
    ListSpec s = {title, g_monitor ? "^v elegir  ESC volver" : "OK cambiar  <> negar/opcion  ESC volver",
                  blockRowCount, blockRow, blockOk, blockSide, nullptr, g_monitor ? (uint16_t)500 : (uint16_t)0};
    pushList(s);
}

// ---------------------------------------------------------------- elegir fuente
uint8_t g_pickTarget = 0; // bloque o ITEM_Q + i
uint8_t g_pickPin = 0;
uint8_t g_pickSrc[3 + NUM_I + NUM_M + NUM_Q + MAX_BLOCKS + 1];
uint8_t g_pickCount = 0;
const uint8_t PICK_NEW = 0xFE;

void buildPick()
{
    g_pickCount = 0;
    g_pickSrc[g_pickCount++] = PICK_NEW;
    g_pickSrc[g_pickCount++] = SRC_OPEN;
    for (uint8_t i = 0; i < NUM_I; i++)
    {
        g_pickSrc[g_pickCount++] = (uint8_t)(SRC_I + i);
    }
    for (uint8_t b = 0; b < core.draft.count; b++)
    {
        if (g_pickTarget >= ITEM_Q || b != g_pickTarget)
        {
            g_pickSrc[g_pickCount++] = (uint8_t)(SRC_B + b);
        }
    }
    for (uint8_t i = 0; i < NUM_M; i++)
    {
        g_pickSrc[g_pickCount++] = (uint8_t)(SRC_M + i);
    }
    for (uint8_t i = 0; i < NUM_Q; i++)
    {
        g_pickSrc[g_pickCount++] = (uint8_t)(SRC_Q + i);
    }
    g_pickSrc[g_pickCount++] = SRC_HI;
    g_pickSrc[g_pickCount++] = SRC_LO;
}

uint16_t pickCount() { return g_pickCount; }

void pickRow(uint16_t i, JWPLC_UIRow r)
{
    const uint8_t s = g_pickSrc[i];
    if (s == PICK_NEW)
    {
        r.left("+ Nuevo bloque...");
        r.right("paleta >");
        return;
    }
    char n[8];
    sourceName(s, n, sizeof(n));
    char l[24];
    const char *desc = "";
    if (s == SRC_OPEN)
    {
        desc = "sin conectar";
    }
    else if (s == SRC_HI)
    {
        desc = "constante 1";
    }
    else if (s == SRC_LO)
    {
        desc = "constante 0";
    }
    else if (s >= SRC_I && s < SRC_I + NUM_I)
    {
        desc = "entrada digital";
    }
    else if (s >= SRC_M && s < SRC_M + NUM_M)
    {
        desc = "marca";
    }
    else if (s >= SRC_Q && s < SRC_Q + NUM_Q)
    {
        desc = "salida (scan anterior)";
    }
    else if (srcIsBlock(s))
    {
        const TypeInfo *ti = typeInfo(core.draft.blocks[srcBlock(s)].type);
        desc = ti ? ti->desc : "";
    }
    snprintf(l, sizeof(l), "%s", n);
    r.left(l);
    r.right(desc);
}

void assignSource(uint8_t s)
{
    pushUndo();
    if (g_pickTarget >= ITEM_Q)
    {
        core.draft.q[g_pickTarget - ITEM_Q] = s;
    }
    else
    {
        core.draft.blocks[g_pickTarget].in[g_pickPin] = s;
    }
}

void newForPin(uint8_t type)
{
    pushUndo();
    const uint8_t nb = addBlock(core.draft, type);
    if (nb == 0xFF)
    {
        undo();
        footerMessage("Programa lleno (64 bloques)", T.error, 2500);
        return;
    }
    if (g_pickTarget >= ITEM_Q)
    {
        core.draft.q[g_pickTarget - ITEM_Q] = (uint8_t)(SRC_B + nb);
    }
    else
    {
        core.draft.blocks[g_pickTarget].in[g_pickPin] = (uint8_t)(SRC_B + nb);
    }
    // Construcción de la salida hacia la entrada, como en el LOGO!: se abre
    // el bloque nuevo para conectar sus entradas.
    popTo(&g_map);
    g_sel = nb;
    computeLayout();
    openBlock(nb);
}

void pickOk(uint16_t i)
{
    const uint8_t s = g_pickSrc[i];
    if (s == PICK_NEW)
    {
        openPalette(newForPin);
        return;
    }
    assignSource(s);
    pop();
    refreshList();
}

void openSourcePicker(uint8_t target, uint8_t pin)
{
    g_pickTarget = target;
    g_pickPin = pin;
    buildPick();
    uint8_t cur = SRC_OPEN;
    if (target >= ITEM_Q)
    {
        cur = core.draft.q[target - ITEM_Q];
    }
    else
    {
        cur = core.draft.blocks[target].in[pin];
    }
    uint16_t sel = 0;
    for (uint8_t i = 0; i < g_pickCount; i++)
    {
        if (g_pickSrc[i] == cur && cur != SRC_OPEN)
        {
            sel = i;
        }
    }
    static char title[24];
    if (target >= ITEM_Q)
    {
        snprintf(title, sizeof(title), "Q%u <-", target - ITEM_Q + 1);
    }
    else
    {
        const TypeInfo *ti = typeInfo(core.draft.blocks[target].type);
        if (ti && ti->pinNames[pin])
        {
            snprintf(title, sizeof(title), "B%02u %s <-", target + 1, ti->pinNames[pin]);
        }
        else
        {
            snprintf(title, sizeof(title), "B%02u ent.%u <-", target + 1, pin + 1);
        }
    }
    ListSpec s = {title, "OK conectar  ESC volver", pickCount, pickRow, pickOk, nullptr, nullptr, 0};
    pushList(s, sel);
}

// ---------------------------------------------------------------- paleta
void drawWave(const char *wave, int16_t x, int16_t y, uint16_t c)
{
    if (!wave)
    {
        return;
    }
    const int16_t step = 7;
    int16_t px = x;
    bool prev = wave[0] == '-';
    for (uint8_t i = 0; wave[i]; i++)
    {
        const bool v = wave[i] == '-';
        if (v != prev)
        {
            JWPLC_TFT.drawFastVLine(px, y, 10, c);
        }
        JWPLC_TFT.drawFastHLine(px, (int16_t)(v ? y : y + 9), step + 1, c);
        prev = v;
        px = (int16_t)(px + step);
    }
}

class PaletteScreen : public Screen
{
public:
    uint8_t cat = CAT_LOGIC;
    uint8_t sel = 0;
    uint8_t types[12];
    uint8_t n = 0;
    uint8_t top = 0;
    void (*onPick)(uint8_t) = nullptr;
    static const int16_t CAT_W = 76;
    static const int16_t LIST_X = 80;
    static const int16_t LIST_W = 92;
    static const int16_t ROW_H = 26;
    static const int16_t HELP_X = 176;

    void load()
    {
        n = typesInCategory(cat, types, 12);
        if (sel >= n)
        {
            sel = n ? (uint8_t)(n - 1) : 0;
        }
        top = sel >= 5 ? (uint8_t)(sel - 4) : 0;
    }

    void drawCats()
    {
        for (uint8_t c = 0; c < CAT_COUNT_ALL; c++)
        {
            const int16_t y = (int16_t)(BODY_Y + 2 + c * 22);
            const bool s = c == cat;
            JWPLC_TFT.fillRoundRect(2, y, CAT_W - 4, 20, 3, s ? C_TILE_SEL : T.surface);
            JWPLC_TFT.drawRoundRect(2, y, CAT_W - 4, 20, 3, s ? C_ON : T.border);
            JWPLC_TFT.setFont(JWPLC_FONT_GLCD);
            JWPLC_TFT.setTextSize(1);
            JWPLC_TFT.setTextDatum(DATUM_MIDDLE_CENTER);
            JWPLC_TFT.setTextColor(s ? C_ON : (c == CAT_ANALOG ? T.textMuted : T.text), s ? C_TILE_SEL : T.surface);
            JWPLC_TFT.drawString(categoryName(c), (int16_t)(2 + (CAT_W - 4) / 2), (int16_t)(y + 10));
            JWPLC_TFT.setTextDatum(DATUM_TOP_LEFT);
        }
    }

    void drawList()
    {
        JWPLC_TFT.fillRect(LIST_X, BODY_Y, LIST_W, BODY_H, T.background);
        if (n == 0)
        {
            text16("Sin", LIST_X + 4, BODY_Y + 8, T.textMuted, T.background);
            text16("entradas", LIST_X + 4, BODY_Y + 26, T.textMuted, T.background);
            text16("analogicas", LIST_X + 4, BODY_Y + 44, T.textMuted, T.background);
            text16("en JWPLC", LIST_X + 4, BODY_Y + 62, T.textMuted, T.background);
            text16("Basic", LIST_X + 4, BODY_Y + 80, T.textMuted, T.background);
            return;
        }
        for (uint8_t i = top; i < n && i < top + 5; i++)
        {
            const int16_t y = (int16_t)(BODY_Y + 2 + (i - top) * ROW_H);
            const bool s = i == sel;
            const TypeInfo *ti = typeInfo(types[i]);
            const uint16_t bg = s ? C_TILE_SEL : T.surface;
            JWPLC_TFT.fillRoundRect(LIST_X, y, LIST_W, ROW_H - 2, 3, bg);
            JWPLC_TFT.drawRoundRect(LIST_X, y, LIST_W, ROW_H - 2, 3, s ? C_ON : T.border);
            textGlcd(ti->name, LIST_X + 4, y + 3, s ? C_ON : T.text, bg, 1);
            char d[16];
            snprintf(d, sizeof(d), "%.14s", ti->desc);
            textGlcd(d, LIST_X + 4, y + 13, T.textMuted, bg, 1);
        }
    }

    void drawHelp()
    {
        const int16_t x = HELP_X, y = BODY_Y + 2, w = SCREEN_W - HELP_X - 2, h = BODY_H - 4;
        JWPLC_TFT.fillRoundRect(x, y, w, h, 4, T.surface);
        JWPLC_TFT.drawRoundRect(x, y, w, h, 4, T.border);
        if (n == 0)
        {
            return;
        }
        const TypeInfo *ti = typeInfo(types[sel]);
        text16(ti->name, x + 6, y + 3, C_ON, T.surface);
        textGlcd(ti->symbol, x + w - 6 - (int16_t)strlen(ti->symbol) * 12, y + 4, T.text, T.surface, 2);
        textGlcd(ti->help1, x + 6, y + 22, T.text, T.surface, 1);
        textGlcd(ti->help2, x + 6, y + 32, T.text, T.surface, 1);
        int16_t wy = y + 46;
        if (ti->waveIn)
        {
            textGlcd(ti->waveInName, x + 6, wy + 1, T.textMuted, T.surface, 1);
            drawWave(ti->waveIn, x + 26, wy, C_ON);
            wy += 16;
        }
        if (ti->waveIn2)
        {
            textGlcd(ti->waveIn2Name, x + 6, wy + 1, T.textMuted, T.surface, 1);
            drawWave(ti->waveIn2, x + 26, wy, C_ON);
            wy += 16;
        }
        textGlcd("Q", x + 6, wy + 1, T.textMuted, T.surface, 1);
        drawWave(ti->waveQ, x + 26, wy, C_STOP);
        // Entradas y parámetros.
        char t[28] = "Entradas: ";
        if (ti->pins == 0)
        {
            strcat(t, "ninguna");
        }
        else if (ti->pinNames[0])
        {
            for (uint8_t k = 0; k < ti->pins; k++)
            {
                strcat(t, ti->pinNames[k]);
                if (k + 1 < ti->pins)
                {
                    strcat(t, ",");
                }
            }
        }
        else
        {
            snprintf(t + strlen(t), sizeof(t) - strlen(t), "%u", ti->pins);
        }
        textGlcd(t, x + 6, y + h - 22, T.textMuted, T.surface, 1);
        char p[28];
        snprintf(p, sizeof(p), "Param: %s%s%s%s", ti->p1Name ? ti->p1Name : "-", ti->p2Name ? ", " : "",
                 ti->p2Name ? ti->p2Name : "", ti->canRetain ? "  Rem" : "");
        textGlcd(p, x + 6, y + h - 12, T.textMuted, T.surface, 1);
    }

    void enter() override
    {
        load();
        clearBody();
        drawStatusBar("NUEVO BLOQUE");
        drawCats();
        drawList();
        drawHelp();
        drawFooter("OK insertar  ^v elegir  <> categoria");
    }

    void update() override
    {
        int8_t d = 0;
        if (key(BTN_RIGHT))
        {
            d = 1;
        }
        else if (key(BTN_LEFT))
        {
            d = -1;
        }
        if (d)
        {
            cat = (uint8_t)((cat + CAT_COUNT_ALL + d) % CAT_COUNT_ALL);
            sel = 0;
            load();
            drawCats();
            drawList();
            drawHelp();
        }
        else if (key(BTN_DOWN) && n)
        {
            sel = (uint8_t)((sel + 1) % n);
            load();
            drawList();
            drawHelp();
        }
        else if (key(BTN_UP) && n)
        {
            sel = (uint8_t)((sel + n - 1) % n);
            load();
            drawList();
            drawHelp();
        }
        else if (key(BTN_OK) && n)
        {
            if (onPick)
            {
                onPick(types[sel]);
            }
        }
        else if (key(BTN_ESC))
        {
            pop();
        }
    }
};
PaletteScreen g_palette;

void openPalette(void (*onPick)(uint8_t))
{
    g_palette.onPick = onPick;
    push(&g_palette);
}

// Ayuda desde el detalle: la misma paleta, en el tipo del bloque.
void showHelp()
{
    const uint8_t type = prog().blocks[g_blk].type;
    const TypeInfo *ti = typeInfo(type);
    if (!ti)
    {
        return;
    }
    g_palette.cat = ti->category;
    uint8_t list[12];
    const uint8_t n = typesInCategory(ti->category, list, 12);
    for (uint8_t i = 0; i < n; i++)
    {
        if (list[i] == type)
        {
            g_palette.sel = i;
        }
    }
    g_palette.onPick = nullptr;
    push(&g_palette);
}

} // namespace

void openEditor(bool monitor)
{
    g_monitor = monitor;
    if (!monitor)
    {
        core.draft = core.engine.program();
        g_undoCount = 0;
        g_dirty = false;
        editing = true;
    }
    g_sx = 0;
    g_sy = 0;
    g_sel = prog().count ? 0 : ITEM_Q;
    push(&g_map);
}

} // namespace rtui
