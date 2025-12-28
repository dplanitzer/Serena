//
//  __strtoi64.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__itoa.h>
#include <limits.h>
#include <kpi/_errno.h>


int __strtoi64(const char * _Nonnull _Restrict str, char * _Nonnull _Restrict * _Nonnull _Restrict str_end, int base, long long min_val, long long max_val, int max_digits, long long * _Nonnull _Restrict result)
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
    unsigned long long val = 0;
    const unsigned long long llbase = (unsigned long long) base;
    const unsigned long long upper_bound = (is_neg) ? -min_val : max_val;
    const char upper_num = (base < 10) ? '0' + base : '9';
    const char upper_lletter = (base > 9) ? 'a' + base - 11 : 'a';
    const char upper_uletter = (base > 9) ? 'A' + base - 11 : 'A';
    int i = 0;

    for (;;) {
        const char ch = str[i];
        unsigned long long digit;

        if (ch >= '0' && ch <= upper_num) {
            digit = ch - '0';
        } else if (base > 9 && ((ch >= 'a' && ch <= upper_lletter) || (ch >= 'A' && ch <= upper_uletter))) {
            digit = (ch >= 'a') ? ch - 'a' : ch - 'A';
        } else {
            break;
        }

        const unsigned long long new_val = (val * llbase) + digit;
        if (new_val < val || new_val > upper_bound || i > max_digits) {
            if (str_end) *str_end = (char*)&str[i + 1];
            *result = (is_neg) ? min_val : max_val;
            return ERANGE;
        }

        val = new_val;
        i++;
    }

    if (str_end) *str_end = (char*)&str[i];
    *result = (is_neg) ? -((long long)val) : (long long)val;
    return 0;
}
