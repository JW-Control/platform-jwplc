#include "JWPLC_UI.h"
#include "JWPLC_UI_RuntimeHooks.h"

#include <Adafruit_ST7789.h>

JWPLC_UIFieldMeta::JWPLC_UIFieldMeta(
    uint8_t idValue,
    uint8_t pageValue,
    JWPLC_UIFieldType typeValue)
    : id(idValue), page(pageValue), type(typeValue)
{
}

JWPLC_UIRect::JWPLC_UIRect(
    int16_t xValue,
    int16_t yValue,
    int16_t widthValue,
    int16_t heightValue)
    : x(xValue), y(yValue), width(widthValue), height(heightValue)
{
}

JWPLC_UIText::JWPLC_UIText(
    const char *labelValue,
    const char *unitValue,
    uint8_t capacityValue)
    : label(labelValue), unit(unitValue), capacity(capacityValue)
{
}

JWPLC_UIColors::JWPLC_UIColors(
    uint16_t labelValue,
    uint16_t valueValue,
    uint16_t backgroundValue,
    uint16_t frameValue)
    : label(labelValue),
      value(valueValue),
      background(backgroundValue),
      frame(frameValue)
{
}

JWPLC_UIStyle::JWPLC_UIStyle(
    uint8_t labelSizeValue,
    uint8_t valueSizeValue,
    bool frameValue,
    JWPLC_UIFieldLayout layoutValue,
    JWPLC_UIValueAlign alignValue,
    JWPLC_UIColors colorsValue)
    : labelTextSize(labelSizeValue),
      valueTextSize(valueSizeValue),
      frame(frameValue),
      layout(layoutValue),
      valueAlign(alignValue),
      colors(colorsValue)
{
}

JWPLC_UIValueFormat::JWPLC_UIValueFormat(
    uint8_t integerDigitsValue,
    uint8_t decimalDigitsValue,
    bool signedValueValue,
    bool leadingZerosValue)
    : integerDigits(integerDigitsValue),
      decimalDigits(decimalDigitsValue),
      signedValue(signedValueValue),
      leadingZeros(leadingZerosValue)
{
}

JWPLC_UIBoolText::JWPLC_UIBoolText(
    const char *falseTextValue,
    const char *trueTextValue)
    : falseText(falseTextValue), trueText(trueTextValue)
{
}

JWPLC_UIRange::JWPLC_UIRange(float minValue, float maxValue)
    : min(minValue), max(maxValue)
{
}

JWPLC_UIFieldOptions::JWPLC_UIFieldOptions(
    uint8_t textCapacityValue,
    JWPLC_UIBoolText boolTextValue,
    JWPLC_UIRange barRangeValue)
    : textCapacity(textCapacityValue),
      boolText(boolTextValue),
      barRange(barRangeValue)
{
}

JWPLC_UIField::JWPLC_UIField(
    JWPLC_UIFieldMeta metaValue,
    JWPLC_UIRect rectValue,
    JWPLC_UIText textValue,
    JWPLC_UIStyle styleValue,
    JWPLC_UIColors colorsValue,
    JWPLC_UIValueFormat formatValue,
    JWPLC_UIBoolText boolTextValue,
    JWPLC_UIRange barRangeValue)
    : meta(metaValue),
      rect(rectValue),
      text(textValue),
      style(styleValue),
      colors(colorsValue),
      format(formatValue),
      boolText(boolTextValue),
      barRange(barRangeValue),
      options(textValue.capacity, boolTextValue, barRangeValue)
{
    style.colors = colors;
}

JWPLC_UIStyle JWPLC_UIValueStyle(
    uint8_t valueTextSize,
    uint8_t labelTextSize,
    bool frame,
    JWPLC_UIFieldLayout layout,
    JWPLC_UIValueAlign align)
{
    return JWPLC_UIStyle(
        labelTextSize,
        valueTextSize,
        frame,
        layout,
        align);
}

JWPLC_UIStyle JWPLC_UITextFieldStyle(
    uint8_t valueTextSize,
    uint8_t labelTextSize,
    bool frame,
    JWPLC_UIFieldLayout layout,
    JWPLC_UIValueAlign align)
{
    return JWPLC_UIValueStyle(
        valueTextSize,
        labelTextSize,
        frame,
        layout,
        align);
}

JWPLC_UIStyle JWPLC_UIBoolStyle(
    uint8_t valueTextSize,
    uint8_t labelTextSize,
    bool frame,
    JWPLC_UIFieldLayout layout,
    JWPLC_UIValueAlign align)
{
    return JWPLC_UIValueStyle(
        valueTextSize,
        labelTextSize,
        frame,
        layout,
        align);
}

