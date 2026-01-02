//
//  strtoul.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <__itoa.h>


unsigned long strtoul(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    unsigned long r;
    int err;

    if ((err = __strtou32(str, str_end, base, ULONG_MAX, __LONG_MAX_BASE_10_DIGITS, &r)) == 0) {
        return r;
    }
    else {
        errno = err;
        return 0;
    }
}
