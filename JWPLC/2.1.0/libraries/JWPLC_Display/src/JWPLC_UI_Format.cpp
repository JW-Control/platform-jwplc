#include "JWPLC_UI_Format.h"

#include <cmath>
#include <cstdio>
#include <cstring>

JWPLC_UIValueFormat::JWPLC_UIValueFormat(
    uint8_t integerDigitsValue,
    uint8_t decimalDigitsValue,
    bool signedValueValue,
    bool leadingZerosValue)
    : JWPLC_UIValueFormat(integerDigitsValue, decimalDigitsValue, signedValueValue, leadingZerosValue, 0)
{
}

JWPLC_UIValueFormat::JWPLC_UIValueFormat(
    uint8_t integerDigitsValue,
    uint8_t decimalDigitsValue,
    bool signedValueValue,
    bool leadingZerosValue,
    char thousandsSeparatorValue)
    : integerDigits(integerDigitsValue),
      decimalDigits(decimalDigitsValue),
      signedValue(signedValueValue),
      leadingZeros(leadingZerosValue),
      thousandsSeparator(thousandsSeparatorValue),
      kind(JWPLC_UI_VALUE_NUMBER),
      durationPattern(JWPLC_UI_DURATION_MM_SS),
      durationUnit(JWPLC_UI_DURATION_SECONDS),
      dateTimePattern(JWPLC_UI_DATETIME_HH_MM)
{
}

JWPLC_UIValueFormat JWPLC_UIDurationFormat(
    JWPLC_UIDurationPattern pattern,
    JWPLC_UIDurationUnit unit,
    uint8_t leadDigits,
    bool signedValue)
{
    JWPLC_UIValueFormat format(leadDigits, 0, signedValue, false, 0);
    format.kind = JWPLC_UI_VALUE_DURATION;
    format.durationPattern = pattern;
    format.durationUnit = unit;
    return format;
}

JWPLC_UIValueFormat JWPLC_UIDateTimeFormat(JWPLC_UIDateTimePattern pattern)
{
    JWPLC_UIValueFormat format(2, 0, false, false, 0);
    format.kind = JWPLC_UI_VALUE_DATETIME;
    format.dateTimePattern = pattern;
    return format;
}

namespace
{
    static constexpr uint8_t MAX_INTEGER_DIGITS = 9;
    static constexpr uint8_t MAX_DECIMAL_DIGITS = 6;
    static constexpr size_t WORK_SIZE = 48;

    uint8_t integerDigitsOf(const JWPLC_UIValueFormat &format)
    {
        if (format.integerDigits == 0)
            return 1;
        return (format.integerDigits > MAX_INTEGER_DIGITS) ? MAX_INTEGER_DIGITS : format.integerDigits;
    }

    uint8_t decimalDigitsOf(const JWPLC_UIValueFormat &format)
    {
        return (format.decimalDigits > MAX_DECIMAL_DIGITS) ? MAX_DECIMAL_DIGITS : format.decimalDigits;
    }

    // Sólo ',', '.' y ' ': cualquier otro valor se trata como "sin separador".
    char separatorOf(const JWPLC_UIValueFormat &format)
    {
        const char sep = format.thousandsSeparator;
        return (sep == ',' || sep == '.' || sep == ' ') ? sep : 0;
    }

    char decimalPointOf(const JWPLC_UIValueFormat &format)
    {
        return (separatorOf(format) == '.') ? ',' : '.';
    }

    void copyOut(const char *text, char *out, size_t outSize)
    {
        strncpy(out, text, outSize - 1);
        out[outSize - 1] = '\0';
    }

