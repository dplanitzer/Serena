//
//  Formatter.c
//  libc
//
//  Created by Dietmar Planitzer on 1/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Formatter.h"
#include <limits.h>
#include <stdlib.h>


void __Formatter_Init(FormatterRef _Nonnull self, Formatter_SinkFunc _Nonnull pSinkFunc, void * _Nullable pContext)
{
    self->sink = pSinkFunc;
    self->context = pContext;
    self->charactersWritten = 0;
    self->bufferCapacity = FORMATTER_BUFFER_CAPACITY;
    self->bufferCount = 0;
}

void __Formatter_Deinit(FormatterRef _Nullable self)
{
    self->sink = NULL;
    self->context = NULL;
}

static errno_t Formatter_Flush(FormatterRef _Nonnull self)
{
    decl_try_err();

    if (self->bufferCount > 0) {
        try(self->sink(self, self->buffer, self->bufferCount));
        self->bufferCount = 0;
    }

catch:
    return err;
}

static errno_t Formatter_WriteChar(FormatterRef _Nonnull self, char ch)
{
    decl_try_err();

    if (self->bufferCount == self->bufferCapacity) {
        try(Formatter_Flush(self));
    }
    self->buffer[self->bufferCount++] = ch;
    self->charactersWritten++;

catch:
    return err;
}

static errno_t Formatter_WriteString(FormatterRef _Nonnull self, const char *str, size_t maxChars)
{
    decl_try_err();

    while (maxChars-- > 0) {
        const char ch = *str++;

        if (ch != '\0') {
            try(Formatter_WriteChar(self, ch));
        } else {
            break;
        }
    }

catch:
    return err;
}

static errno_t Formatter_WriteRepChar(FormatterRef _Nonnull self, char ch, int count)
{
    decl_try_err();

    while(count-- > 0) {
        try(Formatter_WriteChar(self, ch));
    }

catch:
    return err;
}

static const char* _Nonnull Formatter_ParseLengthModifier(FormatterRef _Nonnull self, const char * _Nonnull format, ConversionSpec* _Nonnull spec)
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
    else if (ch == 'j') {
        format++;
        // intmax_t, uintmax_t
        spec->lengthModifier = LENGTH_MODIFIER_j;
    }
    else if (ch == 'z') {
        format++;
        // ssize_t, size_t
        spec->lengthModifier = LENGTH_MODIFIER_z;
    }
    else if (ch == 't') {
        format++;
        // ptrdiff_t
        spec->lengthModifier = LENGTH_MODIFIER_t;
    }
    else if (ch == 'L') {
        format++;
        // long double
        spec->lengthModifier = LENGTH_MODIFIER_L;
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

static errno_t Formatter_FormatStringField(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, const char* _Nonnull str, size_t slen)
{
    decl_try_err();
    const size_t nspaces = (spec->minimumFieldWidth > slen) ? spec->minimumFieldWidth - slen : 0;

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        try(Formatter_WriteRepChar(self, ' ', nspaces));
    }

    try(Formatter_WriteString(self, str, slen));

    if (nspaces > 0 && !spec->flags.isLeftJustified) {
        try(Formatter_WriteRepChar(self, ' ', nspaces));
    }

catch:
    return err;
}

static errno_t Formatter_FormatSignedIntegerField(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, const char* _Nonnull pCanonDigits)
{
    decl_try_err();
    int nSign = 1;
    int nDigits = pCanonDigits[0] - 1;
    int nLeadingZeros = (spec->flags.hasPrecision) ? __max(spec->precision - nDigits, 0) : 0;
    const char* pSign = &pCanonDigits[1];
    const char* pDigits = &pCanonDigits[2];
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
        try(Formatter_WriteRepChar(self, ' ', nspaces));
    }

    if (nSign > 0) {
        try(Formatter_WriteChar(self, *pSign));
    }
    if (!isEmpty) {
        if (nLeadingZeros > 0) {
            try(Formatter_WriteRepChar(self, '0', nLeadingZeros));
        }
        while(nDigits-- > 0) {
            try(Formatter_WriteChar(self, *pDigits++));
        }
    }

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        try(Formatter_WriteRepChar(self, ' ', nspaces));
    }

catch:
    return err;
}

