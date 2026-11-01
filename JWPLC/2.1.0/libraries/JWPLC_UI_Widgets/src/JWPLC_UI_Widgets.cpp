#include "JWPLC_UI_Widgets.h"

#include <JWPLC_GlobalPeripherals.h>
#include <stdlib.h>
#include <string.h>

// ================================================================ tema

const JWPLC_UITheme JWPLC_UI_THEME_DARK = {
    0x0000, // background
    0x18E3, // surface
    0x2124, // surfaceAlt
    0x4A69, // border
    0xFFFF, // text
    0x9CF3, // textMuted
    0x2E8B, // accent (verde de selección, como el Designer)
    0x0000, // accentText
    0x07E0, // ok
    0xFD20, // warn
    0xF800, // error
    0x39E7, // header (C_TITLE_BG de JWPLC_IdleScreen)
    0xFFFF, // headerText
};

namespace
{
    // Avance de cursor/valor con la botonera: pulsaciones y repeticiones de
    // dos teclas, como JW_MatrixButtons::applyAxis, pero devuelto como delta.
    int32_t axisDelta(uint8_t decId, uint8_t incId)
    {
        const uint32_t mid = 1000000UL;
        uint32_t v = mid;
        JWPLC_Buttons.applyAxis(&v, 0, 2 * mid, decId, incId, false, false);
        return (int32_t)v - (int32_t)mid;
    }

    void restoreFont(JWPLC_TFTClass &tft)
    {
        tft.setTextPadding(0);
        tft.setTextDatum(JWPLC_TFTDatum::TOP_LEFT);
        tft.setFont(JWPLC_FONT_GLCD);
        tft.setTextSize(1);
    }

    // Todos los widgets usan la GLCD 5x7 escalada, la misma de la IDLE.
    void useGlcd(JWPLC_TFTClass &tft, uint8_t size)
    {
        tft.setFont(JWPLC_FONT_GLCD);
        tft.setTextSize(size);
    }

    // Textos de lista, mensajes y avisos: 16 px con trazo de 1 px, para que
    // se distingan de los títulos en GLCD x2.
    void useBody(JWPLC_TFTClass &tft)
    {
        tft.setFont(JWPLC_FONT_16);
        tft.setTextSize(1);
    }

    int32_t pow10i(uint8_t n)
    {
        int32_t p = 1;
        while (n--)
        {
            p *= 10;
        }
        return p;
    }
}

// ================================================================ lista

void JWPLC_UIList::begin(int16_t x, int16_t y, int16_t w, int16_t h,
                         uint8_t rowHeight, const JWPLC_UITheme &theme)
{
    _x = x;
    _y = y;
    _w = w;
    _h = h;
    _rowH = rowHeight ? rowHeight : 20;
    _visible = (uint8_t)(_h / _rowH);
    if (_visible == 0)
    {
        _visible = 1;
    }
    _theme = theme;
    invalidate();
}

void JWPLC_UIRow::left(const char *text)
{
    strncpy(_left, text ? text : "", _leftSize - 1);
    _left[_leftSize - 1] = 0;
}

void JWPLC_UIRow::left(const String &text)
{
    left(text.c_str());
}

void JWPLC_UIRow::right(const char *text)
{
    strncpy(_right, text ? text : "", _rightSize - 1);
    _right[_rightSize - 1] = 0;
}

void JWPLC_UIRow::right(const String &text)
{
    right(text.c_str());
}

void JWPLC_UIList::setRows(uint16_t count, JWPLC_UIRowText rowText)
{
    _count = count;
    _rowText = rowText;
    if (_selected >= _count)
    {
        _selected = _count ? (uint16_t)(_count - 1) : 0;
    }
    ensureVisible();
    invalidate();
}

void JWPLC_UIList::setSelected(uint16_t index)
{
    if (_count == 0)
    {
        return;
    }
    _selected = index < _count ? index : (uint16_t)(_count - 1);
    ensureVisible();
}

void JWPLC_UIList::ensureVisible()
{
    if (_selected < _top)
    {
        _top = _selected;
    }
    else if (_selected >= _top + _visible)
    {
        _top = (uint16_t)(_selected - _visible + 1);
    }
}

bool JWPLC_UIList::handleKeys()
{
    if (_count == 0)
    {
        return false;
    }
    uint32_t v = _selected;
    if (!JWPLC_Buttons.applyAxis(&v, 0, (uint32_t)(_count - 1), BTN_UP, BTN_DOWN, true, false))
    {
        return false;
    }
    _selected = (uint16_t)v;
    ensureVisible();
    return true;
}

void JWPLC_UIList::invalidate()
{
    _full = true;
}

void JWPLC_UIList::invalidateRow(uint16_t index)
{
    if (_dirtyRow == 0xFFFF)
    {
        _dirtyRow = index;
    }
    else if (_dirtyRow != index)
    {
        _full = true; // más de una fila: se redibuja la vista entera
    }
}

void JWPLC_UIList::drawRow(JWPLC_TFTClass &tft, uint16_t index)
{
    if (index < _top || index >= _top + _visible)
    {
        return;
    }
    const int16_t rowY = (int16_t)(_y + (index - _top) * _rowH);
    const int16_t rowW = (int16_t)(_w - 5); // 5 px para la barra de scroll

    if (index >= _count)
    {
        tft.fillRect(_x, rowY, rowW, _rowH, _theme.background);
        return;
    }

    const bool sel = index == _selected;
    const uint16_t bg = sel ? _theme.accent : ((index & 1) ? _theme.surfaceAlt : _theme.surface);

    char left[32] = {0};
    char right[24] = {0};
    if (_rowText)
    {
        _rowText(index, JWPLC_UIRow(left, sizeof(left), right, sizeof(right)));
    }

    if (index - _top < HASH_ROWS)
    {
        _rowHash[index - _top] = rowHash(index, left, right);
    }

    // 0.5.0: la fila se arma en un sprite de 16 bits (12 KB) y se envía de una
    // vez, así no se ve el relleno antes del texto. Es de 16 bits porque uno
    // de 4 bits se convierte píxel a píxel al enviarlo y en la placa tarda lo
    // bastante para verse un corte diagonal (tearing) al mover la selección.
    // Si no hay memoria para el sprite se dibuja directo como antes.
    static JWPLC_TFTSprite rowSprite;
    const int16_t sh = (int16_t)(_rowH - 1);
    if (rowSprite.created() && (rowSprite.width() != rowW || rowSprite.height() != sh))
    {
        rowSprite.destroy(); // otra lista con otra medida
    }
    if (!rowSprite.created() && !rowSprite.create(rowW, sh, 16))
    {
        tft.fillRect(_x, rowY, rowW, sh, bg);
        tft.drawFastHLine(_x, (int16_t)(rowY + _rowH - 1), rowW, _theme.background);
        const int16_t midY = (int16_t)(rowY + (_rowH - 1) / 2);
        useBody(tft);
        tft.setTextPadding(0);
        tft.setTextColor(sel ? _theme.accentText : _theme.text, bg);
        tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
        tft.drawString(left, (int16_t)(_x + 6), midY);
        if (right[0])
        {
            tft.setTextColor(sel ? _theme.accentText : _theme.textMuted, bg);
            tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_RIGHT);
            tft.drawString(right, (int16_t)(_x + rowW - 6), midY);
        }
        restoreFont(tft);
        return;
    }
    rowSprite.fillRect(0, 0, rowW, sh, bg);
    rowSprite.setFont(JWPLC_FONT_16);
    rowSprite.setTextSize(1);
    const int16_t midY = (int16_t)((_rowH - 1) / 2);
    rowSprite.setTextColor(sel ? _theme.accentText : _theme.text, bg);
    rowSprite.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
    rowSprite.drawString(left, 6, midY);
    if (right[0])
    {
        rowSprite.setTextColor(sel ? _theme.accentText : _theme.textMuted, bg);
        rowSprite.setTextDatum(JWPLC_TFTDatum::MIDDLE_RIGHT);
        rowSprite.drawString(right, (int16_t)(rowW - 6), midY);
    }
    rowSprite.push(_x, rowY);
    tft.drawFastHLine(_x, (int16_t)(rowY + _rowH - 1), rowW, _theme.background);
}

