#include "JWPLC_UI_Format.h"

#include <cmath>
#include <cstdio>
#include <cstring>

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
        const int pad = leadPadOf(format);
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
            snprintf(work, sizeof(work), "%s%0*llu", sign, pad, (unsigned long long)total);
            copyOut(work, out, outSize);
            return true;

        case JWPLC_UI_DURATION_MM_SS:
        {
            const uint64_t totalMinutes = total / 60ULL;
            if (totalMinutes >= limit)
                break;
            snprintf(work, sizeof(work), "%s%0*llu:%02u", sign, pad, (unsigned long long)totalMinutes, seconds);
            copyOut(work, out, outSize);
            return true;
        }

        case JWPLC_UI_DURATION_HH_MM_SS:
            if (hours >= limit)
                break;
            snprintf(work, sizeof(work), "%s%0*llu:%02u:%02u", sign, pad, (unsigned long long)hours, minutes, seconds);
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
}