static errno_t Formatter_FormatUnsignedIntegerField(FormatterRef _Nonnull self, int radix, bool isUppercase, const ConversionSpec* _Nonnull spec, const char* _Nonnull pCanonDigits)
{
    decl_try_err();
    int nRadixChars = 0;
    int nDigits = pCanonDigits[0] - 1;
    int nLeadingZeros = (spec->flags.hasPrecision) ? __max(spec->precision - nDigits, 0) : 0;
    const char* pRadixChars = "";
    const char* pDigits = &pCanonDigits[2];
    const bool isEmpty = spec->flags.hasPrecision && spec->precision == 0 && pDigits[0] == '0' && nDigits == 1;

    if (spec->flags.isAlternativeForm) {
        switch (radix) {
            case 8: nRadixChars = 1; pRadixChars = "0"; break;
            case 16: nRadixChars = 2; pRadixChars = (isUppercase) ? "0X" : "0x";
            default: break;
        }
    }

    const int slen = nRadixChars + nLeadingZeros + nDigits;
    int nspaces = (spec->minimumFieldWidth > slen) ? spec->minimumFieldWidth - slen : 0;

    if (spec->flags.padWithZeros && !spec->flags.hasPrecision && !spec->flags.isLeftJustified) {
        nLeadingZeros = nspaces;
        nspaces = 0;
    }

    if (nspaces > 0 && !spec->flags.isLeftJustified) {
        try(Formatter_WriteRepChar(self, ' ', nspaces));
    }

    if (!isEmpty) {
        while (nRadixChars-- > 0) {
            try(Formatter_WriteChar(self, *pRadixChars++));
        }
        if (nLeadingZeros > 0) {
            try(Formatter_WriteRepChar(self, '0', nLeadingZeros));
        }
        while(nDigits-- > 0) {
            try(Formatter_WriteChar(self, *pDigits++));
        }
    }
    else if (radix == 8) {
        try(Formatter_WriteChar(self, '0'));
    }

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        try(Formatter_WriteRepChar(self, ' ', nspaces));
    }

catch:
    return err;
}

static errno_t Formatter_FormatChar(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    const char ch = (unsigned char) va_arg(*ap, int);
    return Formatter_FormatStringField(self, spec, &ch, 1);
}

static errno_t Formatter_FormatString(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    return Formatter_FormatStringField(self, spec, va_arg(*ap, const char*), (spec->flags.hasPrecision) ? spec->precision : SIZE_MAX);
}

static errno_t Formatter_FormatSignedInteger(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    int64_t v64;
    int32_t v32;
    bool is32bit;
    char * pCanonDigits;

    switch (spec->lengthModifier) {
        case LENGTH_MODIFIER_hh:    v32 = (int32_t)(signed char)va_arg(*ap, int); is32bit = true;   break;
        case LENGTH_MODIFIER_h:     v32 = (int32_t)(short)va_arg(*ap, int); is32bit = true;         break;
        case LENGTH_MODIFIER_none:  v32 = (int32_t)va_arg(*ap, int); is32bit = true;                break;
        case LENGTH_MODIFIER_l:     v32 = (int32_t)va_arg(*ap, long); is32bit = true;               break;
        case LENGTH_MODIFIER_ll:    v64 = (int64_t)va_arg(*ap, long long); is32bit = false;         break;
#if INTMAX_WIDTH == 32
        case LENGTH_MODIFIER_j:     v32 = (int32_t)va_arg(*ap, intmax_t); is32bit = true;   break;
#elif INTMAX_WIDTH == 64
        case LENGTH_MODIFIER_j:     v64 = (int64_t)va_arg(*ap, intmax_t); is32bit = false;  break;
#else
#error "unknown INTMAX_WIDTH"
#endif
#if __SSIZE_WIDTH == 32
        case LENGTH_MODIFIER_z:     v32 = (int32_t)va_arg(*ap, ssize_t); is32bit = true;    break;
#elif __SIZE_WIDTH == 64
        case LENGTH_MODIFIER_z:     v64 = (int64_t)va_arg(*ap, ssize_t); is32bit = false;   break;
#else
#error "unknown __SIZE_WIDTH"
#endif
#if __PTRDIFF_WIDTH == 32
        case LENGTH_MODIFIER_t:     v32 = (int32_t)va_arg(*ap, ptrdiff_t); is32bit = true; break;
#elif __PTRDIFF_WIDTH == 64
        case LENGTH_MODIFIER_t:     v64 = (int64_t)va_arg(*ap, ptrdiff_t); is32bit = false; break;
#else
#error "unknown __PTRDIFF_WIDTH"
#endif
        case LENGTH_MODIFIER_L:     v64 = (int64_t)va_arg(*ap, long long); is32bit = false; break;
    }

    if (is32bit) {
        pCanonDigits = __i32toa(v32, 10, false, self->digits);
    } else {
        pCanonDigits = __i64toa(v64, 10, false, self->digits);
    }

    return Formatter_FormatSignedIntegerField(self, spec, pCanonDigits);
}

