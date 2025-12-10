//
//  Formatter.c
//  libc
//
//  Created by Dietmar Planitzer on 1/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Formatter.h"
#include "Stream.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>


static void Formatter_WriteChar(FormatterRef _Nonnull self, char ch)
{
    if (self->res > 0) {
        self->res = __fputc(ch, self->stream);

        if (self->res == 1) {
            self->charactersWritten++;
        }
     }
}

static void Formatter_WriteString(FormatterRef _Nonnull self, const char * _Nonnull str)
{
    size_t i = 0;

    for (;;) {
        const char ch = *str++;

        if (ch == '\0') {
            break;
        }

        Formatter_WriteChar(self, ch);
        if (self->res <= 0) {
            break;
        }

        self->charactersWritten++;
    }
}

static void Formatter_WriteStringPrefix(FormatterRef _Nonnull self, const char * _Nonnull str, size_t maxChars)
{
    size_t i = 0;

    for (;;) {
        const char ch = *str++;
        
        if (ch == '\0') {
            break;
        }

        Formatter_WriteChar(self, ch);
        if (self->res <= 0) {
             break;
        }

        i++;
        if (i == maxChars) {
            break;
        }
    }
    self->charactersWritten += i;
}

static void Formatter_WriteRepChar(FormatterRef _Nonnull self, char ch, int count)
{
    int i = 0;

    while(i < count) {
        Formatter_WriteChar(self, ch);
        if (self->res <= 0) {
            break;
        }
        i++;
    }
    self->charactersWritten += i;
}

static const char* _Nonnull Formatter_ParseLengthModifier(FormatterRef _Nonnull self, const char * _Nonnull format, ConversionSpec* _Nonnull spec)
{
    switch (*format) {
        case 'l':
            format++;
            if (*format == 'l') {
                format++;
                spec->lengthModifier = LENGTH_MODIFIER_ll;
            } else {
                spec->lengthModifier = LENGTH_MODIFIER_l;
            }
            break;

        case 'h':
            format++;
            if (*format == 'h') {
                format++;
                spec->lengthModifier = LENGTH_MODIFIER_hh;
            } else {
                spec->lengthModifier = LENGTH_MODIFIER_h;
            }
            break;

        case 'j':
            format++;
            spec->lengthModifier = LENGTH_MODIFIER_j;
            break;

        case 'z':
            format++;
            spec->lengthModifier = LENGTH_MODIFIER_z;
            break;

        case 't':
            format++;
            spec->lengthModifier = LENGTH_MODIFIER_t;
            break;

        case 'L':
            format++;
            spec->lengthModifier = LENGTH_MODIFIER_L;
            break;

        default:
            break;
    }
    
    return format;
}

// Expects that 'format' points to the first character after the '%'.
static const char* _Nonnull Formatter_ParseConversionSpec(FormatterRef _Nonnull self, const char* _Nonnull format, va_list* _Nonnull ap, ConversionSpec* _Nonnull spec)
{
    char ch;

    spec->minimumFieldWidth = 0;
    spec->precision = 0;
    spec->lengthModifier = LENGTH_MODIFIER_none;
    spec->flags.isLeftJustified = false;
    spec->flags.alwaysShowSign = false;
    spec->flags.showSpaceIfPositive = false;
    spec->flags.isAlternativeForm = false;
    spec->flags.padWithZeros = false;
    spec->flags.hasPrecision = false;

    // Flags
    bool done = false;
    while (!done) {
        ch = *format++;
        
        switch (ch) {
            case '\0':  return --format;
            case '-':   spec->flags.isLeftJustified = true; break;
            case '+':   spec->flags.alwaysShowSign = true; break;
            case ' ':   spec->flags.showSpaceIfPositive = true; break;
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
        spec->minimumFieldWidth = (int)strtol(format, &format, 10);
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
            spec->precision = (int)strtol(format, &format, 10);
        }
        spec->flags.hasPrecision = true;
    }
    
    // Length modifier
    return Formatter_ParseLengthModifier(self, format, spec);
}