void JWPLC_UIList::drawScrollBar(JWPLC_TFTClass &tft)
{
    const int16_t bx = (int16_t)(_x + _w - 3);
    const int16_t trackH = (int16_t)(_visible * _rowH);
    tft.fillRect(bx, _y, 3, trackH, _theme.surface);
    if (_count <= _visible)
    {
        return;
    }
    int16_t thumbH = (int16_t)((int32_t)trackH * _visible / _count);
    if (thumbH < 8)
    {
        thumbH = 8;
    }
    const int16_t span = (int16_t)(trackH - thumbH);
    const int16_t thumbY = (int16_t)(_y + (int32_t)span * _top / (_count - _visible));
    tft.fillRect(bx, thumbY, 3, thumbH, _theme.textMuted);
}

void JWPLC_UIList::draw()
{
    draw(JWPLC_TFT);
}

uint32_t JWPLC_UIList::rowHash(uint16_t index, const char *left, const char *right) const
{
    uint32_t h = 2166136261UL ^ (index == _selected ? 0x9E3779B9UL : 0);
    for (const char *p = left; *p; p++)
    {
        h = (h ^ (uint8_t)*p) * 16777619UL;
    }
    h = (h ^ 0xFF) * 16777619UL;
    for (const char *p = right; *p; p++)
    {
        h = (h ^ (uint8_t)*p) * 16777619UL;
    }
    return h;
}

void JWPLC_UIList::refreshChanged()
{
    if (_full || !_rowText)
    {
        return; // el próximo draw() redibuja todo
    }
    for (uint16_t i = 0; i < _visible && i < HASH_ROWS; i++)
    {
        const uint16_t index = (uint16_t)(_top + i);
        if (index >= _count)
        {
            break;
        }
        char left[32] = {0};
        char right[24] = {0};
        _rowText(index, JWPLC_UIRow(left, sizeof(left), right, sizeof(right)));
        if (rowHash(index, left, right) != _rowHash[i])
        {
            drawRow(JWPLC_TFT, index);
        }
    }
}

void JWPLC_UIList::draw(JWPLC_TFTClass &tft)
{
    if (_full || _top != _drawnTop)
    {
        for (uint16_t i = 0; i < _visible; i++)
        {
            drawRow(tft, (uint16_t)(_top + i));
        }
        // Resto del área que no llena una fila completa.
        const int16_t used = (int16_t)(_visible * _rowH);
        if (used < _h)
        {
            tft.fillRect(_x, (int16_t)(_y + used), _w, (int16_t)(_h - used), _theme.background);
        }
        // Franja de 2 px entre las filas y la barra de scroll.
        tft.fillRect((int16_t)(_x + _w - 5), _y, 2, (int16_t)(_visible * _rowH), _theme.background);
        drawScrollBar(tft);
    }
    else
    {
        if (_drawnSelected != _selected)
        {
            drawRow(tft, _drawnSelected);
            drawRow(tft, _selected);
        }
        if (_dirtyRow != 0xFFFF && _dirtyRow != _selected && _dirtyRow != _drawnSelected)
        {
            drawRow(tft, _dirtyRow);
        }
        else if (_dirtyRow == _selected && _drawnSelected == _selected)
        {
            drawRow(tft, _selected);
        }
    }
    _full = false;
    _dirtyRow = 0xFFFF;
    _drawnTop = _top;
    _drawnSelected = _selected;
}

// ================================================================ editor

void JWPLC_UIDigitEditor::begin(int16_t x, int16_t y, uint8_t digits, uint8_t decimals,
                                const char *unit, const JWPLC_UITheme &theme)
{
    _x = x;
    _y = y;
    _digits = digits < 1 ? 1 : (digits > MAX_DIGITS ? MAX_DIGITS : digits);
    _decimals = decimals < _digits ? decimals : (uint8_t)(_digits - 1);
    _unit = unit;
    _theme = theme;
    _max = pow10i(_digits) - 1;
    _min = 0;
    _cursor = 0;
    invalidate();
}

void JWPLC_UIDigitEditor::setRange(int32_t minValue, int32_t maxValue)
{
    const int32_t limit = pow10i(_digits) - 1;
    _min = minValue < -limit ? -limit : minValue;
    _max = maxValue > limit ? limit : maxValue;
    if (_min > _max)
    {
        _min = _max;
    }
    setValue(_value);
}

void JWPLC_UIDigitEditor::setValue(int32_t value)
{
    if (value < _min)
    {
        value = _min;
    }
    if (value > _max)
    {
        value = _max;
    }
    _value = value;
    _original = value;
}

JWPLC_UIEditResult JWPLC_UIDigitEditor::handleKeys()
{
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        _original = _value;
        return UI_EDIT_ACCEPTED;
    }
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        _value = _original;
        return UI_EDIT_CANCELLED;
    }

    JWPLC_UIEditResult r = UI_EDIT_NONE;

    // ◄ sube al dígito de más peso, ► baja; sin vuelta.
    const int32_t move = axisDelta(BTN_RIGHT, BTN_LEFT);
    if (move)
    {
        int32_t c = (int32_t)_cursor + move;
        if (c < 0)
        {
            c = 0;
        }
        if (c > _digits - 1)
        {
            c = _digits - 1;
        }
        _cursor = (uint8_t)c;
    }

    const int32_t steps = axisDelta(BTN_DOWN, BTN_UP);
    if (steps)
    {
        int64_t v = (int64_t)_value + (int64_t)steps * pow10i(_cursor);
        if (v < _min)
        {
            v = _min;
        }
        if (v > _max)
        {
            v = _max;
        }
        if ((int32_t)v != _value)
        {
            _value = (int32_t)v;
            r = UI_EDIT_CHANGED;
        }
    }
    return r;
}

void JWPLC_UIDigitEditor::invalidate()
{
    _full = true;
}

// Una celda por dígito (ceros a la izquierda incluidos) más una de signo si
// el rango admite negativos.
void JWPLC_UIDigitEditor::format(char *cells) const
{
    uint8_t n = 0;
    if (_min < 0)
    {
        cells[n++] = _value < 0 ? '-' : ' ';
    }
    uint32_t a = (uint32_t)(_value < 0 ? -_value : _value);
    for (int8_t pos = (int8_t)(_digits - 1); pos >= 0; pos--)
    {
        cells[n++] = (char)('0' + (a / (uint32_t)pow10i((uint8_t)pos)) % 10);
    }
    cells[n] = 0;
}

