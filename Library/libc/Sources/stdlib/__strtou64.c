//
//  __strtou64.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 1/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <__itoa.h>
#include <kpi/_errno.h>


int __strtou64(const char * _Nonnull _Restrict str, char * _Nonnull _Restrict * _Nonnull _Restrict str_end, int base, unsigned long long max_val, int max_digits, unsigned long long * _Nonnull _Restrict result)
{
    if ((base < 2 && base != 0) || base > 36) {
        *result = 0ll;
        return EINVAL;
    }


    // Skip whitespace
    while (*str != '\0' && (*str == ' ' || *str == '\t')) {
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
    unsigned long long val = 0;
    const unsigned long long llbase = (unsigned long long) base;
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
        if (i > max_digits || new_val < val || new_val > max_val) {
            if (str_end) *str_end = (char*)&str[i + 1];
            *result = max_val;
            return ERANGE;
        }

        val = new_val;
        i++;
    }

    if (str_end) *str_end = (char*)&str[i];
    return 0;
}
