//
//  strtoumax.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>


uintmax_t strtoumax(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    long long r;

    if (__strtoi64(str, str_end, base, 0, UINTMAX_MAX, __UINTMAX_MAX_BASE_10_DIGITS, &r) == 0) {
        return (uintmax_t) r;
    }
    else {
        return 0;
    }
}