int16_t JWPLC_UIDigitEditor::cellX(uint8_t pos) const
{
    // pos: 0 = unidades. Índice de izquierda a derecha entre los dígitos.
    const uint8_t idx = (uint8_t)(_digits - 1 - pos);
    int16_t x = (int16_t)(_x + (_min < 0 ? CELL_W + GAP : 0) + idx * (CELL_W + GAP));
    if (_decimals && pos < _decimals)
    {
        x = (int16_t)(x + POINT_W);
    }
    return x;
}

int16_t JWPLC_UIDigitEditor::width() const
{
    return (int16_t)(cellX(0) + CELL_W - _x);
}

void JWPLC_UIDigitEditor::draw()
{
    draw(JWPLC_TFT);
}

void JWPLC_UIDigitEditor::draw(JWPLC_TFTClass &tft)
{
    char cells[MAX_DIGITS + 2];
    format(cells);
    const uint8_t signCells = _min < 0 ? 1 : 0;

    useGlcd(tft, 2);
    tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_CENTER);
    tft.setTextPadding(0);

    if (_full)
    {
        // Fondo, punto decimal y unidad: estáticos mientras se edita.
        const int16_t w = width();
        tft.fillRect((int16_t)(_x - 2), (int16_t)(_y - 2), (int16_t)(w + 4), (int16_t)(CELL_H + 4), _theme.surface);
        if (_decimals)
        {
            const int16_t px = (int16_t)(cellX(_decimals) + CELL_W + 2);
            tft.fillRect(px, (int16_t)(_y + CELL_H - 7), 3, 3, _theme.text);
        }
        if (_unit && _unit[0])
        {
            useGlcd(tft, 2);
            tft.setTextColor(_theme.textMuted, _theme.surface);
            tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
            tft.drawString(_unit, (int16_t)(_x + w + 8), (int16_t)(_y + CELL_H / 2));
            useGlcd(tft, 2);
            tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_CENTER);
        }
    }

    const uint8_t total = (uint8_t)(signCells + _digits);
    for (uint8_t i = 0; i < total; i++)
    {
        const bool isSign = i < signCells;
        const uint8_t pos = isSign ? 0xFF : (uint8_t)(_digits - 1 - (i - signCells));
        const bool cur = !isSign && pos == _cursor;
        const bool wasCur = !isSign && pos == _drawnCursor;
        if (!_full && cells[i] == _drawn[i] && cur == wasCur)
        {
            continue;
        }
        const int16_t cx = isSign ? _x : cellX(pos);
        const uint16_t bg = cur ? _theme.accent : _theme.surfaceAlt;
        tft.fillRoundRect(cx, _y, CELL_W, CELL_H, 3, bg);
        const char s[2] = {cells[i], 0};
        tft.setTextColor(cur ? _theme.accentText : _theme.text, bg);
        tft.drawString(s, (int16_t)(cx + CELL_W / 2), (int16_t)(_y + CELL_H / 2));
        if (cur)
        {
            // Marcas ▲▼ de que este dígito se edita (después del texto).
            tft.fillTriangle((int16_t)(cx + CELL_W / 2), (int16_t)(_y + 2),
                             (int16_t)(cx + CELL_W / 2 - 3), (int16_t)(_y + 5),
                             (int16_t)(cx + CELL_W / 2 + 3), (int16_t)(_y + 5), _theme.accentText);
            tft.fillTriangle((int16_t)(cx + CELL_W / 2), (int16_t)(_y + CELL_H - 3),
                             (int16_t)(cx + CELL_W / 2 - 3), (int16_t)(_y + CELL_H - 6),
                             (int16_t)(cx + CELL_W / 2 + 3), (int16_t)(_y + CELL_H - 6), _theme.accentText);
        }
    }

    memcpy(_drawn, cells, sizeof(_drawn) < sizeof(cells) ? sizeof(_drawn) : sizeof(cells));
    _drawnCursor = _cursor;
    _full = false;
    restoreFont(tft);
}

// ================================================================ diálogo

namespace
{
    const int16_t DLG_W = 280;
    const int16_t DLG_H = 104;
    const int16_t DLG_X = (320 - DLG_W) / 2;
    const int16_t DLG_Y = (170 - DLG_H) / 2;
    const int16_t BTN_W = 80;
    const int16_t BTN_H = 24;
}

void JWPLC_UIDialog::open(const char *title, const char *message, bool yesNo,
                          const JWPLC_UITheme &theme)
{
    _title = title ? title : "";
    _message = message ? message : "";
    _yesNo = yesNo;
    _theme = theme;
    _choice = 0;
    _drawnChoice = 0xFF;
    _open = true;
    _full = true;
}

JWPLC_UIDialogResult JWPLC_UIDialog::handleKeys()
{
    if (!_open)
    {
        return UI_DIALOG_NONE;
    }
    if (_yesNo)
    {
        const int32_t move = axisDelta(BTN_LEFT, BTN_RIGHT);
        if (move)
        {
            _choice = move > 0 ? 1 : 0;
        }
    }
    else
    {
        axisDelta(BTN_LEFT, BTN_RIGHT); // se consumen para que no queden pendientes
    }
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        _open = false;
        return (_choice == 0) ? UI_DIALOG_YES : UI_DIALOG_NO;
    }
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        _open = false;
        return UI_DIALOG_NO;
    }
    return UI_DIALOG_NONE;
}

void JWPLC_UIDialog::drawButtons(JWPLC_TFTClass &tft)
{
    const int16_t by = (int16_t)(DLG_Y + DLG_H - BTN_H - 10);
    const uint8_t count = _yesNo ? 2 : 1;
    const char *labels[2] = {_yesNo ? "Si" : "Aceptar", "No"};
    useGlcd(tft, 2);
    tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_CENTER);
    // "Aceptar" en GLCD x2 mide 84 px: el botón único es más ancho.
    const int16_t bw = count == 1 ? (int16_t)(BTN_W + 30) : BTN_W;
    for (uint8_t i = 0; i < count; i++)
    {
        const int16_t bx = count == 1
                               ? (int16_t)(DLG_X + (DLG_W - bw) / 2)
                               : (int16_t)(DLG_X + DLG_W / 2 + (i == 0 ? -BTN_W - 8 : 8));
        const bool sel = i == _choice;
        const uint16_t bg = sel ? _theme.accent : _theme.surfaceAlt;
        tft.fillRoundRect(bx, by, bw, BTN_H, 4, bg);
        if (!sel)
        {
            tft.drawRoundRect(bx, by, bw, BTN_H, 4, _theme.border);
        }
        tft.setTextColor(sel ? _theme.accentText : _theme.text, bg);
        tft.drawString(labels[i], (int16_t)(bx + bw / 2), (int16_t)(by + BTN_H / 2));
    }
    restoreFont(tft);
}

void JWPLC_UIDialog::draw()
{
    draw(JWPLC_TFT);
}

