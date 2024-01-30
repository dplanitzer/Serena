//
//  Int.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Types.h"
#include "Error.h"

static ErrorCode __atoi64(const Character * _Nonnull str, Character **str_end, Int base, Int64 min_val, Int64 max_val, Int max_digits, Int64 * _Nonnull result)
{
    if ((base < 2 && base != 0) || base > 36) {
        *result = 0ll;
        return EINVAL;
    }


    // Skip whitespace
    while (*str != '\0' && (*str == ' ' || *str == '\t')) {
        str++;
    }


    // Detect the sign
    const Character sig_ch = *str;
    if (sig_ch == '-' || sig_ch == '+') {
        str++;
    }


    // Handle optional octal/hex prefix
    if ((base == 0 || base == 8 || base == 16) && *str == '0') {
        str++;
        if (*str == 'x' || *str == 'X') {
            str++;
            base = 16;
        } else {
            base = 8;
        }
    }
    if (base == 0) {
        base = 10;
    }


    // Convert digits
    const Bool is_neg = (sig_ch == '-');
    UInt64 val = 0;
    const UInt64 llbase = (UInt64) base;
    const UInt64 upper_bound = (is_neg) ? -min_val : max_val;
    const Character upper_num = (base < 10) ? '0' + base : '9';
    const Character upper_lletter = (base > 9) ? 'a' + base - 11 : 'a';
    const Character upper_uletter = (base > 9) ? 'A' + base - 11 : 'A';
    Int i = 0;

    for (;;) {
        const Character ch = str[i];
        UInt64 digit;

        if (ch >= '0' && ch <= upper_num) {
            digit = ch - '0';
        } else if (base > 9 && ((ch >= 'a' && ch <= upper_lletter) || (ch >= 'A' && ch <= upper_uletter))) {
            digit = (ch >= 'a') ? ch - 'a' : ch - 'A';
        } else {
            break;
        }

        const UInt64 new_val = (val * llbase) + digit;
        if (new_val < val || new_val > upper_bound || i > max_digits) {
            if (str_end) *str_end = (Character*)&str[i + 1];
            *result = (is_neg) ? min_val : max_val;
            return ERANGE;
        }

        val = new_val;
        i++;
    }

    if (str_end) *str_end = (Character*)&str[i];
    *result = (is_neg) ? -((Int64)val) : (Int64)val;
    return 0;
}

Int atoi(const Character *str, Character **str_end, Int base)
{
    Int64 r;

    __atoi64(str, str_end, base, INT_MIN, INT_MAX, __LONG_MAX_BASE_10_DIGITS, &r);
    return (Int) r;
}

static Character* _Nonnull copy_constant(Character* _Nonnull digits, const Character* cst)
{
    Character* p = &digits[1];
    Int i = 0;

    while (*cst != '\0') {
        *p++ = *cst++;
        i++;
    }
    digits[0] = i;
    return digits;
}

static const Character* gLowerDigits = "0123456789abcdef";
static const Character* gUpperDigits = "0123456789ABCDEF";

// 'buf' must be at least DIGIT_BUFFER_CAPACITY bytes big
Character* _Nonnull __i32toa(Int32 val, Int radix, Bool isUppercase, Character* _Nonnull digits)
{
    const Character* ds = (isUppercase) ? gUpperDigits : gLowerDigits;
    Character *p = &digits[DIGIT_BUFFER_CAPACITY - 1];
    Character sign;
    Int i = 1;

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
Character* _Nonnull __i64toa(Int64 val, Int radix, Bool isUppercase, Character* _Nonnull digits)
{
    const Character* ds = (isUppercase) ? gUpperDigits : gLowerDigits;
    Character *p = &digits[DIGIT_BUFFER_CAPACITY - 1];
    Character sign;
    Int64 q, r;
    Int i = 1;

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
        Int64_DivMod(val, radix, &q, &r);
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
Character* _Nonnull __ui32toa(UInt32 val, Int radix, Bool isUppercase, Character* _Nonnull digits)
{
    const Character* ds = (isUppercase) ? gUpperDigits : gLowerDigits;
    Character *p = &digits[DIGIT_BUFFER_CAPACITY - 1];
    Int i = 1;

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
Character* _Nonnull __ui64toa(UInt64 val, Int radix, Bool isUppercase, Character* _Nonnull digits)
{
    const Character* ds = (isUppercase) ? gUpperDigits : gLowerDigits;
    Character *p = &digits[DIGIT_BUFFER_CAPACITY - 1];
    Int64 q, r;
    Int i = 1;

    *p-- = '\0';
    do {
        Int64_DivMod(val, radix, &q, &r);
        *p-- = ds[r];
        val = q;
        i++;
    } while (val != 0);
    
    *p-- = '+';
    *p = i;

    return p;
}

static Character* _Nonnull copy_out(Character* _Nonnull buf, const Character* _Nonnull pCanonDigits)
{
    const Character* p = (pCanonDigits[1] == '+') ? &pCanonDigits[2] : &pCanonDigits[1];

    while (*p != '\0') { *buf++ = *p++; }
    *buf = '\0';
    return buf;
}

// 'pBuffer' must be at least DIGIT_BUFFER_CAPACITY characters long
const Character* _Nonnull Int32_ToString(Int32 val, Int radix, Bool isUppercase, Character* _Nonnull pBuffer)
{
    if (radix != 8 && radix != 10 && radix != 16 || pBuffer == NULL) {
        return NULL;
    }

    char t[DIGIT_BUFFER_CAPACITY];
    return copy_out(pBuffer, __i32toa(val, radix, isUppercase, t));
}

// 'pBuffer' must be at least DIGIT_BUFFER_CAPACITY characters long
const Character* _Nonnull Int64_ToString(Int64 val, Int radix, Bool isUppercase, Character* _Nonnull pBuffer)
{
    if (radix != 8 && radix != 10 && radix != 16 || pBuffer == NULL) {
        return NULL;
    }

    char t[DIGIT_BUFFER_CAPACITY];
    return copy_out(pBuffer, __i64toa(val, radix, isUppercase, t));
}

// 'pBuffer' must be at least DIGIT_BUFFER_CAPACITY characters long
const Character* _Nonnull UInt32_ToString(UInt32 val, Int radix, Bool isUppercase, Character* _Nonnull pBuffer)
{
    if (radix != 8 && radix != 10 && radix != 16 || pBuffer == NULL) {
        return NULL;
    }

    char t[DIGIT_BUFFER_CAPACITY];
    return copy_out(pBuffer, __ui32toa(val, radix, isUppercase, t));
}

// 'pBuffer' must be at least DIGIT_BUFFER_CAPACITY characters long
const Character* _Nonnull UInt64_ToString(UInt64 val, Int radix, Bool isUppercase, Character* _Nonnull pBuffer)
{
    if (radix != 8 && radix != 10 && radix != 16 || pBuffer == NULL) {
        return NULL;
    }

    char t[DIGIT_BUFFER_CAPACITY];
    return copy_out(pBuffer, __ui64toa(val, radix, isUppercase, t));
}

Int Int_NextPowerOf2(Int n)
{
    if (n && !(n & (n - 1))) {
        return n;
    } else {
        Int p = 1;

        while (p < n) {
            p <<= 1;
        }
    
        return p;
    }
}
