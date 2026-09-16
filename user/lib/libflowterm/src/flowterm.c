//
//  flowterm.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"
#include <stdlib.h>
#include <ext/math.h>

static FILE* _Nonnull   __ft_termin_fp;
int                     __ft_termin_fd;
FILE* _Nonnull          __ft_termout_fp;


FILE* _Nonnull ft_termin(FILE* _Nonnull stream)
{
    FILE* old_fp = __ft_termin_fp;
    const int fd = fileno(stream);

    if (fd >= 0) {
        __ft_termin_fd = fd;
        __ft_termin_fp = stream;
        setvbuf(__ft_termin_fp, NULL, _IONBF, 0);
    }

    return old_fp;
}

FILE* _Nonnull ft_termout(FILE* _Nonnull stream)
{
    FILE* old_fp = __ft_termout_fp;

    __ft_termout_fp = stream;
    return old_fp;
}

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