    // Inserta el separador en la parte entera de un número ya formateado
    // ("-001234.50" -> "-001,234.50") y cambia el punto decimal si hace falta.
    void groupThousands(const JWPLC_UIValueFormat &format, const char *in, char *out, size_t outSize)
    {
        const char sep = separatorOf(format);
        const char point = decimalPointOf(format);
        char work[WORK_SIZE] = {};
        size_t pos = 0;
        const char *p = in;

        if (*p == '-' || *p == '+')
            work[pos++] = *p++;

        size_t digits = 0;
        while (p[digits] >= '0' && p[digits] <= '9')
            ++digits;

        for (size_t i = 0; i < digits && pos + 1 < sizeof(work); ++i)
        {
            if (sep != 0 && i > 0 && ((digits - i) % 3) == 0 && pos + 1 < sizeof(work))
                work[pos++] = sep;
            work[pos++] = p[i];
        }
        p += digits;

        for (; *p != '\0' && pos + 1 < sizeof(work); ++p)
            work[pos++] = (*p == '.') ? point : *p;

        work[pos] = '\0';
        copyOut(work, out, outSize);
    }

    // ----- Duración -------------------------------------------------------

    uint64_t pow10u(uint8_t n)
    {
        uint64_t r = 1;
        while (n-- > 0)
            r *= 10;
        return r;
    }

    // Ancho del relleno con ceros de la primera unidad: 2 como máximo
    // (05:03), aunque la reserva sea de 3 o más dígitos.
    uint8_t leadPadOf(const JWPLC_UIValueFormat &format)
    {
        const uint8_t lead = integerDigitsOf(format);
        return (lead > 2) ? 2 : lead;
    }

    void durationShape(const JWPLC_UIValueFormat &format, char digit, char *out, size_t outSize)
    {
        const uint8_t lead = integerDigitsOf(format);
        char work[WORK_SIZE] = {};
        size_t pos = 0;

        for (uint8_t i = 0; i < lead && pos + 1 < sizeof(work); ++i)
            work[pos++] = digit;

        const char *tail = "";
        switch (format.durationPattern)
        {
        case JWPLC_UI_DURATION_MM_SS:
            tail = (digit == '8') ? ":88" : ":--";
            break;
        case JWPLC_UI_DURATION_HH_MM_SS:
            tail = (digit == '8') ? ":88:88" : ":--:--";
            break;
        case JWPLC_UI_DURATION_COMPACT:
            // La forma más ancha es "HHh88m" o "88m88s" (6 caracteres).
            if (lead + 4 < 6 && digit == '8')
            {
                copyOut("88m88s", work, sizeof(work));
                pos = strlen(work);
                tail = "";
            }
            else
            {
                tail = (digit == '8') ? "h88m" : "h--m";
            }
            break;
        default:
            break;
        }

        for (const char *t = tail; *t != '\0' && pos + 1 < sizeof(work); ++t)
            work[pos++] = *t;
        work[pos] = '\0';
        copyOut(work, out, outSize);
    }

    // ----- Texto UTF-8 -> GLCD (cp437) -------------------------------------

    // TFT_eSPI arranca con cp437(false): para un byte > 175 drawChar() dibuja
    // el glifo siguiente. JWPLC_TFT no lo cambia.
    static constexpr uint8_t LEGACY_SHIFT_FROM = 176;

    struct GlyphMap
    {
        uint16_t code; // Unicode
        uint8_t glyph; // glifo de glcdfont.c
    };

