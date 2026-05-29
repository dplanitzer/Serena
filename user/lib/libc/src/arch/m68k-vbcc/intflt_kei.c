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
