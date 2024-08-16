//
//  atoi.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>


int atoi(const char *str)
{
    long long r;

    if (__strtoi64(str, NULL, 10, INT_MIN, INT_MAX, __INT_MAX_BASE_10_DIGITS, &r) == 0) {
        return (int) r;
    } else {
        return 0;
    }
}
