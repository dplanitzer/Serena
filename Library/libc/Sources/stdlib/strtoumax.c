//
//  strtoumax.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>


uintmax_t strtoumax(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    long long r;
    int err;

    if ((err = __strtoi64(str, str_end, base, 0, UINTMAX_MAX, __UINTMAX_MAX_BASE_10_DIGITS, &r)) == 0) {
        return (uintmax_t) r;
    }
    else {
        errno = err;
        return 0;
    }
}