    // Generada junto con poc/designer-cp437.js (MAP + FALLBACK): cp437 exacto
    // y, para letras latinas sin glifo, la letra sin tilde. Ordenada por code.
    static const GlyphMap GLYPH_MAP[] = {
        {0x00A0, 0xFF}, {0x00A1, 0xAD}, {0x00A2, 0x9B}, {0x00A3, 0x9C}, {0x00A5, 0x9D}, {0x00AA, 0xA6},
        {0x00AB, 0xAE}, {0x00AC, 0xAA}, {0x00B0, 0xF8}, {0x00B1, 0xF1}, {0x00B2, 0xFD}, {0x00B5, 0xE6},
        {0x00B7, 0xFA}, {0x00BA, 0xA7}, {0x00BB, 0xAF}, {0x00BC, 0xAC}, {0x00BD, 0xAB}, {0x00BF, 0xA8},
        {0x00C0, 0x41}, {0x00C1, 0x41}, {0x00C2, 0x41}, {0x00C3, 0x41}, {0x00C4, 0x8E}, {0x00C5, 0x8F},
        {0x00C6, 0x92}, {0x00C7, 0x80}, {0x00C8, 0x45}, {0x00C9, 0x90}, {0x00CA, 0x45}, {0x00CB, 0x45},
        {0x00CC, 0x49}, {0x00CD, 0x49}, {0x00CE, 0x49}, {0x00CF, 0x49}, {0x00D1, 0xA5}, {0x00D2, 0x4F},
        {0x00D3, 0x4F}, {0x00D4, 0x4F}, {0x00D5, 0x4F}, {0x00D6, 0x99}, {0x00D9, 0x55}, {0x00DA, 0x55},
        {0x00DB, 0x55}, {0x00DC, 0x9A}, {0x00DD, 0x59}, {0x00DF, 0xE1}, {0x00E0, 0x85}, {0x00E1, 0xA0},
        {0x00E2, 0x83}, {0x00E3, 0x61}, {0x00E4, 0x84}, {0x00E5, 0x86}, {0x00E6, 0x91}, {0x00E7, 0x87},
        {0x00E8, 0x8A}, {0x00E9, 0x82}, {0x00EA, 0x88}, {0x00EB, 0x89}, {0x00EC, 0x8D}, {0x00ED, 0xA1},
        {0x00EE, 0x8C}, {0x00EF, 0x8B}, {0x00F1, 0xA4}, {0x00F2, 0x95}, {0x00F3, 0xA2}, {0x00F4, 0x93},
        {0x00F5, 0x6F}, {0x00F6, 0x94}, {0x00F7, 0xF6}, {0x00F9, 0x97}, {0x00FA, 0xA3}, {0x00FB, 0x96},
        {0x00FC, 0x81}, {0x00FD, 0x79}, {0x00FF, 0x98}, {0x0100, 0x41}, {0x0101, 0x61}, {0x0102, 0x41},
        {0x0103, 0x61}, {0x0104, 0x41}, {0x0105, 0x61}, {0x0106, 0x43}, {0x0107, 0x63}, {0x0108, 0x43},
        {0x0109, 0x63}, {0x010A, 0x43}, {0x010B, 0x63}, {0x010C, 0x43}, {0x010D, 0x63}, {0x010E, 0x44},
        {0x010F, 0x64}, {0x0112, 0x45}, {0x0113, 0x65}, {0x0114, 0x45}, {0x0115, 0x65}, {0x0116, 0x45},
        {0x0117, 0x65}, {0x0118, 0x45}, {0x0119, 0x65}, {0x011A, 0x45}, {0x011B, 0x65}, {0x011C, 0x47},
        {0x011D, 0x67}, {0x011E, 0x47}, {0x011F, 0x67}, {0x0120, 0x47}, {0x0121, 0x67}, {0x0122, 0x47},
        {0x0123, 0x67}, {0x0124, 0x48}, {0x0125, 0x68}, {0x0128, 0x49}, {0x0129, 0x69}, {0x012A, 0x49},
        {0x012B, 0x69}, {0x012C, 0x49}, {0x012D, 0x69}, {0x012E, 0x49}, {0x012F, 0x69}, {0x0130, 0x49},
        {0x0134, 0x4A}, {0x0135, 0x6A}, {0x0136, 0x4B}, {0x0137, 0x6B}, {0x0139, 0x4C}, {0x013A, 0x6C},
        {0x013B, 0x4C}, {0x013C, 0x6C}, {0x013D, 0x4C}, {0x013E, 0x6C}, {0x0143, 0x4E}, {0x0144, 0x6E},
        {0x0145, 0x4E}, {0x0146, 0x6E}, {0x0147, 0x4E}, {0x0148, 0x6E}, {0x014C, 0x4F}, {0x014D, 0x6F},
        {0x014E, 0x4F}, {0x014F, 0x6F}, {0x0150, 0x4F}, {0x0151, 0x6F}, {0x0154, 0x52}, {0x0155, 0x72},
        {0x0156, 0x52}, {0x0157, 0x72}, {0x0158, 0x52}, {0x0159, 0x72}, {0x015A, 0x53}, {0x015B, 0x73},
        {0x015C, 0x53}, {0x015D, 0x73}, {0x015E, 0x53}, {0x015F, 0x73}, {0x0160, 0x53}, {0x0161, 0x73},
        {0x0162, 0x54}, {0x0163, 0x74}, {0x0164, 0x54}, {0x0165, 0x74}, {0x0168, 0x55}, {0x0169, 0x75},
        {0x016A, 0x55}, {0x016B, 0x75}, {0x016C, 0x55}, {0x016D, 0x75}, {0x016E, 0x55}, {0x016F, 0x75},
        {0x0170, 0x55}, {0x0171, 0x75}, {0x0172, 0x55}, {0x0173, 0x75}, {0x0174, 0x57}, {0x0175, 0x77},
        {0x0176, 0x59}, {0x0177, 0x79}, {0x0178, 0x59}, {0x0179, 0x5A}, {0x017A, 0x7A}, {0x017B, 0x5A},
        {0x017C, 0x7A}, {0x017D, 0x5A}, {0x017E, 0x7A}, {0x0192, 0x9F}, {0x0393, 0xE2}, {0x0398, 0xE9},
        {0x03A3, 0xE4}, {0x03A6, 0xE8}, {0x03A9, 0xEA}, {0x03B1, 0xE0}, {0x03B4, 0xEB}, {0x03B5, 0xEE},
        {0x03BC, 0xE6}, {0x03C0, 0xE3}, {0x03C3, 0xE5}, {0x03C4, 0xE7}, {0x03C6, 0xED}, {0x2022, 0xF9},
        {0x207F, 0xFC}, {0x20A7, 0x9E}, {0x2219, 0xF9}, {0x221A, 0xFB}, {0x221E, 0xEC}, {0x2229, 0xEF},
        {0x2248, 0xF7}, {0x2261, 0xF0}, {0x2264, 0xF3}, {0x2265, 0xF2}, {0x2310, 0xA9}, {0x2320, 0xF4},
        {0x2321, 0xF5}, {0x2500, 0xC4}, {0x2502, 0xB3}, {0x250C, 0xDA}, {0x2510, 0xBF}, {0x2514, 0xC0},
        {0x2518, 0xD9}, {0x251C, 0xC3}, {0x2524, 0xB4}, {0x252C, 0xC2}, {0x2534, 0xC1}, {0x253C, 0xC5},
        {0x2550, 0xCD}, {0x2551, 0xBA}, {0x2552, 0xD5}, {0x2553, 0xD6}, {0x2554, 0xC9}, {0x2555, 0xB8},
        {0x2556, 0xB7}, {0x2557, 0xBB}, {0x2558, 0xD4}, {0x2559, 0xD3}, {0x255A, 0xC8}, {0x255B, 0xBE},
        {0x255C, 0xBD}, {0x255D, 0xBC}, {0x255E, 0xC6}, {0x255F, 0xC7}, {0x2560, 0xCC}, {0x2561, 0xB5},
        {0x2562, 0xB6}, {0x2563, 0xB9}, {0x2564, 0xD1}, {0x2565, 0xD2}, {0x2566, 0xCB}, {0x2567, 0xCF},
        {0x2568, 0xD0}, {0x2569, 0xCA}, {0x256A, 0xD8}, {0x256B, 0xD7}, {0x256C, 0xCE}, {0x2580, 0xDF},
        {0x2584, 0xDC}, {0x2588, 0xDB}, {0x258C, 0xDD}, {0x2590, 0xDE}, {0x2592, 0xB1}, {0x2593, 0xB2},
        {0x25A0, 0xFE},
    };

