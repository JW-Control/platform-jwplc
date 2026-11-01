#include "JWPLC_UI_Widgets.h"

#include <JWPLC_GlobalPeripherals.h>
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
    _theme = &theme;
    invalidate();
}

void JWPLC_UIList::setRows(uint16_t count, RowText rowText, void *context)
{
    _count = count;
    _rowText = rowText;
    _context = context;
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
        tft.fillRect(_x, rowY, rowW, _rowH, _theme->background);
        return;
    }

    const bool sel = index == _selected;
    const uint16_t bg = sel ? _theme->accent : ((index & 1) ? _theme->surfaceAlt : _theme->surface);
    tft.fillRect(_x, rowY, rowW, (int16_t)(_rowH - 1), bg);
    tft.drawFastHLine(_x, (int16_t)(rowY + _rowH - 1), rowW, _theme->background);

    char left[32] = {0};
    char right[24] = {0};
    if (_rowText)
    {
        _rowText(index, left, sizeof(left), right, sizeof(right), _context);
    }

    const int16_t midY = (int16_t)(rowY + (_rowH - 1) / 2);
    tft.setFont(JWPLC_FONT_SANS_9);
    tft.setTextPadding(0);
    tft.setTextColor(sel ? _theme->accentText : _theme->text, bg);
    tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
    tft.drawString(left, (int16_t)(_x + 6), midY);
    if (right[0])
    {
        tft.setTextColor(sel ? _theme->accentText : _theme->textMuted, bg);
        tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_RIGHT);
        tft.drawString(right, (int16_t)(_x + rowW - 6), midY);
    }
    restoreFont(tft);
}

void JWPLC_UIList::drawScrollBar(JWPLC_TFTClass &tft)
{
    const int16_t bx = (int16_t)(_x + _w - 3);
    const int16_t trackH = (int16_t)(_visible * _rowH);
    tft.fillRect(bx, _y, 3, trackH, _theme->surface);
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
    tft.fillRect(bx, thumbY, 3, thumbH, _theme->textMuted);
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
            tft.fillRect(_x, (int16_t)(_y + used), _w, (int16_t)(_h - used), _theme->background);
        }
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
    _theme = &theme;
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

JWPLC_UIDigitEditor::Result JWPLC_UIDigitEditor::handleKeys()
{
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        _original = _value;
        return ACCEPTED;
    }
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        _value = _original;
        return CANCELLED;
    }

    Result r = NONE;

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
            r = CHANGED;
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

