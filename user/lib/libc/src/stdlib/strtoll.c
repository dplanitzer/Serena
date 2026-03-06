//
//  strtoll.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <__itoa.h>


long long strtoll(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    long long r;
    const int err = __strtoi64(str, str_end, base, LLONG_MIN, LLONG_MAX, &r);

    if (err != 0) {
        errno = err;
    }

    return r;
}

long long atoll(const char *str)
{
    long long r;
    const int err = __strtoi64(str, NULL, 10, LLONG_MIN, LLONG_MAX, &r);

    if (err != 0) {
        errno = err;
    }

    return r;
}