    int glyphForCode(uint32_t code)
    {
        if (code < 0x80)
            return (int)code;
        size_t lo = 0;
        size_t hi = sizeof(GLYPH_MAP) / sizeof(GLYPH_MAP[0]);
        while (lo < hi)
        {
            const size_t mid = (lo + hi) / 2;
            if (GLYPH_MAP[mid].code == code)
                return GLYPH_MAP[mid].glyph;
            if (GLYPH_MAP[mid].code < code)
                lo = mid + 1;
            else
                hi = mid;
        }
        return -1;
    }

    // Bytes de continuación UTF-8 esperados tras un byte inicial (0 = no es
    // un inicial válido).
    uint8_t utf8Extra(uint8_t lead)
    {
        if (lead >= 0xC2 && lead <= 0xDF)
            return 1;
        if (lead >= 0xE0 && lead <= 0xEF)
            return 2;
        if (lead >= 0xF0 && lead <= 0xF4)
            return 3; // fuera del plano básico (emoji): sin glifo -> '?'
        return 0;
    }

    // ----- Fecha/hora -------------------------------------------------------

    // Forma del patrón con un dígito de relleno ('8' muestra, '-' sin reloj).
    const char *dateTimeShape(JWPLC_UIDateTimePattern pattern, bool dashes)
    {
        switch (pattern)
        {
        case JWPLC_UI_DATETIME_HH_MM_SS:
            return dashes ? "--:--:--" : "88:88:88";
        case JWPLC_UI_DATETIME_DD_MM:
            return dashes ? "--/--" : "88/88";
        case JWPLC_UI_DATETIME_DD_MM_YYYY:
            return dashes ? "--/--/----" : "88/88/8888";
        case JWPLC_UI_DATETIME_DD_MM_YYYY_HH_MM:
            return dashes ? "--/--/---- --:--" : "88/88/8888 88:88";
        case JWPLC_UI_DATETIME_HH_MM:
        default:
            return dashes ? "--:--" : "88:88";
        }
    }

