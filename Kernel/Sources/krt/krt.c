//
//  krt.c
//  crt
//
//  Created by Dietmar Planitzer on 2/4/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "krt.h"
#include <klib/Memory.h>

extern long long _rshsint64(long long x, int s);
extern unsigned long long _rshuint64(unsigned long long x, int s);
extern long long _lshint64(long long x, int s);

extern int _divmods64(long long dividend, long long divisor, long long* quotient, long long* remainder);

extern long long _mulint64_020(long long x, long long y);
extern long long _ui32_64_mul(int x, int y);


UrtFunc gUrtFuncTable[kUrtFunc_Count];

void krt_init(void)
{
    gUrtFuncTable[kUrtFunc_asr64] = (UrtFunc)_rshsint64;
    gUrtFuncTable[kUrtFunc_lsr64] = (UrtFunc)_rshuint64;
    gUrtFuncTable[kUrtFunc_lsl64] = (UrtFunc)_lshint64;
    gUrtFuncTable[kUrtFunc_divmods64_64] = (UrtFunc)_divmods64;
    gUrtFuncTable[kUrtFunc_muls64_64] = (UrtFunc)_mulint64_020;
    gUrtFuncTable[kUrtFunc_muls32_64] = (UrtFunc)_ui32_64_mul;

    gUrtFuncTable[kUrtFunc_memcpy] = (UrtFunc)memcpy;
    gUrtFuncTable[kUrtFunc_memmove] = (UrtFunc)memmove;
    gUrtFuncTable[kUrtFunc_memset] = (UrtFunc)memset;
}
