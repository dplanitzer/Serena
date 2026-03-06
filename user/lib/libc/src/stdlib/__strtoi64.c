//
//  __strtoi64.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__itoa.h>
#include <ctype.h>
#include <kpi/_errno.h>


int __strtoi64(const char * _Nonnull _Restrict str, char * _Nonnull _Restrict * _Nonnull _Restrict str_end, int base, long long min_val, long long max_val, long long * _Nonnull _Restrict result)
{
    if ((base < 2 && base != 0) || base > 36) {
        *result = 0ll;
        return EINVAL;
    }


    // Skip whitespace
    while (isspace(*str)) {
        str++;
    }


    // Detect the sign
    const char sig_ch = *str;
    if (sig_ch == '-' || sig_ch == '+') {
        str++;
    }


    // Handle optional octal/hex prefix and base == 0
    if ((base == 0 || base == 16) && (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))) {
        str += 2;
        base = 16;
    }
    else if ((base == 0 || base == 8) && *str == '0') {
        str++;
        base = 8;
    }
    else if (base == 0) {
        base = 10;
    }


    // Convert digits
    const bool is_neg = (sig_ch == '-');
    unsigned long long val = 0;
    const unsigned long ulbase = (unsigned long long) base;
    const unsigned long long upper_bound = (is_neg) ? -min_val : max_val;
    int i = 0;

    for (;;) {
        const char ch = tolower(str[i]);

        if (ch < '0' || ch > __g_digits_36_lc[base - 1]) {
            break;
        }

        const unsigned long long digit = (ch <= '9') ? ch - '0' : ch - 'a' + 10;
        const unsigned long long new_val = (val * ulbase) + digit;
        if (new_val < val || new_val > upper_bound) {
            if (str_end) *str_end = (char*)&str[i + 1];
            *result = (is_neg) ? min_val : max_val;
            return ERANGE;
        }

        val = new_val;
        i++;
    }

    if (str_end) {
        *str_end = (char*)&str[i];
    }

    *result = (is_neg) ? -((long long)val) : (long long)val;
    return 0;
}
