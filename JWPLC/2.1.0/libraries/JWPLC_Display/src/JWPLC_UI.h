#ifndef JWPLC_UI_H
#define JWPLC_UI_H

#include <Arduino.h>
#include <stddef.h>

class JWPLC_TFTClass;

static constexpr int16_t JWPLC_UI_AUTO = -1;
static constexpr uint8_t JWPLC_UI_MAX_FIELDS = 32;

enum JWPLC_UIFieldType : uint8_t
{
    JWPLC_UI_FIELD_VALUE = 0,
    JWPLC_UI_FIELD_TEXT,
    JWPLC_UI_FIELD_BOOL,
    JWPLC_UI_FIELD_BAR,
    JWPLC_UI_FIELD_ICON,
    JWPLC_UI_FIELD_CIRCULAR
};

enum JWPLC_UIFieldLayout : uint8_t
{
    JWPLC_UI_LAYOUT_INLINE = 0,
    JWPLC_UI_LAYOUT_STACKED
};

enum JWPLC_UIValueAlign : uint8_t
{
    JWPLC_UI_ALIGN_LEFT = 0,
    JWPLC_UI_ALIGN_CENTER,
    JWPLC_UI_ALIGN_RIGHT
};

enum JWPLC_UIRefreshMode : uint8_t
{
    USER_REFRESH_ON_DEMAND = 0,
    USER_REFRESH_PERIODIC
};

// Los grupos mantienen exactamente la API publica Alpha8, pero sus cuerpos
// viven en JWPLC_UI_API.cpp. Esto reduce el trabajo de parseo/instanciacion del
// sketch sin cambiar nombres, parametros ni inicializadores {...}.
struct JWPLC_UIFieldMeta
{
    uint8_t id;
    uint8_t page;
    JWPLC_UIFieldType type;

    JWPLC_UIFieldMeta(
        uint8_t idValue = 0,
        uint8_t pageValue = 0,
        JWPLC_UIFieldType typeValue = JWPLC_UI_FIELD_VALUE);
};

struct JWPLC_UIRect
{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;

    JWPLC_UIRect(
        int16_t xValue = 0,
        int16_t yValue = 0,
        int16_t widthValue = JWPLC_UI_AUTO,
        int16_t heightValue = JWPLC_UI_AUTO);
};

struct JWPLC_UIText
{
    const char *label;
    const char *unit;
    uint8_t capacity;

    JWPLC_UIText(
        const char *labelValue = nullptr,
        const char *unitValue = nullptr,
        uint8_t capacityValue = 12);
};

struct JWPLC_UIColors
{
    uint16_t label;
    uint16_t value;
    uint16_t background;
    uint16_t frame;

    JWPLC_UIColors(
        uint16_t labelValue = 0xFFFF,
        uint16_t valueValue = 0xFFFF,
        uint16_t backgroundValue = 0x0000,
        uint16_t frameValue = 0xFFFF);
};

struct JWPLC_UIStyle
{
    uint8_t labelTextSize;
    uint8_t valueTextSize;
    bool frame;
    JWPLC_UIFieldLayout layout;
    JWPLC_UIValueAlign valueAlign;

    // Mirror interno para el motor Alpha8. El usuario configura colors como
    // grupo propio de JWPLC_UIField, no anidado dentro de style.
    JWPLC_UIColors colors;

    JWPLC_UIStyle(
        uint8_t labelSizeValue = 1,
        uint8_t valueSizeValue = 2,
        bool frameValue = false,
        JWPLC_UIFieldLayout layoutValue = JWPLC_UI_LAYOUT_INLINE,
        JWPLC_UIValueAlign alignValue = JWPLC_UI_ALIGN_RIGHT,
        JWPLC_UIColors colorsValue = JWPLC_UIColors());
};

