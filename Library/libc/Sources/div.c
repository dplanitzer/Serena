//
//  div.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <inttypes.h>
#include <System/_math.h>

div_t div(int x, int y)
{
    const div_t r = { x / y, x % y };
    return r;
}

ldiv_t ldiv(long x, long y)
{
    const ldiv_t r = { x / y, x % y };
    return r;
}

lldiv_t lldiv(long long x, long long y)
{
    const lldiv_t r = { x / y, x % y };
    return r;
}

imaxdiv_t imaxdiv(intmax_t x, intmax_t y)
{
    const imaxdiv_t r = { x / y, x % y };
    return r;
}
