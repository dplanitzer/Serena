//
//  Formatter.c
//  libc
//
//  Created by Dietmar Planitzer on 1/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Formatter.h"


void Formatter_Init(FormatterRef _Nonnull self, Formatter_SinkFunc _Nonnull pSinkFunc, void * _Nullable pContext, Character* _Nonnull pBuffer, Int bufferCapacity)
{
    self->sink = pSinkFunc;
    self->context = pContext;
    self->charactersWritten = 0;
    self->buffer = pBuffer;
    self->bufferCapacity = bufferCapacity;
    self->bufferCount = 0;
}

static ErrorCode Formatter_Flush(FormatterRef _Nonnull self)
{
    decl_try_err();

    if (self->bufferCount > 0) {
        try(self->sink(self, self->buffer, self->bufferCount));
        self->bufferCount = 0;
    }

catch:
    return err;
}

static ErrorCode Formatter_WriteChar(FormatterRef _Nonnull self, Character ch)
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

static ErrorCode Formatter_WriteString(FormatterRef _Nonnull self, const Character * _Nonnull str, ByteCount maxChars)
{
    decl_try_err();

    while (maxChars-- > 0) {
        const Character ch = *str++;

        if (ch != '\0') {
            try(Formatter_WriteChar(self, ch));
        } else {
            break;
        }
    }

catch:
    return err;
}

static ErrorCode Formatter_WriteRepChar(FormatterRef _Nonnull self, Character ch, Int count)
{
    decl_try_err();

    while(count-- > 0) {
        try(Formatter_WriteChar(self, ch));
    }

catch:
    return err;
}

static const Character* _Nonnull Formatter_ParseLengthModifier(FormatterRef _Nonnull self, const Character* _Nonnull format, ConversionSpec* _Nonnull spec)
{
    Character ch = *format;
    
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
        // ByteCount
        spec->lengthModifier = LENGTH_MODIFIER_z;
    }
    
    return format;
}

// Expects that 'format' points to the first character after the '%'.
static const Character* _Nonnull Formatter_ParseConversionSpec(FormatterRef _Nonnull self, const Character* _Nonnull format, va_list* _Nonnull ap, ConversionSpec* _Nonnull spec)
{
    Character ch;

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
    Bool done = false;
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
        spec->minimumFieldWidth = (ByteCount)atoi(format, &format, 10);
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
            spec->precision = (ByteCount)atoi(format, &format, 10);
        }
        spec->flags.hasPrecision = true;
    }
    
    // Length modifier
    return Formatter_ParseLengthModifier(self, format, spec);
}

static ErrorCode Formatter_FormatStringField(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, const Character* _Nonnull str, ByteCount slen)
{
    decl_try_err();
    const ByteCount nspaces = (spec->minimumFieldWidth > slen) ? spec->minimumFieldWidth - slen : 0;

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

static ErrorCode Formatter_FormatSignedIntegerField(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, const Character* _Nonnull pCanonDigits)
{
    decl_try_err();
    int nSign = 1;
    int nDigits = pCanonDigits[0] - 1;
    int nLeadingZeros = (spec->flags.hasPrecision) ? __max(spec->precision - nDigits, 0) : 0;
    const Character* pSign = &pCanonDigits[1];
    const Character* pDigits = &pCanonDigits[2];

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
    if (nLeadingZeros > 0) {
        try(Formatter_WriteRepChar(self, '0', nLeadingZeros));
    }
    while(nDigits-- > 0) {
        try(Formatter_WriteChar(self, *pDigits++));
    }

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        try(Formatter_WriteRepChar(self, ' ', nspaces));
    }

catch:
    return err;
}

static ErrorCode Formatter_FormatUnsignedIntegerField(FormatterRef _Nonnull self, Int radix, Bool isUppercase, const ConversionSpec* _Nonnull spec, const Character* _Nonnull pCanonDigits)
{
    decl_try_err();
    Int nRadixChars = 0;
    Int nDigits = pCanonDigits[0] - 1;
    Int nLeadingZeros = (spec->flags.hasPrecision) ? __max(spec->precision - nDigits, 0) : 0;
    const Character* pRadixChars = "";
    const Character* pDigits = &pCanonDigits[2];

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

    while (nRadixChars-- > 0) {
        try(Formatter_WriteChar(self, *pRadixChars++));
    }
    if (nLeadingZeros > 0) {
        try(Formatter_WriteRepChar(self, '0', nLeadingZeros));
    }
    while(nDigits-- > 0) {
        try(Formatter_WriteChar(self, *pDigits++));
    }

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        try(Formatter_WriteRepChar(self, ' ', nspaces));
    }

catch:
    return err;
}

static ErrorCode Formatter_FormatChar(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    const Character ch = (Character) va_arg(*ap, int);
    return Formatter_FormatStringField(self, spec, &ch, 1);
}

static ErrorCode Formatter_FormatString(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    return Formatter_FormatStringField(self, spec, va_arg(*ap, const Character*), (spec->flags.hasPrecision) ? spec->precision : BYTE_COUNT_MAX);
}

