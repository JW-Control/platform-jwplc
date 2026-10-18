#ifndef JWPLC_UI_FORMAT_H
#define JWPLC_UI_FORMAT_H

#include "JWPLC_UI.h"

// Formateo del valor de JWPLC_UIValueField (interno de JWPLC_Display).
// Separado de JWPLC_UI.cpp para compilarlo solo en host: el Designer
// (poc/designer-value-format.js) debe producir exactamente el mismo texto
// (tests/value-format-parity.cjs).
namespace JWPLCUIFormat
{
    // Texto de muestra con el ancho máximo (reserva del campo).
    void sample(const JWPLC_UIValueFormat &format, char *out, size_t outSize);

    // Texto cuando el valor no cabe (###, --:--).
    void overflow(const JWPLC_UIValueFormat &format, char *out, size_t outSize);

    // false si el valor no cabe o no es finito (out queda con overflow()).
    bool value(const JWPLC_UIValueFormat &format, double value, char *out, size_t outSize);
}

#endif // JWPLC_UI_FORMAT_H