void JWPLC_UIDialog::draw(JWPLC_TFTClass &tft)
{
    if (!_open)
    {
        return;
    }
    if (_full)
    {
        tft.fillRoundRect(DLG_X, DLG_Y, DLG_W, DLG_H, 6, _theme.surface);
        // Barra de título con el gris del título de la IDLE.
        tft.fillRoundRect(DLG_X, DLG_Y, DLG_W, 27, 6, _theme.header);
        tft.fillRect(DLG_X, (int16_t)(DLG_Y + 20), DLG_W, 7, _theme.header);
        tft.drawRoundRect(DLG_X, DLG_Y, DLG_W, DLG_H, 6, _theme.accent);

        useGlcd(tft, 2);
        tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
        tft.setTextColor(_theme.headerText, _theme.header);
        tft.drawString(_title, (int16_t)(DLG_X + 10), (int16_t)(DLG_Y + 14));

        useBody(tft);
        tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_CENTER);
        tft.setTextColor(_theme.text, _theme.surface);
        tft.drawString(_message, (int16_t)(DLG_X + DLG_W / 2), (int16_t)(DLG_Y + 44));
        restoreFont(tft);
    }
    if (_full || _drawnChoice != _choice)
    {
        drawButtons(tft);
        _drawnChoice = _choice;
    }
    _full = false;
}

// ================================================================ aviso

void JWPLC_UIToast::begin(int16_t x, int16_t y, int16_t w, int16_t h, const JWPLC_UITheme &theme)
{
    _x = x;
    _y = y;
    _w = w;
    _h = h;
    _theme = theme;
}

void JWPLC_UIToast::show(const char *text, uint32_t durationMs, uint16_t color)
{
    strncpy(_text, text ? text : "", sizeof(_text) - 1);
    _text[sizeof(_text) - 1] = 0;
    _color = color ? color : _theme.ok;
    _until = millis() + durationMs;
    _visible = true;
    _pendingDraw = true;
}

bool JWPLC_UIToast::draw()
{
    JWPLC_TFTClass &tft = JWPLC_TFT;
    if (!_visible)
    {
        return false;
    }
    if ((int32_t)(millis() - _until) >= 0)
    {
        tft.fillRect(_x, _y, _w, _h, _theme.background);
        _visible = false;
        _pendingDraw = false;
        return true;
    }
    if (_pendingDraw)
    {
        tft.fillRoundRect(_x, _y, _w, _h, 4, _theme.surface);
        tft.fillRect(_x, _y, 4, _h, _color);
        useBody(tft);
        tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
        tft.setTextColor(_theme.text, _theme.surface);
        tft.drawString(_text, (int16_t)(_x + 12), (int16_t)(_y + _h / 2));
        restoreFont(tft);
        _pendingDraw = false;
    }
    return false;
}

// ================================================================ tendencia

JWPLC_UITrend::~JWPLC_UITrend()
{
    releaseStorage();
}

void JWPLC_UITrend::releaseStorage()
{
    free(_storage);
    _storage = nullptr;
    for (uint8_t c = 0; c < MAX_CHANNELS; c++)
    {
        _data[c] = nullptr;
    }
}

// Un solo bloque para todos los canales, reservado una vez (al dibujar o
// recibir la primera muestra), cuando ya se sabe si hay escala y cuántos
// canales hay.
bool JWPLC_UITrend::ensureStorage()
{
    if (_storage || _channels == 0)
    {
        return _storage != nullptr;
    }
    const size_t pw = (size_t)plotWidth();
    _storage = (uint8_t *)malloc(pw * _channels);
    if (_storage == nullptr)
    {
        return false;
    }
    memset(_storage, NO_DATA, pw * _channels);
    for (uint8_t c = 0; c < _channels; c++)
    {
        _data[c] = _storage + pw * c;
    }
    return true;
}

void JWPLC_UITrend::begin(int16_t x, int16_t y, int16_t w, int16_t h,
                          float minValue, float maxValue, const JWPLC_UITheme &theme)
{
    releaseStorage();
    _x = x;
    _y = y;
    _w = w < 8 ? 8 : w;
    _h = h < 8 ? 8 : (h > 250 ? 250 : h); // la altura en píxeles cabe en un uint8_t
    _min = minValue;
    _max = maxValue > minValue ? maxValue : minValue + 1.0f;
    _theme = theme;
    _channels = 0;
    _thresholds = 0;
    _pos = 0;
    _pending = 0;
    _scale = false;
    _full = true;
}

void JWPLC_UITrend::showScale(uint8_t decimals)
{
    if (_storage == nullptr && _w - 2 - SCALE_W >= 8)
    {
        _scale = true;
        _scaleDecimals = decimals > 3 ? 3 : decimals;
        _full = true;
    }
}

bool JWPLC_UITrend::addChannel(uint16_t color)
{
    if (_channels >= MAX_CHANNELS || _storage != nullptr)
    {
        return false;
    }
    _color[_channels] = color;
    _last[_channels] = _min;
    _channels++;
    return true;
}

bool JWPLC_UITrend::addThreshold(float value, uint16_t color)
{
    if (_thresholds >= MAX_THRESHOLDS)
    {
        return false;
    }
    _thrPix[_thresholds] = toPixel(value);
    _thrValue[_thresholds] = value;
    _thrColor[_thresholds] = color;
    _thresholds++;
    _full = true;
    return true;
}

uint8_t JWPLC_UITrend::toPixel(float v) const
{
    float r = (v - _min) / (_max - _min);
    if (r < 0.0f)
    {
        r = 0.0f;
    }
    if (r > 1.0f)
    {
        r = 1.0f;
    }
    return (uint8_t)(r * (float)(plotH() - 1) + 0.5f);
}

void JWPLC_UITrend::push(float value1)
{
    const float v[1] = {value1};
    pushValues(v, 1);
}

void JWPLC_UITrend::push(float value1, float value2)
{
    const float v[2] = {value1, value2};
    pushValues(v, 2);
}

void JWPLC_UITrend::push(float value1, float value2, float value3)
{
    const float v[3] = {value1, value2, value3};
    pushValues(v, 3);
}

void JWPLC_UITrend::pushValues(const float *values, uint8_t count)
{
    if (!ensureStorage())
    {
        return;
    }
    const int16_t pw = plotWidth();
    for (uint8_t c = 0; c < _channels; c++)
    {
        // Canal sin valor en esta llamada: repite el último.
        const float v = c < count ? values[c] : _last[c];
        _data[c][_pos] = toPixel(v);
        _last[c] = v;
        // El hueco por delante del cursor queda sin datos.
        _data[c][(_pos + GAP) % pw] = NO_DATA;
    }
    _pos = (int16_t)((_pos + 1) % pw);
    if (_pending < (uint16_t)pw)
    {
        _pending++;
    }
}

float JWPLC_UITrend::lastValue(uint8_t channel) const
{
    return channel < _channels ? _last[channel] : 0.0f;
}

void JWPLC_UITrend::clear()
{
    if (_storage)
    {
        memset(_storage, NO_DATA, (size_t)plotWidth() * _channels);
    }
    _pos = 0;
    _pending = 0;
    _full = true;
}

void JWPLC_UITrend::invalidate()
{
    _full = true;
}

void JWPLC_UITrend::eraseColumn(JWPLC_TFTClass &tft, int16_t col)
{
    const int16_t sx = (int16_t)(plotX() + 1 + col);
    const int16_t top = (int16_t)(_y + 1);
    tft.drawFastVLine(sx, top, plotH(), _theme.background);
    if ((col & 3) == 0)
    {
        // Rejilla punteada en cuartos.
        for (uint8_t k = 1; k < 4; k++)
        {
            tft.drawPixel(sx, (int16_t)(top + plotH() * k / 4), _theme.border);
        }
    }
    for (uint8_t t = 0; t < _thresholds; t++)
    {
        if ((col % 6) < 3)
        {
            tft.drawPixel(sx, (int16_t)(top + plotH() - 1 - _thrPix[t]), _thrColor[t]);
        }
    }
}