// signedValue=true permite negativos y reserva espacio para '-'.
//
// Alpha15 (I1): además del número, VALUE sabe mostrar duraciones y el
// separador de miles. El formateo vive en JWPLC_UI_Format.cpp.
//
//   JWPLC_UIValueFormat(6, 0, false, false, ',')   -> 123,456
//   JWPLC_UIValueFormat(6, 2, false, false, '.')   -> 123.456,78
//   JWPLC_UIDurationFormat(JWPLC_UI_DURATION_MM_SS) -> 05:03 (variable en s)
//
// Duraciones: la variable trae segundos o milisegundos (lo calcula el PLC; VALUE
// sólo formatea, truncando a segundos). integerDigits es el ancho de la primera
// unidad (SS, MM o HH): fija la reserva y el máximo; si no cabe se ve --:--.
//
// Alpha15 (I2): fecha/hora del RTC de la placa. El campo no tiene variable:
// el firmware lee jwplcGetRTCState() y repinta sólo cuando cambia el texto
// mostrado (el minuto en HH:MM, el segundo en HH:MM:SS). Sin RTC válido se
// ve --:--.
//   JWPLC_UIDateTimeFormat(JWPLC_UI_DATETIME_HH_MM)        -> 14:35
//   JWPLC_UIDateTimeFormat(JWPLC_UI_DATETIME_DD_MM_YYYY)   -> 18/10/2026
#define JWPLC_UI_HAS_VALUE_DURATION 1
#define JWPLC_UI_HAS_VALUE_THOUSANDS 1
#define JWPLC_UI_HAS_VALUE_DATETIME 1

enum JWPLC_UIValueKind : uint8_t
{
    JWPLC_UI_VALUE_NUMBER = 0,
    JWPLC_UI_VALUE_DURATION,
    JWPLC_UI_VALUE_DATETIME
};

enum JWPLC_UIDateTimePattern : uint8_t
{
    JWPLC_UI_DATETIME_HH_MM = 0,       // 14:35
    JWPLC_UI_DATETIME_HH_MM_SS,        // 14:35:07
    JWPLC_UI_DATETIME_DD_MM,           // 18/10
    JWPLC_UI_DATETIME_DD_MM_YYYY,      // 18/10/2026
    JWPLC_UI_DATETIME_DD_MM_YYYY_HH_MM // 18/10/2026 14:35
};

enum JWPLC_UIDurationPattern : uint8_t
{
    JWPLC_UI_DURATION_SS = 0,   // 45      segundos totales
    JWPLC_UI_DURATION_MM_SS,    // 05:03   minutos totales : segundos
    JWPLC_UI_DURATION_HH_MM_SS, // 01:05:03
    JWPLC_UI_DURATION_COMPACT   // 1h05m · 5m03s · 45s
};

enum JWPLC_UIDurationUnit : uint8_t
{
    JWPLC_UI_DURATION_SECONDS = 0,
    JWPLC_UI_DURATION_MILLIS
};

struct JWPLC_UIValueFormat
{
    uint8_t integerDigits;
    uint8_t decimalDigits;
    bool signedValue;
    bool leadingZeros;
    char thousandsSeparator; // 0 = sin separador; ',' '.' o ' '. Con '.' el decimal es ','
    JWPLC_UIValueKind kind;
    JWPLC_UIDurationPattern durationPattern;
    JWPLC_UIDurationUnit durationUnit;
    JWPLC_UIDateTimePattern dateTimePattern;

    // Constructor de 4 argumentos de Alpha11/12: se conserva para que el .a
    // mantenga su símbolo (ABI_COMPAT). Equivale a thousandsSeparator = 0.
    JWPLC_UIValueFormat(
        uint8_t integerDigitsValue = 3,
        uint8_t decimalDigitsValue = 0,
        bool signedValueValue = false,
        bool leadingZerosValue = false);

    JWPLC_UIValueFormat(
        uint8_t integerDigitsValue,
        uint8_t decimalDigitsValue,
        bool signedValueValue,
        bool leadingZerosValue,
        char thousandsSeparatorValue);
};

// Formato de duración para JWPLC_UIValueField (setValue con segundos o ms).
JWPLC_UIValueFormat JWPLC_UIDurationFormat(
    JWPLC_UIDurationPattern pattern = JWPLC_UI_DURATION_MM_SS,
    JWPLC_UIDurationUnit unit = JWPLC_UI_DURATION_SECONDS,
    uint8_t leadDigits = 2,
    bool signedValue = false);

