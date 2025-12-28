//
//  __fmt.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 1/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <ext/__fmt.h>
#include <ext/limits.h>
#include <ext/math.h>
#include <string.h>


static void _write_char(fmt_t* _Nonnull self, char ch)
{
    if ((!self->hasError && self->putc_cb(ch, self->stream) == 1) || (self->hasError && self->doContCountingOnError)) {
        self->charactersWritten++;
    }
    else {
        self->hasError = true;
    }
}

static void _write_string(fmt_t *_Nonnull _Restrict self, const char* _Nonnull _Restrict str, ssize_t len)
{
    if ((!self->hasError && self->write_cb(self->stream, str, len) == len) || (self->hasError && self->doContCountingOnError)) {
        self->charactersWritten += len;
    }
    else {
        self->hasError = true;
    }
}

static void _write_char_rep(fmt_t* _Nonnull self, char ch, int count)
{
    int i = 0;

    while(i < count) {
        if ((!self->hasError && self->putc_cb(ch, self->stream) == 1) || (self->hasError && self->doContCountingOnError)) {
            i++;
        }
        else {
            self->hasError = true;
        }
    }
    self->charactersWritten += i;
}

static const char* _Nonnull _parse_length_mod(fmt_t* _Nonnull _Restrict self, const char * _Nonnull _Restrict format)
{
    fmt_cspec_t* spec = &self->spec;

    switch (*format) {
        case 'l':
            format++;
            if (*format == 'l') {
                format++;
                spec->lengthModifier = FMT_LENMOD_ll;
            } else {
                spec->lengthModifier = FMT_LENMOD_l;
            }
            break;

        case 'h':
            format++;
            if (*format == 'h') {
                format++;
                spec->lengthModifier = FMT_LENMOD_hh;
            } else {
                spec->lengthModifier = FMT_LENMOD_h;
            }
            break;

        case 'j':
            format++;
            spec->lengthModifier = FMT_LENMOD_j;
            break;

        case 'z':
            format++;
            spec->lengthModifier = FMT_LENMOD_z;
            break;

        case 't':
            format++;
            spec->lengthModifier = FMT_LENMOD_t;
            break;

#if ___STDC_HOSTED__ ==1
        case 'L':   // long double
            format++;
            spec->lengthModifier = FMT_LENMOD_L;
            break;
#endif

        default:
            break;
    }
    
    return format;
}

static int _atoi(const char* _Nonnull _Restrict str, char* _Nonnull _Restrict * _Nonnull _Restrict str_end)
{
    long long r;

    if (__strtoi64(str, str_end, 10, LONG_MIN, LONG_MAX, __LONG_MAX_BASE_10_DIGITS, &r) == 0) {
        return (int) r;
    }
    else {
        return 0;
    }
}

// Expects that 'format' points to the first character after the '%'.
static const char* _Nonnull _parse_conv_spec(fmt_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict format, va_list* _Nonnull _Restrict ap)
{
    fmt_cspec_t* spec = &self->spec;
    char ch;

    spec->minimumFieldWidth = 0;
    spec->precision = 0;
    spec->lengthModifier = FMT_LENMOD_none;
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
        spec->minimumFieldWidth = _atoi(format, &format);
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
            spec->precision = _atoi(format, &format);
        }
        spec->flags.hasPrecision = true;
    }
    
    // Length modifier
    return _parse_length_mod(self, format);
}

static void _format_int_field(fmt_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict buf, size_t len)
{
    const fmt_cspec_t* spec = &self->spec;
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
        _write_char_rep(self, ' ', nspaces);
    }

    if (nSign > 0) {
        _write_char(self, *pSign);
    }
    if (!isEmpty) {
        if (nLeadingZeros > 0) {
            _write_char_rep(self, '0', nLeadingZeros);
        }
        _write_string(self, pDigits, nDigits);
    }

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        _write_char_rep(self, ' ', nspaces);
    }
}

