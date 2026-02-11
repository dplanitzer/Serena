/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <ext/__fmt.h>
#include <ext/limits.h>
#include <ext/math.h>
#include <ieeefp/dtoa.h>
#include <ieeefp/ieeefp.h>
#include <stdint.h>
#include <string.h>

//#define _WANT_IO_C99_FORMATS 1

/* For %La, an exponent of 15 bits occupies the exponent character, a
   sign, and up to 5 digits.  */
#define MAXEXPLEN   7
#define DEFPREC     6

#define to_char(n)  ((n) + '0')
#define _LONG_DOUBLE long double

#ifdef _NO_LONGDBL
#define _PRINTF_FLOAT_TYPE double
#define _DTOA_R dtoa
#define FREXP frexp         //XXX add an implementation of frexp() for the 'A' format
#else /* !_NO_LONGDBL */
#define _PRINTF_FLOAT_TYPE _LONG_DOUBLE
#define _DTOA_R _ldtoa_r
#define FREXP frexpl
#endif /* !_NO_LONGDBL */

 #define PRINTANDPAD(self, p, ep, len, pad) { \
    int n = (ep) - (p); \
    if (n > (len)) \
        n = (len); \
    if (n > 0) \
        __fmt_write_string((self), (p), n); \
    __fmt_write_char_rep((self), (pad), (len) - (n > 0 ? n : 0)); \
}


static int __isinf(double x)
{
    union { uint64_t u; double f; } fp;
    
    fp.f = x;
    return ((unsigned)(fp.u >> 32) & 0x7fffffff) == 0x7ff00000 &&
           ((unsigned)fp.u == 0);
}

static int __isnan(double x)
{
    union { uint64_t u; double f; } fp;

    fp.f = x;
    return ((unsigned)(fp.u >> 32) & 0x7fffffff) +
           ((unsigned)fp.u != 0) > 0x7ff00000;
}

static int __signbit(double x)
{
    union double_union tmp;

    tmp.d = x;
    return (word0 (tmp) & Sign_bit) ? 1 : 0;
}

/* Using reentrant DATA, convert finite VALUE into a string of digits
   with no decimal point, using NDIGITS precision and FLAGS as guides
   to whether trailing zeros must be included.  Set *SIGN to nonzero
   if VALUE was negative.  Set *DECPT to the exponent plus one.  Set
   *LENGTH to the length of the returned string.  CH must be one of
   [aAeEfFgG]; if it is [aA], then the return string lives in BUF,
   otherwise the return value shares the mprec reentrant storage.  */
static char *
cvt(_PRINTF_FLOAT_TYPE value, int ndigits, unsigned char flags,
    char *sign, int *decpt, int ch, int *length, char *buf)
{
    int mode, dsgn;
    char *digits, *bp, *rve;
#ifdef _NO_LONGDBL
    union double_union tmp;

    tmp.d = value;
    if (word0 (tmp) & Sign_bit) { /* this will check for < 0 and -0.0 */
        value = -value;
        *sign = '-';
    } else {
        *sign = '\000';
    }
#else /* !_NO_LONGDBL */
    union
    {
        struct ldieee ieee;
        long double val;
    } ld;

    ld.val = value;
    if (ld.ieee.sign) { /* this will check for < 0 and -0.0 */
        value = -value;
        *sign = '-';
    } else {
        *sign = '\000';
    }
#endif /* !_NO_LONGDBL */

#ifdef _WANT_IO_C99_FORMATS
    if (ch == 'a' || ch == 'A') {
        /* This code assumes FLT_RADIX is a power of 2.  The initial
           division ensures the digit before the decimal will be less
           than FLT_RADIX (unless it is rounded later).  There is no
           loss of precision in these calculations.  */
        value = FREXP (value, decpt) / 8;
        if (!value) {
            *decpt = 1;
        }
        digits = ch == 'a' ? "0123456789abcdef" : "0123456789ABCDEF";
        bp = buf;
        do {
            value *= 16;
            mode = (int) value;
            value -= mode;
            *bp++ = digits[mode];
        } while (ndigits-- && value);
        if (value > 0.5 || (value == 0.5 && mode & 1)) {
            /* round to even */
            rve = bp;
            while (*--rve == digits[0xf]) {
                *rve = '0';
            }
            *rve = *rve == '9' ? digits[0xa] : *rve + 1;
        } else {
            while (ndigits-- >= 0) {
                *bp++ = '0';
            }
        }
        *length = bp - buf;
        return buf;
    }
#endif /* _WANT_IO_C99_FORMATS */
    if (ch == 'f' || ch == 'F') {
        mode = 3;               /* ndigits after the decimal point */
    } else {
        /* To obtain ndigits after the decimal point for the 'e'
         * and 'E' formats, round to ndigits + 1 significant
         * figures.
         */
        if (ch == 'e' || ch == 'E') {
            ndigits++;
        }
        mode = 2;               /* ndigits significant digits */
    }

    digits = _DTOA_R (value, mode, ndigits, decpt, &dsgn, &rve); // XXX use __dtoa_r() instead

    if ((ch != 'g' && ch != 'G') || (flags & __FMT_ALTFORM) != 0) {  /* Print trailing zeros */
        bp = digits + ndigits;
        if (ch == 'f' || ch == 'F') {
            if (*digits == '0' && value) {
                *decpt = -ndigits + 1;
            }
            bp += *decpt;
        }
        if (value == 0) { /* kludge for __dtoa irregularity */
            rve = bp;
        }
        while (rve < bp) {
            *rve++ = '0';
        }
    }
    *length = rve - digits;

    return digits;
}

