//
//  xtoa.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#define _OPEN_SYS_ITOA_EXT
#include <stdlib.h>
#include <string.h>
#include <__stddef.h>


char* _Nonnull __i32toa(int32_t val, i32a_t* _Nonnull out)
{
    char *ep = &out->buffer[I32A_BUFFER_SIZE - 1];
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
            out->length = 11;
            out->offset = 0;
            memcpy(out->buffer, "-2147483648", 12);
            return out->buffer;
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
    } while (val);

    *p = sign;

    out->length = ep - p;
    out->offset = p - out->buffer;

    return p;
}

char* _Nonnull __i64toa(int64_t val, i64a_t* _Nonnull out)
{
    char *ep = &out->buffer[I64A_BUFFER_SIZE - 1];
    char *p = ep;
    char sign;
    int64_t q, r;

    if (val < 0) {
        if (val == INT64_MIN) {
            out->length = 20;
            out->offset = 0;
            memcpy(out->buffer, "-9223372036854775808", 21);
            return out->buffer;
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
    } while (val);
    
    *p = sign;

    out->length = ep - p;
    out->offset = p - out->buffer;

    return p;
}


static const char* gLowerDigits = "0123456789abcdef";
static const char* gUpperDigits = "0123456789ABCDEF";

// 'radix' must be 2, 8, 10 or 16
char* _Nonnull __ui32toa(uint32_t val, int radix, bool isUppercase, i32a_t* _Nonnull out)
{
    const char* ds = (isUppercase) ? gUpperDigits : gLowerDigits;
    char *ep = &out->buffer[I32A_BUFFER_SIZE - 1];
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

    *p = '+';
    
    out->length = ep - p;
    out->offset = p - out->buffer;

    return p;
}

// 'radix' must be 2, 8, 10 or 16
char* _Nonnull __ui64toa(uint64_t val, int radix, bool isUppercase, i64a_t* _Nonnull out)
{
    const char* ds = (isUppercase) ? gUpperDigits : gLowerDigits;
    char *ep = &out->buffer[I64A_BUFFER_SIZE - 1];
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
    
    *p = '+';

    out->length = ep - p;
    out->offset = p - out->buffer;

    return p;
}

static void copy_out(char* _Nonnull buf, const char* _Nonnull pCanonDigits)
{
    const char* p = (pCanonDigits[0] == '+') ? &pCanonDigits[1] : pCanonDigits;

    while (*p != '\0') { *buf++ = *p++; }
    *buf = '\0';
}

char *itoa(int val, char *buf, int radix)
{
    i32a_t i32a;
    char* p;

    if (buf) {
        switch (radix) {
            case 10:
                p = __i32toa((int32_t)val, &i32a);
                break;
        
            case 2:
            case 8:
            case 16:
                p = __ui32toa((uint32_t)val, radix, false, &i32a);
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
    i64a_t i64a;
    char* p;

    if (buf) {
        switch (radix) {
            case 10:
                p = __i64toa((int64_t)val, &i64a);
                break;
        
            case 2:
            case 8:
            case 16:
                p = __ui64toa((uint64_t)val, radix, false, &i64a);
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
    i32a_t i32a;
    char* p;

    if (buf) {
        switch (radix) {
            case 2:
            case 8:
            case 10:
            case 16:
                p = __ui32toa((uint32_t)val, radix, false, &i32a);
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
    i64a_t i64a;
    char* p;

    if (buf) {
        switch (radix) {
            case 2:
            case 8:
            case 10:
            case 16:
                p = __ui64toa((uint64_t)val, radix, false, &i64a);
                break;

            default:
                errno = EINVAL;
                return NULL;
        }
        copy_out(buf, p);
    }
    return buf;
}
