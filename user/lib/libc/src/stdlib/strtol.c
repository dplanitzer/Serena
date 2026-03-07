//
//  strtol.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdlib.h>
#include <__itoa.h>


long strtol(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    long r;
    const int err = __strtoi32(str, str_end, base, &r);

    if (err != 0) {
        errno = err;
    }
    return r;
}

long atol(const char *str)
{
    long r;
    const int err = __strtoi32(str, NULL, 10, &r);

    if (err != 0) {
        errno = err;
    }
    return r;
}
