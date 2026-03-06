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
    const int err = __strtou64(str, str_end, base, UINTMAX_MAX, &r);

    if (err != 0) {
        errno = err;
    }

    return r;
}
