//
//  __strtou32.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 1/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <__itoa.h>
#include <ctype.h>
#include <limits.h>
#include <kpi/_errno.h>


int __strtou32(const char * _Nonnull _Restrict str, char * _Nonnull _Restrict * _Nonnull _Restrict str_end, int base, unsigned long * _Nonnull _Restrict result)
{
    const char* orig_str = str;
    int r = 0;

    if ((base < 2 && base != 0) || base > 36) {
        *result = 0ul;
        return EINVAL;
    }


    // Skip whitespace
    while (isspace(*str)) {
        str++;
    }


    // Sign
    const char sig_ch = *str;
    if (sig_ch == '-' || sig_ch == '+') {
        str++;
    }


    // Handle optional octal/hex prefix and base == 0
    if ((base == 0 || base == 16) && (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))) {
        str += 2;
        base = 16;
    }
    else if ((base == 0 || base == 2) && (str[0] == '0' && (str[1] == 'b' || str[1] == 'B'))) {
        str += 2;
        base = 2;
    }
    else if ((base == 0 || base == 8) && *str == '0') {
        str++;
        base = 8;
    }
    else if (base == 0) {
        base = 10;
    }


    // Convert digits
    unsigned long num = 0;
    const unsigned long b = (unsigned long) base;
    const unsigned long ovf_num = ULONG_MAX / b;
    const unsigned long ovf_d = ULONG_MAX % b;
    bool overflowed = false, has_some_digits = false;

    for (;;) {
        const char ch = *str++;
        unsigned long d;

        if (isdigit(ch)) {
            d = ch - '0';
        }
        else if (isupper(ch)) {
            d = ch - 'A' + 10;
        }
        else if (islower(ch)) {
            d = ch - 'a' + 10;
        }
        else {
            break;
        }

        if (d >= b) {
            break;
        }

        if (num > ovf_num || (num == ovf_num && d > ovf_d)) {
            overflowed = true;
        }
        else if (!overflowed) {
            num = num * b + d;
            has_some_digits = true;
        }
    }

    if (str_end) {
        *str_end = (has_some_digits) ? (char*)(str - 1) : orig_str;
    }

    if (overflowed) {
        *result = ULONG_MAX;
        r = ERANGE;
    }
    else {
        *result = (sig_ch == '-') ? -num : num;
    }

    return r;
}
