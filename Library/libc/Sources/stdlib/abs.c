//
//  abs.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <_absdiv.h>
#include <_imaxabsdiv.h>
#include <ext/math.h>

#if !defined(__M68K__)
int abs(int n)
{
    return __abs(n);
}

long labs(long n)
{
    return __abs(n);
}
#endif

long long llabs(long long n)
{
    return __abs(n);
}

intmax_t imaxabs(intmax_t n)
{
    return __abs(n);
}