    bool formatDuration(const JWPLC_UIValueFormat &format, double value, char *out, size_t outSize)
    {
        if (!std::isfinite(value) || (value < 0.0 && !format.signedValue))
        {
            durationShape(format, '-', out, outSize);
            return false;
        }

        double magnitude = std::fabs(value);
        if (format.durationUnit == JWPLC_UI_DURATION_MILLIS)
            magnitude /= 1000.0;

        // 1e15 s ya supera cualquier reserva de 9 dígitos y evita desbordar uint64.
        if (magnitude >= 1e15)
        {
            durationShape(format, '-', out, outSize);
            return false;
        }

        const uint64_t total = (uint64_t)std::floor(magnitude);
        const uint8_t lead = integerDigitsOf(format);
        const uint8_t pad = leadPadOf(format); // 1 o 2: relleno con ceros de la primera unidad
        const uint64_t limit = pow10u(lead);
        const bool negative = value < 0.0 && total > 0;
        const char *sign = negative ? "-" : "";
        char work[WORK_SIZE] = {};

        const uint64_t hours = total / 3600ULL;
        const unsigned minutes = (unsigned)((total / 60ULL) % 60ULL);
        const unsigned seconds = (unsigned)(total % 60ULL);

        switch (format.durationPattern)
        {
        case JWPLC_UI_DURATION_SS:
            if (total >= limit)
                break;
            snprintf(work, sizeof(work), (pad == 2) ? "%s%02llu" : "%s%llu", sign, (unsigned long long)total);
            copyOut(work, out, outSize);
            return true;

        case JWPLC_UI_DURATION_MM_SS:
        {
            const uint64_t totalMinutes = total / 60ULL;
            if (totalMinutes >= limit)
                break;
            snprintf(work, sizeof(work), (pad == 2) ? "%s%02llu:%02u" : "%s%llu:%02u", sign, (unsigned long long)totalMinutes, seconds);
            copyOut(work, out, outSize);
            return true;
        }

        case JWPLC_UI_DURATION_HH_MM_SS:
            if (hours >= limit)
                break;
            snprintf(work, sizeof(work), (pad == 2) ? "%s%02llu:%02u:%02u" : "%s%llu:%02u:%02u", sign, (unsigned long long)hours, minutes, seconds);
            copyOut(work, out, outSize);
            return true;

        case JWPLC_UI_DURATION_COMPACT:
            if (hours >= limit)
                break;
            if (hours > 0)
                snprintf(work, sizeof(work), "%s%lluh%02um", sign, (unsigned long long)hours, minutes);
            else if (minutes > 0)
                snprintf(work, sizeof(work), "%s%um%02us", sign, minutes, seconds);
            else
                snprintf(work, sizeof(work), "%s%us", sign, seconds);
            copyOut(work, out, outSize);
            return true;

        default:
            break;
        }

        durationShape(format, '-', out, outSize);
        return false;
    }
}

