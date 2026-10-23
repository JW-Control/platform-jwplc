// Código generado por JWPLC HMI Designer
// API pública JWPLC_UI · Alpha11 A11-4
// Destino recomendado: JWPLC_HMI_Generated.h
// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.

#pragma once

#include <JWPLC_Display.h>

enum HMIPageId : uint8_t
{
    PAGE_PRINCIPAL = 0,
    PAGE_MOTOR = 1,
    PAGE_NIVEL = 2
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Principal
    FIELD_TEMP = 1,
    FIELD_ESTADO = 2,

    // Página 02 · Motor
    FIELD_MOTOR = 3,

    // Página 03 · Nivel
    FIELD_NIVEL = 4,
};

// Variables HMI
// Página 01 · Principal
float temperatura = 0.0f;
char estado[11] = {};

// Página 02 · Motor
bool motor = false;

// Página 03 · Nivel
float nivel = 0.0f;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_TEMP,
        JWPLC_UIRect(8, 34),
        JWPLC_UIText("Temp", "C"),
        JWPLC_UIValueFormat(
            3,
            1,
            true,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UITextField(
        FIELD_ESTADO,
        JWPLC_UIRect(8, 84),
        JWPLC_UIText("Estado", nullptr, 10),
        JWPLC_UITextFieldStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_LEFT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_MOTOR,
        JWPLC_UIRect(8, 34),
        JWPLC_UIText("Motor", nullptr),
        JWPLC_UIBoolText(
            "PARADO",
            "MARCHA"),
        JWPLC_UIBoolStyle(
            2,
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_CENTER),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0xFFE0,
            0x0000,
            0xFFFF)),
    // Barra Sólida (HORIZONTAL) - Estilo: SOLID, Radio: 0
    JWPLC_UIBarField(
        FIELD_NIVEL,
        JWPLC_UIRect(12, 40, 200, JWPLC_UI_AUTO),
        JWPLC_UIText("Nivel", "%"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UIBarStyle(
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED),
        2,
        JWPLC_UIColors(
            0xFFFF,
            0xFD20,
            0x18C3,
            0xFFFF))
};

void jwplcHMISetup()
{
    JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));
    JWPLC_Display.setUserPageCount(3);
    JWPLC_Display.setUserPage(PAGE_PRINCIPAL);
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);
}

// Primitivas auxiliares (sólo usan API pública de JWPLC_TFT).
#if defined(JWPLC_TFT_VERSION_MAJOR) && \
    ((JWPLC_TFT_VERSION_MAJOR > 0) || (JWPLC_TFT_VERSION_MINOR >= 2))
#define JWPLC_HMI_TFT_NATIVE_SHAPES 1
#else
#define JWPLC_HMI_TFT_NATIVE_SHAPES 0
#endif

static inline void jwplcHMISwap(int16_t &a, int16_t &b)
{
    const int16_t t = a;
    a = b;
    b = t;
}

static void jwplcHMIFillTriangle(JWPLC_TFTClass &tft,
    int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
#if JWPLC_HMI_TFT_NATIVE_SHAPES
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
#else
    if (y0 > y1) { jwplcHMISwap(y0, y1); jwplcHMISwap(x0, x1); }
    if (y1 > y2) { jwplcHMISwap(y2, y1); jwplcHMISwap(x2, x1); }
    if (y0 > y1) { jwplcHMISwap(y0, y1); jwplcHMISwap(x0, x1); }

    int16_t a;
    int16_t b;
    if (y0 == y2)
    {
        a = b = x0;
        if (x1 < a) a = x1; else if (x1 > b) b = x1;
        if (x2 < a) a = x2; else if (x2 > b) b = x2;
        tft.drawFastHLine(a, y0, b - a + 1, color);
        return;
    }

    const int32_t dx01 = x1 - x0, dy01 = y1 - y0;
    const int32_t dx02 = x2 - x0, dy02 = y2 - y0;
    const int32_t dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t sa = 0;
    int32_t sb = 0;
    const int16_t last = (y1 == y2) ? y1 : (int16_t)(y1 - 1);
    int16_t y = y0;

    for (; y <= last; y++)
    {
        a = (int16_t)(x0 + sa / dy01);
        b = (int16_t)(x0 + sb / dy02);
        sa += dx01;
        sb += dx02;
        if (a > b) jwplcHMISwap(a, b);
        tft.drawFastHLine(a, y, b - a + 1, color);
    }

    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++)
    {
        a = (int16_t)(x1 + sa / dy12);
        b = (int16_t)(x0 + sb / dy02);
        sa += dx12;
        sb += dx02;
        if (a > b) jwplcHMISwap(a, b);
        tft.drawFastHLine(a, y, b - a + 1, color);
    }
