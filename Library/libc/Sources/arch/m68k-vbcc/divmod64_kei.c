//
//  divmod64_kei.c
//  libc
//
//  Created by Dietmar Planitzer on 12/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <__crt.h>
#include <__kei.h>


long long _divsint64_020(long long dividend, long long divisor)
{
    return ((long long (*)(long long, long long))__gKeiTab[KEI_divs64])(dividend, divisor);
}

long long _divsint64_060(long long dividend, long long divisor)
{
    return ((long long (*)(long long, long long))__gKeiTab[KEI_divs64])(dividend, divisor);
}


unsigned long long _divuint64_020(unsigned long long dividend, unsigned long long divisor)
{
    return ((unsigned long long (*)(unsigned long long, unsigned long long))__gKeiTab[KEI_divu64])(dividend, divisor);
}

unsigned long long _divuint64_060(unsigned long long dividend, unsigned long long divisor)
{
    return ((unsigned long long (*)(unsigned long long, unsigned long long))__gKeiTab[KEI_divu64])(dividend, divisor);
}


long long _modsint64_020(long long dividend, long long divisor)
{
    return ((long long (*)(long long, long long))__gKeiTab[KEI_mods64])(dividend, divisor);
}

long long _modsint64_060(long long dividend, long long divisor)
{
    return ((long long (*)(long long, long long))__gKeiTab[KEI_mods64])(dividend, divisor);
}


unsigned long long _moduint64_020(unsigned long long dividend, unsigned long long divisor)
{
    return ((unsigned long long (*)(unsigned long long, unsigned long long))__gKeiTab[KEI_modu64])(dividend, divisor);
}

unsigned long long _moduint64_060(unsigned long long dividend, unsigned long long divisor)
{
    return ((unsigned long long (*)(unsigned long long, unsigned long long))__gKeiTab[KEI_modu64])(dividend, divisor);
}


void _divs64(const iu64_t _Nonnull dividend_divisor[2], iu64_t* _Nonnull _Restrict quotient, iu64_t* _Nullable _Restrict remainder)
{
    ((void (*)(const iu64_t[2], iu64_t* _Restrict, iu64_t* _Restrict))__gKeiTab[KEI_divmods64])(dividend_divisor, quotient, remainder);
}

void _divu64(const iu64_t _Nonnull dividend_divisor[2], iu64_t* _Nonnull _Restrict quotient, iu64_t* _Nullable _Restrict remainder)
{
    ((void (*)(const iu64_t[2], iu64_t* _Restrict, iu64_t* _Restrict))__gKeiTab[KEI_divmodu64])(dividend_divisor, quotient, remainder);
}
