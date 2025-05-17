//
//  strtoll.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>


long long strtoll(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    long long r;

    errno = __strtoi64(str, str_end, base, LLONG_MIN, LLONG_MAX, __LLONG_MAX_BASE_10_DIGITS, &r);
    return r;
}

long long atoll(const char *str)
{
    long long r;

    if (__strtoi64(str, NULL, 10, LLONG_MIN, LLONG_MAX, __LLONG_MAX_BASE_10_DIGITS, &r) == 0) {
        return r;
    } else {
        return 0;
    }
}
