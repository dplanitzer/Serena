//
//  atox.c
//  libcs
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>


uintmax_t strtoumax(const char *str, char **str_end, int base)
{
    long long r;

    errno = __strtoi64(str, str_end, base, 0, UINTMAX_MAX, __UINTMAX_MAX_BASE_10_DIGITS, &r);
    return (uintmax_t) r;
}
