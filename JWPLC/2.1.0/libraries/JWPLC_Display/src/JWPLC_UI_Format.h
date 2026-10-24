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

    // Fecha/hora (I2). Sin la estructura del core para compilar en host.
    struct Clock
    {
        bool valid;
        uint16_t year;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
    };

    // false si el reloj no es válido (out queda con guiones: --:--).
    bool dateTime(const JWPLC_UIValueFormat &format, const Clock &clock, char *out, size_t outSize);

    // Alpha15 I3: texto UTF-8 (á, ñ, °…) -> bytes de la fuente GLCD (cp437,
    // con el desfase de TFT_eSPI cp437(false) para glifos > 176). Letras sin
    // glifo -> sin tilde (Á -> A); el resto -> '?'. Un byte que no inicia una
    // secuencia UTF-8 válida pasa tal cual (textos ya escritos en cp437).
    // Devuelve la cantidad de bytes escritos (sin el '\0').
    size_t utf8ToGlcd(const char *in, char *out, size_t outSize);
}

#endif // JWPLC_UI_FORMAT_H