static void _format_uint_field(fmt_t* _Nonnull _Restrict self, int radix, bool isUppercase, const char* _Nonnull _Restrict buf, size_t len)
{
    const fmt_cspec_t* spec = &self->spec;
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
        _write_char_rep(self, ' ', nspaces);
    }

    if (!isEmpty) {
        while (nRadixChars-- > 0) {
            _write_char(self, *pRadixChars++);
        }
        if (nLeadingZeros > 0) {
            _write_char_rep(self, '0', nLeadingZeros);
        }
        _write_string(self, buf, len);
    }
    else if (radix == 8) {
        _write_char(self, '0');
    }

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        _write_char_rep(self, ' ', nspaces);
    }
}

static void _format_char(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    const fmt_cspec_t* spec = &self->spec;
    const char ch = (unsigned char) va_arg(*ap, int);
    const size_t nspaces = (spec->minimumFieldWidth > 1) ? spec->minimumFieldWidth - 1 : 0;

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        _write_char_rep(self, ' ', nspaces);
    }

    _write_char(self, ch);

    if (nspaces > 0 && !spec->flags.isLeftJustified) {
        _write_char_rep(self, ' ', nspaces);
    }
}

static void _format_string(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    const fmt_cspec_t* spec = &self->spec;
    const char* str = va_arg(*ap, const char*);
    const size_t slen = strlen(str);
    const size_t flen = (spec->flags.hasPrecision || spec->minimumFieldWidth > 0) ? slen : 0;
    const size_t adj_flen = (spec->flags.hasPrecision) ? __min(flen, spec->precision) : flen;
    const size_t nspaces = (spec->minimumFieldWidth > adj_flen) ? spec->minimumFieldWidth - adj_flen : 0;

    if (nspaces > 0 && spec->flags.isLeftJustified) {
        _write_char_rep(self, ' ', nspaces);
    }

    _write_string(self, str, (spec->flags.hasPrecision) ? __min(slen, adj_flen) : slen);

    if (nspaces > 0 && !spec->flags.isLeftJustified) {
        _write_char_rep(self, ' ', nspaces);
    }
}

static void _format_int(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    int64_t v64;
    int32_t v32;
    int nbits;
    char * pCanonDigits;

    switch (self->spec.lengthModifier) {
        case FMT_LENMOD_hh:    v32 = (int32_t)(signed char)va_arg(*ap, int); nbits = INT32_WIDTH;   break;
        case FMT_LENMOD_h:     v32 = (int32_t)(short)va_arg(*ap, int); nbits = INT32_WIDTH;         break;
        case FMT_LENMOD_none:  v32 = (int32_t)va_arg(*ap, int); nbits = INT32_WIDTH;                break;
#if __LONG_WIDTH == 64
        case FMT_LENMOD_l:     v64 = (int64_t)va_arg(*ap, long); nbits = INT64_WIDTH;               break;
#else
        case FMT_LENMOD_l:     v32 = (int32_t)va_arg(*ap, long); nbits = INT32_WIDTH;               break;
#endif
        case FMT_LENMOD_L:
        case FMT_LENMOD_ll:    v64 = (int64_t)va_arg(*ap, long long); nbits = INT64_WIDTH;          break;
#if INTMAX_WIDTH == 64
        case FMT_LENMOD_j:     v64 = (int64_t)va_arg(*ap, intmax_t); nbits = INTMAX_WIDTH; break;
#else
        case FMT_LENMOD_j:     v32 = (int32_t)va_arg(*ap, intmax_t); nbits = INTMAX_WIDTH; break;
#endif
#if SSIZE_WIDTH == 64
        case FMT_LENMOD_z:     v64 = (int64_t)va_arg(*ap, ssize_t); nbits = SSIZE_WIDTH; break;
#else
        case FMT_LENMOD_z:     v32 = (int32_t)va_arg(*ap, ssize_t); nbits = SSIZE_WIDTH; break;
#endif
#if PTRDIFF_WIDTH == 64
        case FMT_LENMOD_t:     v64 = (int64_t)va_arg(*ap, ptrdiff_t); nbits = PTRDIFF_WIDTH; break;
#else
        case FMT_LENMOD_t:     v32 = (int32_t)va_arg(*ap, ptrdiff_t); nbits = PTRDIFF_WIDTH; break;
#endif
    }

    if (nbits == 64) {
        pCanonDigits = __i64toa(v64, ia_sign_plus_minus, &self->i64a);
    } else {
        pCanonDigits = __i32toa(v32, ia_sign_plus_minus, (i32a_t*)&self->i64a);
    }

    _format_int_field(self, pCanonDigits, self->i64a.length);
}

