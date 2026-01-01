//
//  divmod64_kei.c
//  libc
//
//  Created by Dietmar Planitzer on 12/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <__crt.h>
#include <__kei.h>


long long _mulint64_020(long long x, long long y)
{
    return ((long long (*)(long long, long long))__gKeiTab[KEI_muls64_64])(x, y);
}


long long _mulsint64_020(long long x, long long y)
{
    return ((long long (*)(long long, long long))__gKeiTab[KEI_muls64_64])(x, y);
}

long long _mulsint64_060(long long x, long long y)
{
    return ((long long (*)(long long, long long))__gKeiTab[KEI_muls64_64])(x, y);
}


unsigned long long _muluint64_020(unsigned long long x, unsigned long long y)
{
    return ((unsigned long long (*)(unsigned long long, unsigned long long))__gKeiTab[KEI_mulu64_64])(x, y);
}

unsigned long long _muluint64_060(unsigned long long x, unsigned long long y)
{
    return ((unsigned long long (*)(unsigned long long, unsigned long long))__gKeiTab[KEI_mulu64_64])(x, y);
}
