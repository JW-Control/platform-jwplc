#pragma once

// JWPLC_UI_Widgets 0.3.0 (alpha.16): widgets de pantalla para el modo USER y
// para el Runtime de panel.
//
// Reglas comunes:
// - API solo con «.» (regla del propietario, 01/11): sin «::», «*» ni «&» en
//   el sketch. Los resultados son constantes globales (UI_EDIT_ACCEPTED,
//   UI_DIALOG_YES...) y draw() dibuja en JWPLC_TFT sin argumentos.
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

#define JWPLC_UI_WIDGETS_VERSION "0.3.0"

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
// Fila que la lista entrega a la función de la aplicación para que escriba
// sus dos columnas:
//
//   void filaParametro(uint16_t i, JWPLC_UIRow fila)
//   {
//       fila.left(nombres[i]);
//       fila.right(String(valores[i]) + " s");
//   }
class JWPLC_UIRow
{
public:
    void left(const char *text);   // columna izquierda
    void left(const String &text);
    void right(const char *text);  // columna derecha, alineada a la derecha
    void right(const String &text);

private:
    friend class JWPLC_UIList;
    JWPLC_UIRow(char *leftBuf, size_t leftSize, char *rightBuf, size_t rightSize)
        : _left(leftBuf), _leftSize(leftSize), _right(rightBuf), _rightSize(rightSize) {}
    char *_left;
    size_t _leftSize;
    char *_right;
    size_t _rightSize;
};

typedef void (*JWPLC_UIRowText)(uint16_t index, JWPLC_UIRow row);

// Lista con scroll: la aplicación entrega el texto de cada fila con una
// función (ver JWPLC_UIRow), así no hace falta guardar las filas en RAM.
class JWPLC_UIList
{
public:

    void begin(int16_t x, int16_t y, int16_t w, int16_t h,
               uint8_t rowHeight = 20,
               const JWPLC_UITheme &theme = JWPLC_UI_THEME_DARK);

    void setRows(uint16_t count, JWPLC_UIRowText rowText);
    uint16_t rowCount() const { return _count; }

    // ▲ ▼ mueven la selección (circular al pulsar, sin vuelta al mantener).
    // Devuelve true si la selección cambió.
    bool handleKeys();

    uint16_t selected() const { return _selected; }
    void setSelected(uint16_t index);

    void invalidate();               // redibuja todo en el próximo draw()
    void invalidateRow(uint16_t index); // p. ej. al cambiar un valor
    void draw();

    // Vuelve a pedir el texto de las filas visibles y redibuja solo las que
    // cambiaron (valores en vivo sin parpadeo del resto de la lista).
    void refreshChanged();

    uint8_t visibleRows() const { return _visible; }
    uint16_t firstVisible() const { return _top; }

private:
    void draw(JWPLC_TFTClass &tft);
    void drawRow(JWPLC_TFTClass &tft, uint16_t index);
    void drawScrollBar(JWPLC_TFTClass &tft);
    void ensureVisible();

    JWPLC_UITheme _theme = JWPLC_UI_THEME_DARK;
    JWPLC_UIRowText _rowText = nullptr;
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
    static const uint8_t HASH_ROWS = 12;
    uint32_t _rowHash[HASH_ROWS] = {0};
    uint32_t rowHash(uint16_t index, const char *left, const char *right) const;
};

// ---------------------------------------------------------------- editor
// Editor numérico por dígito. Se dibuja sobre theme.surface (un panel), como el de parametrización del LOGO!:
// ◄ ► eligen el dígito, ▲ ▼ lo cambian con acarreo (al mantener, el paso se
// acelera), OK acepta y ESC cancela y restaura el valor original.
// El valor es un entero escalado: 12.5 s con 1 decimal se guarda como 125.
// Resultado de JWPLC_UIDigitEditor.handleKeys().
enum JWPLC_UIEditResult : uint8_t
{
    UI_EDIT_NONE = 0,
    UI_EDIT_CHANGED,   // el valor cambió (sigue la edición)
    UI_EDIT_ACCEPTED,  // OK
    UI_EDIT_CANCELLED  // ESC: el valor vuelve al original
};

class JWPLC_UIDigitEditor
{
public:
    static const uint8_t MAX_DIGITS = 9;

