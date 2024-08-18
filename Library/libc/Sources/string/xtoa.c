//
//  xtoa.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#define _OPEN_SYS_ITOA_EXT
#include <stdlib.h>
#include <__stddef.h>

static char* _Nonnull copy_constant(char* _Nonnull digits, const char* cst)
{
    char* p = &digits[1];
    int i = 0;

    while (*cst != '\0') {
        *p++ = *cst++;
        i++;
    }
    digits[0] = i;
    return digits;
}


// 'buf' must be at least DIGIT_BUFFER_CAPACITY bytes big
char* _Nonnull __i32toa(int32_t val, char* _Nonnull digits)
{
    char *ep = &digits[DIGIT_BUFFER_CAPACITY - 1];
    char *p = ep;
    char sign;

    if (val < 0) {
        // Check for the smallest possible negative value and handle it specially.
        // We do it this way because negating the smallest possible value causes
        // an overflow and we end up with the original value again. Secondly we
        // do not want to have to implement two loops: one for positive and one
        // for negative values because that would effectively cause us to implement
        // the same algorithm twice (with only diff being that we'd subtract the
        // remainder from '0' instead of adding it). Doing that would be stupid.
        if (val == INT32_MIN) {
            return copy_constant(digits, "-2147483648");
        }
        val = -val;
        sign = '-';
    } else {
        sign = '+';
    }

    *p-- = '\0';
    do {
        *p-- = '0' + (char)(val % 10);
        val /= 10;
    } while (val != 0);

    *p-- = sign;
    *p = ep - (p + 1);

    return p;
}

// 'digits' must be at least DIGIT_BUFFER_CAPACITY bytes big
char* _Nonnull __i64toa(int64_t val, char* _Nonnull digits)
{
    char *ep = &digits[DIGIT_BUFFER_CAPACITY - 1];
    char *p = ep;
    char sign;
    int64_t q, r;

    if (val < 0) {
        if (val == INT64_MIN) {
            return copy_constant(digits, "-9223372036854775808");
        }
        val = -val;
        sign = '-';
    } else {
        sign = '+';
    }

    *p-- = '\0';
    do {
        _divmods64(val, 10, &q, &r);
        *p-- = '0' + (char)r;
        val = q;
    } while (val != 0);
    
    *p-- = sign;
    *p = ep - (p + 1);

    return p;
}


static const char* gLowerDigits = "0123456789abcdef";
static const char* gUpperDigits = "0123456789ABCDEF";

// 'buf' must be at least DIGIT_BUFFER_CAPACITY bytes big
// 'radix' must be 2, 8, 10 or 16
char* _Nonnull __ui32toa(uint32_t val, int radix, bool isUppercase, char* _Nonnull digits)
{
    const char* ds = (isUppercase) ? gUpperDigits : gLowerDigits;
    char *ep = &digits[DIGIT_BUFFER_CAPACITY - 1];
    char *p = ep;

    *p-- = '\0';
    switch (radix) {
        case 2:
            do {
                *p-- = ds[val & 0x1u];
                val >>= 1u;
            } while (val);
            break;

        case 16:
            do {
                *p-- = ds[val & 0x0fu];
                val >>= 4u;
            } while (val);
            break;

        default:
            do {
                *p-- = ds[val % radix];
                val /= radix;
            } while (val);
            break;
    }

    *p-- = '+';
    *p = ep - (p + 1);

    return p;
}

// 'digits' must be at least DIGIT_BUFFER_CAPACITY bytes big
// 'radix' must be 2, 8, 10 or 16
char* _Nonnull __ui64toa(uint64_t val, int radix, bool isUppercase, char* _Nonnull digits)
{
    const char* ds = (isUppercase) ? gUpperDigits : gLowerDigits;
    char *ep = &digits[DIGIT_BUFFER_CAPACITY - 1];
    char *p = ep;
    int64_t q, r;

    *p-- = '\0';
    switch (radix) {
        case 2:
            do {
                *p-- = ds[(uint8_t)val & 0x1u];
                val >>= 1ull;
            } while (val);
            break;

        case 16:
            do {
                *p-- = ds[(uint8_t)val & 0x0fu];
                val >>= 4ull;
            } while (val);
            break;

        default:
            do {
                _divmods64(val, radix, &q, &r);
                *p-- = ds[r];
                val = q;
            } while (val);
            break;
    }
    
    *p-- = '+';
    *p = ep - (p + 1);

    return p;
}

static void copy_out(char* _Nonnull buf, const char* _Nonnull pCanonDigits)
{
    const char* p = (pCanonDigits[1] == '+') ? &pCanonDigits[2] : &pCanonDigits[1];

    while (*p != '\0') { *buf++ = *p++; }
    *buf = '\0';
}

char *itoa(int val, char *buf, int radix)
{
    char t[DIGIT_BUFFER_CAPACITY];
    char* p;

    if (buf) {
        switch (radix) {
            case 10:
                p = __i32toa((int32_t)val, t);
                break;
        
            case 2:
            case 8:
            case 16:
                p = __ui32toa((uint32_t)val, radix, false, t);
                break;

            default:
                errno = EINVAL;
                return NULL;
        }
        copy_out(buf, p);
    }
    return buf;
}

char *ltoa(long val, char *buf, int radix)
{
    return itoa(val, buf, radix);
}

char *lltoa(long long val, char *buf, int radix)
{
    char t[DIGIT_BUFFER_CAPACITY];
    char* p;

    if (buf) {
        switch (radix) {
            case 10:
                p = __i64toa((int64_t)val, t);
                break;
        
            case 2:
            case 8:
            case 16:
                p = __ui64toa((uint64_t)val, radix, false, t);
                break;

            default:
                errno = EINVAL;
                return NULL;
        }
        copy_out(buf, p);
    }
    return buf;
}


char *utoa(unsigned int val, char *buf, int radix)
{
    char t[DIGIT_BUFFER_CAPACITY];
    char* p;

    if (buf) {
        switch (radix) {
            case 2:
            case 8:
            case 10:
            case 16:
                p = __ui32toa((uint32_t)val, radix, false, t);
                break;

            default:
                errno = EINVAL;
                return NULL;
        }
        copy_out(buf, p);
    }
    return buf;
}

char *ultoa(unsigned long val, char *buf, int radix)
{
    return utoa(val, buf, radix);
}

char *ulltoa(unsigned long long val, char *buf, int radix)
{
    char t[DIGIT_BUFFER_CAPACITY];
    char* p;

    if (buf) {
        switch (radix) {
            case 2:
            case 8:
            case 10:
            case 16:
                p = __ui64toa((uint64_t)val, radix, false, t);
                break;

            default:
                errno = EINVAL;
                return NULL;
        }
        copy_out(buf, p);
    }
    return buf;
}