#endif
}

static void jwplcHMIFillEllipse(JWPLC_TFTClass &tft,
    int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color)
{
#if JWPLC_HMI_TFT_NATIVE_SHAPES
    tft.fillEllipse(cx, cy, rx, ry, color);
#else
    if (rx <= 0 || ry <= 0)
    {
        tft.drawFastHLine(cx - rx, cy, 2 * rx + 1, color);
        return;
    }
    for (int16_t dy = -ry; dy <= ry; dy++)
    {
        const float t = 1.0f - ((float)dy * (float)dy) / ((float)ry * (float)ry);
        const int16_t dx = (int16_t)((float)rx * sqrtf(t > 0.0f ? t : 0.0f) + 0.5f);
        tft.drawFastHLine(cx - dx, cy + dy, 2 * dx + 1, color);
    }
#endif
}

static void jwplcHMIDrawEllipse(JWPLC_TFTClass &tft,
    int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color)
{
#if JWPLC_HMI_TFT_NATIVE_SHAPES
    tft.drawEllipse(cx, cy, rx, ry, color);
#else
    if (rx <= 0 || ry <= 0)
    {
        tft.drawLine(cx - rx, cy - ry, cx + rx, cy + ry, color);
        return;
    }
    // Elipse por punto medio (misma lógica que el preview del Designer).
    const int32_t rx2 = (int32_t)rx * rx;
    const int32_t ry2 = (int32_t)ry * ry;
    int32_t x = 0;
    int32_t y = ry;
    int32_t px = 0;
    int32_t py = 2 * rx2 * y;
    int32_t p = ry2 - rx2 * ry + rx2 / 4;
    while (px <= py)
    {
        tft.drawPixel(cx + x, cy + y, color);
        tft.drawPixel(cx - x, cy + y, color);
        tft.drawPixel(cx + x, cy - y, color);
        tft.drawPixel(cx - x, cy - y, color);
        x++;
        px += 2 * ry2;
        if (p < 0)
        {
            p += ry2 + px;
        }
        else
        {
            y--;
            py -= 2 * rx2;
            p += ry2 + px - py;
        }
    }
    p = (int32_t)(ry2 * (x + 0.5f) * (x + 0.5f) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0)
    {
        tft.drawPixel(cx + x, cy + y, color);
        tft.drawPixel(cx - x, cy + y, color);
        tft.drawPixel(cx + x, cy - y, color);
        tft.drawPixel(cx - x, cy - y, color);
        y--;
        py -= 2 * rx2;
        if (p > 0)
        {
            p += rx2 - py;
        }
        else
        {
            x++;
            px += 2 * ry2;
            p += rx2 - py + px;
        }
    }
#endif
}

// Barra de Navegación HMI Autogenerada (ST7789)
// Se dibuja una vez por página desde jwplcUIPageEnter().
static void jwplcDrawPageNavBar(uint8_t activePage)
{
    JWPLC_TFTClass &tft = JWPLC_Display.tft();
    tft.fillRect(0, 147, 320, 22, 0x0000);
    tft.setTextSize(1);

    switch (activePage)
    {
    case 0:
        {
            tft.fillRoundRect(22, 148, 90, 20, 3, 0xFFFF);
            tft.drawRoundRect(22, 148, 90, 20, 3, 0x03FF);
            tft.setTextColor(0x03FF, 0xFFFF);
            tft.setCursor(55, 154);
            tft.print("PRNC");
            tft.fillRoundRect(115, 148, 90, 20, 3, 0x0000);
            tft.drawRoundRect(115, 148, 90, 20, 3, 0xFFFF);
            tft.setTextColor(0xFFFF, 0x0000);
            tft.setCursor(151, 154);
            tft.print("MTR");
            tft.fillRoundRect(208, 148, 90, 20, 3, 0x0000);
            tft.drawRoundRect(208, 148, 90, 20, 3, 0xF81F);
            tft.setTextColor(0xF81F, 0x0000);
            tft.setCursor(244, 154);
            tft.print("NVL");
        }
        break;
    case 1:
        {
            tft.fillRoundRect(22, 148, 90, 20, 3, 0x0000);
            tft.drawRoundRect(22, 148, 90, 20, 3, 0x03FF);
            tft.setTextColor(0x03FF, 0x0000);
            tft.setCursor(55, 154);
            tft.print("PRNC");
            tft.fillRoundRect(115, 148, 90, 20, 3, 0xFFFF);
            tft.drawRoundRect(115, 148, 90, 20, 3, 0xFFFF);
            tft.setTextColor(0x0000, 0xFFFF);
            tft.setCursor(151, 154);
            tft.print("MTR");
            tft.fillRoundRect(208, 148, 90, 20, 3, 0x0000);
            tft.drawRoundRect(208, 148, 90, 20, 3, 0xF81F);
            tft.setTextColor(0xF81F, 0x0000);
            tft.setCursor(244, 154);
            tft.print("NVL");
        }
        break;
    case 2:
        {
            tft.fillRoundRect(22, 148, 90, 20, 3, 0x0000);
            tft.drawRoundRect(22, 148, 90, 20, 3, 0x03FF);
            tft.setTextColor(0x03FF, 0x0000);
            tft.setCursor(55, 154);
            tft.print("PRNC");
            tft.fillRoundRect(115, 148, 90, 20, 3, 0x0000);
            tft.drawRoundRect(115, 148, 90, 20, 3, 0xFFFF);
            tft.setTextColor(0xFFFF, 0x0000);
            tft.setCursor(151, 154);
            tft.print("MTR");
            tft.fillRoundRect(208, 148, 90, 20, 3, 0xFFFF);
            tft.drawRoundRect(208, 148, 90, 20, 3, 0xF81F);
            tft.setTextColor(0xF81F, 0xFFFF);
            tft.setCursor(244, 154);
            tft.print("NVL");
        }
        break;
    default:
        break;
    }
}