void JWPLC_UITrend::drawColumn(JWPLC_TFTClass &tft, int16_t col)
{
    const int16_t pw = plotWidth();
    const int16_t sx = (int16_t)(plotX() + 1 + col);
    const int16_t base = (int16_t)(_y + plotH()); // fila del valor 0
    const int16_t prevCol = (int16_t)((col + pw - 1) % pw);
    if (_storage == nullptr)
    {
        return;
    }
    // Del último canal al primero: el canal 0 queda encima.
    for (int8_t c = (int8_t)(_channels - 1); c >= 0; c--)
    {
        const uint8_t cur = _data[c][col];
        if (cur == NO_DATA)
        {
            continue;
        }
        uint8_t prev = _data[c][prevCol];
        if (prev == NO_DATA)
        {
            prev = cur;
        }
        // Tramo vertical entre la muestra anterior y la actual: la curva queda
        // continua aunque cambie rápido.
        const uint8_t lo = prev < cur ? prev : cur;
        const uint8_t hi = prev < cur ? cur : prev;
        tft.drawFastVLine(sx, (int16_t)(base - hi), (int16_t)(hi - lo + 1), _color[c]);
    }
}

void JWPLC_UITrend::draw()
{
    draw(JWPLC_TFT);
}

void JWPLC_UITrend::draw(JWPLC_TFTClass &tft)
{
    ensureStorage();
    const int16_t pw = plotWidth();
    if (_full)
    {
        tft.drawRect(plotX(), _y, (int16_t)(plotWidth() + 2), _h, _theme.border);
        if (_scale)
        {
            drawScale(tft);
        }
        for (int16_t col = 0; col < pw; col++)
        {
            eraseColumn(tft, col);
        }
        for (int16_t col = 0; col < pw; col++)
        {
            drawColumn(tft, col);
        }
        _full = false;
        _pending = 0;
        return;
    }
    if (_pending == 0)
    {
        return;
    }
    int16_t col = (int16_t)((_pos - _pending + pw) % pw);
    while (_pending)
    {
        eraseColumn(tft, (int16_t)((col + GAP) % pw));
        drawColumn(tft, col);
        col = (int16_t)((col + 1) % pw);
        _pending--;
    }
}

void JWPLC_UITrend::scaleLabel(JWPLC_TFTClass &tft, float value, uint8_t pix, uint16_t color)
{
    char txt[12];
    const int32_t m = pow10i(_scaleDecimals);
    const int32_t v = (int32_t)(value * (float)m + (value < 0 ? -0.5f : 0.5f));
    if (_scaleDecimals == 0)
    {
        snprintf(txt, sizeof(txt), "%ld", (long)v);
    }
    else
    {
        const int32_t a = v < 0 ? -v : v;
        snprintf(txt, sizeof(txt), "%s%ld.%0*ld", v < 0 ? "-" : "", (long)(a / m),
                 (int)_scaleDecimals, (long)(a % m));
    }
    // La etiqueta queda centrada en su fila, sin salirse del alto del widget.
    int16_t cy = (int16_t)(_y + plotH() - pix);
    if (cy < _y + 4)
    {
        cy = (int16_t)(_y + 4);
    }
    if (cy > _y + _h - 5)
    {
        cy = (int16_t)(_y + _h - 5);
    }
    tft.setTextColor(color, _theme.background);
    tft.drawString(txt, (int16_t)(_x + SCALE_W - 3), cy);
}

void JWPLC_UITrend::drawScale(JWPLC_TFTClass &tft)
{
    tft.fillRect(_x, _y, SCALE_W, _h, _theme.background);
    useGlcd(tft, 1);
    tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_RIGHT);
    uint8_t used[5];
    for (uint8_t k = 0; k <= 4; k++)
    {
        const float v = _min + (_max - _min) * (float)k / 4.0f;
        used[k] = toPixel(v);
        scaleLabel(tft, v, used[k], _theme.textMuted);
    }
    // Umbrales en su color, si no tapan una etiqueta de la rejilla.
    for (uint8_t t = 0; t < _thresholds; t++)
    {
        bool libre = true;
        for (uint8_t k = 0; k <= 4; k++)
        {
            const int d = (int)_thrPix[t] - (int)used[k];
            if (d > -9 && d < 9)
            {
                libre = false;
            }
        }
        if (libre)
        {
            scaleLabel(tft, _thrValue[t], _thrPix[t], _thrColor[t]);
        }
    }
    restoreFont(tft);
}

// ================================================================ teclado

namespace
{
    const char kKeysLower[4][11] = {"1234567890", "qwertyuiop", "asdfghjkl@", "zxcvbnm.-_"};
    const char kKeysUpper[4][11] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL@", "ZXCVBNM.-_"};
    const char kKeysSymbols[4][11] = {"1234567890", "!\"#$%&/()=", "?+*<>[]{}'", ";:,\\|^~`.@"};
    const int16_t kFuncX[5] = {0, 48, 96, 192, 256};
    const int16_t kFuncW[5] = {48, 48, 96, 64, 64};
}

void JWPLC_UIKeyboard::begin(int16_t y, const JWPLC_UITheme &theme)
{
    _y = y;
    _theme = theme;
    _full = true;
}

void JWPLC_UIKeyboard::open(const char title[], const char initial[], uint8_t maxLength, bool password)
{
    _title = title ? title : "";
    _maxLen = maxLength == 0 ? 1 : (maxLength > MAX_TEXT ? MAX_TEXT : maxLength);
    strncpy(_text, initial ? initial : "", _maxLen);
    _text[_maxLen] = 0;
    memcpy(_original, _text, sizeof(_original));
    _password = password;
    _page = PAGE_LOWER;
    _row = 1;
    _col = 0;
    _open = true;
    _full = true;
}

char JWPLC_UIKeyboard::keyChar(uint8_t row, uint8_t col) const
{
    if (row >= 4 || col >= COLS)
    {
        return 0;
    }
    if (_page == PAGE_UPPER)
    {
        return kKeysUpper[row][col];
    }
    if (_page == PAGE_SYMBOLS)
    {
        return kKeysSymbols[row][col];
    }
    return kKeysLower[row][col];
}

void JWPLC_UIKeyboard::funcRect(uint8_t index, int16_t &x, int16_t &w) const
{
    x = kFuncX[index];
    w = kFuncW[index];
}

// Al pasar de una fila de caracteres a la de funciones (y al revés) se
// conserva la posición horizontal.
uint8_t JWPLC_UIKeyboard::colToFunc(uint8_t col) const
{
    const int16_t cx = (int16_t)(col * 32 + 16);
    for (uint8_t f = 0; f < 5; f++)
    {
        if (cx >= kFuncX[f] && cx < kFuncX[f] + kFuncW[f])
        {
            return f;
        }
    }
    return 4;
}

uint8_t JWPLC_UIKeyboard::funcToCol(uint8_t func) const
{
    return (uint8_t)((kFuncX[func] + kFuncW[func] / 2) / 32);
}