static void Formatter_FormatSignedIntegerField(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, const char* _Nonnull buf, size_t len)
{
    int nSign = 1;
    int nDigits = len - 1;
    int nLeadingZeros = (spec->flags.hasPrecision) ? __max(spec->precision - nDigits, 0) : 0;
    const char* pSign = buf;
    const char* pDigits = &buf[1];
    const bool isEmpty = spec->flags.hasPrecision && spec->precision == 0 && pDigits[0] == '0' && nDigits == 1;

    if (!spec->flags.alwaysShowSign && *pSign == '+') {
        if (spec->flags.showSpaceIfPositive) {
            pSign = " ";
        } else {
            pSign = "";
            nSign = 0;
        }
    }

    const int slen = nSign + nLeadingZeros + nDigits;
    int nspaces = (spec->minimumFieldWidth > slen) ? spec->minimumFieldWidth - slen : 0;

    if (spec->flags.padWithZeros && !spec->flags.hasPrecision && !spec->flags.isLeftJustified) {
        nLeadingZeros = nspaces;
        nspaces = 0;
    }

    if (nspaces > 0 && !spec->flags.isLeftJustified) {
        Formatter_WriteRepChar(self, ' ', nspaces);
    }

    if (nSign > 0) {
        Formatter_WriteChar(self, *pSign);
    }
    if (!isEmpty) {
        if (nLeadingZeros > 0) {
            Formatter_WriteRepChar(self, '0', nLeadingZeros);
        }
        Formatter_WriteString(self, pDigits);
    }

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        Formatter_WriteRepChar(self, ' ', nspaces);
    }
}

static void Formatter_FormatUnsignedIntegerField(FormatterRef _Nonnull self, int radix, bool isUppercase, const ConversionSpec* _Nonnull spec, const char* _Nonnull buf, size_t len)
{
    int nRadixChars = 0;
    int nLeadingZeros = (spec->flags.hasPrecision) ? __max(spec->precision - len, 0) : 0;
    const char* pRadixChars = "";
    const bool isEmpty = spec->flags.hasPrecision && spec->precision == 0 && buf[0] == '0' && len == 1;

    if (spec->flags.isAlternativeForm) {
        switch (radix) {
            case 8: nRadixChars = 1; pRadixChars = "0"; break;
            case 16: nRadixChars = 2; pRadixChars = (isUppercase) ? "0X" : "0x";
            default: break;
        }
    }

    const int slen = nRadixChars + nLeadingZeros + len;
    int nspaces = (spec->minimumFieldWidth > slen) ? spec->minimumFieldWidth - slen : 0;

    if (spec->flags.padWithZeros && !spec->flags.hasPrecision && !spec->flags.isLeftJustified) {
        nLeadingZeros = nspaces;
        nspaces = 0;
    }

    if (nspaces > 0 && !spec->flags.isLeftJustified) {
        Formatter_WriteRepChar(self, ' ', nspaces);
    }

    if (!isEmpty) {
        while (nRadixChars-- > 0) {
            Formatter_WriteChar(self, *pRadixChars++);
        }
        if (nLeadingZeros > 0) {
            Formatter_WriteRepChar(self, '0', nLeadingZeros);
        }
        Formatter_WriteString(self, buf);
    }
    else if (radix == 8) {
        Formatter_WriteChar(self, '0');
    }

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        Formatter_WriteRepChar(self, ' ', nspaces);
    }
}

static void Formatter_FormatChar(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    const char ch = (unsigned char) va_arg(*ap, int);
    const size_t nspaces = (spec->minimumFieldWidth > 1) ? spec->minimumFieldWidth - 1 : 0;

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        Formatter_WriteRepChar(self, ' ', nspaces);
    }

    Formatter_WriteChar(self, ch);

    if (nspaces > 0 && !spec->flags.isLeftJustified) {
        Formatter_WriteRepChar(self, ' ', nspaces);
    }
}

static void Formatter_FormatString(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    const char* str = va_arg(*ap, const char*);
    const size_t slen = (spec->flags.hasPrecision || spec->minimumFieldWidth > 0) ? strlen(str) : 0;
    const size_t adj_slen = (spec->flags.hasPrecision) ? __min(slen, spec->precision) : slen;
    const size_t nspaces = (spec->minimumFieldWidth > adj_slen) ? spec->minimumFieldWidth - adj_slen : 0;

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        Formatter_WriteRepChar(self, ' ', nspaces);
    }

    if (spec->flags.hasPrecision) {
        Formatter_WriteStringPrefix(self, str, adj_slen);
    }
    else {
        Formatter_WriteString(self, str);
    }

    if (nspaces > 0 && !spec->flags.isLeftJustified) {
        Formatter_WriteRepChar(self, ' ', nspaces);
    }
}

