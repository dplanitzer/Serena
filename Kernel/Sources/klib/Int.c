//
//  Int.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <kern/try.h>
#include <kern/types.h>
#include <kern/kernlib.h>

extern int _divmods64(long long dividend, long long divisor, long long* quotient, long long* remainder);


static errno_t __atoi64(const char * _Nonnull str, char **str_end, int base, int64_t min_val, int64_t max_val, int max_digits, int64_t * _Nonnull result)
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
    const char sig_ch = *str;
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
    const bool is_neg = (sig_ch == '-');
    uint64_t val = 0;
    const uint64_t llbase = (uint64_t) base;
    const uint64_t upper_bound = (is_neg) ? -min_val : max_val;
    const char upper_num = (base < 10) ? '0' + base : '9';
    const char upper_lletter = (base > 9) ? 'a' + base - 11 : 'a';
    const char upper_uletter = (base > 9) ? 'A' + base - 11 : 'A';
    int i = 0;

    for (;;) {
        const char ch = str[i];
        uint64_t digit;

        if (ch >= '0' && ch <= upper_num) {
            digit = ch - '0';
        } else if (base > 9 && ((ch >= 'a' && ch <= upper_lletter) || (ch >= 'A' && ch <= upper_uletter))) {
            digit = (ch >= 'a') ? ch - 'a' : ch - 'A';
        } else {
            break;
        }

        const uint64_t new_val = (val * llbase) + digit;
        if (new_val < val || new_val > upper_bound || i > max_digits) {
            if (str_end) *str_end = (char*)&str[i + 1];
            *result = (is_neg) ? min_val : max_val;
            return ERANGE;
        }

        val = new_val;
        i++;
    }

    if (str_end) *str_end = (char*)&str[i];
    *result = (is_neg) ? -((int64_t)val) : (int64_t)val;
    return 0;
}

int _atoi(const char *str, char **str_end, int base)
{
    int64_t r;

    __atoi64(str, str_end, base, INT_MIN, INT_MAX, __LONG_MAX_BASE_10_DIGITS, &r);
    return (int) r;
}

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
        *p-- = '0' + (char)(val % 10);
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
        _divmods64(val, 10, &q, &r);
        *p-- = '0' + (char)r;
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

// 'pBuffer' must be at least DIGIT_BUFFER_CAPACITY characters long
const char* _Nonnull Int32_ToString(int32_t val, int radix, bool isUppercase, char* _Nonnull pBuffer)
{
    char t[DIGIT_BUFFER_CAPACITY];
    char* p;

    if (pBuffer) {
        switch (radix) {
            case 10:
                p = __i32toa((int32_t)val, t);
                break;
        
            case 8:
            case 16:
                p = __ui32toa((uint32_t)val, radix, isUppercase, t);
                break;

            default:
                return NULL;
        }
        return copy_out(pBuffer, p);
    }
    else {
        return NULL;
    }
}

// 'pBuffer' must be at least DIGIT_BUFFER_CAPACITY characters long
const char* _Nonnull Int64_ToString(int64_t val, int radix, bool isUppercase, char* _Nonnull pBuffer)
{
    char t[DIGIT_BUFFER_CAPACITY];
    char* p;

    if (pBuffer) {
        switch (radix) {
            case 10:
                p = __i64toa((int64_t)val, t);
                break;
        
            case 8:
            case 16:
                p = __ui64toa((uint64_t)val, radix, false, t);
                break;

            default:
                return NULL;
        }
        return copy_out(pBuffer, p);
    }
    else {
        return pBuffer;
    }
}

// 'pBuffer' must be at least DIGIT_BUFFER_CAPACITY characters long
const char* _Nonnull UInt32_ToString(uint32_t val, int radix, bool isUppercase, char* _Nonnull pBuffer)
{
    if (radix != 8 && radix != 10 && radix != 16 || pBuffer == NULL) {
        return NULL;
    }

    char t[DIGIT_BUFFER_CAPACITY];
    return copy_out(pBuffer, __ui32toa(val, radix, isUppercase, t));
}

// 'pBuffer' must be at least DIGIT_BUFFER_CAPACITY characters long
const char* _Nonnull UInt64_ToString(uint64_t val, int radix, bool isUppercase, char* _Nonnull pBuffer)
{
    if (radix != 8 && radix != 10 && radix != 16 || pBuffer == NULL) {
        return NULL;
    }

    char t[DIGIT_BUFFER_CAPACITY];
    return copy_out(pBuffer, __ui64toa(val, radix, isUppercase, t));
}

bool ul_ispow2(unsigned long n)
{
    return (n && (n & (n - 1ul)) == 0ul) ? true : false;
}

bool ull_ispow2(unsigned long long n)
{
    return (n && (n & (n - 1ull)) == 0ull) ? true : false;
}

unsigned long ul_pow2_ceil(unsigned long n)
{
    if (n && !(n & (n - 1))) {
        return n;
    } else {
        unsigned long p = 1;
        
        while (p < n) {
            p <<= 1;
        }
        
        return p;
    }
}

unsigned long long ull_pow2_ceil(unsigned long long n)
{
    if (n && !(n & (n - 1))) {
        return n;
    } else {
        unsigned long long p = 1;
        
        while (p < n) {
            p <<= 1;
        }
        
        return p;
    }
}

unsigned int ul_log2(unsigned long n)
{
    unsigned long p = 1ul;
    unsigned int b = 0;

    while (p < n) {
        p <<= 1ul;
        b++;
    }
        
    return b;
}

unsigned int ull_log2(unsigned long long n)
{
    unsigned long long p = 1ull;
    unsigned int b = 0;
        
    while (p < n) {
        p <<= 1ull;
        b++;
    }
        
    return b;
}
