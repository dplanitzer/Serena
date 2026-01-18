//
//  ulldiv.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 1/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <__crt.h>
#include <_absdiv.h>
#include <_imaxabsdiv.h>

ulldiv_t ulldiv(unsigned long long x, unsigned long long y)
{
    iu64_t xy[2], q, r;
    ulldiv_t z;

    xy[0].u64 = x;
    xy[1].u64 = y;

    _divmodu64(xy, &q, &r);
    
    z.quot = q.u64;
    z.rem = r.u64;

    return z;
}

uimaxdiv_t uimaxdiv(uintmax_t x, uintmax_t y)
{
    iu64_t xy[2], q, r;
    uimaxdiv_t z;

    xy[0].u64 = x;
    xy[1].u64 = y;

    _divmodu64(xy, &q, &r);
    
    z.quot = q.u64;
    z.rem = r.u64;
    
    return z;
}
