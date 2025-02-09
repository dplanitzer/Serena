//
//  Formatter.c
//  libc
//
//  Created by Dietmar Planitzer on 1/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Formatter.h"


void Formatter_Init(struct Formatter* _Nonnull self, Formatter_Sink _Nonnull sink, void * _Nullable ctx)
{
    self->sink = sink;
    self->context = ctx;
    self->charactersWritten = 0;
}

static void Formatter_WriteChar(struct Formatter* _Nonnull self, char ch)
{
    self->sink(self, &ch, 1);
    self->charactersWritten++;
}

static void Formatter_WriteString(struct Formatter* _Nonnull self, const char * _Nonnull str, ssize_t maxChars)
{
    const char* p = str;
    size_t len = 0;

    while (*p++ != '\0' && maxChars-- > 0) {
        len++;
    }

    self->sink(self, str, len);
    self->charactersWritten += len;
}

static void Formatter_WriteRepChar(struct Formatter* _Nonnull self, char ch, int count)
{
    while(count-- > 0) {
        self->sink(self, &ch, 1);
    }
    self->charactersWritten += count;
}

static const char* _Nonnull Formatter_ParseLengthModifier(struct Formatter* _Nonnull self, const char* _Nonnull format, struct ConversionSpec* _Nonnull spec)
{
    char ch = *format;
    
    if (ch == 'l') {
        format++;
        if (*format == 'l') {
            format++;
            spec->lengthModifier = LENGTH_MODIFIER_ll;
        } else {
            spec->lengthModifier = LENGTH_MODIFIER_l;
        }
    }
    else if (ch == 'h') {
        format++;
        if (*format == 'h') {
            format++;
            spec->lengthModifier = LENGTH_MODIFIER_hh;
        } else {
            spec->lengthModifier = LENGTH_MODIFIER_h;
        }
    }
    else if (ch == 'z') {
        format++;
        // ssize_t
        spec->lengthModifier = LENGTH_MODIFIER_z;
    }
    
    return format;
}

// Expects that 'format' points to the first character after the '%'.
static const char* _Nonnull Formatter_ParseConversionSpec(struct Formatter* _Nonnull self, const char* _Nonnull format, va_list* _Nonnull ap, struct ConversionSpec* _Nonnull spec)
{
    char ch;

    spec->minimumFieldWidth = 0;
    spec->precision = 0;
    spec->lengthModifier = LENGTH_MODIFIER_none;
    spec->flags.isAlternativeForm = false;
    spec->flags.padWithZeros = false;
    spec->flags.hasPrecision = false;

    // Flags
    bool done = false;
    while (!done) {
        ch = *format++;
        
        switch (ch) {
            case '\0':  return --format;
            case '#':   spec->flags.isAlternativeForm = true; break;
            case '0':   spec->flags.padWithZeros = true; break;
            default:    --format; done = true; break;
        }
    }

    // Minimum field width
    ch = *format;
    if (ch == '*') {
        spec->minimumFieldWidth = va_arg(*ap, int);
        format++;
    }
    else if (ch >= '1' && ch <= '9') {
        spec->minimumFieldWidth = (ssize_t)atoi(format, &format, 10);
    }

    // Precision
    if (*format == '.') {
        format++;
        ch = *format;

        if (ch == '*') {
            spec->precision = va_arg(*ap, int);
            format++;
        }
        else if (ch >= '0' && ch <= '9') {
            spec->precision = (ssize_t)atoi(format, &format, 10);
        }
        spec->flags.hasPrecision = true;
    }
    
    // Length modifier
    return Formatter_ParseLengthModifier(self, format, spec);
}

static void Formatter_FormatStringField(struct Formatter* _Nonnull self, const struct ConversionSpec* _Nonnull spec, const char* _Nonnull str, ssize_t slen)
{
    const ssize_t nspaces = (spec->minimumFieldWidth > slen) ? spec->minimumFieldWidth - slen : 0;

    if (nspaces > 0) {
        Formatter_WriteRepChar(self, ' ', nspaces);
    }

    Formatter_WriteString(self, str, slen);
}

