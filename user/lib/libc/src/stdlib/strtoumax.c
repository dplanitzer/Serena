//
//  strtoumax.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <__itoa.h>


uintmax_t strtoumax(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    unsigned long long r;
    int err;

    if ((err = __strtou64(str, str_end, base, UINTMAX_MAX, __UINTMAX_MAX_BASE_10_DIGITS, &r)) == 0) {
        return r;
    }
    else {
        errno = err;
        return 0;
    }
}
