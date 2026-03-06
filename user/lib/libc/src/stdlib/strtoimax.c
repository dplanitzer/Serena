//
//  strtoimax.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <__itoa.h>


intmax_t strtoimax(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    long long r;
    const int err = __strtoi64(str, str_end, base, INTMAX_MIN, INTMAX_MAX, &r);

    if (err != 0) {
        errno = err;
    }

    return (intmax_t) r;
}