static void Formatter_FormatSignedIntegerField(struct Formatter* _Nonnull self, const struct ConversionSpec* _Nonnull spec, const char* _Nonnull pCanonDigits)
{
    int nSign = 1;
    int nDigits = pCanonDigits[0] - 1;
    int nLeadingZeros = (spec->flags.hasPrecision) ? __max(spec->precision - nDigits, 0) : 0;
    const char* pSign = &pCanonDigits[1];
    const char* pDigits = &pCanonDigits[2];

    if (*pSign == '+') {
        pSign = "";
        nSign = 0;
    }

    const int slen = nSign + nLeadingZeros + nDigits;
    int nspaces = (spec->minimumFieldWidth > slen) ? spec->minimumFieldWidth - slen : 0;

    if (spec->flags.padWithZeros && !spec->flags.hasPrecision) {
        nLeadingZeros = nspaces;
        nspaces = 0;
    }

    if (nspaces > 0) {
        Formatter_WriteRepChar(self, ' ', nspaces);
    }

    if (nSign > 0) {
        Formatter_WriteChar(self, *pSign);
    }
    if (nLeadingZeros > 0) {
        Formatter_WriteRepChar(self, '0', nLeadingZeros);
    }
    while(nDigits-- > 0) {
        Formatter_WriteChar(self, *pDigits++);
    }
}

static void Formatter_FormatUnsignedIntegerField(struct Formatter* _Nonnull self, int radix, bool isUppercase, const struct ConversionSpec* _Nonnull spec, const char* _Nonnull pCanonDigits)
{
    int nRadixChars = 0;
    int nDigits = pCanonDigits[0] - 1;
    int nLeadingZeros = (spec->flags.hasPrecision) ? __max(spec->precision - nDigits, 0) : 0;
    const char* pRadixChars = "";
    const char* pDigits = &pCanonDigits[2];

    if (spec->flags.isAlternativeForm) {
        switch (radix) {
            case 8: nRadixChars = 1; pRadixChars = "0"; break;
            case 16: nRadixChars = 2; pRadixChars = (isUppercase) ? "0X" : "0x";
            default: break;
        }
    }

    const int slen = nRadixChars + nLeadingZeros + nDigits;
    int nspaces = (spec->minimumFieldWidth > slen) ? spec->minimumFieldWidth - slen : 0;

    if (spec->flags.padWithZeros && !spec->flags.hasPrecision) {
        nLeadingZeros = nspaces;
        nspaces = 0;
    }

    if (nspaces > 0) {
        Formatter_WriteRepChar(self, ' ', nspaces);
    }

    while (nRadixChars-- > 0) {
        Formatter_WriteChar(self, *pRadixChars++);
    }
    if (nLeadingZeros > 0) {
        Formatter_WriteRepChar(self, '0', nLeadingZeros);
    }
    while(nDigits-- > 0) {
        Formatter_WriteChar(self, *pDigits++);
    }
}

static void Formatter_FormatChar(struct Formatter* _Nonnull self, const struct ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    const char ch = (char) va_arg(*ap, int);
    Formatter_FormatStringField(self, spec, &ch, 1);
}

static void Formatter_FormatString(struct Formatter* _Nonnull self, const struct ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    Formatter_FormatStringField(self, spec, va_arg(*ap, const char*), (spec->flags.hasPrecision) ? spec->precision : SSIZE_MAX);
}

static void Formatter_FormatSignedInteger(struct Formatter* _Nonnull self, const struct ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    int64_t v64;
    int32_t v32;
    int nbits;
    char* pCanonDigits;

    switch (spec->lengthModifier) {
        case LENGTH_MODIFIER_hh:    v32 = (int32_t)(signed char)va_arg(*ap, int); nbits = INT32_WIDTH;   break;
        case LENGTH_MODIFIER_h:     v32 = (int32_t)(short)va_arg(*ap, int); nbits = INT32_WIDTH;         break;
        case LENGTH_MODIFIER_none:  v32 = (int32_t)va_arg(*ap, int); nbits = INT32_WIDTH;                break;
#if __LONG_WIDTH == 64
        case LENGTH_MODIFIER_l:     v64 = (int64_t)va_arg(*ap, long); nbits = INT64_WIDTH;               break;
#else
        case LENGTH_MODIFIER_l:     v32 = (int32_t)va_arg(*ap, long); nbits = INT32_WIDTH;               break;
#endif
        case LENGTH_MODIFIER_ll:    v64 = (int64_t)va_arg(*ap, long long); nbits = INT64_WIDTH;          break;
#if __SIZE_WIDTH == 64
        case LENGTH_MODIFIER_z:     v64 = (int64_t)va_arg(*ap, ssize_t); nbits = SSIZE_WIDTH; break;
#else
        case LENGTH_MODIFIER_z:     v32 = (int32_t)va_arg(*ap, ssize_t); nbits = SSIZE_WIDTH; break;
#endif
    }

    if (nbits == 64) {
        pCanonDigits = __i64toa(v64, self->digits);
    } else {
        pCanonDigits = __i32toa(v32, self->digits);
    }

    Formatter_FormatSignedIntegerField(self, spec, pCanonDigits);
}

