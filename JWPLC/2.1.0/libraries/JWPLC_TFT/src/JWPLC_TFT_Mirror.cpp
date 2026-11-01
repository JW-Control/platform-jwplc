// JWPLC_TFT_Mirror.cpp: decodificador ST7789 y búfer con paleta (ver el .h).
#include "JWPLC_TFT_Mirror.h"

#include <stdlib.h>
#include <string.h>

#include "tft_setup.h"

volatile bool jwplcMirrorOn = false;
volatile uint8_t jwplcMirrorDC = 1;

namespace
{
const uint32_t BUF_BYTES = (uint32_t)TFT_WIDTH * (uint32_t)TFT_HEIGHT;
const uint16_t MAX_SIDE = (TFT_WIDTH > TFT_HEIGHT) ? TFT_WIDTH : TFT_HEIGHT;
const uint16_t HASH_SLOTS = 512;

uint8_t *g_buf = nullptr;
volatile uint32_t g_rowVer[MAX_SIDE];
volatile uint32_t g_ver = 1;

// Geometría lógica actual (setRotation) y desplazamiento CGRAM.
int32_t g_w = TFT_WIDTH;
int32_t g_h = TFT_HEIGHT;
int32_t g_col = 0;
int32_t g_row = 0;

// Paleta: solo crece. g_hash guarda índice + 1 (0 = libre).
uint16_t g_pal[256];
volatile uint16_t g_palN = 0;
uint16_t g_hashColor[HASH_SLOTS];
uint16_t g_hash[HASH_SLOTS];
uint16_t g_lastColor = 0;
uint8_t g_lastIndex = 0;
bool g_lastValid = false;

// Estado del decodificador (lo que el ST7789 recuerda).
uint8_t s_cmd = 0;
uint8_t s_np = 0;
uint8_t s_par[4];
int32_t s_xs = 0, s_xe = 0, s_ys = 0, s_ye = 0;
int32_t s_x = 0, s_y = 0;
bool s_ram = false;
bool s_half = false;
uint8_t s_hi = 0;

uint8_t nearestIndex(uint16_t c)
{
    const int r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
    uint32_t best = 0xFFFFFFFFu;
    uint8_t bi = 0;
    for (uint16_t i = 0; i < g_palN; i++)
    {
        const uint16_t p = g_pal[i];
        const int dr = r - ((p >> 11) & 0x1F), dg = g - ((p >> 5) & 0x3F), db = b - (p & 0x1F);
        const uint32_t d = (uint32_t)(dr * dr * 4 + dg * dg + db * db * 4);
        if (d < best)
        {
            best = d;
            bi = (uint8_t)i;
        }
    }
    return bi;
}

uint8_t indexOf(uint16_t c)
{
    if (g_lastValid && c == g_lastColor)
    {
        return g_lastIndex;
    }
    uint16_t slot = (uint16_t)(((uint32_t)c * 40503u) >> 7) & (HASH_SLOTS - 1);
    uint8_t idx = 0;
    for (;;)
    {
        if (g_hash[slot] == 0)
        {
            if (g_palN < 256)
            {
                idx = (uint8_t)g_palN;
                g_pal[idx] = c;
                g_hashColor[slot] = c;
                g_hash[slot] = (uint16_t)(idx + 1);
                g_palN = (uint16_t)(g_palN + 1); // después de escribir el color
            }
            else
            {
                idx = nearestIndex(c);
            }
            break;
        }
        if (g_hashColor[slot] == c)
        {
            idx = (uint8_t)(g_hash[slot] - 1);
            break;
        }
        slot = (uint16_t)((slot + 1) & (HASH_SLOTS - 1));
    }
    g_lastColor = c;
    g_lastIndex = idx;
    g_lastValid = true;
    return idx;
}

// Escribe n píxeles del mismo índice desde el cursor, con el salto de fila de la ventana.
void putRun(uint8_t idx, uint32_t n)
{
    const uint32_t ver = g_ver;
    while (n)
    {
        if (s_y > s_ye)
        {
            return; // el ST7789 descarta lo que sobra de la ventana
        }
        uint32_t span = (uint32_t)(s_xe - s_x + 1);
        if (span > n)
        {
            span = n;
        }
        const int32_t ly = s_y - g_row;
        if (ly >= 0 && ly < g_h)
        {
            int32_t lx0 = s_x - g_col;
            int32_t lx1 = lx0 + (int32_t)span; // exclusivo
            if (lx0 < 0)
            {
                lx0 = 0;
            }
            if (lx1 > g_w)
            {
                lx1 = g_w;
            }
            if (lx1 > lx0)
            {
                memset(g_buf + (uint32_t)ly * (uint32_t)g_w + (uint32_t)lx0, idx, (size_t)(lx1 - lx0));
                g_rowVer[ly] = ver;
            }
        }
        s_x += (int32_t)span;
        n -= span;
        if (s_x > s_xe)
        {
            s_x = s_xs;
            s_y++;
        }
    }
}

inline void putPixel(uint16_t c)
{
    putRun(indexOf(c), 1);
}

void feedByte(uint8_t b)
{
    if (jwplcMirrorDC == 0)
    {
        s_cmd = b;
        s_np = 0;
        s_half = false;
        s_ram = (b == 0x2C);
        if (s_ram)
        {
            s_x = s_xs;
            s_y = s_ys;
        }
        return;
    }
    if (s_ram)
    {
        if (!s_half)
        {
            s_hi = b;
            s_half = true;
        }
        else
        {
            s_half = false;
            putPixel((uint16_t)((uint16_t)s_hi << 8 | b));
        }
        return;
    }
    if ((s_cmd == 0x2A || s_cmd == 0x2B) && s_np < 4)
    {
        s_par[s_np++] = b;
        if (s_np == 4)
        {
            const int32_t a = (int32_t)((uint16_t)s_par[0] << 8 | s_par[1]);
            const int32_t e = (int32_t)((uint16_t)s_par[2] << 8 | s_par[3]);
            if (s_cmd == 0x2A)
            {
                s_xs = a;
                s_xe = e;
            }
            else
            {
                s_ys = a;
                s_ye = e;
            }
        }
    }
}
} // namespace