static void Formatter_FormatSignedInteger(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    int64_t v64;
    int32_t v32;
    int nbits;
    char * pCanonDigits;

    switch (spec->lengthModifier) {
        case LENGTH_MODIFIER_hh:    v32 = (int32_t)(signed char)va_arg(*ap, int); nbits = INT32_WIDTH;   break;
        case LENGTH_MODIFIER_h:     v32 = (int32_t)(short)va_arg(*ap, int); nbits = INT32_WIDTH;         break;
        case LENGTH_MODIFIER_none:  v32 = (int32_t)va_arg(*ap, int); nbits = INT32_WIDTH;                break;
#if __LONG_WIDTH == 64
        case LENGTH_MODIFIER_l:     v64 = (int64_t)va_arg(*ap, long); nbits = INT64_WIDTH;               break;
#else
        case LENGTH_MODIFIER_l:     v32 = (int32_t)va_arg(*ap, long); nbits = INT32_WIDTH;               break;
#endif
        case LENGTH_MODIFIER_L:
        case LENGTH_MODIFIER_ll:    v64 = (int64_t)va_arg(*ap, long long); nbits = INT64_WIDTH;          break;
#if INTMAX_WIDTH == 64
        case LENGTH_MODIFIER_j:     v64 = (int64_t)va_arg(*ap, intmax_t); nbits = INTMAX_WIDTH; break;
#else
        case LENGTH_MODIFIER_j:     v32 = (int32_t)va_arg(*ap, intmax_t); nbits = INTMAX_WIDTH; break;
#endif
#if __SIZE_WIDTH == 64
        case LENGTH_MODIFIER_z:     v64 = (int64_t)va_arg(*ap, ssize_t); nbits = __SSIZE_WIDTH; break;
#else
        case LENGTH_MODIFIER_z:     v32 = (int32_t)va_arg(*ap, ssize_t); nbits = __SSIZE_WIDTH; break;
#endif
#if __PTRDIFF_WIDTH == 64
        case LENGTH_MODIFIER_t:     v64 = (int64_t)va_arg(*ap, ptrdiff_t); nbits = __PTRDIFF_WIDTH; break;
#else
        case LENGTH_MODIFIER_t:     v32 = (int32_t)va_arg(*ap, ptrdiff_t); nbits = __PTRDIFF_WIDTH; break;
#endif
    }

    if (nbits == 64) {
        pCanonDigits = __i64toa(v64, ia_sign_plus_minus, &self->i64a);
    } else {
        pCanonDigits = __i32toa(v32, ia_sign_plus_minus, (i32a_t*)&self->i64a);
    }

    Formatter_FormatSignedIntegerField(self, spec, pCanonDigits, self->i64a.length);
}

static void Formatter_FormatUnsignedInteger(FormatterRef _Nonnull self, int radix, bool isUppercase, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    uint64_t v64;
    uint32_t v32;
    int nbits;
    char * pCanonDigits;

    switch (spec->lengthModifier) {
        case LENGTH_MODIFIER_hh:    v32 = (uint32_t)(unsigned char)va_arg(*ap, unsigned int); nbits = UINT32_WIDTH;     break;
        case LENGTH_MODIFIER_h:     v32 = (uint32_t)(unsigned short)va_arg(*ap, unsigned int); nbits = UINT32_WIDTH;    break;
        case LENGTH_MODIFIER_none:  v32 = (uint32_t)va_arg(*ap, unsigned int); nbits = UINT32_WIDTH;                    break;
#if __ULONG_WIDTH == 64
        case LENGTH_MODIFIER_l:     v64 = (uint64_t)va_arg(*ap, unsigned long); nbits = UINT64_WIDTH;                   break;
#else
        case LENGTH_MODIFIER_l:     v32 = (uint32_t)va_arg(*ap, unsigned long); nbits = UINT32_WIDTH;                   break;
#endif
        case LENGTH_MODIFIER_L:
        case LENGTH_MODIFIER_ll:    v64 = (uint64_t)va_arg(*ap, unsigned long long); nbits = UINT64_WIDTH;              break;
#if UINTMAX_WIDTH == 64
        case LENGTH_MODIFIER_j:     v64 = (uint64_t)va_arg(*ap, uintmax_t); nbits = UINTMAX_WIDTH;  break;
#else
        case LENGTH_MODIFIER_j:     v32 = (uint32_t)va_arg(*ap, uintmax_t); nbits = UINTMAX_WIDTH;  break;
#endif
#if __SIZE_WIDTH == 64
        case LENGTH_MODIFIER_z:     v64 = (uint64_t)va_arg(*ap, size_t); nbits = __SIZE_WIDTH;  break;
#else
        case LENGTH_MODIFIER_z:     v32 = (uint32_t)va_arg(*ap, size_t); nbits = __SIZE_WIDTH;  break;
#endif
#if __PTRDIFF_WIDTH == 64
        case LENGTH_MODIFIER_t:     v64 = (uint64_t)va_arg(*ap, ptrdiff_t); nbits = __PTRDIFF_WIDTH;    break;
#else
        case LENGTH_MODIFIER_t:     v32 = (uint32_t)va_arg(*ap, ptrdiff_t); nbits = __PTRDIFF_WIDTH;    break;
#endif
    }

    if (nbits == 64) {
        pCanonDigits = __u64toa(v64, radix, isUppercase, &self->i64a);
    } else {
        pCanonDigits = __u32toa(v32, radix, isUppercase, (i32a_t*)&self->i64a);
    }

    Formatter_FormatUnsignedIntegerField(self, radix, isUppercase, spec, pCanonDigits, self->i64a.length);
}