JWPLC_UIEditResult JWPLC_UIKeyboard::press()
{
    const size_t len = strlen(_text);
    if (_row < 4)
    {
        if (len < _maxLen)
        {
            _text[len] = keyChar(_row, _col);
            _text[len + 1] = 0;
            _fieldDirty = true;
            return UI_EDIT_CHANGED;
        }
        return UI_EDIT_NONE;
    }
    switch (_col)
    {
    case 0: // mayúsculas / minúsculas
        _page = _page == PAGE_LOWER ? PAGE_UPPER : PAGE_LOWER;
        return UI_EDIT_NONE;
    case 1: // símbolos
        _page = _page == PAGE_SYMBOLS ? PAGE_LOWER : PAGE_SYMBOLS;
        return UI_EDIT_NONE;
    case 2: // espacio
        if (len < _maxLen)
        {
            _text[len] = ' ';
            _text[len + 1] = 0;
            _fieldDirty = true;
            return UI_EDIT_CHANGED;
        }
        return UI_EDIT_NONE;
    case 3: // borrar
        if (len > 0)
        {
            _text[len - 1] = 0;
            _fieldDirty = true;
            return UI_EDIT_CHANGED;
        }
        return UI_EDIT_NONE;
    default: // OK
        _open = false;
        return UI_EDIT_ACCEPTED;
    }
}

JWPLC_UIEditResult JWPLC_UIKeyboard::handleKeys()
{
    if (!_open)
    {
        return UI_EDIT_NONE;
    }
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        memcpy(_text, _original, sizeof(_text));
        _open = false;
        return UI_EDIT_CANCELLED;
    }

    // ▲ ▼ cambian de fila (con vuelta), conservando la columna.
    const int32_t dRow = axisDelta(BTN_UP, BTN_DOWN);
    if (dRow)
    {
        const uint8_t oldRow = _row;
        int32_t r = ((int32_t)_row + dRow) % ROWS;
        if (r < 0)
        {
            r += ROWS;
        }
        _row = (uint8_t)r;
        if (oldRow < 4 && _row == 4)
        {
            _col = colToFunc(_col);
        }
        else if (oldRow == 4 && _row < 4)
        {
            _col = funcToCol(_col);
        }
    }

    // ◄ ► cambian de tecla (con vuelta).
    const int32_t dCol = axisDelta(BTN_LEFT, BTN_RIGHT);
    if (dCol)
    {
        const int32_t n = _row == 4 ? funcCount() : COLS;
        int32_t c = ((int32_t)_col + dCol) % n;
        if (c < 0)
        {
            c += n;
        }
        _col = (uint8_t)c;
    }

    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        return press();
    }
    return UI_EDIT_NONE;
}

void JWPLC_UIKeyboard::drawKey(JWPLC_TFTClass &tft, uint8_t row, uint8_t col)
{
    const bool sel = row == _row && col == _col;
    int16_t x;
    int16_t w;
    if (row < 4)
    {
        x = (int16_t)(col * 32);
        w = 32;
    }
    else
    {
        funcRect(col, x, w);
    }
    const int16_t y = (int16_t)(keysY() + row * KEY_H);
    const uint16_t bg = sel ? _theme.accent : (row < 4 ? _theme.surfaceAlt : _theme.surface);
    tft.fillRect(x, y, w, KEY_H, _theme.background);
    tft.fillRoundRect((int16_t)(x + 1), y, (int16_t)(w - 2), (int16_t)(KEY_H - 2), 3, bg);

    useGlcd(tft, 2);
    tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_CENTER);
    tft.setTextColor(sel ? _theme.accentText : _theme.text, bg);
    const int16_t cx = (int16_t)(x + w / 2);
    const int16_t cy = (int16_t)(y + (KEY_H - 2) / 2 + 1);
    if (row < 4)
    {
        const char s[2] = {keyChar(row, col), 0};
        tft.drawString(s, cx, cy);
    }
    else
    {
        const char *label = "OK";
        switch (col)
        {
        case 0: label = _page == PAGE_UPPER ? "abc" : "ABC"; break;
        case 1: label = _page == PAGE_SYMBOLS ? "abc" : "#+="; break;
        case 2: label = "espacio"; break;
        case 3: label = "<-"; break;
        default: break;
        }
        if (col == 2)
        {
            useGlcd(tft, 1);
        }
        tft.drawString(label, cx, cy);
    }
    restoreFont(tft);
}

void JWPLC_UIKeyboard::drawField(JWPLC_TFTClass &tft)
{
    const int16_t fy = _y;
    tft.fillRect(0, fy, 320, FIELD_H, _theme.background);
    tft.fillRoundRect(2, fy, 316, FIELD_H, 4, _theme.surface);

    useBody(tft);
    tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
    tft.setTextColor(_theme.textMuted, _theme.surface);
    const int16_t titleW = tft.drawString(_title, 8, (int16_t)(fy + FIELD_H / 2));

    // Texto en GLCD x2 (12 px por carácter); si no cabe se muestra el final.
    const int16_t tx = (int16_t)(8 + (titleW > 0 ? titleW : 0) + 10);
    const int16_t room = (int16_t)((316 - tx - 14) / 12);
    const size_t len = strlen(_text);
    const size_t first = (room > 0 && len > (size_t)room) ? len - (size_t)room : 0;
    char shown[MAX_TEXT + 2];
    size_t n = 0;
    for (size_t i = first; i < len; i++)
    {
        shown[n++] = _password ? '*' : _text[i];
    }
    shown[n++] = '_'; // cursor
    shown[n] = 0;
    useGlcd(tft, 2);
    tft.setTextColor(_theme.text, _theme.surface);
    tft.drawString(shown, tx, (int16_t)(fy + FIELD_H / 2 + 1));
    restoreFont(tft);
    _fieldDirty = false;
}

void JWPLC_UIKeyboard::draw()
{
    draw(JWPLC_TFT);
}

void JWPLC_UIKeyboard::draw(JWPLC_TFTClass &tft)
{
    if (!_open)
    {
        return;
    }
    if (_full || _page != _drawnPage)
    {
        if (_full)
        {
            drawField(tft);
        }
        for (uint8_t r = 0; r < ROWS; r++)
        {
            const uint8_t n = r == 4 ? funcCount() : COLS;
            for (uint8_t c = 0; c < n; c++)
            {
                drawKey(tft, r, c);
            }
        }
    }
    else if (_row != _drawnRow || _col != _drawnCol)
    {
        drawKey(tft, _drawnRow, _drawnCol);
        drawKey(tft, _row, _col);
    }
    if (_fieldDirty)
    {
        drawField(tft);
    }
    _drawnRow = _row;
    _drawnCol = _col;
    _drawnPage = _page;
    _full = false;
}

// ================================================================ QR
// Codificador QR propio (modo byte, versiones 1 a 10, corrección L/M/Q/H).
// Sigue ISO/IEC 18004: datos + Reed-Solomon por bloques, patrones fijos,
// información de formato/versión y la máscara de menor penalización.

namespace
{
    const int8_t kEccPerBlock[4][11] = {
        {-1, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18},   // L
        {-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26},  // M
        {-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24},  // Q
        {-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28}}; // H
    const int8_t kNumBlocks[4][11] = {
        {-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4},
        {-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5},
        {-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8},
        {-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8}};
    // Bits de formato del nivel (L=01, M=00, Q=11, H=10).
    const uint8_t kFormatBits[4] = {1, 0, 3, 2};

    int qrRawModules(int ver)
    {
        int r = (16 * ver + 128) * ver + 64;
        if (ver >= 2)
        {
            const int n = ver / 7 + 2;
            r -= (25 * n - 10) * n - 55;
            if (ver >= 7)
            {
                r -= 36;
            }
        }
        return r;
    }

