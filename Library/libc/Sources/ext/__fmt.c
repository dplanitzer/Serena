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

#define FMT_IS_LEFTJUST(x)      (((x) & __FMT_LEFTJUST) == __FMT_LEFTJUST)
#define FMT_IS_FORCESIGN(x)     (((x) & __FMT_FORCESIGN) == __FMT_FORCESIGN)
#define FMT_IS_SPACEIFPOS(x)    (((x) & __FMT_SPACEIFPOS) == __FMT_SPACEIFPOS)
#define FMT_IS_ALTFORM(x)       (((x) & __FMT_ALTFORM) == __FMT_ALTFORM)
#define FMT_IS_PADZEROS(x)      (((x) & __FMT_PADZEROS) == __FMT_PADZEROS)
#define FMT_IS_HASPREC(x)       (((x) & __FMT_HASPREC) == __FMT_HASPREC)

#define FMT_HAS_ERROR(x)                (((x) & __FMT_HASERR) == __FMT_HASERR)
#define FMT_CONT_COUNTING_ON_ERROR(x)   (((x) & __FMT_CONTCNTONERR) == __FMT_CONTCNTONERR)


static void _write_char(fmt_t* _Nonnull self, char ch)
{
    if ((!FMT_HAS_ERROR(self->flags) && self->putc_cb(ch, self->stream) == 1)
        || (FMT_HAS_ERROR(self->flags) && FMT_CONT_COUNTING_ON_ERROR(self->flags))) {
        self->charactersWritten++;
    }
    else {
        self->flags |= __FMT_HASERR;
    }
}

static void _write_string(fmt_t *_Nonnull _Restrict self, const char* _Nonnull _Restrict str, ssize_t len)
{
    if ((!FMT_HAS_ERROR(self->flags) && self->write_cb(self->stream, str, len) == len)
        || (FMT_HAS_ERROR(self->flags) && FMT_CONT_COUNTING_ON_ERROR(self->flags))) {
        self->charactersWritten += len;
    }
    else {
        self->flags |= __FMT_HASERR;
    }
}