static void Formatter_FormatPointer(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    ConversionSpec spec2 = *spec;
    spec2.flags.isAlternativeForm = true;
    spec2.flags.hasPrecision = true;
    spec2.flags.padWithZeros = true;

#if __INTPTR_WIDTH == 64
    char* pCanonDigits = __u64toa((uint64_t)va_arg(*ap, void*), 16, false, &self->i64a);
    spec2.precision = 16;
#else
    char* pCanonDigits = __u32toa((uint32_t)va_arg(*ap, void*), 16, false, (i32a_t*)&self->i64a);
    spec2.precision = 8;
#endif

    Formatter_FormatUnsignedIntegerField(self, 16, false, &spec2, pCanonDigits, self->i64a.length);
}

static void Formatter_WriteNumberOfCharactersWritten(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    char* p = va_arg(*ap, char*);
    const size_t n = self->charactersWritten;

    switch (spec->lengthModifier) {
        case LENGTH_MODIFIER_hh:    *((signed char*)p) = __min(n, SCHAR_MAX);   break;
        case LENGTH_MODIFIER_h:     *((short*)p) = __min(n, SHRT_MAX);          break;
        case LENGTH_MODIFIER_none:  *((int*)p) = __min(n, INT_MAX);             break;
        case LENGTH_MODIFIER_l:     *((long*)p) = __min(n, LONG_MAX);           break;
        case LENGTH_MODIFIER_ll:    *((long long*)p) = __min(n, LLONG_MAX);     break;
        case LENGTH_MODIFIER_j:     *((intmax_t*)p) = __min(n, INTMAX_MAX);     break;
        case LENGTH_MODIFIER_z:     *((ssize_t*)p) = __min(n, __SSIZE_MAX);     break;
        case LENGTH_MODIFIER_t:     *((ptrdiff_t*)p) = __min(n, PTRDIFF_MAX);   break;
        case LENGTH_MODIFIER_L:     break;
    }
}

static void Formatter_FormatArgument(FormatterRef _Nonnull self, char conversion, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    switch (conversion) {
        case '%':   Formatter_WriteChar(self, '%'); break;
        case 'c':   Formatter_FormatChar(self, spec, ap); break;
        case 's':   Formatter_FormatString(self, spec, ap); break;
        case 'd':   // fall through
        case 'i':   Formatter_FormatSignedInteger(self, spec, ap); break;
        case 'o':   Formatter_FormatUnsignedInteger(self, 8, false, spec, ap); break;
        case 'x':   Formatter_FormatUnsignedInteger(self, 16, false, spec, ap); break;
        case 'X':   Formatter_FormatUnsignedInteger(self, 16, true, spec, ap); break;
        case 'u':   Formatter_FormatUnsignedInteger(self, 10, false, spec, ap); break;
        case 'f':   break; // XXX
        case 'F':   break; // XXX
        case 'e':   break; // XXX
        case 'E':   break; // XXX
        case 'a':   break; // XXX
        case 'A':   break; // XXX
        case 'g':   break; // XXX
        case 'G':   break; // XXX
        case 'n':   Formatter_WriteNumberOfCharactersWritten(self, spec, ap); break;
        case 'p':   Formatter_FormatPointer(self, spec, ap); break;
        default:    break;
    }
}

int __Formatter_vFormat(FormatterRef _Nonnull self, const char* _Nonnull format, va_list ap)
{
    ConversionSpec spec;

    while (self->res > 0) {
        switch (*format) {
            case '\0':
                return (self->charactersWritten > INT_MAX) ? INT_MAX : (int)self->charactersWritten;

            case '%':
                format = Formatter_ParseConversionSpec(self, ++format, &ap, &spec);
                Formatter_FormatArgument(self, *format++, &spec, &ap);
                break;

            default:
                while (true) {
                    const char ch = *format;

                    if (ch == '\0' || ch == '%') {
                        break;
                    }
                    Formatter_WriteChar(self, ch);
                    format++;
                }
                break;
        }
    }

    if (self->res == 0) {
        return FMTRES_EOF;
    }
    else {
        return FMTRES_ERR;
     }
}