    void begin(int16_t x, int16_t y,
               uint8_t digits, uint8_t decimals = 0,
               const char *unit = nullptr,
               const JWPLC_UITheme &theme = JWPLC_UI_THEME_DARK);

    void setRange(int32_t minValue, int32_t maxValue);
    void setValue(int32_t value); // también fija el valor que restaura ESC
    int32_t value() const { return _value; }
    uint8_t cursor() const { return _cursor; } // 0 = dígito de las unidades

    JWPLC_UIEditResult handleKeys();

    void invalidate();
    void draw();

    int16_t width() const;
    int16_t height() const { return CELL_H; }

private:
    void draw(JWPLC_TFTClass &tft);
    static const int16_t CELL_W = 18;
    static const int16_t CELL_H = 30;
    static const int16_t GAP = 2;
    static const int16_t POINT_W = 8;

    void format(char *cells) const; // un carácter por posición, signo incluido
    int16_t cellX(uint8_t pos) const;

    JWPLC_UITheme _theme = JWPLC_UI_THEME_DARK;
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
// Resultado de JWPLC_UIDialog.handleKeys().
enum JWPLC_UIDialogResult : uint8_t
{
    UI_DIALOG_NONE = 0,
    UI_DIALOG_YES, // OK sobre Sí/Aceptar
    UI_DIALOG_NO   // OK sobre No, o ESC
};

class JWPLC_UIDialog
{
public:
    void open(const char *title, const char *message, bool yesNo = true,
              const JWPLC_UITheme &theme = JWPLC_UI_THEME_DARK);
    void close() { _open = false; }
    bool isOpen() const { return _open; }
    // Deja el cursor en "No" (para confirmar acciones que borran o detienen).
    void selectNo() { _choice = _yesNo ? 1 : 0; }

    // ◄ ► eligen, OK confirma, ESC equivale a No. Cierra el diálogo al
    // devolver UI_DIALOG_YES o UI_DIALOG_NO.
    JWPLC_UIDialogResult handleKeys();
    void draw();

private:
    void draw(JWPLC_TFTClass &tft);
    void drawButtons(JWPLC_TFTClass &tft);

    JWPLC_UITheme _theme = JWPLC_UI_THEME_DARK;
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
    bool draw();

private:
    JWPLC_UITheme _theme = JWPLC_UI_THEME_DARK;
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
// El historial lo reserva el widget al dibujar o recibir la primera muestra:
// un byte por columna y canal (unos 300 B por canal a ancho completo).
//
//   tendencia.begin(0, 44, 320, 104, 0, 100);
//   tendencia.showScale(0);
//   tendencia.addChannel(NARANJA);
//   tendencia.addThreshold(85, ROJO);
//   ...
//   tendencia.push(temperatura);   // una muestra por canal
//   tendencia.draw();
class JWPLC_UITrend
{
public:
    static const uint8_t MAX_CHANNELS = 3;
    static const uint8_t MAX_THRESHOLDS = 2;

    ~JWPLC_UITrend();

    void begin(int16_t x, int16_t y, int16_t w, int16_t h,
               float minValue, float maxValue,
               const JWPLC_UITheme &theme = JWPLC_UI_THEME_DARK);

    // Escala vertical a la izquierda con los valores de la rejilla (0, 25,
    // 50, 75 y 100 % del rango) y de los umbrales. Ocupa 26 px del ancho.
    void showScale(uint8_t decimals = 0);

    // Canales en el orden de push(); el primero se dibuja encima.
    bool addChannel(uint16_t color);
    uint8_t channelCount() const { return _channels; }

    // Línea horizontal punteada (umbral o consigna).
    bool addThreshold(float value, uint16_t color);

    // Una muestra por canal, en el orden en que se agregaron.
    void push(float value1);
    void push(float value1, float value2);
    void push(float value1, float value2, float value3);
    float lastValue(uint8_t channel) const;

    void clear();      // borra el historial
    void invalidate(); // redibuja marco, rejilla e historial en el próximo draw()
    void draw();

    // Columnas del área de trazado (= muestras visibles en una pantalla).
    int16_t plotWidth() const { return (int16_t)(_w - 2 - (_scale ? SCALE_W : 0)); }