JWPLC_UIStyle JWPLC_UIBarStyle(
    uint8_t labelTextSize,
    bool frame,
    JWPLC_UIFieldLayout layout)
{
    return JWPLC_UIValueStyle(
        1,
        labelTextSize,
        frame,
        layout,
        JWPLC_UI_ALIGN_LEFT);
}

JWPLC_UIField JWPLC_UIValueField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text,
    JWPLC_UIValueFormat format,
    JWPLC_UIStyle style,
    uint8_t page,
    JWPLC_UIColors colors)
{
    return JWPLC_UIField(
        JWPLC_UIFieldMeta(id, page, JWPLC_UI_FIELD_VALUE),
        rect,
        text,
        style,
        colors,
        format);
}

JWPLC_UIField JWPLC_UITextField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text,
    JWPLC_UIStyle style,
    uint8_t page,
    JWPLC_UIColors colors)
{
    return JWPLC_UIField(
        JWPLC_UIFieldMeta(id, page, JWPLC_UI_FIELD_TEXT),
        rect,
        text,
        style,
        colors);
}

JWPLC_UIField JWPLC_UIBoolField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text,
    JWPLC_UIBoolText boolText,
    JWPLC_UIStyle style,
    uint8_t page,
    JWPLC_UIColors colors)
{
    return JWPLC_UIField(
        JWPLC_UIFieldMeta(id, page, JWPLC_UI_FIELD_BOOL),
        rect,
        text,
        style,
        colors,
        JWPLC_UIValueFormat(),
        boolText);
}

JWPLC_UIField JWPLC_UIBarField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text,
    JWPLC_UIRange range,
    JWPLC_UIStyle style,
    uint8_t page,
    JWPLC_UIColors colors)
{
    return JWPLC_UIField(
        JWPLC_UIFieldMeta(id, page, JWPLC_UI_FIELD_BAR),
        rect,
        text,
        style,
        colors,
        JWPLC_UIValueFormat(),
        JWPLC_UIBoolText(),
        range);
}

JWPLC_UIField JWPLC_UIValueField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label,
    const char *unit,
    JWPLC_UIValueFormat format,
    uint8_t page)
{
    return JWPLC_UIValueField(
        id,
        JWPLC_UIRect(x, y),
        JWPLC_UIText(label, unit),
        format,
        JWPLC_UIValueStyle(),
        page);
}

JWPLC_UIField JWPLC_UITextField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label,
    uint8_t textCapacity,
    uint8_t page)
{
    return JWPLC_UITextField(
        id,
        JWPLC_UIRect(x, y),
        JWPLC_UIText(label, nullptr, textCapacity),
        JWPLC_UITextFieldStyle(),
        page);
}

JWPLC_UIField JWPLC_UIBoolField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label,
    JWPLC_UIBoolText boolText,
    uint8_t page)
{
    return JWPLC_UIBoolField(
        id,
        JWPLC_UIRect(x, y),
        JWPLC_UIText(label),
        boolText,
        JWPLC_UIBoolStyle(),
        page);
}

JWPLC_UIField JWPLC_UIBarField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label,
    JWPLC_UIRange range,
    int16_t width,
    int16_t height,
    uint8_t page)
{
    return JWPLC_UIBarField(
        id,
        JWPLC_UIRect(x, y, width, height),
        JWPLC_UIText(label),
        range,
        JWPLC_UIBarStyle(),
        page);
}

// Implementaciones strong de los hooks internos. Este TU solo se enlaza cuando
// la HMI se usa: JWPLC_UI.cpp depende de los constructores definidos arriba.
extern "C" bool jwplcUIRuntimeRefreshNeeded(void)
{
    return JWPLCUI::refreshNeeded();
}

extern "C" void jwplcUIRuntimeInvalidateAll(bool redrawStatic)
{
    JWPLCUI::invalidateAll(redrawStatic);
}

extern "C" void jwplcUIRuntimePrepareEnter(void)
{
    JWPLCUI::prepareEnter();
}

extern "C" uint8_t jwplcUIRuntimeCurrentPage(void)
{
    return JWPLCUI::currentPage();
}

extern "C" bool jwplcUIRuntimePageRedrawPending(void)
{
    return JWPLCUI::pageRedrawPending();
}

extern "C" void jwplcUIRuntimeConsumePageRedrawPending(void)
{
    JWPLCUI::consumePageRedrawPending();
}

extern "C" void jwplcUIRuntimeConsumeRefreshRequest(void)
{
    JWPLCUI::consumeRefreshRequest();
}

extern "C" void jwplcUIRuntimeDrawStatic(Adafruit_ST7789 *tft)
{
    if (tft != nullptr)
    {
        JWPLCUI::drawStatic(*tft);
    }
}

extern "C" void jwplcUIRuntimeDrawDirty(Adafruit_ST7789 *tft)
{
    if (tft != nullptr)
    {
        JWPLCUI::drawDirty(*tft);
    }
}
