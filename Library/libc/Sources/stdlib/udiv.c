//
//  udiv.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <_absdiv.h>

#if !defined(__M68K__)
udiv_t udiv(unsigned int x, unsigned int y)
{
    const udiv_t r = { x / y, x % y };
    return r;
}

uldiv_t uldiv(unsigned long x, unsigned long y)
{
    const uldiv_t r = { x / y, x % y };
    return r;
}
#endif
