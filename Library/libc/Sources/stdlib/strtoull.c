//
//  strtoull.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <__itoa.h>


unsigned long long strtoull(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    long long r;
    int err;

    if ((err = __strtoi64(str, str_end, base, 0, ULLONG_MAX, __LLONG_MAX_BASE_10_DIGITS, &r)) == 0) {
        return (unsigned long long) r;
    }
    else {
        errno = err;
        return 0;
    }
}
