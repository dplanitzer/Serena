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



unsigned long long strtoull(const char *str, char **str_end, int base)
{
    long long r;

    errno = __strtoi64(str, str_end, base, 0, ULLONG_MAX, __LLONG_MAX_BASE_10_DIGITS, &r);
    return (unsigned long long) r;
}