static void _format_uint(fmt_t* _Nonnull _Restrict self, int radix, bool isUppercase, va_list* _Nonnull _Restrict ap)
{
    uint64_t v64;
    uint32_t v32;
    int nbits;
    char * pCanonDigits;

    switch (self->spec.lengthModifier) {
        case FMT_LENMOD_hh:    v32 = (uint32_t)(unsigned char)va_arg(*ap, unsigned int); nbits = UINT32_WIDTH;     break;
        case FMT_LENMOD_h:     v32 = (uint32_t)(unsigned short)va_arg(*ap, unsigned int); nbits = UINT32_WIDTH;    break;
        case FMT_LENMOD_none:  v32 = (uint32_t)va_arg(*ap, unsigned int); nbits = UINT32_WIDTH;                    break;
#if ULONG_WIDTH == 64
        case FMT_LENMOD_l:     v64 = (uint64_t)va_arg(*ap, unsigned long); nbits = UINT64_WIDTH;                   break;
#else
        case FMT_LENMOD_l:     v32 = (uint32_t)va_arg(*ap, unsigned long); nbits = UINT32_WIDTH;                   break;
#endif
        case FMT_LENMOD_L:
        case FMT_LENMOD_ll:    v64 = (uint64_t)va_arg(*ap, unsigned long long); nbits = UINT64_WIDTH;              break;
#if UINTMAX_WIDTH == 64
        case FMT_LENMOD_j:     v64 = (uint64_t)va_arg(*ap, uintmax_t); nbits = UINTMAX_WIDTH;  break;
#else
        case FMT_LENMOD_j:     v32 = (uint32_t)va_arg(*ap, uintmax_t); nbits = UINTMAX_WIDTH;  break;
#endif
#if SIZE_WIDTH == 64
        case FMT_LENMOD_z:     v64 = (uint64_t)va_arg(*ap, size_t); nbits = SIZE_WIDTH;  break;
#else
        case FMT_LENMOD_z:     v32 = (uint32_t)va_arg(*ap, size_t); nbits = SIZE_WIDTH;  break;
#endif
#if PTRDIFF_WIDTH == 64
        case FMT_LENMOD_t:     v64 = (uint64_t)va_arg(*ap, ptrdiff_t); nbits = PTRDIFF_WIDTH;    break;
#else
        case FMT_LENMOD_t:     v32 = (uint32_t)va_arg(*ap, ptrdiff_t); nbits = PTRDIFF_WIDTH;    break;
#endif
    }

    if (nbits == 64) {
        pCanonDigits = __u64toa(v64, radix, isUppercase, &self->i64a);
    } else {
        pCanonDigits = __u32toa(v32, radix, isUppercase, (i32a_t*)&self->i64a);
    }

    _format_uint_field(self, radix, isUppercase, pCanonDigits, self->i64a.length);
}

static void _format_ptr(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    fmt_cspec_t spec2 = self->spec;
    self->spec.flags.isAlternativeForm = true;
    self->spec.flags.hasPrecision = true;
    self->spec.flags.padWithZeros = true;

#if __INTPTR_WIDTH == 64
    char* pCanonDigits = __u64toa((uint64_t)va_arg(*ap, void*), 16, false, &self->i64a);
    self->spec.precision = 16;
#else
    char* pCanonDigits = __u32toa((uint32_t)va_arg(*ap, void*), 16, false, (i32a_t*)&self->i64a);
    self->spec.precision = 8;
#endif

    _format_uint_field(self, 16, false, pCanonDigits, self->i64a.length);
    self->spec = spec2;
}

