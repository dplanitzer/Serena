//
//  intflt_kei.c
//  libc
//
//  Created by Dietmar Planitzer on 5/29/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <__crt.h>
#include <__kei.h>


double _uint64toflt64(unsigned long long x)
{
    return ((double (*)(unsigned long long))__gKeiTab[KEI_uint64toflt64])(x);
}

double _sint64toflt64(long long x)
{
    return ((double (*)(long long))__gKeiTab[KEI_sint64toflt64])(x);
}


unsigned long long _flt64touint64(double x)
{
    return ((unsigned long long (*)(double))__gKeiTab[KEI_flt64touint64])(x);
}

long long _flt64tosint64(double x)
{
    return ((long long (*)(double))__gKeiTab[KEI_flt64tosint64])(x);
}
