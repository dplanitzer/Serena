//
//  div.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <_absdiv.h>

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
