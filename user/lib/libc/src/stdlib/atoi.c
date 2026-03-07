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
#if INT_WIDTH == LONG_WIDTH
    long r;
    const int err = __strtoi32(str, NULL, 10, &r);
#else
#error "atoi(): int bit width not supported"
#endif

    if (err != 0) {
        errno = err;
    }
    return (int) r;
}
