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

union s32_64 {
    long long   l64;
    long        l32;
};

union u32_64 {
    unsigned long long  u64;
    unsigned long       u32;
};

#define FMT_HAS_ERROR(x)                (((x) & __FMT_HASERR) == __FMT_HASERR)
#define FMT_CONT_COUNTING_ON_ERROR(x)   (((x) & __FMT_CONTCNTONERR) == __FMT_CONTCNTONERR)


void __fmt_write_char(fmt_t* _Nonnull self, char ch)
{
    if ((!FMT_HAS_ERROR(self->flags) && self->putc_cb(ch, self->stream) == 1)
        || (FMT_HAS_ERROR(self->flags) && FMT_CONT_COUNTING_ON_ERROR(self->flags))) {
        self->chars_written++;
    }
    else {
        self->flags |= __FMT_HASERR;
    }
}

void __fmt_write_string(fmt_t *_Nonnull _Restrict self, const char* _Nonnull _Restrict str, ssize_t len)
{
    if ((!FMT_HAS_ERROR(self->flags) && self->write_cb(self->stream, str, len) == len)
        || (FMT_HAS_ERROR(self->flags) && FMT_CONT_COUNTING_ON_ERROR(self->flags))) {
        self->chars_written += len;
    }
    else {
        self->flags |= __FMT_HASERR;
    }
}

void __fmt_write_char_rep(fmt_t* _Nonnull self, char ch, int count)
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
    self->chars_written += i;
}

static const char* _Nonnull _parse_length_mod(fmt_t* _Nonnull _Restrict self, const char * _Nonnull _Restrict format)
{
    fmt_cspec_t* spec = &self->spec;

    switch (*format) {
        case 'l':
            format++;
            if (*format == 'l') {
                format++;
                spec->lm = FMT_LM_ll;
            } else {
                spec->lm = FMT_LM_l;
            }
            break;

        case 'h':
            format++;
            if (*format == 'h') {
                format++;
                spec->lm = FMT_LM_hh;
            } else {
                spec->lm = FMT_LM_h;
            }
            break;

        case 'j':
            format++;
#if INTMAX_WIDTH == 64
            spec->lm = FMT_LM_ll;
#else
            spec->lm = FMT_LM_none;
#endif
            break;

        case 'z':
            format++;
#if SSIZE_WIDTH == 64
            spec->lm = FMT_LM_ll;
#else
            spec->lm = FMT_LM_none;
#endif
            break;

        case 't':
            format++;
#if PTRDIFF_WIDTH == 64
            spec->lm = FMT_LM_ll;
#else
            spec->lm = FMT_LM_none;
#endif
            break;

#if ___STDC_HOSTED__ == 1
        case 'L':   // long double
            format++;
            spec->lm = FMT_LM_L;
            break;
#endif

        default:
            break;
    }
    
    return format;
}

