//
//  xtoa.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <__stddef.h>
#include "printf.h"

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
        *p-- = '0' + (val % 10);
        val /= 10;
        i++;
    } while (val != 0);

    *p-- = sign;
    *p = i;

    return p;
}

// 'digits' must be at least DIGIT_BUFFER_CAPACITY bytes big
char* _Nonnull __i64toa(int64_t val, char* _Nonnull digits)
{
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
        __Int64_DivMod(val, 10, &q, &r);
        *p-- = '0' + r;
        val = q;
        i++;
    } while (val != 0);
    
    *p-- = sign;
    *p = i;

    return p;
}


static const char* gLowerDigits = "0123456789abcdef";
static const char* gUpperDigits = "0123456789ABCDEF";

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
        __Int64_DivMod(val, radix, &q, &r);
        *p-- = ds[r];
        val = q;
        i++;
    } while (val != 0);
    
    *p-- = '+';
    *p = i;

    return p;
}

/// @brief /////////////////////////////////////////////////////////////////////

const char *__lltoa(int64_t val, int radix, bool isUppercase, int fieldWidth, char paddingChar, char *pString, size_t maxLength)
{
    char *p0 = &pString[__max(maxLength - fieldWidth - 1, 0)];
    char *p = &pString[maxLength - 1];
    const char* digits = (isUppercase) ? gUpperDigits : gLowerDigits;
    int64_t absval = (val < 0) ? -val : val;
    int64_t q, r;
    
    if (val < 0) {
        p0--;
    }
    
    pString[maxLength - 1] = '\0';
    do {
        p--;
        __Int64_DivMod(absval, radix, &q, &r);
        *p = digits[r];
        absval = q;
    } while (absval && p >= p0);
    
    if (val < 0) {
        p--;
        *p = '-';
        // Convert a zero padding char to a space 'cause zero padding doesn't make
        // sense if the number is negative.
        if (paddingChar == '0') {
            paddingChar = ' ';
        }
    }
    
    if (paddingChar != '\0') {
        while (p > p0) {
            p--;
            *p = paddingChar;
        }
    }
    
    return __max(p, p0);
}

const char *__ulltoa(uint64_t val, int radix, bool isUppercase, int fieldWidth, char paddingChar, char *pString, size_t maxLength)
{
    char *p0 = &pString[__max(maxLength - fieldWidth - 1, 0)];
    char *p = &pString[maxLength - 1];
    const char* digits = (isUppercase) ? gUpperDigits : gLowerDigits;
    int64_t q, r;
    
    pString[maxLength - 1] = '\0';
    do {
        p--;
        __Int64_DivMod(val, radix, &q, &r);
        *p = digits[r];
        val = q;
    } while (val && p >= p0);
    
    if (paddingChar != '\0') {
        while (p > p0) {
            p--;
            *p = paddingChar;
        }
    }
    
    return __max(p, p0);
}

char *itoa(int val, char *buf, int radix)
{
    if (radix != 8 && radix != 10 && radix != 16 || buf == NULL) {
        return NULL;
    }

    char t[12];
    const char* p = __lltoa(val, radix, 0, 11, 0, t, sizeof(t));
    while (*p != '\0') { *buf++ = *p++; }
    *buf = '\0';
}

char *ltoa(long val, char *buf, int radix)
{
    if (radix != 8 && radix != 10 && radix != 16 || buf == NULL) {
        return NULL;
    }

    char t[12];
    const char* p = __lltoa(val, radix, 0, 11, 0, t, sizeof(t));

    while (*p != '\0') { *buf++ = *p++; }
    *buf = '\0';
}

char *lltoa(long long val, char *buf, int radix)
{
    if (radix != 8 && radix != 10 && radix != 16 || buf == NULL) {
        return NULL;
    }

    char t[22];
    const char* p = __lltoa(val, radix, 0, 21, 0, t, sizeof(t));
    while (*p != '\0') { *buf++ = *p++; }
    *buf = '\0';
}