static int
exponent(char *p0, int exp, int fmtch)
{
    register char *p, *t;
    char expbuf[MAXEXPLEN];
#ifdef _WANT_IO_C99_FORMATS
    const int isa = (fmtch == 'a' || fmtch == 'A');
#else
    const int isa = 0;
#endif

    p = p0;
    *p++ = isa ? 'p' - 'a' + fmtch : fmtch;
    if (exp < 0) {
        exp = -exp;
        *p++ = '-';
    }
    else {
        *p++ = '+';
    }
    
    t = expbuf + MAXEXPLEN;
    if (exp > 9) {
        do {
            *--t = to_char (exp % 10);
        } while ((exp /= 10) > 9);
        *--t = to_char (exp);
        for (; t < expbuf + MAXEXPLEN; *p++ = *t++);
    }
    else {
        if (!isa) {
            *p++ = '0';
        }
        *p++ = to_char (exp);
    }

    return p - p0;
}

static void __fmt_format_fp(fmt_t* _Nonnull _Restrict self, char ch, va_list* _Nonnull _Restrict ap)
{
    register char *cp;      /* handy char pointer (short term usage) */ //XXX needs to be defined
    _PRINTF_FLOAT_TYPE _fpvalue = 0;
    char sign = '\0';       /* sign prefix (' ', '+', '-', or \0) */
    char softsign;          /* temporary negative sign for floats */
    int expt;               /* integer value of exponent */
    int expsize = 0;        /* character count for expstr */
    char expstr[MAXEXPLEN]; /* buffer for exponent string */
    int lead = 0;           /* sig figs before decimal or group sep */
    char *decimal_point = "."; //_localeconv_r (data)->decimal_point;
    size_t decp_len = strlen (decimal_point);
    int ndig = 0;           /* actual number of digits returned by cvt */
    int realsz;             /* field size expanded by dprec */
    int size;               /* size of converted field or string */
    int dprec = 0;          /* a copy of prec if [diouxX], 0 otherwise */
    char ox[2];             /* space for 0x hex-prefix */
    char doHexPrefix = 0;
#if defined (_WANT_IO_C99_FORMATS)
                                 /* locale specific numeric grouping */
    char *thousands_sep = "";
    size_t thsnd_len = 0;
    const char *grouping = "";
    int nseps;              /* number of group separators with ' */
    int nrepeats;           /* number of repeats of the last group */
#endif

    switch (ch) {
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'a':
        case 'A':
        case 'g':
        case 'G':
#ifdef _NO_LONGDBL
            if (self->spec.lenMod == FMT_LENMOD_L) {
                _fpvalue = (double) fmt_arg(ap, _LONG_DOUBLE);
            } else {
                _fpvalue = fmt_arg(ap, double);
            }

            /* do this before tricky precision changes
               If the output is infinite or NaN, leading
               zeros are not permitted.  Otherwise, scanf
               could not read what printf wrote.
            */
            if (__isinf (_fpvalue)) {
                if (_fpvalue < 0)
                    sign = '-';
                if (ch <= 'G') /* 'A', 'E', 'F', or 'G' */
                    cp = "INF";
                else
                    cp = "inf";
                size = 3;
                self->spec.flags &= ~__FMT_PADZEROS;
                break;
            }
            if (__isnan (_fpvalue)) {
                if (__signbit (_fpvalue))
                    sign = '-';
                if (ch <= 'G') /* 'A', 'E', 'F', or 'G' */
                    cp = "NAN";
                else
                    cp = "nan";
                size = 3;
                self->spec.flags &= ~__FMT_PADZEROS;
                break;
            }

#else /* !_NO_LONGDBL */

            if (self->spec.lenMod == FMT_LENMOD_L) {
                _fpvalue = fmt_arg(ap, _LONG_DOUBLE);
            } else {
                _fpvalue = (_LONG_DOUBLE)fmt_arg(ap, double);
            }

            /* do this before tricky precision changes */
            expt = _ldcheck (&_fpvalue);
            if (expt == 2) {
                if (_fpvalue < 0)
                    sign = '-';
                if (ch <= 'G') /* 'A', 'E', 'F', or 'G' */
                    cp = "INF";
                else
                    cp = "inf";
                size = 3;
                self->spec.flags &= ~__FMT_PADZEROS;
                break;
            }
            if (expt == 1) {
                if (signbit (_fpvalue))
                    sign = '-';
                if (ch <= 'G') /* 'A', 'E', 'F', or 'G' */
                    cp = "NAN";
                else
                    cp = "nan";
                size = 3;
                self->spec.flags &= ~__FMT_PADZEROS;
                break;
            }
#endif /* !_NO_LONGDBL */

#ifdef _WANT_IO_C99_FORMATS
            if (ch == 'a' || ch == 'A') {
                ox[0] = '0';
                ox[1] = ch == 'a' ? 'x' : 'X';
                doHexPrefix = 1;
                if (self->spec.prec >= BUF)
                {
                    //XXX fix me
                    if ((malloc_buf = (char *)_malloc_r (data, self->spec.prec + 1)) == NULL) {
                        fp->_flags |= __SERR;
                        goto error;
                    }
                    cp = malloc_buf;
                }
                else
                    cp = buf;
            } else
#endif /* _WANT_IO_C99_FORMATS */
            if (!FMT_IS_HASPREC(self->spec.flags)) {
                self->spec.prec = DEFPREC;
            } else if ((ch == 'g' || ch == 'G') && self->spec.prec == 0) {
                self->spec.prec = 1;
            }

            //XXX the 'cp' that we pass to cvt() should be a buffer (used for 'A' format)
            cp = cvt (_fpvalue, self->spec.prec, self->spec.flags, &softsign, &expt, ch, &ndig, cp);

            if (ch == 'g' || ch == 'G') {
                if (expt <= -4 || expt > self->spec.prec) {
                    ch -= 2; /* 'e' or 'E' */
                } else {
                    ch = 'g';
                }
            }
#ifdef _WANT_IO_C99_FORMATS
            else if (ch == 'F') {
                ch = 'f';
            }
#endif
            if (ch <= 'e') {        /* 'a', 'A', 'e', or 'E' fmt */
                --expt;
                expsize = exponent (expstr, expt, ch);
                size = expsize + ndig;
                if (ndig > 1 || (self->spec.flags & __FMT_ALTFORM) != 0) {
                    size += decp_len;
                }
#ifdef _WANT_IO_C99_FORMATS
                self->spec.flags &= ~GROUPING;
#endif
            } else {
                if (ch == 'f') {                /* f fmt */
                    if (expt > 0) {
                        size = expt;
                        if (self->spec.prec || (self->spec.flags & __FMT_ALTFORM) != 0)
                            size += self->spec.prec + decp_len;
                    } else  /* "0.X" */
                        size = (self->spec.prec || (self->spec.flags & __FMT_ALTFORM) != 0)
                                    ? self->spec.prec + 1 + decp_len
                                    : 1;
                } else if (expt >= ndig) { /* fixed g fmt */
                    size = expt;
                    if ((self->spec.flags & __FMT_ALTFORM) != 0)
                        size += decp_len;
                } else {
                    size = ndig + decp_len;
                    if (expt <= 0) {
                        size += 1 - expt;
                    }
                }
#ifdef _WANT_IO_C99_FORMATS
                if ((self->spec.flags & GROUPING) != 0 && expt > 0) {
                    /* space for thousands' grouping */
                    nseps = nrepeats = 0;
                    lead = expt;
                    while (*grouping != CHAR_MAX) {
                        if (lead <= *grouping) {
                            break;
                        }
                        lead -= *grouping;
                        if (grouping[1]) {
                            nseps++;
                            grouping++;
                        } else {
                            nrepeats++;
                        }
                    }
                    size += (nseps + nrepeats) * thsnd_len;
                } else
#endif
                    lead = expt;
            }

            if (softsign) {
                sign = '-';
            }
            break;

        default:
            return;
    }

    /*
     * All reasonable formats wind up here.  At this point, `cp'
     * points to a string which (if not flags&LADJUST) should be
     * padded out to `width' places.  If flags&ZEROPAD, it should
     * first be prefixed by any sign or other prefix; otherwise,
     * it should be blank padded before the prefix is emitted.
     * After any left-hand padding and prefixing, emit zeroes
     * required by a decimal [diouxX] precision, then print the
     * string proper, then emit zeroes required by any leftover
     * floating precision; finally, if LADJUST, pad with blanks.
     * If flags&FPT, ch must be in [aAeEfg].
     *
     * Compute actual size, so we know how much to pad.
     * size excludes decimal prec; realsz includes it.
     */
    realsz = dprec > size ? dprec : size;
    if (sign) {
        realsz++;
    }
    if (doHexPrefix) {
        realsz+= 2;
    }

    /* right-adjusting blank padding */
    if (!FMT_IS_LEFTJUST(self->spec.flags)) {
        __fmt_write_char_rep (self, ' ', self->spec.minFieldWidth - realsz);
    }

    /* prefix */
    if (sign) {
        __fmt_write_char (self, sign);
    }
    if (doHexPrefix) {
        __fmt_write_string (self, ox, 2);
    }

    /* right-adjusting zero padding */
    if (FMT_IS_PADZEROS(self->spec.flags)) {
        __fmt_write_char_rep (self, '0', self->spec.minFieldWidth - realsz);
    }

    /* leading zeroes from decimal precision */
    __fmt_write_char_rep (self, '0', dprec - size);

    /* the string or number proper */
    /* glue together f_p fragments */
    if (ch >= 'f') {        /* 'f' or 'g' */
        if (_fpvalue == 0) {
            /* kludge for __dtoa irregularity */
            __fmt_write_char (self, '0');
            if (expt < ndig || (self->spec.flags & __FMT_ALTFORM) != 0) {
                __fmt_write_string (self, decimal_point, decp_len);
                __fmt_write_char_rep (self, '0', ndig - 1);
            }
        } else if (expt <= 0) {
            __fmt_write_char (self, '0');
            if (expt || ndig || (self->spec.flags & __FMT_ALTFORM) != 0) {
                __fmt_write_string (self, decimal_point, decp_len);
                __fmt_write_char_rep (self, '0', -expt);
                __fmt_write_string (self, cp, ndig);
            }
        } else {
            char *convbuf = cp;
            PRINTANDPAD(self, cp, convbuf + ndig, lead, '0');
            cp += lead;
#ifdef _WANT_IO_C99_FORMATS
            if (flags & GROUPING) {
                while (nseps > 0 || nrepeats > 0) {
                    if (nrepeats > 0) {
                        nrepeats--;
                    } else {
                        grouping--;
                        nseps--;
                    }
                    __fmt_write_string (self, thousands_sep, thsnd_len);
                    PRINTANDPAD (self, cp, convbuf + ndig, *grouping, '0');
                    cp += *grouping;
                }
                if (cp > convbuf + ndig) {
                    cp = convbuf + ndig;
                }
            }
#endif
            if (expt < ndig || (self->spec.flags & __FMT_ALTFORM) != 0) {
                __fmt_write_string (self, decimal_point, decp_len);
            }
            PRINTANDPAD (self, cp, convbuf + ndig, ndig - expt, '0');
        }
    } else {        /* 'a', 'A', 'e', or 'E' */
        if (ndig > 1 || (self->spec.flags & __FMT_ALTFORM) != 0) {
            __fmt_write_string (self, cp, 1);
            cp++;
            __fmt_write_string (self, decimal_point, decp_len);
            if (_fpvalue) {
                __fmt_write_string (self, cp, ndig - 1);
            } else {  /* 0.[0..] */
                /* __dtoa irregularity */
                __fmt_write_char_rep (self, '0', ndig - 1);
            }
        } else {  /* XeYYY */
            __fmt_write_string (self, cp, 1);
        }
        __fmt_write_string (self, expstr, expsize);
    }
}

void __fmt_init_fp(fmt_t* _Nonnull _Restrict self, void* _Nullable _Restrict s, fmt_putc_func_t _Nonnull putc_f, fmt_write_func_t _Nonnull write_f, bool doContCountingOnError)
{
    __fmt_common_init(self, s, putc_f, write_f, doContCountingOnError);
    self->format_cb = __fmt_format_fp;
}