    int qrDataCodewords(int ver, int ecl)
    {
        return qrRawModules(ver) / 8 - kEccPerBlock[ecl][ver] * kNumBlocks[ecl][ver];
    }

    uint8_t gfMul(uint8_t x, uint8_t y)
    {
        int z = 0;
        for (int i = 7; i >= 0; i--)
        {
            z = (z << 1) ^ ((z >> 7) * 0x11D);
            z ^= ((y >> i) & 1) * x;
        }
        return (uint8_t)z;
    }

    // Divisor Reed-Solomon de grado `degree` (como en el estándar).
    void rsDivisor(int degree, uint8_t out[])
    {
        memset(out, 0, (size_t)degree);
        out[degree - 1] = 1;
        uint8_t root = 1;
        for (int i = 0; i < degree; i++)
        {
            for (int j = 0; j < degree; j++)
            {
                out[j] = gfMul(out[j], root);
                if (j + 1 < degree)
                {
                    out[j] ^= out[j + 1];
                }
            }
            root = gfMul(root, 0x02);
        }
    }

    void rsRemainder(const uint8_t data[], int len, const uint8_t div[], int degree, uint8_t out[])
    {
        memset(out, 0, (size_t)degree);
        for (int i = 0; i < len; i++)
        {
            const uint8_t f = data[i] ^ out[0];
            memmove(out, out + 1, (size_t)(degree - 1));
            out[degree - 1] = 0;
            for (int j = 0; j < degree; j++)
            {
                out[j] ^= gfMul(div[j], f);
            }
        }
    }
}

bool JWPLC_UIQR::getBit(const uint8_t grid[], int x, int y) const
{
    const int i = y * _size + x;
    return (grid[i >> 3] >> (i & 7)) & 1;
}

void JWPLC_UIQR::setBit(uint8_t grid[], int x, int y, bool v)
{
    const int i = y * _size + x;
    if (v)
    {
        grid[i >> 3] |= (uint8_t)(1 << (i & 7));
    }
    else
    {
        grid[i >> 3] &= (uint8_t)~(1 << (i & 7));
    }
}

void JWPLC_UIQR::setFunction(int x, int y, bool dark)
{
    setBit(_modules, x, y, dark);
    setBit(s_function, x, y, true);
}

void JWPLC_UIQR::drawFinder(int cx, int cy)
{
    for (int dy = -4; dy <= 4; dy++)
    {
        for (int dx = -4; dx <= 4; dx++)
        {
            const int x = cx + dx;
            const int y = cy + dy;
            if (x < 0 || y < 0 || x >= _size || y >= _size)
            {
                continue;
            }
            const int d = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
            setFunction(x, y, d != 2 && d != 4);
        }
    }
}

void JWPLC_UIQR::drawAlignment(int cx, int cy)
{
    for (int dy = -2; dy <= 2; dy++)
    {
        for (int dx = -2; dx <= 2; dx++)
        {
            const int d = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
            setFunction(cx + dx, cy + dy, d != 1);
        }
    }
}

void JWPLC_UIQR::drawFormat(int mask)
{
    const int data = (kFormatBits[_ecl] << 3) | mask;
    int rem = data;
    for (int i = 0; i < 10; i++)
    {
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    }
    const int bits = ((data << 10) | rem) ^ 0x5412;

    for (int i = 0; i <= 5; i++)
    {
        setFunction(8, i, (bits >> i) & 1);
    }
    setFunction(8, 7, (bits >> 6) & 1);
    setFunction(8, 8, (bits >> 7) & 1);
    setFunction(7, 8, (bits >> 8) & 1);
    for (int i = 9; i < 15; i++)
    {
        setFunction(14 - i, 8, (bits >> i) & 1);
    }
    for (int i = 0; i < 8; i++)
    {
        setFunction(_size - 1 - i, 8, (bits >> i) & 1);
    }
    for (int i = 8; i < 15; i++)
    {
        setFunction(8, _size - 15 + i, (bits >> i) & 1);
    }
    setFunction(8, _size - 8, true); // módulo oscuro fijo
}

void JWPLC_UIQR::drawFunctionPatterns()
{
    for (int i = 0; i < _size; i++)
    {
        setFunction(6, i, i % 2 == 0);
        setFunction(i, 6, i % 2 == 0);
    }
    drawFinder(3, 3);
    drawFinder(_size - 4, 3);
    drawFinder(3, _size - 4);

    if (_version >= 2)
    {
        const int n = _version / 7 + 2;
        const int step = (_version * 8 + n * 3 + 5) / (n * 4 - 4) * 2;
        int pos[7];
        pos[0] = 6;
        for (int i = n - 1, p = _size - 7; i >= 1; i--, p -= step)
        {
            pos[i] = p;
        }
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if ((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0))
                {
                    continue;
                }
                drawAlignment(pos[i], pos[j]);
            }
        }
    }

    drawFormat(0); // reserva el área; se reescribe con la máscara elegida

    if (_version >= 7)
    {
        int rem = _version;
        for (int i = 0; i < 12; i++)
        {
            rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
        }
        const long bits = ((long)_version << 12) | rem;
        for (int i = 0; i < 18; i++)
        {
            const bool b = (bits >> i) & 1;
            const int a = _size - 11 + i % 3;
            const int c = i / 3;
            setFunction(a, c, b);
            setFunction(c, a, b);
        }
    }
}

void JWPLC_UIQR::placeCodewords(const uint8_t data[], int len)
{
    int i = 0;
    for (int right = _size - 1; right >= 1; right -= 2)
    {
        if (right == 6)
        {
            right = 5;
        }
        for (int vert = 0; vert < _size; vert++)
        {
            for (int j = 0; j < 2; j++)
            {
                const int x = right - j;
                const bool upward = ((right + 1) & 2) == 0;
                const int y = upward ? _size - 1 - vert : vert;
                if (!getBit(s_function, x, y) && i < len * 8)
                {
                    setBit(_modules, x, y, (data[i >> 3] >> (7 - (i & 7))) & 1);
                    i++;
                }
            }
        }
    }
}

void JWPLC_UIQR::applyMask(int mask)
{
    for (int y = 0; y < _size; y++)
    {
        for (int x = 0; x < _size; x++)
        {
            bool inv;
            switch (mask)
            {
            case 0: inv = (x + y) % 2 == 0; break;
            case 1: inv = y % 2 == 0; break;
            case 2: inv = x % 3 == 0; break;
            case 3: inv = (x + y) % 3 == 0; break;
            case 4: inv = (x / 3 + y / 2) % 2 == 0; break;
            case 5: inv = x * y % 2 + x * y % 3 == 0; break;
            case 6: inv = (x * y % 2 + x * y % 3) % 2 == 0; break;
            default: inv = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
            }
            if (inv && !getBit(s_function, x, y))
            {
                setBit(_modules, x, y, !getBit(_modules, x, y));
            }
        }
    }
}

