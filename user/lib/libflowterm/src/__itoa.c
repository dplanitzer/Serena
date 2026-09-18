//
//  __itoa.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"
#include <stdlib.h>
#include <ext/math.h>

char* _Nonnull __ft_itoa(int val, char* _Nonnull buf)
{
    static char digits[_FT_ITOA_BUF_SIZE];
    char* ep = &digits[_FT_ITOA_BUF_SIZE];
    char *p = ep - 1;

    val = __max(__min(val, _FT_ITOA_MAX), _FT_ITOA_MIN);

    do {
        const div_t r = div(val, 10);

        *p-- = '0' + (char)r.rem;
        val = r.quot;
    } while (val);

    p++;
    while (p < ep) {
        *buf++ = *p++;
    }

    return buf;
}
