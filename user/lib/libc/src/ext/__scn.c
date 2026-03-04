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
#include <string.h>
#ifndef EOF
#define EOF -1
#endif


int __scn_getc(scn_t* _Nonnull self)
{
    char ch;
    const ssize_t r = self->getc_cb(&ch, self->stream);

    if (r == 1) {
        self->chars_read++;
        return (int)ch;
    }
    else if (r == 0) {
        scn_setfailed(self);
    }
    else {
        scn_seteof(self);
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

    switch (*format) {
        case 'l':
            format++;
            if (*format == 'l') {
                format++;
                spec->lm = SCN_LM_ll;
            } else {
                spec->lm = SCN_LM_l;
            }
            break;

        case 'h':
            format++;
            if (*format == 'h') {
                format++;
                spec->lm = SCN_LM_hh;
            } else {
                spec->lm = SCN_LM_h;
            }
            break;

        case 'j':
            format++;
            spec->lm = SCN_LM_j;
            break;

        case 'z':
            format++;
            spec->lm = SCN_LM_z;
            break;

        case 't':
            format++;
            spec->lm = SCN_LM_t;
            break;

        case 'L':   // long double
            format++;
            spec->lm = SCN_LM_L;
            break;

        default:
            break;
    }
    
    return format;
}

static int _atoi(const char* _Nonnull _Restrict str, char* _Nonnull _Restrict * _Nonnull _Restrict str_end)
{
    long r;

    if (__strtoi32(str, str_end, 10, LONG_MIN, LONG_MAX, __LONG_MAX_BASE_10_DIGITS, &r) == 0) {
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

    spec->max_field_width = 0;
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

static void _scan_out_nchars(scn_t* _Nonnull _Restrict self, va_list* _Nonnull _Restrict ap)
{
    char* p = scn_arg(ap, char*);
    const size_t n = self->chars_read;

    if (SCN_SUPPRESSED(self->spec.flags)) {
        return;
    }

    switch (self->spec.lm) {
        case SCN_LM_hh:     *((signed char*)p) = __min(n, SCHAR_MAX);   break;
        case SCN_LM_h:      *((short*)p) = __min(n, SHRT_MAX);          break;
        case SCN_LM_none:   *((int*)p) = __min(n, INT_MAX);             break;
        case SCN_LM_l:      *((long*)p) = __min(n, LONG_MAX);           break;
        case SCN_LM_ll:     *((long long*)p) = __min(n, LLONG_MAX);     break;
        case SCN_LM_j:      *((intmax_t*)p) = __min(n, INTMAX_MAX);     break;
        case SCN_LM_z:      *((ssize_t*)p) = __min(n, SSIZE_MAX);       break;
        case SCN_LM_t:      *((ptrdiff_t*)p) = __min(n, PTRDIFF_MAX);   break;
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

        case 'c':   break;
        case 's':   break;
        case '[':   break;
        case 'd':   // fall through
        case 'i':   break;
        case 'o':   break;
        case 'x':   break;
        case 'X':   break;
        case 'u':   break;
        case 'n':   _scan_out_nchars(self, ap); break;
        case 'p':   break;
        
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
    self->fieldsAssigned = 0;
    self->flags = 0;

    while (self->flags == 0) {
        switch (*format) {
            case '\0':
                return self->fieldsAssigned;

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

    return (self->fieldsAssigned == 0) ? -1 : self->fieldsAssigned;
}