static void _format_out_nchars(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    char* p = va_arg(*ap, char*);
    const size_t n = self->charactersWritten;

    switch (self->spec.lengthModifier) {
        case FMT_LENMOD_hh:     *((signed char*)p) = __min(n, SCHAR_MAX);   break;
        case FMT_LENMOD_h:      *((short*)p) = __min(n, SHRT_MAX);          break;
        case FMT_LENMOD_none:   *((int*)p) = __min(n, INT_MAX);             break;
        case FMT_LENMOD_l:      *((long*)p) = __min(n, LONG_MAX);           break;
        case FMT_LENMOD_ll:     *((long long*)p) = __min(n, LLONG_MAX);     break;
        case FMT_LENMOD_j:      *((intmax_t*)p) = __min(n, INTMAX_MAX);     break;
        case FMT_LENMOD_z:      *((ssize_t*)p) = __min(n, SSIZE_MAX);       break;
        case FMT_LENMOD_t:      *((ptrdiff_t*)p) = __min(n, PTRDIFF_MAX);   break;
#if ___STDC_HOSTED__ == 1
        case FMT_LENMOD_L:      break;  // long double
#endif
    }
}

static void _format_arg(fmt_t* _Nonnull _Restrict self, char conversion, va_list* _Nonnull _Restrict ap)
{
    switch (conversion) {
        case '%':   _write_char(self, '%'); break;
        case 'c':   _format_char(self, ap); break;
        case 's':   _format_string(self, ap); break;
        case 'd':   // fall through
        case 'i':   _format_int(self, ap); break;
        case 'o':   _format_uint(self, 8, false, ap); break;
        case 'x':   _format_uint(self, 16, false, ap); break;
        case 'X':   _format_uint(self, 16, true, ap); break;
        case 'u':   _format_uint(self, 10, false, ap); break;
#if ___STDC_HOSTED__ == 1
        case 'f':   break; // XXX
        case 'F':   break; // XXX
        case 'e':   break; // XXX
        case 'E':   break; // XXX
        case 'a':   break; // XXX
        case 'A':   break; // XXX
        case 'g':   break; // XXX
        case 'G':   break; // XXX
#endif
        case 'n':   _format_out_nchars(self, ap); break;
        case 'p':   _format_ptr(self, ap); break;
        default:    break;
    }
}


void __fmt_init(fmt_t* _Nonnull _Restrict self, void* _Nullable _Restrict s, fmt_putc_func_t _Nonnull putc_f, fmt_write_func_t _Nonnull write_f, bool doContCountingOnError)
{
    self->stream = s;
    self->putc_cb = putc_f;
    self->write_cb = write_f;
    self->charactersWritten = 0;
    self->hasError = false;
    self->doContCountingOnError = doContCountingOnError;
}

void __fmt_deinit(fmt_t* _Nullable self)
{
    if (self) {
        self->stream = NULL;
        self->putc_cb = NULL;
        self->write_cb = NULL;
    }
}

int __fmt_format(fmt_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict format, va_list ap)
{
    const char* pformat = format;

    self->charactersWritten = 0;
    self->hasError = false;

    while (!self->hasError) {
        switch (*format) {
            case '\0':
                if (format != pformat) {
                    _write_string(self, pformat, format - pformat);
                }
                return (int)__min(self->charactersWritten, INT_MAX);

            case '%':
                if (format != pformat) {
                    _write_string(self, pformat, format - pformat);
                }
                format = _parse_conv_spec(self, ++format, &ap);
                _format_arg(self, *format++, &ap);
                pformat = format;
                break;

            default:
                format++;
                break;
        }
    }

    return (self->charactersWritten > 0) ? (int)__min(self->charactersWritten, INT_MAX) : -1;
}