static ErrorCode Formatter_FormatSignedInteger(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    Int64 v64;
    Int32 v32;
    Int nbits;
    Character * pCanonDigits;

    switch (spec->lengthModifier) {
        case LENGTH_MODIFIER_hh:    v32 = (Int32)(signed char)va_arg(*ap, int); nbits = INT32_WIDTH;   break;
        case LENGTH_MODIFIER_h:     v32 = (Int32)(short)va_arg(*ap, int); nbits = INT32_WIDTH;         break;
        case LENGTH_MODIFIER_none:  v32 = (Int32)va_arg(*ap, int); nbits = INT32_WIDTH;                break;
#if __LONG_WIDTH == 64
        case LENGTH_MODIFIER_l:     v64 = (Int64)va_arg(*ap, long); nbits = INT64_WIDTH;               break;
#else
        case LENGTH_MODIFIER_l:     v32 = (Int32)va_arg(*ap, long); nbits = INT32_WIDTH;               break;
#endif
        case LENGTH_MODIFIER_ll:    v64 = (Int64)va_arg(*ap, long long); nbits = INT64_WIDTH;          break;
#if __SIZE_WIDTH == 64
        case LENGTH_MODIFIER_z:     v64 = (Int64)va_arg(*ap, ByteCount); nbits = BYTE_COUNT_WIDTH; break;
#else
        case LENGTH_MODIFIER_z:     v32 = (Int32)va_arg(*ap, ByteCount); nbits = BYTE_COUNT_WIDTH; break;
#endif
    }

    if (nbits == 64) {
        pCanonDigits = __i64toa(v64, 10, false, self->digits);
    } else {
        pCanonDigits = __i32toa(v32, 10, false, self->digits);
    }

    return Formatter_FormatSignedIntegerField(self, spec, pCanonDigits);
}

static ErrorCode Formatter_FormatUnsignedInteger(FormatterRef _Nonnull self, Int radix, Bool isUppercase, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    UInt64 v64;
    UInt32 v32;
    Int nbits;
    Character * pCanonDigits;

    switch (spec->lengthModifier) {
        case LENGTH_MODIFIER_hh:    v32 = (UInt32)(unsigned char)va_arg(*ap, unsigned int); nbits = UINT32_WIDTH;     break;
        case LENGTH_MODIFIER_h:     v32 = (UInt32)(unsigned short)va_arg(*ap, unsigned int); nbits = UINT32_WIDTH;    break;
        case LENGTH_MODIFIER_none:  v32 = (UInt32)va_arg(*ap, unsigned int); nbits = UINT32_WIDTH;                    break;
#if __ULONG_WIDTH == 64
        case LENGTH_MODIFIER_l:     v64 = (UInt64)va_arg(*ap, unsigned long); nbits = UINT64_WIDTH;                   break;
#else
        case LENGTH_MODIFIER_l:     v32 = (UInt32)va_arg(*ap, unsigned long); nbits = UINT32_WIDTH;                   break;
#endif
        case LENGTH_MODIFIER_ll:    v64 = (UInt64)va_arg(*ap, unsigned long long); nbits = UINT64_WIDTH;              break;
    }

    if (nbits == 64) {
        pCanonDigits = __ui64toa(v64, radix, isUppercase, self->digits);
    } else {
        pCanonDigits = __ui32toa(v32, radix, isUppercase, self->digits);
    }

    return Formatter_FormatUnsignedIntegerField(self, radix, isUppercase, spec, pCanonDigits);
}

static ErrorCode Formatter_FormatPointer(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    ConversionSpec spec2 = *spec;
    spec2.flags.isAlternativeForm = true;
    spec2.flags.padWithZeros = true;

#if __VOID_PTR_WIDTH == 64
    char* pCanonDigits = __ui64toa((UInt64)va_arg(*ap, void*), 16, false, self->digits);
    spec2.precision = 16;
#else
    char* pCanonDigits = __ui32toa((UInt32)va_arg(*ap, void*), 16, false, self->digits);
    spec2.precision = 8;
#endif

    return Formatter_FormatUnsignedIntegerField(self, 16, false, &spec2, pCanonDigits);
}

static ErrorCode Formatter_WriteNumberOfCharactersWritten(FormatterRef _Nonnull self, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
{
    Character* p = va_arg(*ap, Character*);
    ByteCount n = self->charactersWritten;

    switch (spec->lengthModifier) {
        case LENGTH_MODIFIER_hh:    *((signed char*)p) = __min(n, INT8_MAX);        break;
        case LENGTH_MODIFIER_h:     *((short*)p) = __min(n, INT16_MAX);             break;
        case LENGTH_MODIFIER_none:  *((int*)p) = __min(n, INT32_MAX);               break;
#if __LONG_WIDTH == 64
        case LENGTH_MODIFIER_l:     *((long*)p) = __min(n, INT64_MAX);              break;
#else
        case LENGTH_MODIFIER_l:     *((long*)p) = __min(n, INT32_MAX);              break;
#endif
        case LENGTH_MODIFIER_ll:    *((long long*)p) = __min(n, INT64_MAX);         break;
        case LENGTH_MODIFIER_z:     *((ByteCount*)p) = __min(n, BYTE_COUNT_MAX);    break;
    }
    return 0;
}

static ErrorCode Formatter_FormatArgument(FormatterRef _Nonnull self, Character conversion, const ConversionSpec* _Nonnull spec, va_list* _Nonnull ap)
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
        case 'n':   Formatter_WriteNumberOfCharactersWritten(self, spec, ap);
        case 'p':   return Formatter_FormatPointer(self, spec, ap);
        default:    return 0;
    }
}

ErrorCode Formatter_vFormat(FormatterRef _Nonnull self, const char* _Nonnull format, va_list ap)
{
    decl_try_err();
    ConversionSpec spec;
    Character ch;

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
