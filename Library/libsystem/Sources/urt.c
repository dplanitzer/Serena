//
//  Urt.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/4/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Urt.h>
#include <System/Types.h>
#include <System/Process.h>


static UrtFunc* __gUrtFuncTable;


void __UrtInit(ProcessArguments* _Nonnull argsp)
{
    __gUrtFuncTable = argsp->urt_funcs;
}


void *__Memset(void *dst, int c, size_t count)
{
    return ((void* (*)(void*, int, size_t))__gUrtFuncTable[kUrtFunc_memset])(dst, c, count);
}

void *__Memcpy(void * _Restrict dst, const void * _Restrict src, size_t count)
{
    return ((void* (*)(void* _Restrict, const void* _Restrict, size_t))__gUrtFuncTable[kUrtFunc_memcpy])(dst, src, count);
}

void *__Memmove(void * _Restrict dst, const void * _Restrict src, size_t count)
{
    return ((void* (*)(void* _Restrict, const void* _Restrict, size_t))__gUrtFuncTable[kUrtFunc_memmove])(dst, src, count);
}


long long _rshsint64(long long x, int s)
{
    return ((long long (*)(long long, int))__gUrtFuncTable[kUrtFunc_asr64])(x, s);
}

unsigned long long _rshuint64(unsigned long long x, int s)
{
    return ((unsigned long long (*)(unsigned long long, int))__gUrtFuncTable[kUrtFunc_lsr64])(x, s);
}

long long _lshint64(long long x, int s)
{
    return ((long long (*)(long long, int))__gUrtFuncTable[kUrtFunc_lsl64])(x, s);
}

long long __lshuint64(long long x, int s)
{
    return ((unsigned long long (*)(unsigned long long, int))__gUrtFuncTable[kUrtFunc_lsl64])(x, s);
}


long long _mulint64_020(long long x, long long y)
{
    return ((long long (*)(long long, long long))__gUrtFuncTable[kUrtFunc_muls64_64])(x, y);
}

long long _mulint64_060(long long x, long long y)
{
    return ((long long (*)(long long, long long))__gUrtFuncTable[kUrtFunc_muls64_64])(x, y);
}


int _divmods64(long long dividend, long long divisor, long long* quotient, long long* remainder)
{
    return (errno_t)((int (*)(long long, long long, long long*, long long*))__gUrtFuncTable[kUrtFunc_divmods64_64])(dividend, divisor, quotient, remainder);
}

long long _divsint64_020(long long dividend, long long divisor)
{
    long long quo;
    
    ((int (*)(long long, long long, long long*, long long*))__gUrtFuncTable[kUrtFunc_divmods64_64])(dividend, divisor, &quo, NULL);
    
    return quo;
}

long long _divsint64_060(long long dividend, long long divisor)
{
    long long quo;
    
    ((int (*)(long long, long long, long long*, long long*))__gUrtFuncTable[kUrtFunc_divmods64_64])(dividend, divisor, &quo, NULL);
    
    return quo;
}

long long _modsint64_020(long long dividend, long long divisor)
{
    long long quo, rem;
    
    ((int (*)(long long, long long, long long*, long long*))__gUrtFuncTable[kUrtFunc_divmods64_64])(dividend, divisor, &quo, &rem);
    
    return rem;
}

long long _modsint64_060(long long dividend, long long divisor)
{
    long long quo, rem;
    
    ((int (*)(long long, long long, long long*, long long*))__gUrtFuncTable[kUrtFunc_divmods64_64])(dividend, divisor, &quo, &rem);
    
    return rem;
}

unsigned long long _divuint64_20(unsigned long long dividend, unsigned long long divisor)
{
    long long quo;
    
    ((int (*)(long long, long long, long long*, long long*))__gUrtFuncTable[kUrtFunc_divmods64_64])(dividend, divisor, &quo, NULL);
    
    return (unsigned long long) quo;
}

unsigned long long _moduint64_20(unsigned long long dividend, unsigned long long divisor)
{
    long long quo, rem;
    
    ((int (*)(long long, long long, long long*, long long*))__gUrtFuncTable[kUrtFunc_divmods64_64])(dividend, divisor, &quo, &rem);
    
    return (unsigned long long) rem;
}
