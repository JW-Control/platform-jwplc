#pragma once

// JWPLC_UI_Widgets 0.1.0 (alpha.16): widgets de pantalla para el modo USER y
// para el Runtime de panel.
//
// Reglas comunes:
// - Dibujan solo lo que cambió (dirty refresh). La primera llamada a draw()
//   después de begin()/invalidate() dibuja todo el widget.
// - draw() se llama desde jwplcUIUpdate()/jwplcUIEnter(), que ya corren dentro
//   del batch de JWPLC_Display: no abren ni cierran el bus por su cuenta.
// - handleKeys() consume solo las teclas que el widget declara (regla Alpha8):
//     JWPLC_UIList          ▲ ▼ (con repetición)
//     JWPLC_UIDigitEditor   ◄ ► ▲ ▼ OK ESC
//     JWPLC_UIDialog        ◄ ► OK ESC
//   Si la pantalla usa ESC, configurar
//   JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED).
// - Fuentes: títulos, botones y dígitos en GLCD x2 (la de la IDLE); filas,
//   mensajes y avisos en JWPLC_FONT_16 (trazo de 1 px). Al terminar se deja
//   JWPLC_FONT_GLCD activa (JWPLC_Display dibuja en GLCD).

#include <Arduino.h>
#include <JWPLC_TFT.h>

#define JWPLC_UI_WIDGETS_VERSION "0.1.0"

// ---------------------------------------------------------------- tema
// Colores RGB565 compartidos por los widgets (y, más adelante, por el
// Runtime y el HMI Designer).
struct JWPLC_UITheme
{
    uint16_t background; // fondo de la pantalla
    uint16_t surface;    // fondo de filas, paneles y diálogos
    uint16_t surfaceAlt; // filas alternas
    uint16_t border;     // marcos y separadores
    uint16_t text;       // texto principal
    uint16_t textMuted;  // unidades, ayudas
    uint16_t accent;     // selección
    uint16_t accentText; // texto sobre la selección
    uint16_t ok;         // estado correcto, avisos de éxito
    uint16_t warn;       // advertencias
    uint16_t error;      // errores
    uint16_t header;     // barra de título (gris del título «JWPLC Basic» de la IDLE)
    uint16_t headerText; // texto de la barra de título
};

extern const JWPLC_UITheme JWPLC_UI_THEME_DARK;

// ---------------------------------------------------------------- lista
// Lista con scroll: la aplicación entrega el texto de cada fila con un
// callback (columna izquierda y columna derecha alineada a la derecha), así
// no hace falta guardar las filas en RAM.
class JWPLC_UIList
{
public:
    typedef void (*RowText)(uint16_t index,
                            char *left, size_t leftSize,
                            char *right, size_t rightSize,
                            void *context);

    void begin(int16_t x, int16_t y, int16_t w, int16_t h,
               uint8_t rowHeight = 20,
               const JWPLC_UITheme &theme = JWPLC_UI_THEME_DARK);

    void setRows(uint16_t count, RowText rowText, void *context = nullptr);
    uint16_t rowCount() const { return _count; }

    // ▲ ▼ mueven la selección (circular al pulsar, sin vuelta al mantener).
    // Devuelve true si la selección cambió.
    bool handleKeys();

    uint16_t selected() const { return _selected; }
    void setSelected(uint16_t index);

    void invalidate();               // redibuja todo en el próximo draw()
    void invalidateRow(uint16_t index); // p. ej. al cambiar un valor
    void draw(JWPLC_TFTClass &tft);

    uint8_t visibleRows() const { return _visible; }
    uint16_t firstVisible() const { return _top; }

private:
    void drawRow(JWPLC_TFTClass &tft, uint16_t index);
    void drawScrollBar(JWPLC_TFTClass &tft);
    void ensureVisible();

    const JWPLC_UITheme *_theme = &JWPLC_UI_THEME_DARK;
    RowText _rowText = nullptr;
    void *_context = nullptr;
    int16_t _x = 0, _y = 0, _w = 0, _h = 0;
    uint8_t _rowH = 20;
    uint8_t _visible = 0;
    uint16_t _count = 0;
    uint16_t _selected = 0;
    uint16_t _top = 0;
    uint16_t _drawnSelected = 0xFFFF;
    uint16_t _drawnTop = 0xFFFF;
    uint16_t _dirtyRow = 0xFFFF;
    bool _full = true;
};

// ---------------------------------------------------------------- editor
// Editor numérico por dígito. Se dibuja sobre theme.surface (un panel), como el de parametrización del LOGO!:
// ◄ ► eligen el dígito, ▲ ▼ lo cambian con acarreo (al mantener, el paso se
// acelera), OK acepta y ESC cancela y restaura el valor original.
// El valor es un entero escalado: 12.5 s con 1 decimal se guarda como 125.
class JWPLC_UIDigitEditor
{
public:
    enum Result : uint8_t
    {
        NONE = 0,
        CHANGED,
        ACCEPTED,
        CANCELLED
    };

    static const uint8_t MAX_DIGITS = 9;

    void begin(int16_t x, int16_t y,
               uint8_t digits, uint8_t decimals = 0,
               const char *unit = nullptr,
               const JWPLC_UITheme &theme = JWPLC_UI_THEME_DARK);

    void setRange(int32_t minValue, int32_t maxValue);
    void setValue(int32_t value); // también fija el valor que restaura ESC
    int32_t value() const { return _value; }
    uint8_t cursor() const { return _cursor; } // 0 = dígito de las unidades

    Result handleKeys();

    void invalidate();
    void draw(JWPLC_TFTClass &tft);