void jwplcMirrorWrite(uint32_t wireBits, uint8_t bits)
{
    if (!g_buf)
    {
        return;
    }
    // El ESP32 saca primero el byte bajo del registro W0.
    for (uint8_t i = 0; i < bits; i = (uint8_t)(i + 8))
    {
        feedByte((uint8_t)(wireBits >> i));
    }
}

void jwplcMirrorBlock(uint16_t color, uint32_t len)
{
    if (!g_buf || !s_ram)
    {
        return;
    }
    s_half = false;
    putRun(indexOf(color), len);
}

void jwplcMirrorPixels(const void *data, uint32_t len, bool hostOrder)
{
    if (!g_buf || !s_ram)
    {
        return;
    }
    s_half = false;
    const uint16_t *p = (const uint16_t *)data;
    // Agrupa píxeles iguales seguidos (fondos de sprites) en un solo memset.
    uint32_t i = 0;
    while (i < len)
    {
        uint16_t v = p[i];
        const uint16_t c = hostOrder ? v : (uint16_t)(v << 8 | v >> 8);
        uint32_t n = 1;
        while (i + n < len && p[i + n] == v)
        {
            n++;
        }
        putRun(indexOf(c), n);
        i += n;
    }
}

void jwplcMirrorGeometry(int32_t colstart, int32_t rowstart, int32_t width, int32_t height)
{
    g_col = colstart;
    g_row = rowstart;
    if ((uint32_t)width * (uint32_t)height <= BUF_BYTES && height <= (int32_t)MAX_SIDE)
    {
        if (width != g_w || height != g_h)
        {
            g_w = width;
            g_h = height;
            if (g_buf)
            {
                memset(g_buf, 0, BUF_BYTES);
            }
            g_ver = g_ver + 1;
            for (uint16_t y = 0; y < MAX_SIDE; y++)
            {
                g_rowVer[y] = g_ver;
            }
        }
    }
}

bool jwplcMirrorStart()
{
    if (g_buf)
    {
        return true;
    }
    g_buf = (uint8_t *)malloc(BUF_BYTES);
    if (!g_buf)
    {
        return false;
    }
    memset(g_buf, 0, BUF_BYTES);
    memset(g_hash, 0, sizeof(g_hash));
    g_palN = 0;
    g_lastValid = false;
    indexOf(0x0000); // índice 0 = negro (búfer recién borrado)
    for (uint16_t y = 0; y < MAX_SIDE; y++)
    {
        g_rowVer[y] = g_ver;
    }
    jwplcMirrorOn = true;
    return true;
}

bool jwplcMirrorActive() { return g_buf != nullptr; }
uint16_t jwplcMirrorWidth() { return (uint16_t)g_w; }
uint16_t jwplcMirrorHeight() { return (uint16_t)g_h; }
uint32_t jwplcMirrorSnapshot() { return g_ver = g_ver + 1; }
uint32_t jwplcMirrorRowVersion(uint16_t y) { return y < MAX_SIDE ? g_rowVer[y] : 0; }
const uint8_t *jwplcMirrorRow(uint16_t y)
{
    return (g_buf && y < g_h) ? g_buf + (uint32_t)y * (uint32_t)g_w : nullptr;
}
uint16_t jwplcMirrorPaletteCount() { return g_palN; }
uint16_t jwplcMirrorPaletteColor(uint16_t index) { return index < 256 ? g_pal[index] : 0; }
