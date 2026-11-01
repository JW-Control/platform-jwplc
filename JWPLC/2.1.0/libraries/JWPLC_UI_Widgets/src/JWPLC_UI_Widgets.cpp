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
    tft.fillRect(_x, rowY, rowW, (int16_t)(_rowH - 1), bg);
    tft.drawFastHLine(_x, (int16_t)(rowY + _rowH - 1), rowW, _theme.background);

    char left[32] = {0};
    char right[24] = {0};
    if (_rowText)
    {
        _rowText(index, JWPLC_UIRow(left, sizeof(left), right, sizeof(right)));
    }

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
    for (uint8_t i = 0; i < count; i++)
    {
        const int16_t bx = count == 1
                               ? (int16_t)(DLG_X + (DLG_W - BTN_W) / 2)
                               : (int16_t)(DLG_X + DLG_W / 2 + (i == 0 ? -BTN_W - 8 : 8));
        const bool sel = i == _choice;
        const uint16_t bg = sel ? _theme.accent : _theme.surfaceAlt;
        tft.fillRoundRect(bx, by, BTN_W, BTN_H, 4, bg);
        if (!sel)
        {
            tft.drawRoundRect(bx, by, BTN_W, BTN_H, 4, _theme.border);
        }
        tft.setTextColor(sel ? _theme.accentText : _theme.text, bg);
        tft.drawString(labels[i], (int16_t)(bx + BTN_W / 2), (int16_t)(by + BTN_H / 2));
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
