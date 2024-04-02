//
//  Formatter.c
//  libc
//
//  Created by Dietmar Planitzer on 1/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Formatter.h"


void Formatter_Init(FormatterRef _Nonnull self, Formatter_SinkFunc _Nonnull pSinkFunc, void * _Nullable pContext, char* _Nonnull pBuffer, int bufferCapacity)
{
    self->sink = pSinkFunc;
    self->context = pContext;
    self->charactersWritten = 0;
    self->buffer = pBuffer;
    self->bufferCapacity = bufferCapacity;
    self->bufferCount = 0;
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

static errno_t Formatter_WriteString(FormatterRef _Nonnull self, const char * _Nonnull str, ssize_t maxChars)
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

static const char* _Nonnull Formatter_ParseLengthModifier(FormatterRef _Nonnull self, const char* _Nonnull format, ConversionSpec* _Nonnull spec)
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
static const char* _Nonnull Formatter_ParseConversionSpec(FormatterRef _Nonnull self, const char* _Nonnull format, va_list* _Nonnull ap, ConversionSpec* _Nonnull spec)
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

static errno_t Formatter_FormatStringField(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, const char* _Nonnull str, ssize_t slen)
{
    decl_try_err();
    const ssize_t nspaces = (spec->minimumFieldWidth > slen) ? spec->minimumFieldWidth - slen : 0;

    if (nspaces > 0) {
        try(Formatter_WriteRepChar(self, ' ', nspaces));
    }

    try(Formatter_WriteString(self, str, slen));

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
        try(Formatter_WriteRepChar(self, ' ', nspaces));
    }

    if (nSign > 0) {
        try(Formatter_WriteChar(self, *pSign));
    }
    if (nLeadingZeros > 0) {
        try(Formatter_WriteRepChar(self, '0', nLeadingZeros));
    }
    while(nDigits-- > 0) {
        try(Formatter_WriteChar(self, *pDigits++));
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
        try(Formatter_WriteRepChar(self, ' ', nspaces));
    }

    while (nRadixChars-- > 0) {
        try(Formatter_WriteChar(self, *pRadixChars++));
    }
    if (nLeadingZeros > 0) {
        try(Formatter_WriteRepChar(self, '0', nLeadingZeros));
    }
    while(nDigits-- > 0) {
        try(Formatter_WriteChar(self, *pDigits++));
    }

catch:
    return err;
}

static errno_t Formatter_FormatChar(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    const char ch = (char) va_arg(*ap, int);
    return Formatter_FormatStringField(self, spec, &ch, 1);
}

static errno_t Formatter_FormatString(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    return Formatter_FormatStringField(self, spec, va_arg(*ap, const char*), (spec->flags.hasPrecision) ? spec->precision : SSIZE_MAX);
}

static errno_t Formatter_FormatSignedInteger(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
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
        case LENGTH_MODIFIER_ll:    v64 = (int64_t)va_arg(*ap, long long); nbits = INT64_WIDTH;          break;
#if __SIZE_WIDTH == 64
        case LENGTH_MODIFIER_z:     v64 = (int64_t)va_arg(*ap, ssize_t); nbits = SSIZE_WIDTH; break;
#else
        case LENGTH_MODIFIER_z:     v32 = (int32_t)va_arg(*ap, ssize_t); nbits = SSIZE_WIDTH; break;
#endif
    }

    if (nbits == 64) {
        pCanonDigits = __i64toa(v64, 10, false, self->digits);
    } else {
        pCanonDigits = __i32toa(v32, 10, false, self->digits);
    }

    return Formatter_FormatSignedIntegerField(self, spec, pCanonDigits);
}

static errno_t Formatter_FormatUnsignedInteger(FormatterRef _Nonnull self, int radix, bool isUppercase, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
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

    return Formatter_FormatUnsignedIntegerField(self, radix, isUppercase, spec, pCanonDigits);
}

static errno_t Formatter_FormatPointer(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    ConversionSpec spec2 = *spec;
    spec2.flags.isAlternativeForm = true;
    spec2.flags.padWithZeros = true;

#if __INTPTR_WIDTH == 64
    char* pCanonDigits = __ui64toa((uint64_t)va_arg(*ap, void*), 16, false, self->digits);
    spec2.precision = 16;
#else
    char* pCanonDigits = __ui32toa((uint32_t)va_arg(*ap, void*), 16, false, self->digits);
    spec2.precision = 8;
#endif

    return Formatter_FormatUnsignedIntegerField(self, 16, false, &spec2, pCanonDigits);
}

static errno_t Formatter_FormatArgument(FormatterRef _Nonnull self, char conversion, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    switch (conversion) {
        case '%':   return Formatter_WriteChar(self, '%');
        case 'c':   return Formatter_FormatChar(self, spec, ap);
        case 's':   return Formatter_FormatString(self, spec, ap);
        case 'd':   return Formatter_FormatSignedInteger(self, spec, ap);
        case 'o':   return Formatter_FormatUnsignedInteger(self, 8, false, spec, ap);
        case 'x':   return Formatter_FormatUnsignedInteger(self, 16, false, spec, ap);
        case 'X':   return Formatter_FormatUnsignedInteger(self, 16, true, spec, ap);
        case 'u':   return Formatter_FormatUnsignedInteger(self, 10, false, spec, ap);
        case 'p':   return Formatter_FormatPointer(self, spec, ap);
        default:    return 0;
    }
}

errno_t Formatter_vFormat(FormatterRef _Nonnull self, const char* _Nonnull format, va_list ap)
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