// Fecha/hora del RTC para JWPLC_UIValueField (sin variable ni setValue).
JWPLC_UIValueFormat JWPLC_UIDateTimeFormat(
    JWPLC_UIDateTimePattern pattern = JWPLC_UI_DATETIME_HH_MM);

// Alpha15: colores del valor por estado.
//   (falso, verdadero)                          -> colors.value / colors.background
//   (falso, verdadero, colorF, colorV)          -> texto por estado
//   (falso, verdadero, colorF, colorV, fondoF, fondoV) -> texto por estado y
//                                                  fondo de todo el campo por
//                                                  estado (el marco no cambia)
#define JWPLC_UI_HAS_BOOL_STATE_COLORS 1
#define JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS 1

static constexpr uint8_t JWPLC_UI_BOOL_STATE_TEXT = 0x01;
static constexpr uint8_t JWPLC_UI_BOOL_STATE_BACKGROUND = 0x02;

// Alpha15 (I1): representación del valor. PILOT dibuja un piloto (círculo de
// 7 × escala px, a la altura del texto) con aro del color de la etiqueta y
// relleno del color del estado. Se crea con JWPLC_UIBoolPilot().
#define JWPLC_UI_HAS_BOOL_PILOT 1

enum JWPLC_UIBoolRepresentation : uint8_t
{
    JWPLC_UI_BOOL_AS_TEXT = 0,
    JWPLC_UI_BOOL_AS_PILOT
};

struct JWPLC_UIBoolText
{
    const char *falseText;
    const char *trueText;
    uint16_t falseColor;
    uint16_t trueColor;
    uint16_t falseBackground;
    uint16_t trueBackground;
    uint8_t stateColors; // JWPLC_UI_BOOL_STATE_TEXT | JWPLC_UI_BOOL_STATE_BACKGROUND
    JWPLC_UIBoolRepresentation representation;

    JWPLC_UIBoolText(
        const char *falseTextValue = "OFF",
        const char *trueTextValue = "ON");

    JWPLC_UIBoolText(
        const char *falseTextValue,
        const char *trueTextValue,
        uint16_t falseColorValue,
        uint16_t trueColorValue);

    JWPLC_UIBoolText(
        const char *falseTextValue,
        const char *trueTextValue,
        uint16_t falseColorValue,
        uint16_t trueColorValue,
        uint16_t falseBackgroundValue,
        uint16_t trueBackgroundValue);
};

// Piloto: color del círculo por estado y, opcional, fondo del campo por estado.
JWPLC_UIBoolText JWPLC_UIBoolPilot(
    uint16_t falseColor,
    uint16_t trueColor);

JWPLC_UIBoolText JWPLC_UIBoolPilot(
    uint16_t falseColor,
    uint16_t trueColor,
    uint16_t falseBackground,
    uint16_t trueBackground);

// Alpha16: ICONO con estado. Una imagen por estado (motor parado / en
// marcha / falla...), elegida con setIconState() o setBool(); sin animación.
//
// Formato de cada imagen: 4 bits por píxel, índice en una paleta de 16
// colores RGB565. Dos píxeles por byte (el de la izquierda en el nibble alto)
// y cada fila empieza en un byte nuevo: (width + 1) / 2 bytes por fila.
// transparentIndex: ese índice toma el fondo del campo
// (JWPLC_UI_ICON_NO_TRANSPARENT = ninguno). Varios estados pueden compartir
// los píxeles y cambiar sólo la paleta. Todo es const: vive en flash.
//
//   static const uint8_t  MOTOR_PX[] = { ... };              // 32x32 -> 512 B
//   static const uint16_t MOTOR_OFF[16] = { ... }, MOTOR_ON[16] = { ... };
//   static const JWPLC_UIIconState MOTOR_STATES[] = { { MOTOR_PX, MOTOR_OFF }, { MOTOR_PX, MOTOR_ON } };
//   static const JWPLC_UIIcon MOTOR(32, 32, 2, MOTOR_STATES, 0);
//   JWPLC_UIIconField(ID_MOTOR, JWPLC_UIRect(10, 20), &MOTOR, JWPLC_UIText("Motor"))
//   JWPLC_Display.setIconState(ID_MOTOR, marcha);   // 0..stateCount-1
//
// Sin fondo (último parámetro de JWPLC_UIIconField en true): el recuadro del
// campo no se pinta y la etiqueta va sin fondo; los píxeles transparentes en
// todos los estados no se tocan (se ve lo que dibujó jwplcUIPageEnter). Los
// que son transparentes sólo en algunos estados toman el fondo del campo, así
// un cambio de estado nunca deja restos del anterior.
#define JWPLC_UI_HAS_ICON 1