namespace JWPLCUIFormat
{
    void sample(const JWPLC_UIValueFormat &format, char *out, size_t outSize)
    {
        if (out == nullptr || outSize == 0)
            return;

        char work[WORK_SIZE] = {};

        if (format.kind == JWPLC_UI_VALUE_DATETIME)
        {
            copyOut(dateTimeShape(format.dateTimePattern, false), out, outSize);
            return;
        }

        if (format.kind == JWPLC_UI_VALUE_DURATION)
        {
            const size_t sign = format.signedValue ? 1 : 0;
            work[0] = '-';
            durationShape(format, '8', work + sign, sizeof(work) - sign);
            copyOut(work, out, outSize);
            return;
        }

        const uint8_t integers = integerDigitsOf(format);
        const uint8_t decimals = decimalDigitsOf(format);
        size_t pos = 0;

        if (format.signedValue)
            work[pos++] = '-';
        for (uint8_t i = 0; i < integers; ++i)
            work[pos++] = '8';
        if (decimals > 0)
        {
            work[pos++] = '.';
            for (uint8_t i = 0; i < decimals; ++i)
                work[pos++] = '8';
        }
        work[pos] = '\0';
        groupThousands(format, work, out, outSize);
    }

    void overflow(const JWPLC_UIValueFormat &format, char *out, size_t outSize)
    {
        if (out == nullptr || outSize == 0)
            return;

        if (format.kind == JWPLC_UI_VALUE_DATETIME)
        {
            copyOut(dateTimeShape(format.dateTimePattern, true), out, outSize);
            return;
        }

        if (format.kind == JWPLC_UI_VALUE_DURATION)
        {
            durationShape(format, '-', out, outSize);
            return;
        }

        // Mismo largo que la muestra, todo en '#'.
        char shape[WORK_SIZE] = {};
        sample(format, shape, sizeof(shape));
        size_t slots = strlen(shape);
        if (slots > outSize - 1)
            slots = outSize - 1;
        memset(out, '#', slots);
        out[slots] = '\0';
    }

    bool value(const JWPLC_UIValueFormat &format, double value, char *out, size_t outSize)
    {
        if (out == nullptr || outSize == 0)
            return false;

        if (format.kind == JWPLC_UI_VALUE_DURATION)
            return formatDuration(format, value, out, outSize);

        // Un VALUE de fecha/hora no recibe números (lo llena pollClockFields).
        if (format.kind == JWPLC_UI_VALUE_DATETIME)
        {
            overflow(format, out, outSize);
            return false;
        }

        if (!std::isfinite(value))
        {
            overflow(format, out, outSize);
            return false;
        }

        const bool negative = value < 0.0;
        if (negative && !format.signedValue)
        {
            overflow(format, out, outSize);
            return false;
        }

        const uint8_t decimals = decimalDigitsOf(format);
        const uint8_t integers = integerDigitsOf(format);
        char work[WORK_SIZE] = {};

        // Un valor fuera de toda reserva no pasa por snprintf (evita textos
        // de cientos de dígitos en el buffer de trabajo).
        if (std::fabs(value) >= 1e15)
        {
            overflow(format, out, outSize);
            return false;
        }

        if (format.leadingZeros)
        {
            const int magnitudeWidth = integers + ((decimals > 0) ? (1 + decimals) : 0);
            const size_t sign = negative ? 1 : 0;
            work[0] = '-';
            snprintf(work + sign, sizeof(work) - sign, "%0*.*f", magnitudeWidth, decimals, std::fabs(value));
        }
        else
        {
            snprintf(work, sizeof(work), "%.*f", decimals, value);
        }

        const char *p = work;
        if (*p == '-' || *p == '+')
            ++p;

        uint8_t integerCount = 0;
        while (*p != '\0' && *p != '.')
        {
            if (*p >= '0' && *p <= '9')
                ++integerCount;
            ++p;
        }

        if (integerCount > integers)
        {
            overflow(format, out, outSize);
            return false;
        }

        groupThousands(format, work, out, outSize);
        return true;
    }

