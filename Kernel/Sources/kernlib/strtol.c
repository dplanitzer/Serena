//
//  strtol.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <__itoa.h>
#include <kern/kernlib.h>


long strtol(const char * _Restrict str, char ** _Restrict str_end, int base)
{
    long r;

    if (__strtoi32(str, str_end, base, LONG_MIN, LONG_MAX, __LONG_MAX_BASE_10_DIGITS, &r) == 0) {
        return r;
    }
    else {
        return 0;
    }
}
