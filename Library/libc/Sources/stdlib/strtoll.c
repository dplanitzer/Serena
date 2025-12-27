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
    int err;

    if ((err = __strtoi64(str, str_end, base, LLONG_MIN, LLONG_MAX, __LLONG_MAX_BASE_10_DIGITS, &r)) == 0) {
        return r;
    }
    else {
        errno = err;
        return 0;
    }
}

long long atoll(const char *str)
{
    long long r;
    int err;

    if ((err = __strtoi64(str, NULL, 10, LLONG_MIN, LLONG_MAX, __LLONG_MAX_BASE_10_DIGITS, &r)) == 0) {
        return r;
    }
    else {
        errno = err;
        return 0;
    }
}