    bool dateTime(const JWPLC_UIValueFormat &format, const Clock &c, char *out, size_t outSize)
    {
        if (out == nullptr || outSize == 0)
            return false;

        const bool valid = c.valid &&
                           c.month >= 1 && c.month <= 12 &&
                           c.day >= 1 && c.day <= 31 &&
                           c.hour < 24 && c.minute < 60 && c.second < 60 &&
                           c.year <= 9999;
        if (!valid)
        {
            overflow(format, out, outSize);
            return false;
        }

        char work[WORK_SIZE] = {};
        switch (format.dateTimePattern)
        {
        case JWPLC_UI_DATETIME_HH_MM_SS:
            snprintf(work, sizeof(work), "%02u:%02u:%02u", c.hour, c.minute, c.second);
            break;
        case JWPLC_UI_DATETIME_DD_MM:
            snprintf(work, sizeof(work), "%02u/%02u", c.day, c.month);
            break;
        case JWPLC_UI_DATETIME_DD_MM_YYYY:
            snprintf(work, sizeof(work), "%02u/%02u/%04u", c.day, c.month, c.year);
            break;
        case JWPLC_UI_DATETIME_DD_MM_YYYY_HH_MM:
            snprintf(work, sizeof(work), "%02u/%02u/%04u %02u:%02u", c.day, c.month, c.year, c.hour, c.minute);
            break;
        case JWPLC_UI_DATETIME_HH_MM:
        default:
            snprintf(work, sizeof(work), "%02u:%02u", c.hour, c.minute);
            break;
        }
        copyOut(work, out, outSize);
        return true;
    }

    size_t utf8ToGlcd(const char *in, char *out, size_t outSize)
    {
        if (out == nullptr || outSize == 0)
            return 0;

        size_t pos = 0;
        const uint8_t *p = (const uint8_t *)((in != nullptr) ? in : "");

        while (*p != 0 && pos + 1 < outSize)
        {
            const uint8_t lead = *p;
            const uint8_t extra = utf8Extra(lead);
            bool valid = extra > 0;
            bool truncated = false;
            for (uint8_t i = 1; valid && i <= extra; ++i)
            {
                truncated = p[i] == 0;
                valid = (p[i] & 0xC0) == 0x80;
            }

            // Un strncpy que cortó «ó» a la mitad deja el byte inicial solo
            // al final: se descarta en vez de dibujar un glifo de más.
            if (truncated)
                break;

            if (!valid)
            {
                out[pos++] = (char)lead; // ASCII o byte cp437 ya escrito
                ++p;
                continue;
            }

            uint32_t code = (extra == 1) ? (lead & 0x1F) : (extra == 2) ? (lead & 0x0F) : (lead & 0x07);
            for (uint8_t i = 1; i <= extra; ++i)
                code = (code << 6) | (p[i] & 0x3F);
            p += 1 + extra;

            int glyph = glyphForCode(code);
            if (glyph < 0)
                glyph = '?';
            out[pos++] = (char)((glyph > LEGACY_SHIFT_FROM) ? glyph - 1 : glyph);
        }

        out[pos] = '\0';
        return pos;
    }
}