static int _atoi(const char* _Nonnull _Restrict str, char* _Nonnull _Restrict * _Nonnull _Restrict str_end)
{
    long r;

    if (__strtoi32(str, str_end, 10, &r) == 0) {
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

    spec->min_field_width = 0;
    spec->prec = 0;
    spec->lm = FMT_LM_none;
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
        spec->min_field_width = fmt_arg(ap, int);
        format++;
    }
    else if (ch >= '1' && ch <= '9') {
        spec->min_field_width = _atoi(format, &format);
    }

    // Precision
    if (*format == '.') {
        format++;
        ch = *format;

        if (ch == '*') {
            spec->prec = fmt_arg(ap, int);
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
    int nspaces = (spec->min_field_width > slen) ? spec->min_field_width - slen : 0;

    if (FMT_IS_PADZEROS(spec->flags) && !FMT_IS_HASPREC(spec->flags) && !FMT_IS_LEFTJUST(spec->flags)) {
        nLeadingZeros = nspaces;
        nspaces = 0;
    }

    if (nspaces > 0 && !FMT_IS_LEFTJUST(spec->flags)) {
        __fmt_write_char_rep(self, ' ', nspaces);
    }

    if (nSign > 0) {
        __fmt_write_char(self, *sign);
    }
    if (!isEmpty) {
        if (nLeadingZeros > 0) {
            __fmt_write_char_rep(self, '0', nLeadingZeros);
        }
        __fmt_write_string(self, digits, nDigits);
    }

    if (nspaces > 0 && FMT_IS_LEFTJUST(spec->flags)) {
        __fmt_write_char_rep(self, ' ', nspaces);
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
    int nspaces = (spec->min_field_width > slen) ? spec->min_field_width - slen : 0;

    if (FMT_IS_PADZEROS(spec->flags) && !FMT_IS_HASPREC(spec->flags) && !FMT_IS_LEFTJUST(spec->flags)) {
        nLeadingZeros = nspaces;
        nspaces = 0;
    }

    if (nspaces > 0 && !FMT_IS_LEFTJUST(spec->flags)) {
        __fmt_write_char_rep(self, ' ', nspaces);
    }

    if (!isEmpty) {
        while (nRadixChars-- > 0) {
            __fmt_write_char(self, *radixChars++);
        }
        if (nLeadingZeros > 0) {
            __fmt_write_char_rep(self, '0', nLeadingZeros);
        }
        __fmt_write_string(self, buf, len);
    }
    else if (radix == 8) {
        __fmt_write_char(self, '0');
    }

    if (nspaces > 0 && FMT_IS_LEFTJUST(spec->flags)) {
        __fmt_write_char_rep(self, ' ', nspaces);
    }
}

static void _format_char(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    const fmt_cspec_t* spec = &self->spec;
    const char ch = (unsigned char) fmt_arg(ap, int);
    const size_t nspaces = (spec->min_field_width > 1) ? spec->min_field_width - 1 : 0;

    if (nspaces > 0 && FMT_IS_LEFTJUST(spec->flags)) {
        __fmt_write_char_rep(self, ' ', nspaces);
    }

    __fmt_write_char(self, ch);

    if (nspaces > 0 && !FMT_IS_LEFTJUST(spec->flags)) {
        __fmt_write_char_rep(self, ' ', nspaces);
    }
}

static void _format_string(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    const fmt_cspec_t* spec = &self->spec;
    const char* str = fmt_arg(ap, const char*);
    const size_t slen = strlen(str);
    const size_t flen = (FMT_IS_HASPREC(spec->flags) || spec->min_field_width > 0) ? slen : 0;
    const size_t adj_flen = FMT_IS_HASPREC(spec->flags) ? __min(flen, spec->prec) : flen;
    const size_t nspaces = (spec->min_field_width > adj_flen) ? spec->min_field_width - adj_flen : 0;

    if (nspaces > 0 && FMT_IS_LEFTJUST(spec->flags)) {
        __fmt_write_char_rep(self, ' ', nspaces);
    }

    __fmt_write_string(self, str, FMT_IS_HASPREC(spec->flags) ? __min(slen, adj_flen) : slen);

    if (nspaces > 0 && !FMT_IS_LEFTJUST(spec->flags)) {
        __fmt_write_char_rep(self, ' ', nspaces);
    }
}

static void _format_int(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    union s32_64 val;
    char * p;

    switch (self->spec.lm) {
        case FMT_LM_hh:     val.l32 = (int32_t)(signed char)fmt_arg(ap, int); break;
        case FMT_LM_h:      val.l32 = (int32_t)(short)fmt_arg(ap, int); break;
        case FMT_LM_none:   val.l32 = (int32_t)fmt_arg(ap, int); break;
#if ULONG_WIDTH == 64
        case FMT_LM_l:      val.l64 = (int64_t)fmt_arg(ap, long long); break;
#else
        case FMT_LM_l:      val.l32 = (int32_t)fmt_arg(ap, long); break;
#endif
        case FMT_LM_ll:     val.l64 = (int64_t)fmt_arg(ap, long long); break;
        case FMT_LM_L:      val.l32 = 0; break;
    }

    if (self->spec.lm == FMT_LM_ll
#if LONG_WIDTH == 64
        || self->spec.lm == FMT_LM_l
#endif
    ) {
        p = __i64toa(val.l64, ia_sign_plus_minus, &self->i64a);
    }
    else {
        p = __i32toa(val.l32, ia_sign_plus_minus, (i32a_t*)&self->i64a);
    }

    _format_int_field(self, p, self->i64a.length);
}

static void _format_uint(fmt_t* _Nonnull _Restrict self, int radix, bool isUppercase, va_list* _Nonnull _Restrict ap)
{
    union u32_64 val;
    char * p;

    switch (self->spec.lm) {
        case FMT_LM_hh:     val.u32 = (uint32_t)(unsigned char)fmt_arg(ap, unsigned int); break;
        case FMT_LM_h:      val.u32 = (uint32_t)(unsigned short)fmt_arg(ap, unsigned int); break;
        case FMT_LM_none:   val.u32 = (uint32_t)fmt_arg(ap, unsigned int); break;
#if ULONG_WIDTH == 64
        case FMT_LM_l:      val.u64 = (uint64_t)fmt_arg(ap, unsigned long long); break;
#else
        case FMT_LM_l:      val.u32 = (uint32_t)fmt_arg(ap, unsigned long); break;
#endif
        case FMT_LM_ll:     val.u64 = (uint64_t)fmt_arg(ap, unsigned long long); break;
        case FMT_LM_L:      val.u32 = 0; break;
    }

    if (self->spec.lm == FMT_LM_ll
#if LONG_WIDTH == 64
        || self->spec.lm == FMT_LM_l
#endif
    ) {
        p = __u64toa(val.u64, radix, isUppercase, &self->i64a);
    }
    else {
        p = __u32toa(val.u32, radix, isUppercase, (i32a_t*)&self->i64a);
    }

    _format_uint_field(self, radix, isUppercase, p, self->i64a.length);
}

static void _format_ptr(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    char* p;
    
    self->spec.flags |= (__FMT_ALTFORM | __FMT_PADZEROS | __FMT_HASPREC);

#if __INTPTR_WIDTH == 64
    p = __u64toa((uint64_t)fmt_arg(ap, void*), 16, false, &self->i64a);
    self->spec.prec = 16;
#else
    p = __u32toa((uint32_t)fmt_arg(ap, void*), 16, false, (i32a_t*)&self->i64a);
    self->spec.prec = 8;
#endif

    _format_uint_field(self, 16, false, p, self->i64a.length);
}

static void _format_out_nchars(fmt_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    char* p = fmt_arg(ap, char*);
    const size_t n = self->chars_written;

    switch (self->spec.lm) {
        case FMT_LM_hh:     *(signed char*)p = __min(n, SCHAR_MAX);   break;
        case FMT_LM_h:      *(short*)p = __min(n, SHRT_MAX);          break;
        case FMT_LM_none:   *(int*)p = __min(n, INT_MAX);             break;
        case FMT_LM_l:      *(long*)p = __min(n, LONG_MAX);           break;
        case FMT_LM_ll:     *(long long*)p = __min(n, LLONG_MAX);     break;
        case FMT_LM_L:      /* ignore */ break;
    }
}

static void _format_arg(fmt_t* _Nonnull _Restrict self, char conversion, va_list* _Nonnull _Restrict ap)
{
    switch (conversion) {
        case '%':   __fmt_write_char(self, '%'); break;
        case 'c':   _format_char(self, ap); break;
        case 's':   _format_string(self, ap); break;
        case 'd':   // fall through
        case 'i':   _format_int(self, ap); break;
        case 'o':   _format_uint(self, 8, false, ap); break;
        case 'x':   _format_uint(self, 16, false, ap); break;
        case 'X':   _format_uint(self, 16, true, ap); break;
        case 'u':   _format_uint(self, 10, false, ap); break;
        case 'n':   _format_out_nchars(self, ap); break;
        case 'p':   _format_ptr(self, ap); break;
        
        default:
            if (self->format_cb) {
                self->format_cb(self, conversion, ap);
            }
            break;
    }
}


void __fmt_init_i(fmt_t* _Nonnull _Restrict self, void* _Nullable _Restrict s, fmt_putc_t _Nonnull putc_f, fmt_write_t _Nonnull write_f, bool doContCountingOnError)
{
    __fmt_common_init(self, s, putc_f, write_f, doContCountingOnError);
    self->format_cb = NULL;
}

void __fmt_deinit(fmt_t* _Nullable self)
{
    if (self) {
        self->stream = NULL;
        self->putc_cb = NULL;
        self->write_cb = NULL;
        self->format_cb = NULL;
    }
}

int __fmt_format(fmt_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict format, va_list ap)
{
    const char* pformat = format;

    self->chars_written = 0;
    self->flags &= ~__FMT_HASERR;

    while (!FMT_HAS_ERROR(self->flags)) {
        switch (*format) {
            case '\0':
                if (format != pformat) {
                    __fmt_write_string(self, pformat, format - pformat);
                }
                return (int)__min(self->chars_written, INT_MAX);

            case '%':
                if (format != pformat) {
                    __fmt_write_string(self, pformat, format - pformat);
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

    return (self->chars_written > 0) ? (int)__min(self->chars_written, INT_MAX) : -1;
}