static constexpr uint8_t JWPLC_UI_ICON_MAX_SIZE = 64;
static constexpr uint8_t JWPLC_UI_ICON_MAX_STATES = 16;
static constexpr uint8_t JWPLC_UI_ICON_NO_TRANSPARENT = 0xFF;

struct JWPLC_UIIconFrame
{
    const uint8_t *pixels;
    const uint16_t *palette;
};

struct JWPLC_UIIconState
{
    const uint8_t *pixels;
    const uint16_t *palette;
    uint8_t frameCount;
    uint16_t frameIntervalMs;
    const JWPLC_UIIconFrame *frames;

    constexpr JWPLC_UIIconState(
        const uint8_t *pixelsValue = nullptr,
        const uint16_t *paletteValue = nullptr,
        uint8_t frameCountValue = 1,
        uint16_t frameIntervalMsValue = 0,
        const JWPLC_UIIconFrame *framesValue = nullptr)
        : pixels(pixelsValue),
          palette(paletteValue),
          frameCount(frameCountValue),
          frameIntervalMs(frameIntervalMsValue),
          frames(framesValue)
    {
    }
};

struct JWPLC_UIIcon
{
    uint8_t width;
    uint8_t height;
    uint8_t stateCount;
    uint8_t transparentIndex;
    const JWPLC_UIIconState *states;

    constexpr JWPLC_UIIcon(
        uint8_t widthValue = 0,
        uint8_t heightValue = 0,
        uint8_t stateCountValue = 0,
        const JWPLC_UIIconState *statesValue = nullptr,
        uint8_t transparentIndexValue = JWPLC_UI_ICON_NO_TRANSPARENT)
        : width(widthValue),
          height(heightValue),
          stateCount(stateCountValue),
          transparentIndex(transparentIndexValue),
          states(statesValue)
    {
    }
};

#define JWPLC_UI_HAS_CIRCULAR 1

struct JWPLC_UICircularStyle
{
    uint16_t sweep;
    uint8_t thickness;
    uint8_t centerMode;       // 0 = NONE, 1 = PERCENT, 2 = VALUE
    uint8_t labelTextSize;
    bool frame;
    uint16_t trackColor;
    uint8_t startQuadrant;    // 0 = TOP, 1 = RIGHT, 2 = BOTTOM, 3 = LEFT
    bool clockwise;
    bool segmented;
    uint8_t centerTextSize;
    uint16_t centerTextColor;

    constexpr JWPLC_UICircularStyle(
        uint16_t sweepValue = 360,
        uint8_t thicknessValue = 5,
        uint8_t centerModeValue = 1,
        uint8_t labelTextSizeValue = 1,
        bool frameValue = false,
        uint16_t trackColorValue = 0x0000,
        uint8_t startQuadrantValue = 0,
        bool clockwiseValue = true,
        bool segmentedValue = false,
        uint8_t centerTextSizeValue = 1,
        uint16_t centerTextColorValue = 0xFFFF)
        : sweep(sweepValue),
          thickness(thicknessValue),
          centerMode(centerModeValue),
          labelTextSize(labelTextSizeValue),
          frame(frameValue),
          trackColor(trackColorValue),
          startQuadrant(startQuadrantValue),
          clockwise(clockwiseValue),
          segmented(segmentedValue),
          centerTextSize(centerTextSizeValue),
          centerTextColor(centerTextColorValue)
    {
    }
};

