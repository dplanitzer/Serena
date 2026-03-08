//
//  __scn_fp.c
//  libc
//
//  Created by Dietmar Planitzer on 3/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ctype.h>
#include <ext/__scn.h>
#include <ext/limits.h>
#include <ext/math.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#define __ST_MANT_SIGN  0
#define __ST_HEX_BASE   1
#define __ST_HEX_BASE_X 2
#define __ST_MANT       3
#define __ST_EXP_SIGN   4
#define __ST_EXP        5
#define __ST_END        6

// Scans a floating point number of the form:
// [+|-](0x|0X)hhhh[(p|P)(+|-)pddd]
// [+|-]fff.ffff[(e|E)(+|-)ddd]
// [+|-](inf|Inf|infinity|Infinity)
// (nan|Nan)
static void _lex_ieeefp(scn_t* _Nonnull self, int base)
{
    char* p = self->u.digits;
    size_t dig_lim = (self->spec.max_field_width < 0) ? INT_MAX : self->spec.max_field_width;
    int state = __ST_MANT_SIGN, i = 0, d;
    char has_dec_pt = false, ch = __scn_getc(self);

    while (state < __ST_END && i < dig_lim) {
        switch (state) {
            case __ST_MANT_SIGN:
                if (ch == '-' || ch == '+') {
                    p[i++] = ch;
                    ch = __scn_getc(self);
                }

                state = (base == 16) ? __ST_HEX_BASE : __ST_MANT;
                break;

            case __ST_HEX_BASE:
                if (ch == '0') {
                    ch = __scn_getc(self);
                    state = __ST_HEX_BASE_X;
                }
                else {
                    state = __ST_END;
                }
                break;

            case __ST_HEX_BASE_X:
                if (ch == 'x' || ch == 'X') {
                    ch = __scn_getc(self);
                    state = __ST_MANT;
                }
                else {
                    state = __ST_END;
                }
                break;

            case __ST_MANT:
                if (ch == EOF) {
                    state = __ST_END;
                    break;
                }

                if (isdigit(ch)) {
                    d = ch - '0';
                }
                else if (base == 16 && isupper(ch)) {
                    d = ch - 'A' + 10;
                }
                else if (base == 16 && islower(ch)) {
                    d = ch - 'a' + 10;
                }
                else if (ch == '.' && !has_dec_pt) {
                    d = 0;  // accept the first decimal point that we see
                    has_dec_pt = true;
                }
                else if ((base == 10 && (ch == 'e' || ch == 'E')) || ((base == 16) && (ch == 'p' || ch == 'P'))) {
                    d = 0;
                    state = __ST_EXP_SIGN;  // accept the exponent indicator character
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
                if (i < __SCN_MAX_DIGITS) p[i++] = ch;
                ch = __scn_getc(self);
                break;

            case __ST_EXP_SIGN:
                if (ch == '-' || ch == '+') {
                    if (i < __SCN_MAX_DIGITS) p[i++] = ch;
                    ch = __scn_getc(self);
                }

                state = __ST_EXP;
                break;

            case __ST_EXP:
                if (ch == EOF) {
                    state = __ST_END;
                    break;
                }

                if (isdigit(ch)) {
                    d = ch - '0';
                }
                else {
                    state = __ST_END;
                    break;
                }

                if (d >= 10) {
                    state = __ST_END;
                    break;
                }

                if (i < __SCN_MAX_DIGITS) p[i++] = ch;
                ch = __scn_getc(self);
                break;
        }
    }

    p[i] = '\0';
    __scn_ungetc(self, ch);
}

static void _scan_ieeefp(scn_t* _Nonnull _Restrict self, int base, va_list* _Nonnull _Restrict ap)
{
    double d64;

    __scn_skip_ws(self);
    _lex_ieeefp(self, base);

    if (scn_failed(self) || self->u.digits[0] == '\0' || SCN_SUPPRESSED(self->spec.flags)) {
        return;
    }


    //XXX create a __strtod() variant similar to __strtoi32() which returns the error code and doesn't touch errno 
    const int saved_errno = errno;
    d64 = strtod(self->u.digits, NULL);
    errno = saved_errno;

    
    void* p = scn_arg(ap, void*);
    switch (self->spec.lm) {
        case SCN_LM_none:   *(float*)p = (float)d64; break;
        case SCN_LM_l:      *(double*)p = d64; break;
        case SCN_LM_L:      *(long double*)p = d64; break;   // XXX add proper long double support
    }

    self->fields_assigned++;
}

static const char* _Nonnull __scn_scan_fp(scn_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict format, va_list* _Nonnull _Restrict ap)
{
    switch (*format++) {
        case 'a':
        case 'A':
            _scan_ieeefp(self, 16, ap);
            break;

        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
            _scan_ieeefp(self, 10, ap);
            break;

        default:
            break;
    }

    return format;
}

void __scn_init_fp(scn_t* _Nonnull _Restrict self, void* _Nullable _Restrict s, scn_getc_t _Nonnull getc_f, scn_ungetc_t _Nonnull ungetc_f)
{
    __scn_common_init(self, s, getc_f, ungetc_f);
    self->scan_cb = __scn_scan_fp;
}