static void Formatter_FormatUnsignedInteger(struct Formatter* _Nonnull self, int radix, bool isUppercase, const struct ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    uint64_t v64;
    uint32_t v32;
    int nbits;
    char* pCanonDigits;

    switch (spec->lengthModifier) {
        case LENGTH_MODIFIER_hh:    v32 = (uint32_t)(unsigned char)va_arg(*ap, unsigned int); nbits = UINT32_WIDTH;     break;
        case LENGTH_MODIFIER_h:     v32 = (uint32_t)(unsigned short)va_arg(*ap, unsigned int); nbits = UINT32_WIDTH;    break;
        case LENGTH_MODIFIER_none:  v32 = (uint32_t)va_arg(*ap, unsigned int); nbits = UINT32_WIDTH;                    break;
#if __ULONG_WIDTH == 64
        case LENGTH_MODIFIER_l:     v64 = (uint64_t)va_arg(*ap, unsigned long); nbits = UINT64_WIDTH;                   break;
#else
        case LENGTH_MODIFIER_l:     v32 = (uint32_t)va_arg(*ap, unsigned long); nbits = UINT32_WIDTH;                   break;
#endif
        case LENGTH_MODIFIER_ll:    v64 = (uint64_t)va_arg(*ap, unsigned long long); nbits = UINT64_WIDTH;              break;
#if __SIZE_WIDTH == 64
        case LENGTH_MODIFIER_z:     v64 = (uint64_t)va_arg(*ap, size_t); nbits = SIZE_WIDTH; break;
#else
        case LENGTH_MODIFIER_z:     v32 = (uint32_t)va_arg(*ap, size_t); nbits = SIZE_WIDTH; break;
#endif
    }

    if (nbits == 64) {
        pCanonDigits = __ui64toa(v64, radix, isUppercase, self->digits);
    } else {
        pCanonDigits = __ui32toa(v32, radix, isUppercase, self->digits);
    }

    Formatter_FormatUnsignedIntegerField(self, radix, isUppercase, spec, pCanonDigits);
}

static void Formatter_FormatPointer(struct Formatter* _Nonnull self, const struct ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    struct ConversionSpec spec2 = *spec;
    spec2.flags.isAlternativeForm = true;
    spec2.flags.hasPrecision = true;
    spec2.flags.padWithZeros = true;

#if __INTPTR_WIDTH == 64
    char* pCanonDigits = __ui64toa((uint64_t)va_arg(*ap, void*), 16, false, self->digits);
    spec2.precision = 16;
#else
    char* pCanonDigits = __ui32toa((uint32_t)va_arg(*ap, void*), 16, false, self->digits);
    spec2.precision = 8;
#endif

    Formatter_FormatUnsignedIntegerField(self, 16, false, &spec2, pCanonDigits);
}

static void Formatter_FormatArgument(struct Formatter* _Nonnull self, char conversion, const struct ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    switch (conversion) {
        case '%':   Formatter_WriteChar(self, '%'); break;
        case 'c':   Formatter_FormatChar(self, spec, ap); break;
        case 's':   Formatter_FormatString(self, spec, ap); break;
        case 'd':   Formatter_FormatSignedInteger(self, spec, ap); break;
        case 'o':   Formatter_FormatUnsignedInteger(self, 8, false, spec, ap); break;
        case 'x':   Formatter_FormatUnsignedInteger(self, 16, false, spec, ap); break;
        case 'X':   Formatter_FormatUnsignedInteger(self, 16, true, spec, ap); break;
        case 'u':   Formatter_FormatUnsignedInteger(self, 10, false, spec, ap); break;
        case 'p':   Formatter_FormatPointer(self, spec, ap); break;
        default:    break;
    }
}

void Formatter_vFormat(struct Formatter* _Nonnull self, const char* _Nonnull format, va_list ap)
{
    struct ConversionSpec spec;

    for(;;) {
        const char ch = *format++;

        switch (ch) {
            case '\0':
                return;

            case '%':
                format = Formatter_ParseConversionSpec(self, format, &ap, &spec);
                Formatter_FormatArgument(self, *format++, &spec, &ap);
                break;

            default:
                Formatter_WriteChar(self, ch);
                break;
        }
    }
}