struct JWPLC_UIRange
{
    float min;
    float max;

    JWPLC_UIRange(float minValue = 0.0f, float maxValue = 100.0f);
};

// Mirror interno para el motor actual. No forma parte de la inicializacion
// publica: se deriva de text.capacity, boolText y barRange.
struct JWPLC_UIFieldOptions
{
    uint8_t textCapacity;
    JWPLC_UIBoolText boolText;
    JWPLC_UIRange barRange;

    JWPLC_UIFieldOptions(
        uint8_t textCapacityValue = 12,
        JWPLC_UIBoolText boolTextValue = JWPLC_UIBoolText(),
        JWPLC_UIRange barRangeValue = JWPLC_UIRange());
};

// Definicion HMI agrupada. La inicializacion publica queda una fila por grupo:
// { meta }, { rect }, { text }, { style }, { colors }, { format },
// { boolText }, { barRange }.
struct JWPLC_UIField
{
    JWPLC_UIFieldMeta meta;
    JWPLC_UIRect rect;
    JWPLC_UIText text;
    JWPLC_UIStyle style;
    JWPLC_UIColors colors;
    JWPLC_UIValueFormat format;
    JWPLC_UIBoolText boolText;
    JWPLC_UIRange barRange;

    JWPLC_UIFieldOptions options;

    // ICON: imagen por estado (JWPLC_UIIconField). nullptr en los demás tipos.
    const JWPLC_UIIcon *icon = nullptr;
    // ICON sin fondo: no pinta el recuadro del campo ni los píxeles que son
    // transparentes en todos los estados (ahí se ve la gráfica estática).
    bool iconTransparent = false;

    // Medidor circular (JWPLC_UICircularField)
    JWPLC_UICircularStyle circularStyle = JWPLC_UICircularStyle();

    JWPLC_UIField(
        JWPLC_UIFieldMeta metaValue = JWPLC_UIFieldMeta(),
        JWPLC_UIRect rectValue = JWPLC_UIRect(),
        JWPLC_UIText textValue = JWPLC_UIText(),
        JWPLC_UIStyle styleValue = JWPLC_UIStyle(),
        JWPLC_UIColors colorsValue = JWPLC_UIColors(),
        JWPLC_UIValueFormat formatValue = JWPLC_UIValueFormat(),
        JWPLC_UIBoolText boolTextValue = JWPLC_UIBoolText(),
        JWPLC_UIRange barRangeValue = JWPLC_UIRange());
};

JWPLC_UIStyle JWPLC_UIValueStyle(
    uint8_t valueTextSize = 2,
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE,
    JWPLC_UIValueAlign align = JWPLC_UI_ALIGN_RIGHT);

JWPLC_UIStyle JWPLC_UITextFieldStyle(
    uint8_t valueTextSize = 1,
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE,
    JWPLC_UIValueAlign align = JWPLC_UI_ALIGN_LEFT);

JWPLC_UIStyle JWPLC_UIBoolStyle(
    uint8_t valueTextSize = 2,
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE,
    JWPLC_UIValueAlign align = JWPLC_UI_ALIGN_CENTER);

JWPLC_UIStyle JWPLC_UIBarStyle(
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_STACKED);

// ICON: la etiqueta (opcional) va arriba (STACKED) o a la izquierda (INLINE);
// el icono se alinea a la izquierda del área de valor.
JWPLC_UIStyle JWPLC_UIIconStyle(
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_STACKED);

// ---------------------------------------------------------------------
// Helpers agrupados: control completo sin llenar el struct miembro a miembro.
// ---------------------------------------------------------------------
JWPLC_UIField JWPLC_UIValueField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = JWPLC_UIText(),
    JWPLC_UIValueFormat format = JWPLC_UIValueFormat(),
    JWPLC_UIStyle style = JWPLC_UIValueStyle(),
    uint8_t page = 0,
    JWPLC_UIColors colors = JWPLC_UIColors());

JWPLC_UIField JWPLC_UITextField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = JWPLC_UIText(),
    JWPLC_UIStyle style = JWPLC_UITextFieldStyle(),
    uint8_t page = 0,
    JWPLC_UIColors colors = JWPLC_UIColors());