    int16_t width() const;
    int16_t height() const { return CELL_H; }

private:
    static const int16_t CELL_W = 18;
    static const int16_t CELL_H = 30;
    static const int16_t GAP = 2;
    static const int16_t POINT_W = 8;

    void format(char *cells) const; // un carácter por posición, signo incluido
    int16_t cellX(uint8_t pos) const;

    const JWPLC_UITheme *_theme = &JWPLC_UI_THEME_DARK;
    const char *_unit = nullptr;
    int16_t _x = 0, _y = 0;
    uint8_t _digits = 1;
    uint8_t _decimals = 0;
    uint8_t _cursor = 0;
    int32_t _min = 0, _max = 0;
    int32_t _value = 0;
    int32_t _original = 0;
    char _drawn[MAX_DIGITS + 2] = {0};
    uint8_t _drawnCursor = 0xFF;
    bool _full = true;
};

// ---------------------------------------------------------------- diálogo
// Diálogo modal centrado: Sí/No o solo Aceptar. Al cerrarse, la aplicación
// debe invalidar lo que quedó debajo (p. ej. list.invalidate()).
class JWPLC_UIDialog
{
public:
    enum Result : uint8_t
    {
        NONE = 0,
        YES,
        NO
    };

    void open(const char *title, const char *message, bool yesNo = true,
              const JWPLC_UITheme &theme = JWPLC_UI_THEME_DARK);
    void close() { _open = false; }
    bool isOpen() const { return _open; }

    // ◄ ► eligen, OK confirma, ESC equivale a No. Cierra el diálogo al
    // devolver YES o NO.
    Result handleKeys();
    void draw(JWPLC_TFTClass &tft);

private:
    void drawButtons(JWPLC_TFTClass &tft);

    const JWPLC_UITheme *_theme = &JWPLC_UI_THEME_DARK;
    const char *_title = "";
    const char *_message = "";
    bool _yesNo = true;
    bool _open = false;
    bool _full = true;
    uint8_t _choice = 0; // 0 = Sí/Aceptar, 1 = No
    uint8_t _drawnChoice = 0xFF;
};

// ---------------------------------------------------------------- aviso
// Aviso temporal en una franja que la pantalla reserva (por ejemplo, el pie).
class JWPLC_UIToast
{
public:
    void begin(int16_t x, int16_t y, int16_t w, int16_t h,
               const JWPLC_UITheme &theme = JWPLC_UI_THEME_DARK);
    void show(const char *text, uint32_t durationMs = 1500, uint16_t color = 0);
    bool visible() const { return _visible; }

    // Dibuja el aviso al mostrarlo y borra la franja al vencer. Devuelve true
    // en el ciclo en que se borró, para que la pantalla redibuje esa franja.
    bool draw(JWPLC_TFTClass &tft);

private:
    const JWPLC_UITheme *_theme = &JWPLC_UI_THEME_DARK;
    int16_t _x = 0, _y = 0, _w = 0, _h = 0;
    char _text[40] = {0};
    uint16_t _color = 0;
    uint32_t _until = 0;
    bool _visible = false;
    bool _pendingDraw = false;
};

// ---------------------------------------------------------------- tendencia
// Gráfica de tendencia con barrido (como un registrador): un cursor avanza de
// izquierda a derecha y cada muestra nueva dibuja solo su columna y borra un
// hueco por delante. Nunca se redibuja toda la gráfica por muestra.
// La RAM de cada canal la pone la aplicación: un uint8_t por columna
// (plotWidth() bytes).
class JWPLC_UITrend
{
public:
    static const uint8_t MAX_CHANNELS = 3;
    static const uint8_t MAX_THRESHOLDS = 2;

    void begin(int16_t x, int16_t y, int16_t w, int16_t h,
               float minValue, float maxValue,
               const JWPLC_UITheme &theme = JWPLC_UI_THEME_DARK);

    // storage: plotWidth() bytes que el widget usa como historial del canal.
    // El primer canal agregado se dibuja encima de los demás.
    bool addChannel(uint16_t color, uint8_t *storage, size_t storageSize);
    uint8_t channelCount() const { return _channels; }

    // Línea horizontal punteada (umbral o consigna).
    bool addThreshold(float value, uint16_t color);

    // Agrega una muestra por canal (values[0..channelCount()-1]).
    void push(const float *values);
    float lastValue(uint8_t channel) const;

    void clear();      // borra el historial
    void invalidate(); // redibuja marco, rejilla e historial en el próximo draw()
    void draw(JWPLC_TFTClass &tft);

    int16_t plotWidth() const { return (int16_t)(_w - 2); }

private:
    static const uint8_t GAP = 6;     // columnas que se borran por delante
    static const uint8_t NO_DATA = 0xFF;

    uint8_t toPixel(float v) const;
    void eraseColumn(JWPLC_TFTClass &tft, int16_t col);
    void drawColumn(JWPLC_TFTClass &tft, int16_t col);
    int16_t plotH() const { return (int16_t)(_h - 2); }

    const JWPLC_UITheme *_theme = &JWPLC_UI_THEME_DARK;
    int16_t _x = 0, _y = 0, _w = 0, _h = 0;
    float _min = 0, _max = 100;
    uint8_t _channels = 0;
    uint16_t _color[MAX_CHANNELS] = {0};
    uint8_t *_data[MAX_CHANNELS] = {nullptr};
    float _last[MAX_CHANNELS] = {0};
    uint8_t _thresholds = 0;
    uint8_t _thrPix[MAX_THRESHOLDS] = {0};
    uint16_t _thrColor[MAX_THRESHOLDS] = {0};
    int16_t _pos = 0;      // próxima columna a escribir
    uint16_t _pending = 0; // muestras sin dibujar
    bool _full = true;
};