    // Muestreo, grosor y auto-escala configurables
    uint16_t sampleIntervalMs() const { return _sampleIntervalMs; }
    void setSampleIntervalMs(uint16_t ms) { _sampleIntervalMs = ms > 0 ? ms : 1000; }

    uint8_t lineWidth() const { return _lineWidth; }
    void setLineWidth(uint8_t w) { _lineWidth = (w == 1) ? 1 : 2; }

    bool autoScale() const { return _autoScale; }
    void setAutoScale(bool enable) { _autoScale = enable; }

    // Consulta histórica desde tarjeta MicroSD por diezmado en streaming.
    // Utiliza un buffer de salida pre-reservado para no saturar la RAM.
    bool querySDHistory(const char *filename, uint32_t startSec, uint32_t durationSec, float *outBuffer, uint16_t outPoints);

private:
    static const int16_t SCALE_W = 26;
    static const uint8_t GAP = 6;     // columnas que se borran por delante
    static const uint8_t NO_DATA = 0xFF;

    void draw(JWPLC_TFTClass &tft);
    bool ensureStorage();
    void releaseStorage();
    void pushValues(const float *values, uint8_t count);
    uint8_t toPixel(float v) const;
    void drawScale(JWPLC_TFTClass &tft);
    void scaleLabel(JWPLC_TFTClass &tft, float value, uint8_t pix, uint16_t color);
    int16_t plotX() const { return (int16_t)(_x + (_scale ? SCALE_W : 0)); }
    void eraseColumn(JWPLC_TFTClass &tft, int16_t col);
    void drawColumn(JWPLC_TFTClass &tft, int16_t col);
    int16_t plotH() const { return (int16_t)(_h - 2); }

    JWPLC_UITheme _theme = JWPLC_UI_THEME_DARK;
    int16_t _x = 0, _y = 0, _w = 0, _h = 0;
    float _min = 0, _max = 100;
    uint8_t _channels = 0;
    uint16_t _color[MAX_CHANNELS] = {0};
    uint8_t *_data[MAX_CHANNELS] = {nullptr};
    uint8_t *_storage = nullptr;
    float _last[MAX_CHANNELS] = {0};
    uint8_t _thresholds = 0;
    uint8_t _thrPix[MAX_THRESHOLDS] = {0};
    uint16_t _thrColor[MAX_THRESHOLDS] = {0};
    int16_t _pos = 0;      // próxima columna a escribir
    uint16_t _pending = 0; // muestras sin dibujar
    bool _scale = false;
    uint8_t _scaleDecimals = 0;
    float _thrValue[MAX_THRESHOLDS] = {0};
    bool _full = true;
    uint16_t _sampleIntervalMs = 1000;
    uint8_t _lineWidth = 1;
    bool _autoScale = true;
};

// ---------------------------------------------------------------- teclado
// Teclado en pantalla para nombres, textos y contraseñas. Ocupa todo el
// ancho desde `y` hasta abajo: campo de texto, cuatro filas de 10 teclas y
// una fila de funciones (mayúsculas, símbolos, espacio, borrar y OK).
//
//   teclado.begin(24);
//   teclado.open("Nombre", "Bomba 1", 16);
//   ...
//   uint8_t r = teclado.handleKeys();     // en jwplcUIUpdate()
//   if (r == UI_EDIT_ACCEPTED) Serial.println(teclado.text());
//   teclado.draw();
//
// ◄ ► ▲ ▼ mueven la tecla marcada, OK la pulsa y ESC cancela. handleKeys()
// devuelve las mismas constantes que el editor numérico.
class JWPLC_UIKeyboard
{
public:
    static const uint8_t MAX_TEXT = 32;

    void begin(int16_t y = 24, const JWPLC_UITheme &theme = JWPLC_UI_THEME_DARK);

    // password = true muestra «*» en lugar del texto.
    void open(const char title[], const char initial[] = "", uint8_t maxLength = 16,
              bool password = false);
    void close() { _open = false; }
    bool isOpen() const { return _open; }

    JWPLC_UIEditResult handleKeys();
    const char *text() const { return _text; }

