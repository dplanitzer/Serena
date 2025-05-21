//
//  strtol.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>


long strtol(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    long long r;

    if (__strtoi64(str, str_end, base, LONG_MIN, LONG_MAX, __LONG_MAX_BASE_10_DIGITS, &r) == 0) {
        return (long) r;
    }
    else {
        return 0;
    }
}

long atol(const char *str)
{
    long long r;

    if (__strtoi64(str, NULL, 10, LONG_MIN, LONG_MAX, __LONG_MAX_BASE_10_DIGITS, &r) == 0) {
        return (long) r;
    } else {
        return 0;
    }
}
