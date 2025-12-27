//
//  atoi.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <__itoa.h>


int atoi(const char *str)
{
    long long r;
    int err;

    if ((err = __strtoi64(str, NULL, 10, INT_MIN, INT_MAX, __INT_MAX_BASE_10_DIGITS, &r)) == 0) {
        return (int) r;
    } else {
        errno = err;
        return 0;
    }
}
