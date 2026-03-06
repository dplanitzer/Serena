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

    __strtoi32(str, str_end, base, LONG_MIN, LONG_MAX, &r);
    return r;   // LONG_MAX in case of overflow
}
