//
//  xtoa.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

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

static const char* gLowerDigits = "0123456789abcdef";
static const char* gUpperDigits = "0123456789ABCDEF";

// 'buf' must be at least DIGIT_BUFFER_CAPACITY bytes big
char* _Nonnull __i32toa(int32_t val, int radix, bool isUppercase, char* _Nonnull digits)
{
    const char* ds = (isUppercase) ? gUpperDigits : gLowerDigits;
    char *p = &digits[DIGIT_BUFFER_CAPACITY - 1];
    char sign;
    int i = 1;

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
        *p-- = ds[val % radix];
        val /= radix;
        i++;
    } while (val != 0);

    *p-- = sign;
    *p = i;

    return p;
}

// 'digits' must be at least DIGIT_BUFFER_CAPACITY bytes big
char* _Nonnull __i64toa(int64_t val, int radix, bool isUppercase, char* _Nonnull digits)
{
    const char* ds = (isUppercase) ? gUpperDigits : gLowerDigits;
    char *p = &digits[DIGIT_BUFFER_CAPACITY - 1];
    char sign;
    int64_t q, r;
    int i = 1;

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
        _divmods64(val, radix, &q, &r);
        *p-- = ds[r];
        val = q;
        i++;
    } while (val != 0);
    
    *p-- = sign;
    *p = i;

    return p;
}

// 'buf' must be at least DIGIT_BUFFER_CAPACITY bytes big
// 'radix' must be 8, 10 or 16
char* _Nonnull __ui32toa(uint32_t val, int radix, bool isUppercase, char* _Nonnull digits)
{
    const char* ds = (isUppercase) ? gUpperDigits : gLowerDigits;
    char *p = &digits[DIGIT_BUFFER_CAPACITY - 1];
    int i = 1;

    *p-- = '\0';
    do {
        *p-- = ds[val % radix];
        val /= radix;
        i++;
    } while (val != 0);

    *p-- = '+';
    *p = i;

    return p;
}

// 'digits' must be at least DIGIT_BUFFER_CAPACITY bytes big
// 'radix' must be 8, 10 or 16
char* _Nonnull __ui64toa(uint64_t val, int radix, bool isUppercase, char* _Nonnull digits)
{
    const char* ds = (isUppercase) ? gUpperDigits : gLowerDigits;
    char *p = &digits[DIGIT_BUFFER_CAPACITY - 1];
    int64_t q, r;
    int i = 1;

    *p-- = '\0';
    do {
        _divmods64(val, radix, &q, &r);
        *p-- = ds[r];
        val = q;
        i++;
    } while (val != 0);
    
    *p-- = '+';
    *p = i;

    return p;
}

static char* _Nonnull copy_out(char* _Nonnull buf, const char* _Nonnull pCanonDigits)
{
    const char* p = (pCanonDigits[1] == '+') ? &pCanonDigits[2] : &pCanonDigits[1];

    while (*p != '\0') { *buf++ = *p++; }
    *buf = '\0';
    return buf;
}

char *itoa(int val, char *buf, int radix)
{
    if (radix != 8 && radix != 10 && radix != 16 || buf == NULL) {
        return NULL;
    }

    char t[DIGIT_BUFFER_CAPACITY];
    return copy_out(buf, __i32toa((int32_t)val, radix, false, t));
}

char *ltoa(long val, char *buf, int radix)
{
    if (radix != 8 && radix != 10 && radix != 16 || buf == NULL) {
        return NULL;
    }

    char t[DIGIT_BUFFER_CAPACITY];
    return copy_out(buf, __i32toa((int32_t)val, radix, false, t));
}

char *lltoa(long long val, char *buf, int radix)
{
    if (radix != 8 && radix != 10 && radix != 16 || buf == NULL) {
        return NULL;
    }

    char t[DIGIT_BUFFER_CAPACITY];
    return copy_out(buf, __i64toa((int64_t)val, radix, false, t));
}