static errno_t Formatter_FormatUnsignedInteger(FormatterRef _Nonnull self, int radix, bool isUppercase, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    uint64_t v64;
    uint32_t v32;
    bool is32bit;
    char * pCanonDigits;

    switch (spec->lengthModifier) {
        case LENGTH_MODIFIER_hh:    v32 = (uint32_t)(unsigned char)va_arg(*ap, unsigned int); is32bit = true;   break;
        case LENGTH_MODIFIER_h:     v32 = (uint32_t)(unsigned short)va_arg(*ap, unsigned int); is32bit = true;  break;
        case LENGTH_MODIFIER_none:  v32 = (uint32_t)va_arg(*ap, unsigned int); is32bit = true;                  break;
        case LENGTH_MODIFIER_l:     v32 = (uint32_t)va_arg(*ap, unsigned long); is32bit = true;                 break;
        case LENGTH_MODIFIER_ll:    v64 = (uint64_t)va_arg(*ap, unsigned long long); is32bit = false;           break;
#if INTMAX_WIDTH == 32
        case LENGTH_MODIFIER_j:     v32 = (uint32_t)va_arg(*ap, uintmax_t); is32bit = true;     break;
#elif INTMAX_WIDTH == 64
        case LENGTH_MODIFIER_j:     v64 = (uint64_t)va_arg(*ap, uintmax_t); is32bit = false;    break;
#else
#error "unknown INTMAX_WIDTH"
#endif
#if __SSIZE_WIDTH == 32
        case LENGTH_MODIFIER_z:     v32 = (uint32_t)va_arg(*ap, size_t); is32bit = true;    break;
#elif __SIZE_WIDTH == 64
        case LENGTH_MODIFIER_z:     v64 = (uint64_t)va_arg(*ap, size_t); is32bit = false;   break;
#else
#error "unknown __SIZE_WIDTH"
#endif
#if __PTRDIFF_WIDTH == 32
        case LENGTH_MODIFIER_t:     v32 = (uint32_t)va_arg(*ap, ptrdiff_t); is32bit = true;     break;
#elif __PTRDIFF_WIDTH == 64
        case LENGTH_MODIFIER_t:     v64 = (uint64_t)va_arg(*ap, ptrdiff_t); is32bit = false;    break;
#else
#error "unknown __PTRDIFF_WIDTH"
#endif
        case LENGTH_MODIFIER_L:     v64 = (uint64_t)va_arg(*ap, unsigned long long); is32bit = false;   break;
    }

    if (is32bit) {
        pCanonDigits = __ui32toa(v32, radix, isUppercase, self->digits);
    } else {
        pCanonDigits = __ui64toa(v64, radix, isUppercase, self->digits);
    }

    return Formatter_FormatUnsignedIntegerField(self, radix, isUppercase, spec, pCanonDigits);
}

static errno_t Formatter_FormatPointer(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    ConversionSpec spec2 = *spec;
    spec2.flags.isAlternativeForm = true;
    spec2.flags.padWithZeros = true;

#if __VOID_PTR_WIDTH == 32
    char* pCanonDigits = __ui32toa((uint32_t)va_arg(*ap, void*), 16, false, self->digits);
    spec2.precision = 8;
#elif __VOID_PTR_WIDTH == 64
    char* pCanonDigits = __ui64toa((uint64_t)va_arg(*ap, void*), 16, false, self->digits);
    spec2.precision = 16;
#else
#error "unknown __VOID_PTR_WIDTH"
#endif

    return Formatter_FormatUnsignedIntegerField(self, 16, false, &spec2, pCanonDigits);
}

static errno_t Formatter_WriteNumberOfCharactersWritten(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    char* p = va_arg(*ap, char*);
    size_t n = self->charactersWritten;

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
    return 0;
}

static errno_t Formatter_FormatArgument(FormatterRef _Nonnull self, char conversion, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    switch (conversion) {
        case '%':   return Formatter_WriteChar(self, '%');
        case 'c':   return Formatter_FormatChar(self, spec, ap);
        case 's':   return Formatter_FormatString(self, spec, ap);
        case 'd':   // fall through
        case 'i':   return Formatter_FormatSignedInteger(self, spec, ap);
        case 'o':   return Formatter_FormatUnsignedInteger(self, 8, false, spec, ap);
        case 'x':   return Formatter_FormatUnsignedInteger(self, 16, false, spec, ap);
        case 'X':   return Formatter_FormatUnsignedInteger(self, 16, true, spec, ap);
        case 'u':   return Formatter_FormatUnsignedInteger(self, 10, false, spec, ap);
        case 'f':   break; // XXX
        case 'F':   break; // XXX
        case 'e':   break; // XXX
        case 'E':   break; // XXX
        case 'a':   break; // XXX
        case 'A':   break; // XXX
        case 'g':   break; // XXX
        case 'G':   return 0; // XXX
        case 'n':   Formatter_WriteNumberOfCharactersWritten(self, spec, ap);
        case 'p':   return Formatter_FormatPointer(self, spec, ap);
        default:    return 0;
    }
}

errno_t __Formatter_vFormat(FormatterRef _Nonnull self, const char* _Nonnull format, va_list ap)
{
    decl_try_err();
    ConversionSpec spec;
    char ch;

    while ((ch = *format) != '\0') {
        if (ch == '%') {
            format = Formatter_ParseConversionSpec(self, ++format, &ap, &spec);
            try(Formatter_FormatArgument(self, *format++, &spec, &ap));
        }
        else {
            try(Formatter_WriteChar(self, ch));
            format++;
        }
    }

    return Formatter_Flush(self);

catch:
    return err;
}
