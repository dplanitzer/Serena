//
//  strtoimax.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>


intmax_t strtoimax(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    long long r;

    if (__strtoi64(str, str_end, base, INTMAX_MIN, INTMAX_MAX, __INTMAX_MAX_BASE_10_DIGITS, &r) == 0) {
        return (intmax_t) r;
    }
    else {
        return 0;
    }
}