    void invalidate() { _full = true; }
    void draw();

private:
    enum : uint8_t
    {
        PAGE_LOWER = 0,
        PAGE_UPPER,
        PAGE_SYMBOLS
    };
    static const uint8_t COLS = 10;
    static const uint8_t ROWS = 5; // 4 de caracteres + funciones
    static const int16_t KEY_H = 22;
    static const int16_t FIELD_H = 24;

    char keyChar(uint8_t row, uint8_t col) const;
    uint8_t funcCount() const { return 5; }
    void funcRect(uint8_t index, int16_t &x, int16_t &w) const;
    uint8_t colToFunc(uint8_t col) const;
    uint8_t funcToCol(uint8_t func) const;
    JWPLC_UIEditResult press();
    void draw(JWPLC_TFTClass &tft);
    void drawKey(JWPLC_TFTClass &tft, uint8_t row, uint8_t col);
    void drawField(JWPLC_TFTClass &tft);
    int16_t keysY() const { return (int16_t)(_y + FIELD_H + 4); }

    JWPLC_UITheme _theme = JWPLC_UI_THEME_DARK;
    int16_t _y = 24;
    const char *_title = "";
    char _text[MAX_TEXT + 1] = {0};
    char _original[MAX_TEXT + 1] = {0};
    uint8_t _maxLen = 16;
    bool _password = false;
    bool _open = false;
    uint8_t _page = PAGE_LOWER;
    uint8_t _row = 1;
    uint8_t _col = 0;
    uint8_t _drawnRow = 0xFF;
    uint8_t _drawnCol = 0xFF;
    uint8_t _drawnPage = 0xFF;
    bool _fieldDirty = true;
    bool _full = true;
};

// ---------------------------------------------------------------- QR
// Código QR (modo byte, versiones 1 a 10: hasta 271 caracteres en nivel L o
// 213 en nivel M). setText() lo calcula una vez; draw() lo dibuja con su
// margen blanco de 4 módulos, en negro sobre blanco para que lo lea
// cualquier celular.
//
//   qr.setText("jwplc://vincular?id=JWPLC-0001&codigo=482913");
//   qr.draw(100, 30, 3);   // 3 px por módulo
//
// Nivel de corrección: QR_LEVEL_L (7 %), QR_LEVEL_M (15 %, por defecto),
// QR_LEVEL_Q (25 %), QR_LEVEL_H (30 %).
enum JWPLC_UIQRLevel : uint8_t
{
    QR_LEVEL_L = 0,
    QR_LEVEL_M,
    QR_LEVEL_Q,
    QR_LEVEL_H
};

class JWPLC_UIQR
{
public:
    static const uint8_t MAX_VERSION = 10;

    // Devuelve false si el texto no cabe en la versión 10 con ese nivel.
    bool setText(const char text[], uint8_t level = QR_LEVEL_M);
    void clear() { _size = 0; }

    uint8_t version() const { return (uint8_t)_version; }
    uint8_t size() const { return (uint8_t)_size; } // módulos por lado (0 = vacío)
    bool module(uint8_t x, uint8_t y) const;       // true = oscuro

    // Lado total en píxeles con el margen, para centrarlo.
    int16_t drawSize(uint8_t scale) const { return (int16_t)((_size + 8) * scale); }
    void draw(int16_t x, int16_t y, uint8_t scale = 3);

private:
    static const int MAX_SIDE = MAX_VERSION * 4 + 17;
    static const int MAX_CODEWORDS = 346;
    static const int MAX_BLOCKS = 8;
    static uint8_t s_function[(MAX_SIDE * MAX_SIDE + 7) / 8];

    bool encode(const uint8_t text[], int len, int ecl, int forcedMask);
    bool getBit(const uint8_t grid[], int x, int y) const;
    void setBit(uint8_t grid[], int x, int y, bool v);
    void setFunction(int x, int y, bool dark);
    void drawFinder(int cx, int cy);
    void drawAlignment(int cx, int cy);
    void drawFormat(int mask);
    void drawFunctionPatterns();
    void placeCodewords(const uint8_t data[], int len);
    void applyMask(int mask);
    long penalty() const;

    uint8_t _modules[(MAX_SIDE * MAX_SIDE + 7) / 8] = {0};
    int _size = 0;
    int _version = 0;
    int _ecl = 0;
    uint8_t _mask = 0;
};