JWPLC_UIField JWPLC_UIBoolField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = JWPLC_UIText(),
    JWPLC_UIBoolText boolText = JWPLC_UIBoolText(),
    JWPLC_UIStyle style = JWPLC_UIBoolStyle(),
    uint8_t page = 0,
    JWPLC_UIColors colors = JWPLC_UIColors());

JWPLC_UIField JWPLC_UIBarField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = JWPLC_UIText(),
    JWPLC_UIRange range = JWPLC_UIRange(),
    JWPLC_UIStyle style = JWPLC_UIBarStyle(),
    uint8_t page = 0,
    JWPLC_UIColors colors = JWPLC_UIColors());

JWPLC_UIField JWPLC_UIIconField(
    uint8_t id,
    JWPLC_UIRect rect,
    const JWPLC_UIIcon *icon,
    JWPLC_UIText text = JWPLC_UIText(),
    JWPLC_UIStyle style = JWPLC_UIIconStyle(),
    uint8_t page = 0,
    JWPLC_UIColors colors = JWPLC_UIColors(),
    bool transparentBackground = false);

JWPLC_UIField JWPLC_UICircularField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = JWPLC_UIText(),
    JWPLC_UIRange range = JWPLC_UIRange(),
    JWPLC_UICircularStyle circularStyle = JWPLC_UICircularStyle(),
    uint8_t page = 0,
    JWPLC_UIColors colors = JWPLC_UIColors());

// ---------------------------------------------------------------------
// Helpers cortos para el caso comun.
// ---------------------------------------------------------------------
JWPLC_UIField JWPLC_UIValueField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    const char *unit = nullptr,
    JWPLC_UIValueFormat format = JWPLC_UIValueFormat(),
    uint8_t page = 0);

JWPLC_UIField JWPLC_UITextField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    uint8_t textCapacity = 12,
    uint8_t page = 0);

JWPLC_UIField JWPLC_UIBoolField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    JWPLC_UIBoolText boolText = JWPLC_UIBoolText(),
    uint8_t page = 0);

JWPLC_UIField JWPLC_UIBarField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    JWPLC_UIRange range = JWPLC_UIRange(),
    int16_t width = 100,
    int16_t height = 28,
    uint8_t page = 0);

JWPLC_UIField JWPLC_UICircularField(
    uint8_t id,
    int16_t x,
    int16_t y,
    int16_t diameter = 52,
    const char *label = nullptr,
    JWPLC_UIRange range = JWPLC_UIRange(),
    uint16_t sweep = 360,
    uint8_t page = 0);

namespace JWPLCUI
{
    bool setFields(const JWPLC_UIField *fields, size_t count);
    void clearFields();
    size_t fieldCount();

    bool setNumericValue(uint8_t fieldId, double value);
    bool setText(uint8_t fieldId, const char *value);
    bool setBool(uint8_t fieldId, bool value);
    bool setBar(uint8_t fieldId, float value);
    // ICON: estado 0..stateCount-1 (fuera de rango se ve el último estado).
    bool setIconState(uint8_t fieldId, uint8_t state);

    // Alpha15 I2: actualiza los VALUE de fecha/hora desde el RTC (cada ciclo
    // USER, en jwplcUIRuntimeServiceInput). Sólo marca dirty si cambia el texto.
    void pollClockFields();

    void setRefreshMode(JWPLC_UIRefreshMode mode);
    JWPLC_UIRefreshMode refreshMode();

    void requestRefresh();
    bool refreshNeeded();
    void consumeRefreshRequest();

    void setPage(uint8_t page);
    uint8_t currentPage();

    bool pageRedrawPending();
    void consumePageRedrawPending();

    void invalidateField(uint8_t fieldId);
    void invalidateAll(bool redrawStatic);

    void prepareEnter();

    void drawStatic(JWPLC_TFTClass &tft);
    void drawDirty(JWPLC_TFTClass &tft);
}

#include "JWPLC_UI_Menu.h"
#include "JWPLC_UI_EventLog.h"

#endif // JWPLC_UI_H