static void _write_char_rep(fmt_t* _Nonnull self, char ch, int count)
{
    int i = 0;

    while(i < count) {
        if ((!FMT_HAS_ERROR(self->flags) && self->putc_cb(ch, self->stream) == 1)
            || (FMT_HAS_ERROR(self->flags) && FMT_CONT_COUNTING_ON_ERROR(self->flags))) {
            i++;
        }
        else {
            self->flags |= __FMT_HASERR;
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
                spec->lenMod = FMT_LENMOD_ll;
            } else {
                spec->lenMod = FMT_LENMOD_l;
            }
            break;

        case 'h':
            format++;
            if (*format == 'h') {
                format++;
                spec->lenMod = FMT_LENMOD_hh;
            } else {
                spec->lenMod = FMT_LENMOD_h;
            }
            break;

        case 'j':
            format++;
            spec->lenMod = FMT_LENMOD_j;
            break;

        case 'z':
            format++;
            spec->lenMod = FMT_LENMOD_z;
            break;

        case 't':
            format++;
            spec->lenMod = FMT_LENMOD_t;
            break;

#if ___STDC_HOSTED__ ==1
        case 'L':   // long double
            format++;
            spec->lenMod = FMT_LENMOD_L;
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

    spec->minFieldWidth = 0;
    spec->prec = 0;
    spec->lenMod = FMT_LENMOD_none;
    spec->flags = 0;

    // Flags
    bool done = false;
    while (!done) {
        ch = *format++;
        
        switch (ch) {
            case '\0':  return --format;
            case '-':   spec->flags |= __FMT_LEFTJUST; break;
            case '+':   spec->flags |= __FMT_FORCESIGN; break;
            case ' ':   spec->flags |= __FMT_SPACEIFPOS; break;
            case '#':   spec->flags |= __FMT_ALTFORM; break;
            case '0':   spec->flags |= __FMT_PADZEROS; break;
            default:    --format; done = true; break;
        }
    }

    // Minimum field width
    ch = *format;
    if (ch == '*') {
        spec->minFieldWidth = va_arg(*ap, int);
        format++;
    }
    else if (ch >= '1' && ch <= '9') {
        spec->minFieldWidth = _atoi(format, &format);
    }

    // Precision
    if (*format == '.') {
        format++;
        ch = *format;

        if (ch == '*') {
            spec->prec = va_arg(*ap, int);
            format++;
        }
        else if (ch >= '0' && ch <= '9') {
            spec->prec = _atoi(format, &format);
        }
        spec->flags |= __FMT_HASPREC;
    }
    
    // Length modifier
    return _parse_length_mod(self, format);
}

static void _format_int_field(fmt_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict buf, size_t len)
{
    const fmt_cspec_t* spec = &self->spec;
    int nSign = 1;
    int nDigits = len - 1;
    int nLeadingZeros = FMT_IS_HASPREC(spec->flags) ? __max(spec->prec - nDigits, 0) : 0;
    const char* sign = buf;
    const char* digits = &buf[1];
    const bool isEmpty = FMT_IS_HASPREC(spec->flags) && spec->prec == 0 && digits[0] == '0' && nDigits == 1;

    if (!FMT_IS_FORCESIGN(spec->flags) && *sign == '+') {
        if (FMT_IS_SPACEIFPOS(spec->flags)) {
            sign = " ";
        } else {
            sign = "";
            nSign = 0;
        }
    }

    const int slen = nSign + nLeadingZeros + nDigits;
    int nspaces = (spec->minFieldWidth > slen) ? spec->minFieldWidth - slen : 0;

    if (FMT_IS_PADZEROS(spec->flags) && !FMT_IS_HASPREC(spec->flags) && !FMT_IS_LEFTJUST(spec->flags)) {
        nLeadingZeros = nspaces;
        nspaces = 0;
    }

    if (nspaces > 0 && !FMT_IS_LEFTJUST(spec->flags)) {
        _write_char_rep(self, ' ', nspaces);
    }

    if (nSign > 0) {
        _write_char(self, *sign);
    }
    if (!isEmpty) {
        if (nLeadingZeros > 0) {
            _write_char_rep(self, '0', nLeadingZeros);
        }
        _write_string(self, digits, nDigits);
    }

    if (nspaces > 0 && FMT_IS_LEFTJUST(spec->flags)) {
        _write_char_rep(self, ' ', nspaces);
    }
}

static void _format_uint_field(fmt_t* _Nonnull _Restrict self, int radix, bool isUppercase, const char* _Nonnull _Restrict buf, size_t len)
{
    const fmt_cspec_t* spec = &self->spec;
    int nRadixChars = 0;
    int nLeadingZeros = FMT_IS_HASPREC(spec->flags) ? __max(spec->prec - len, 0) : 0;
    const char* radixChars = "";
    const bool isEmpty = FMT_IS_HASPREC(spec->flags) && spec->prec == 0 && buf[0] == '0' && len == 1;

    if (FMT_IS_ALTFORM(spec->flags)) {
        switch (radix) {
            case 8: nRadixChars = 1; radixChars = "0"; break;
            case 16: nRadixChars = 2; radixChars = (isUppercase) ? "0X" : "0x";
            default: break;
        }
    }

    const int slen = nRadixChars + nLeadingZeros + len;
    int nspaces = (spec->minFieldWidth > slen) ? spec->minFieldWidth - slen : 0;

    if (FMT_IS_PADZEROS(spec->flags) && !FMT_IS_HASPREC(spec->flags) && !FMT_IS_LEFTJUST(spec->flags)) {
        nLeadingZeros = nspaces;
        nspaces = 0;
    }

    if (nspaces > 0 && !FMT_IS_LEFTJUST(spec->flags)) {
        _write_char_rep(self, ' ', nspaces);
    }

    if (!isEmpty) {
        while (nRadixChars-- > 0) {
            _write_char(self, *radixChars++);
        }
        if (nLeadingZeros > 0) {
            _write_char_rep(self, '0', nLeadingZeros);
        }
        _write_string(self, buf, len);
    }
    else if (radix == 8) {
        _write_char(self, '0');
    }

    if (nspaces > 0 && FMT_IS_LEFTJUST(spec->flags)) {
        _write_char_rep(self, ' ', nspaces);
    }
}

static void _format_char(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    const fmt_cspec_t* spec = &self->spec;
    const char ch = (unsigned char) va_arg(*ap, int);
    const size_t nspaces = (spec->minFieldWidth > 1) ? spec->minFieldWidth - 1 : 0;

    if (nspaces > 0 && FMT_IS_LEFTJUST(spec->flags)) {
        _write_char_rep(self, ' ', nspaces);
    }

    _write_char(self, ch);

    if (nspaces > 0 && !FMT_IS_LEFTJUST(spec->flags)) {
        _write_char_rep(self, ' ', nspaces);
    }
}

static void _format_string(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    const fmt_cspec_t* spec = &self->spec;
    const char* str = va_arg(*ap, const char*);
    const size_t slen = strlen(str);
    const size_t flen = (FMT_IS_HASPREC(spec->flags) || spec->minFieldWidth > 0) ? slen : 0;
    const size_t adj_flen = FMT_IS_HASPREC(spec->flags) ? __min(flen, spec->prec) : flen;
    const size_t nspaces = (spec->minFieldWidth > adj_flen) ? spec->minFieldWidth - adj_flen : 0;

    if (nspaces > 0 && FMT_IS_LEFTJUST(spec->flags)) {
        _write_char_rep(self, ' ', nspaces);
    }

    _write_string(self, str, FMT_IS_HASPREC(spec->flags) ? __min(slen, adj_flen) : slen);

    if (nspaces > 0 && !FMT_IS_LEFTJUST(spec->flags)) {
        _write_char_rep(self, ' ', nspaces);
    }
}

static void _format_int(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    int64_t v64;
    int32_t v32;
    int nbits;
    char * cd;

    switch (self->spec.lenMod) {
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
        cd = __i64toa(v64, ia_sign_plus_minus, &self->i64a);
    } else {
        cd = __i32toa(v32, ia_sign_plus_minus, (i32a_t*)&self->i64a);
    }

    _format_int_field(self, cd, self->i64a.length);
}

static void _format_uint(fmt_t* _Nonnull _Restrict self, int radix, bool isUppercase, va_list* _Nonnull _Restrict ap)
{
    uint64_t v64;
    uint32_t v32;
    int nbits;
    char * cd;

    switch (self->spec.lenMod) {
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
        cd = __u64toa(v64, radix, isUppercase, &self->i64a);
    } else {
        cd = __u32toa(v32, radix, isUppercase, (i32a_t*)&self->i64a);
    }

    _format_uint_field(self, radix, isUppercase, cd, self->i64a.length);
}

static void _format_ptr(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    fmt_cspec_t spec2 = self->spec;
    
    self->spec.flags |= (__FMT_ALTFORM | __FMT_PADZEROS | __FMT_HASPREC);

#if __INTPTR_WIDTH == 64
    char* cd = __u64toa((uint64_t)va_arg(*ap, void*), 16, false, &self->i64a);
    self->spec.prec = 16;
#else
    char* cd = __u32toa((uint32_t)va_arg(*ap, void*), 16, false, (i32a_t*)&self->i64a);
    self->spec.prec = 8;
#endif

    _format_uint_field(self, 16, false, cd, self->i64a.length);
    self->spec = spec2;
}

static void _format_out_nchars(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    char* p = va_arg(*ap, char*);
    const size_t n = self->charactersWritten;

    switch (self->spec.lenMod) {
        case FMT_LENMOD_hh:     *((signed char*)p) = __min(n, SCHAR_MAX);   break;
        case FMT_LENMOD_h:      *((short*)p) = __min(n, SHRT_MAX);          break;
        case FMT_LENMOD_none:   *((int*)p) = __min(n, INT_MAX);             break;
        case FMT_LENMOD_l:      *((long*)p) = __min(n, LONG_MAX);           break;
        case FMT_LENMOD_ll:     *((long long*)p) = __min(n, LLONG_MAX);     break;
        case FMT_LENMOD_j:      *((intmax_t*)p) = __min(n, INTMAX_MAX);     break;
        case FMT_LENMOD_z:      *((ssize_t*)p) = __min(n, SSIZE_MAX);       break;
        case FMT_LENMOD_t:      *((ptrdiff_t*)p) = __min(n, PTRDIFF_MAX);   break;
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
    self->flags = 0;
    if (doContCountingOnError) {
        self->flags |= __FMT_CONTCNTONERR;
    }
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
    self->flags &= ~__FMT_HASERR;

    while (!FMT_HAS_ERROR(self->flags)) {
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
