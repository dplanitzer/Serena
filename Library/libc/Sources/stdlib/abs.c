//
//  abs.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <inttypes.h>
#include <ext/math.h>

int abs(int n)
{
    return __abs(n);
}

long labs(long n)
{
    return __abs(n);
}

long long llabs(long long n)
{
    return __abs(n);
}

intmax_t imaxabs(intmax_t n)
{
    return __abs(n);
}