// Gráfica estática por página (se dibuja una vez al entrar a la página).
// Para lógica propia al cambiar de página, define en el sketch:
//   extern "C" void jwplcHMIUserPageEnter(uint8_t page) { ... }
extern "C" void jwplcHMIUserPageEnter(uint8_t page) __attribute__((weak));

void jwplcUIPageEnter(uint8_t page)
{
    JWPLC_TFTClass &tft = JWPLC_Display.tft();

    switch (page)
    {
    case PAGE_PRINCIPAL:
        // Indicador
        jwplcHMIFillEllipse(tft, 273, 54, 37, 20, 0x07E0);
        jwplcHMIDrawEllipse(tft, 273, 54, 37, 20, 0xFFFF);
        // Cabecera
        tft.fillRect(0, 0, 230, 22, 0x0010);
        tft.drawRect(0, 0, 230, 22, 0x041F);
        // Titulo
        tft.setCursor(6, 4);
        tft.setTextSize(2);
        tft.setTextColor(0xFFFF);
        tft.print("JWPLC HMI A15");
        break;

    case PAGE_MOTOR:
        // Separador
        tft.drawLine(0, 120, 319, 120, 0x7BEF);
        // Hexagono
        tft.drawLine(203, 30, 232, 47, 0x07FF);
        tft.drawLine(232, 47, 232, 80, 0x07FF);
        tft.drawLine(232, 80, 203, 96, 0x07FF);
        tft.drawLine(203, 96, 174, 80, 0x07FF);
        tft.drawLine(174, 80, 174, 47, 0x07FF);
        tft.drawLine(174, 47, 203, 30, 0x07FF);
        // Flecha
        jwplcHMIFillTriangle(tft, 278, 30, 306, 86, 250, 86, 0xFD20);
        tft.drawLine(278, 30, 306, 86, 0xFFFF);
        tft.drawLine(306, 86, 250, 86, 0xFFFF);
        tft.drawLine(250, 86, 278, 30, 0xFFFF);
        break;

    case PAGE_NIVEL:
        // Circulo
        tft.drawCircle(275, 65, 25, 0xF81F);
        // Marco nivel
        tft.drawRoundRect(4, 30, 226, 70, 6, 0x7BEF);
        break;

    default:
        break;
    }

    jwplcDrawPageNavBar(page);

    if (jwplcHMIUserPageEnter)
    {
        jwplcHMIUserPageEnter(page);
    }
}

// Sincronización gráfica autogenerada (cada tick de refresco).
// Sólo setters: el runtime redibuja únicamente los campos que cambiaron.
// La lógica de aplicación debe permanecer en loop().
void jwplcUIUpdate()
{
    switch (JWPLC_Display.userPage())
    {
    case PAGE_PRINCIPAL:
        // Página 01 · Principal
        JWPLC_Display.setValue(FIELD_TEMP, temperatura);
        JWPLC_Display.setText(FIELD_ESTADO, estado);
        break;

    case PAGE_MOTOR:
        // Página 02 · Motor
        JWPLC_Display.setBool(FIELD_MOTOR, motor);
        break;

    case PAGE_NIVEL:
        // Página 03 · Nivel
        JWPLC_Display.setBar(FIELD_NIVEL, nivel);
        break;

    default:
        break;
    }
}
