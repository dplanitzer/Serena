//
//  strtoull.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>



unsigned long long strtoull(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    long long r;

    if (__strtoi64(str, str_end, base, 0, ULLONG_MAX, __LLONG_MAX_BASE_10_DIGITS, &r) == 0) {
        return (unsigned long long) r;
    }
    else {
        return 0;
    }
}