// Penalización de ISO/IEC 18004 §8.8.2 (N1 = 3, N2 = 3, N3 = 40, N4 = 10).
long JWPLC_UIQR::penalty() const
{
    long result = 0;
    // N1 y N3 por filas y por columnas.
    for (int pass = 0; pass < 2; pass++)
    {
        for (int a = 0; a < _size; a++)
        {
            int run = 0;
            bool prev = false;
            for (int b = 0; b < _size; b++)
            {
                const bool v = pass == 0 ? getBit(_modules, b, a) : getBit(_modules, a, b);
                if (b > 0 && v == prev)
                {
                    run++;
                }
                else
                {
                    if (run >= 5)
                    {
                        result += 3 + (run - 5);
                    }
                    run = 1;
                    prev = v;
                }
            }
            if (run >= 5)
            {
                result += 3 + (run - 5);
            }
            // 1:1:3:1:1 con 4 módulos claros a un lado (fuera = claro).
            for (int b = -4; b < _size; b++)
            {
                bool p[11];
                for (int k = 0; k < 11; k++)
                {
                    const int c = b + k;
                    p[k] = (c < 0 || c >= _size) ? false
                                                  : (pass == 0 ? getBit(_modules, c, a) : getBit(_modules, a, c));
                }
                const bool core = p[4] && !p[5] && p[6] && p[7] && p[8] && !p[9] && p[10];
                const bool core2 = p[0] && !p[1] && p[2] && p[3] && p[4] && !p[5] && p[6];
                if (core && !p[0] && !p[1] && !p[2] && !p[3])
                {
                    result += 40;
                }
                if (core2 && !p[7] && !p[8] && !p[9] && !p[10])
                {
                    result += 40;
                }
            }
        }
    }
    // N2: bloques 2x2 del mismo color.
    int dark = 0;
    for (int y = 0; y < _size; y++)
    {
        for (int x = 0; x < _size; x++)
        {
            const bool v = getBit(_modules, x, y);
            dark += v;
            if (x + 1 < _size && y + 1 < _size && v == getBit(_modules, x + 1, y) &&
                v == getBit(_modules, x, y + 1) && v == getBit(_modules, x + 1, y + 1))
            {
                result += 3;
            }
        }
    }
    // N4: proporción de oscuros.
    const long total = (long)_size * _size;
    const long k = (labs(dark * 20L - total * 10L) + total - 1) / total - 1;
    result += (k > 0 ? k : 0) * 10;
    return result;
}

bool JWPLC_UIQR::encode(const uint8_t text[], int len, int ecl, int forcedMask)
{
    _ecl = ecl;
    _version = 0;
    for (int v = 1; v <= MAX_VERSION; v++)
    {
        const int cap = qrDataCodewords(v, ecl) * 8;
        const int need = 4 + (v < 10 ? 8 : 16) + len * 8;
        if (need <= cap)
        {
            _version = v;
            break;
        }
    }
    if (_version == 0)
    {
        _size = 0;
        return false;
    }
    _size = _version * 4 + 17;

    // --- datos: modo byte, longitud, texto, terminador y relleno
    static uint8_t buf[MAX_CODEWORDS]; // estáticos: fuera de la pila de la tarea
    const int dataLen = qrDataCodewords(_version, ecl);
    memset(buf, 0, sizeof(buf));
    int bit = 0;
    auto put = [&](uint32_t val, int n) {
        for (int i = n - 1; i >= 0; i--, bit++)
        {
            if ((val >> i) & 1)
            {
                buf[bit >> 3] |= (uint8_t)(0x80 >> (bit & 7));
            }
        }
    };
    put(4, 4);
    put((uint32_t)len, _version < 10 ? 8 : 16);
    for (int i = 0; i < len; i++)
    {
        put(text[i], 8);
    }
    const int cap = dataLen * 8;
    put(0, (cap - bit) < 4 ? (cap - bit) : 4);
    put(0, (8 - bit % 8) % 8);
    for (uint8_t pad = 0xEC; bit < cap; pad ^= 0xEC ^ 0x11)
    {
        put(pad, 8);
    }

    // --- Reed-Solomon por bloques e intercalado
    const int numBlocks = kNumBlocks[ecl][_version];
    const int eccLen = kEccPerBlock[ecl][_version];
    const int raw = qrRawModules(_version) / 8;
    const int numShort = numBlocks - raw % numBlocks;
    const int shortLen = raw / numBlocks;
    uint8_t div[30];
    rsDivisor(eccLen, div);
    static uint8_t out[MAX_CODEWORDS];
    static uint8_t ecc[MAX_BLOCKS][30];
    int offs[MAX_BLOCKS];
    for (int b = 0, k = 0; b < numBlocks; b++)
    {
        const int dl = shortLen - eccLen + (b < numShort ? 0 : 1);
        offs[b] = k;
        rsRemainder(buf + k, dl, div, eccLen, ecc[b]);
        k += dl;
    }
    int o = 0;
    for (int i = 0; i <= shortLen - eccLen; i++)
    {
        for (int b = 0; b < numBlocks; b++)
        {
            const int dl = shortLen - eccLen + (b < numShort ? 0 : 1);
            if (i < dl)
            {
                out[o++] = buf[offs[b] + i];
            }
        }
    }
    for (int i = 0; i < eccLen; i++)
    {
        for (int b = 0; b < numBlocks; b++)
        {
            out[o++] = ecc[b][i];
        }
    }

    // --- matriz
    memset(_modules, 0, sizeof(_modules));
    memset(s_function, 0, sizeof(s_function));
    drawFunctionPatterns();
    placeCodewords(out, o);

    int best = forcedMask;
    if (best < 0 || best > 7)
    {
        long bestPenalty = 0x7FFFFFFF;
        for (int m = 0; m < 8; m++)
        {
            applyMask(m);
            drawFormat(m);
            const long p = penalty();
            if (p < bestPenalty)
            {
                bestPenalty = p;
                best = m;
            }
            applyMask(m); // deshace (XOR)
        }
    }
    applyMask(best);
    drawFormat(best);
    _mask = (uint8_t)best;
    return true;
}

uint8_t JWPLC_UIQR::s_function[(MAX_SIDE * MAX_SIDE + 7) / 8];

bool JWPLC_UIQR::setText(const char text[], uint8_t level)
{
    if (text == nullptr)
    {
        _size = 0;
        return false;
    }
    return encode((const uint8_t *)text, (int)strlen(text), level > QR_LEVEL_H ? QR_LEVEL_M : level, -1);
}

bool JWPLC_UIQR::module(uint8_t x, uint8_t y) const
{
    return x < _size && y < _size && getBit(_modules, x, y);
}

void JWPLC_UIQR::draw(int16_t x, int16_t y, uint8_t scale)
{
    if (_size == 0)
    {
        return;
    }
    if (scale == 0)
    {
        scale = 1;
    }
    JWPLC_TFTClass &tft = JWPLC_TFT;
    // Margen blanco de 4 módulos y módulos oscuros por tramos de cada fila.
    tft.fillRect(x, y, drawSize(scale), drawSize(scale), 0xFFFF);
    const int16_t ox = (int16_t)(x + 4 * scale);
    const int16_t oy = (int16_t)(y + 4 * scale);
    for (int my = 0; my < _size; my++)
    {
        int mx = 0;
        while (mx < _size)
        {
            if (!getBit(_modules, mx, my))
            {
                mx++;
                continue;
            }
            int run = 1;
            while (mx + run < _size && getBit(_modules, mx + run, my))
            {
                run++;
            }
            tft.fillRect((int16_t)(ox + mx * scale), (int16_t)(oy + my * scale),
                         (int16_t)(run * scale), scale, 0x0000);
            mx += run;
        }
    }
}