void JWPLC_UIDigitEditor::draw(JWPLC_TFTClass &tft)
{
    char cells[MAX_DIGITS + 2];
    format(cells);
    const uint8_t signCells = _min < 0 ? 1 : 0;

    tft.setFont(JWPLC_FONT_MONO_BOLD_12);
    tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_CENTER);
    tft.setTextPadding(0);

    if (_full)
    {
        // Fondo, punto decimal y unidad: estáticos mientras se edita.
        const int16_t w = width();
        tft.fillRect((int16_t)(_x - 2), (int16_t)(_y - 2), (int16_t)(w + 4), (int16_t)(CELL_H + 4), _theme->surface);
        if (_decimals)
        {
            const int16_t px = (int16_t)(cellX(_decimals) + CELL_W + 2);
            tft.fillRect(px, (int16_t)(_y + CELL_H - 7), 3, 3, _theme->text);
        }
        if (_unit && _unit[0])
        {
            tft.setFont(JWPLC_FONT_SANS_9);
            tft.setTextColor(_theme->textMuted, _theme->surface);
            tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
            tft.drawString(_unit, (int16_t)(_x + w + 8), (int16_t)(_y + CELL_H / 2));
            tft.setFont(JWPLC_FONT_MONO_BOLD_12);
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
        const uint16_t bg = cur ? _theme->accent : _theme->surfaceAlt;
        tft.fillRoundRect(cx, _y, CELL_W, CELL_H, 3, bg);
        const char s[2] = {cells[i], 0};
        tft.setTextColor(cur ? _theme->accentText : _theme->text, bg);
        tft.drawString(s, (int16_t)(cx + CELL_W / 2), (int16_t)(_y + CELL_H / 2));
        if (cur)
        {
            // Marcas ▲▼ de que este dígito se edita (después del texto).
            tft.fillTriangle((int16_t)(cx + CELL_W / 2), (int16_t)(_y + 2),
                             (int16_t)(cx + CELL_W / 2 - 3), (int16_t)(_y + 5),
                             (int16_t)(cx + CELL_W / 2 + 3), (int16_t)(_y + 5), _theme->accentText);
            tft.fillTriangle((int16_t)(cx + CELL_W / 2), (int16_t)(_y + CELL_H - 3),
                             (int16_t)(cx + CELL_W / 2 - 3), (int16_t)(_y + CELL_H - 6),
                             (int16_t)(cx + CELL_W / 2 + 3), (int16_t)(_y + CELL_H - 6), _theme->accentText);
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
    const int16_t DLG_W = 240;
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
    _theme = &theme;
    _choice = 0;
    _drawnChoice = 0xFF;
    _open = true;
    _full = true;
}

JWPLC_UIDialog::Result JWPLC_UIDialog::handleKeys()
{
    if (!_open)
    {
        return NONE;
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
        return (_choice == 0) ? YES : NO;
    }
    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        _open = false;
        return NO;
    }
    return NONE;
}

void JWPLC_UIDialog::drawButtons(JWPLC_TFTClass &tft)
{
    const int16_t by = (int16_t)(DLG_Y + DLG_H - BTN_H - 10);
    const uint8_t count = _yesNo ? 2 : 1;
    const char *labels[2] = {_yesNo ? "Si" : "Aceptar", "No"};
    tft.setFont(JWPLC_FONT_SANS_BOLD_9);
    tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_CENTER);
    for (uint8_t i = 0; i < count; i++)
    {
        const int16_t bx = count == 1
                               ? (int16_t)(DLG_X + (DLG_W - BTN_W) / 2)
                               : (int16_t)(DLG_X + DLG_W / 2 + (i == 0 ? -BTN_W - 8 : 8));
        const bool sel = i == _choice;
        const uint16_t bg = sel ? _theme->accent : _theme->surfaceAlt;
        tft.fillRoundRect(bx, by, BTN_W, BTN_H, 4, bg);
        if (!sel)
        {
            tft.drawRoundRect(bx, by, BTN_W, BTN_H, 4, _theme->border);
        }
        tft.setTextColor(sel ? _theme->accentText : _theme->text, bg);
        tft.drawString(labels[i], (int16_t)(bx + BTN_W / 2), (int16_t)(by + BTN_H / 2));
    }
    restoreFont(tft);
}

void JWPLC_UIDialog::draw(JWPLC_TFTClass &tft)
{
    if (!_open)
    {
        return;
    }
    if (_full)
    {
        tft.fillRoundRect(DLG_X, DLG_Y, DLG_W, DLG_H, 6, _theme->surface);
        tft.drawRoundRect(DLG_X, DLG_Y, DLG_W, DLG_H, 6, _theme->accent);
        tft.drawFastHLine((int16_t)(DLG_X + 8), (int16_t)(DLG_Y + 26), (int16_t)(DLG_W - 16), _theme->border);

        tft.setFont(JWPLC_FONT_SANS_BOLD_9);
        tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
        tft.setTextColor(_theme->text, _theme->surface);
        tft.drawString(_title, (int16_t)(DLG_X + 10), (int16_t)(DLG_Y + 14));

        tft.setFont(JWPLC_FONT_SANS_9);
        tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_CENTER);
        tft.setTextColor(_theme->text, _theme->surface);
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
    _theme = &theme;
}

void JWPLC_UIToast::show(const char *text, uint32_t durationMs, uint16_t color)
{
    strncpy(_text, text ? text : "", sizeof(_text) - 1);
    _text[sizeof(_text) - 1] = 0;
    _color = color ? color : _theme->ok;
    _until = millis() + durationMs;
    _visible = true;
    _pendingDraw = true;
}

bool JWPLC_UIToast::draw(JWPLC_TFTClass &tft)
{
    if (!_visible)
    {
        return false;
    }
    if ((int32_t)(millis() - _until) >= 0)
    {
        tft.fillRect(_x, _y, _w, _h, _theme->background);
        _visible = false;
        _pendingDraw = false;
        return true;
    }
    if (_pendingDraw)
    {
        tft.fillRoundRect(_x, _y, _w, _h, 4, _theme->surface);
        tft.fillRect(_x, _y, 4, _h, _color);
        tft.setFont(JWPLC_FONT_SANS_9);
        tft.setTextDatum(JWPLC_TFTDatum::MIDDLE_LEFT);
        tft.setTextColor(_theme->text, _theme->surface);
        tft.drawString(_text, (int16_t)(_x + 12), (int16_t)(_y + _h / 2));
        restoreFont(tft);
        _pendingDraw = false;
    }
    return false;
}
