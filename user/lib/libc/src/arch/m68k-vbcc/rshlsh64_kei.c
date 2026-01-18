//
//  rshlsh64_kei.c
//  libc
//
//  Created by Dietmar Planitzer on 12/31/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <__crt.h>
#include <__kei.h>


long long _lshint64(long long x, int s)
{
    return ((long long (*)(long long, int))__gKeiTab[KEI_lsl64])(x, s);
}

unsigned long long _lshuint64(unsigned long long x, int s)
{
    return ((unsigned long long (*)(unsigned long long, int))__gKeiTab[KEI_lsl64])(x, s);
}


long long _rshsint64(long long x, int s)
{
    return ((long long (*)(long long, int))__gKeiTab[KEI_asr64])(x, s);
}

unsigned long long _rshuint64(unsigned long long x, int s)
{
    return ((unsigned long long (*)(unsigned long long, int))__gKeiTab[KEI_lsr64])(x, s);
}
