//
//  strtoul.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>


unsigned long strtoul(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    long long r;

    errno = __strtoi64(str, str_end, base, 0, ULONG_MAX, __LONG_MAX_BASE_10_DIGITS, &r);
    return (unsigned long) r;
}
