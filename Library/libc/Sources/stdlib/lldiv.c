//
//  lldiv.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <_absdiv.h>
#include <_imaxabsdiv.h>
#include <__divmod64.h>

lldiv_t lldiv(long long x, long long y)
{
    iu64_t xy[2], q, r;
    lldiv_t z;

    xy[0].s64 = x;
    xy[1].s64 = y;

    _divs64(xy, &q, &r);
    
    z.quot = q.s64;
    z.rem = r.s64;

    return z;
}

imaxdiv_t imaxdiv(intmax_t x, intmax_t y)
{
    iu64_t xy[2], q, r;
    imaxdiv_t z;

    xy[0].s64 = x;
    xy[1].s64 = y;

    _divs64(xy, &q, &r);
    
    z.quot = q.s64;
    z.rem = r.s64;
    
    return z;
}
