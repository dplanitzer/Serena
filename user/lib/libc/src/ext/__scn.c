//
//  __scn.c
//  libc
//
//  Created by Dietmar Planitzer on 3/2/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ctype.h>
#include <ext/__scn.h>
#include <ext/limits.h>
#include <ext/math.h>
#include <float.h>
#include <string.h>
#include <stdio.h>

union s32_64 {
    long long   l64;
    long        l32;
};

union u32_64 {
    unsigned long long  u64;
    unsigned long       u32;
};


int __scn_getc(scn_t* _Nonnull self)
{
    char ch;
    const ssize_t r = self->getc_cb(&ch, self->stream);

    if (r == 1) {
        self->chars_read++;
        return (int)ch;
    }
    else if (r == 0) {
        scn_seteof(self);
    }
    else {
        scn_setfailed(self);
    }

    return EOF;
}

int __scn_ungetc(scn_t* _Nonnull self, int ch)
{
    const int r = self->ungetc_cb(ch, self->stream);

    if (r == -1 && ch != -1) {
        scn_setfailed(self);
    }
    if (ch != -1) {
        self->chars_read--;
    }

    return ch;
}

static const char* _Nonnull _parse_length_mod(scn_t* _Nonnull _Restrict self, const char * _Nonnull _Restrict format)
{
    scn_cspec_t* spec = &self->spec;

    switch (*format++) {
        case 'l':
            if (*format == 'l') {
                format++;
                spec->lm = SCN_LM_ll;
            } else {
                spec->lm = SCN_LM_l;
            }
            break;

        case 'h':
            if (*format == 'h') {
                format++;
                spec->lm = SCN_LM_hh;
            } else {
                spec->lm = SCN_LM_h;
            }
            break;

        case 'j':
#if INTMAX_WIDTH == 64
            spec->lm = SCN_LM_ll;
#else
            spec->lm = SCN_LM_none;
#endif
            break;

        case 'z':
#if SSIZE_WIDTH == 64
            spec->lm = SCN_LM_ll;
#else
            spec->lm = SCN_LM_none;
#endif
            break;

        case 't':
#if PTRDIFF_WIDTH == 64
            spec->lm = SCN_LM_ll;
#else
            spec->lm = SCN_LM_none;
#endif
            break;

        case 'L':   // long double
            spec->lm = SCN_LM_L;
            break;

        default:
            format--;
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
static const char* _Nonnull _parse_conv_spec(scn_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict format, va_list* _Nonnull _Restrict ap)
{
    scn_cspec_t* spec = &self->spec;
    char ch;

    spec->max_field_width = -1;
    spec->lm = SCN_LM_none;
    spec->flags = 0;

    // Flags
    if (*format == '*') {
        format++;
        spec->flags |= __SCN_SUPPRESSED;
    }

    // Maximum field width
    ch = *format;
    if (ch >= '1' && ch <= '9') {
        spec->max_field_width = _atoi(format, &format);
    }
    
    // Length modifier
    return _parse_length_mod(self, format);
}


void __scn_skip_ws(scn_t* _Nonnull self)
{
    for (;;) {
        const int ch = __scn_getc(self);

        if (ch == EOF || !isspace(ch)) {
            __scn_ungetc(self, ch);
            break;
        }
    }
}

#define __ST_SIGN   0
#define __ST_BASE   1
#define __ST_NUM    2
#define __ST_END    3

// Scans an integer of the form:
// [+|-][0x|0X|0](0-9)+
static void _lex_int(scn_t* _Nonnull self, int base)
{
    char* p = self->u.digits;
    size_t dig_lim = (self->spec.max_field_width < 0) ? INT_MAX : self->spec.max_field_width;
    int state = __ST_SIGN, i = 0, d;
    char ch = __scn_getc(self);

    while (state < __ST_END && i < dig_lim) {
        switch (state) {
            case __ST_SIGN:
                if (ch == '-' || ch == '+') {
                    p[i++] = ch;
                    ch = __scn_getc(self);
                }
                state = __ST_BASE;
                break;

            case __ST_BASE:
                if ((base == 0 || base == 8 || base == 16) && ch == '0') {
                    if (i == dig_lim) {
                        state = __ST_END;
                        break;
                    }

                    ch = __scn_getc(self);

                    if ((base == 0 || base == 8) && (ch >= '0' && ch <= '7')) {
                        p[i++] = '0';
                        base = 8;
                        state = __ST_NUM;
                    }
                    else if ((base == 0 || base == 16) && (ch == 'x' || ch == 'X')) {
                        p[i++] = '0';
                        p[i++] = 'x';
                        base = 16;
                        state = __ST_NUM;
                        ch = __scn_getc(self);
                    }
                    else if (ch == EOF) {
                        p[i++] = '0';
                        base = 10;
                        state = __ST_END;
                    }
                    else {
                        base = 10;
                        state = __ST_NUM;
                    }
                }
                else {
                    state = __ST_NUM;
                }
                break;

            case __ST_NUM:
                if (ch == EOF) {
                    state = __ST_END;
                    break;
                }

                if (isdigit(ch)) {
                    d = ch - '0';
                }
                else if (isupper(ch)) {
                    d = ch - 'A' + 10;
                }
                else if (islower(ch)) {
                    d = ch - 'a' + 10;
                }
                else {
                    state = __ST_END;
                    break;
                }

                if (d >= base) {
                    state = __ST_END;
                    break;
                }

                // Stores at most __SCN_MAX_DIGITS digits in the buffer. This number
                // includes the sign, prefix and one carry digit to enable the caller to
                // detect overflow. We continue to consume consecutive digits once we
                // hit overflow until there is no more valid digit.
                if (i < __SCN_MAX_DIGITS) {
                    p[i++] = ch;
                }
                ch = __scn_getc(self);
                break;
        }
    }

    p[i] = '\0';
    __scn_ungetc(self, ch);
}


static void _scan_chars(scn_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    char* p = scn_arg(ap, char*);
    size_t nchars = (self->spec.max_field_width < 0) ? 1 : self->spec.max_field_width;
    const bool suppressed = SCN_SUPPRESSED(self->spec.flags);
    bool assigned = false;

    while (nchars-- > 0) {
        const int ch = __scn_getc(self);

        if (ch == EOF) {
            break;
        }
        
        if (!suppressed) {
            *p++ = (unsigned char)ch;
            assigned = true;
        }
    }

    if (assigned) {
        self->fields_assigned++;
    }
}

static void _scan_string(scn_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    char* p = scn_arg(ap, char*);
    size_t nchars = (self->spec.max_field_width < 0) ? INT_MAX : self->spec.max_field_width;
    const bool suppressed = SCN_SUPPRESSED(self->spec.flags);
    bool assigned = false;

    __scn_skip_ws(self);

    for (;;) {
        const int ch = __scn_getc(self);

        if (ch == EOF || isspace(ch) || nchars == 0) {
            break;
        }

        nchars--;
        if (!suppressed) {
            *p++ = (unsigned char)ch;
            assigned = true;
        }
    }

    if (assigned) {
        *p = '\0';
        self->fields_assigned++;
    }
}

// original call: scanf("%[0-9]hello")
// 'format' at entry: "0-9]hello"
// 'format' at return: "hello"
// See C23 draft, p336
static const char* _Nonnull _parse_scanset(scn_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict format, bool * _Nonnull _Restrict pNegated)
{
    memset(&self->u.scanset, 0, __SCN_SCANSET_SIZE);


    // look for '^' -> negated
    if (*format == '^') {
        format++;
        *pNegated = true;
    }
    
    // look for ']' -> scan for closing bracket too
    if (*format == ']') {
        format++;
        scn_addchar(self, ']');
    }

    // pick up all remaining characters while handling range specifiers properly:
    // [-x] -> not a range; '-' and 'x' are set members
    // [x-] -> not a range; 'x' and '-' are set members
    // [0-9] -> range '0', '1', ... '9' are set members
    // [a-zA-Z] -> two ranges
    for (;;) {
        const unsigned char ch = *format;

        if (ch == '\0') {
            break;
        }
        if (ch == ']') {
            // consume ']'
            format++;
            break;
        }
        

        // check whether this is a range
        if (format[1] == '-' && format[2] != '\0' && format[2] != ']') {
            // range
            const unsigned char lo = ch;
            const unsigned char up = format[2];

            for (unsigned char c = lo; c <= up; c++) {
                scn_addchar(self, c);
            }

            format += 2;
        }
        else {
            // individual character
            scn_addchar(self, ch);
            format++;
        }
    }

    return format;
}

static const char* _scan_set(scn_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap, const char* _Nonnull _Restrict format)
{
    char* p = scn_arg(ap, char*);
    size_t nchars = (self->spec.max_field_width < 0) ? INT_MAX : self->spec.max_field_width;
    const bool suppressed = SCN_SUPPRESSED(self->spec.flags);
    bool assigned = false, negated = false;

    format = _parse_scanset(self, format, &negated);

    while (nchars-- > 0) {
        const int ch = __scn_getc(self);

        if (ch == EOF) {
            break;
        }
        
        const int is = scn_haschar(self, ch);
        if ((!negated && !is) || (negated && is)) {
            __scn_ungetc(self, ch);
            break;
        }

        if (!suppressed) {
            *p++ = (unsigned char)ch;
            assigned = true;
        }
    }

    if (assigned) {
        *p = '\0';
        self->fields_assigned++;
    }

    return format;
}

static int _lm_conv_nbits(char lm)
{
    switch (lm) {
        case SCN_LM_hh:     return INT32_WIDTH;
        case SCN_LM_h:      return INT32_WIDTH;
        case SCN_LM_none:   return INT32_WIDTH;
        case SCN_LM_l:      return LONG_WIDTH;
        case SCN_LM_ll:     return LLONG_WIDTH;
        case SCN_LM_L:      return LDBL_WIDTH;
    }
}

static void _scan_int(scn_t* _Nonnull _Restrict self, int base, va_list* _Nonnull _Restrict ap)
{
    union s32_64 val;

    __scn_skip_ws(self);
    _lex_int(self, base);

    if (scn_failed(self) || self->u.digits[0] == '\0' || SCN_SUPPRESSED(self->spec.flags) || self->spec.lm == SCN_LM_L) {
        return;
    }


    if (_lm_conv_nbits(self->spec.lm) == 64) {
        __strtoi64(self->u.digits, NULL, base, &val.l64);
    }
    else {
        __strtoi32(self->u.digits, NULL, base, &val.l32);
    }

    
    void* p = scn_arg(ap, void*);
    switch (self->spec.lm) {
        case SCN_LM_hh:    *(signed char*)p = __clamp(val.l32, SCHAR_MIN, SCHAR_MAX); break;
        case SCN_LM_h:     *(signed short*)p = __clamp(val.l32, SHRT_MIN, SHRT_MAX); break;
        case SCN_LM_none:  *(signed int*)p = val.l32; break;
        case SCN_LM_ll:    *(signed long long*)p = val.l64; break;
    }

    self->fields_assigned++;
}

static void _scan_uint(scn_t* _Nonnull _Restrict self, int base, va_list* _Nonnull _Restrict ap)
{
    union u32_64 val;

    __scn_skip_ws(self);
    _lex_int(self, base);

    if (scn_failed(self) || self->u.digits[0] == '\0' || SCN_SUPPRESSED(self->spec.flags) || self->spec.lm == SCN_LM_L) {
        return;
    }


    if (_lm_conv_nbits(self->spec.lm) == 64) {
        __strtou64(self->u.digits, NULL, base, &val.u64);
    }
    else {
        __strtou32(self->u.digits, NULL, base, &val.u32);
    }

    
    void* p = scn_arg(ap, void*);
    switch (self->spec.lm) {
        case SCN_LM_hh:    *(unsigned char*)p = __min(val.u32, UCHAR_MAX); break;
        case SCN_LM_h:     *(unsigned short*)p = __min(val.u32, USHRT_MAX); break;
        case SCN_LM_none:  *(unsigned int*)p = val.u32; break;
        case SCN_LM_ll:    *(unsigned long long*)p = val.u64; break;
    }

    self->fields_assigned++;
}

static void _scan_out_nchars(scn_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    char* p = scn_arg(ap, char*);
    const size_t n = self->chars_read;

    if (SCN_SUPPRESSED(self->spec.flags) || self->spec.lm == SCN_LM_L) {
        return;
    }

    switch (self->spec.lm) {
        case SCN_LM_hh:     *(signed char*)p = __min(n, SCHAR_MAX);   break;
        case SCN_LM_h:      *(short*)p = __min(n, SHRT_MAX);          break;
        case SCN_LM_none:   *(int*)p = __min(n, INT_MAX);             break;
        case SCN_LM_l:      *(long*)p = __min(n, LONG_MAX);           break;
        case SCN_LM_ll:     *(long long*)p = __min(n, LLONG_MAX);     break;
    }
}

static const char* _Nonnull _scan_arg(scn_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict format, va_list* _Nonnull _Restrict ap)
{
    switch (*format++) {
        case '%':
            if (__scn_getc(self) != '%') {
                scn_setfailed(self);
            }
            break;

        case 'c':   _scan_chars(self, ap); break;
        case 's':   _scan_string(self, ap); break;
        case '[':   format = _scan_set(self, ap, format); break;
        case 'd':   _scan_int(self, 10, ap); break;
        case 'i':   _scan_int(self, 0, ap); break;
        case 'o':   _scan_uint(self, 8, ap); break;
        case 'x':   // fall through
        case 'X':   _scan_uint(self, 16, ap); break;
        case 'u':   _scan_uint(self, 10, ap); break;
        case 'n':   _scan_out_nchars(self, ap); break;
        case 'p':
#if defined(__LP64__) || defined(__LLP64__)
            if (self->spec.lm == SCN_LM_none) {
                self->spec.lm = SCN_LM_ll;
            }
#endif
            _scan_uint(self, 16, ap);
            break;
        
        default:
            if (self->scan_cb) {
                format = self->scan_cb(self, --format, ap);
            }
            break;
    }

    return format;
}


void __scn_init_i(scn_t* _Nonnull _Restrict self, void* _Nullable _Restrict s, scn_getc_t _Nonnull getc_f, scn_ungetc_t _Nonnull ungetc_f)
{
    __scn_common_init(self, s, getc_f, ungetc_f);
}

void __scn_deinit(scn_t* _Nullable self)
{
    if (self) {
        self->stream = NULL;
        self->getc_cb = NULL;
        self->ungetc_cb = NULL;
        self->scan_cb = NULL;
    }
}

int __scn_scan(scn_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict format, va_list ap)
{
    self->chars_read = 0;
    self->fields_assigned = 0;
    self->flags = 0;

    while (self->flags == 0) {
        switch (*format) {
            case '\0':
                return self->fields_assigned;

            case '%':
                format = _scan_arg(self, _parse_conv_spec(self, ++format, &ap), &ap);
                break;

            case ' ':
            case '\t':
            case '\v':
            case '\n':
            case '\r':
                format++;
                for (;;) {
                    const int ch = __scn_getc(self);

                    if (ch == EOF || !isspace(ch)) {
                        __scn_ungetc(self, ch);
                        break;
                    }
                }
                break;

            default:
                if (*format++ != __scn_getc(self)) {
                    scn_setfailed(self);
                }
                break;
        }
    }
    // Parsing failed syntactically or because of a read error

    return (self->fields_assigned == 0) ? -1 : self->fields_assigned;
}
