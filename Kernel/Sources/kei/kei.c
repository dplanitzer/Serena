//
//  kei.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "kei.h"
#include <kern/string.h>

extern long long _rshsint64(long long x, int s);
extern unsigned long long _rshuint64(unsigned long long x, int s);
extern long long _lshint64(long long x, int s);

extern int _divmods64(long long dividend, long long divisor, long long* quotient, long long* remainder);

extern long long _mulint64_020(long long x, long long y);
extern long long _ui32_64_mul(int x, int y);


kei_func_t gKeiTable[KEI_Count];

void kei_init(void)
{
    gKeiTable[KEI_asr64] = (kei_func_t)_rshsint64;
    gKeiTable[KEI_lsr64] = (kei_func_t)_rshuint64;
    gKeiTable[KEI_lsl64] = (kei_func_t)_lshint64;
    gKeiTable[KEI_divmods64_64] = (kei_func_t)_divmods64;
    gKeiTable[KEI_muls64_64] = (kei_func_t)_mulint64_020;
    gKeiTable[KEI_muls32_64] = (kei_func_t)_ui32_64_mul;

    gKeiTable[KEI_memcpy] = (kei_func_t)memcpy;
    gKeiTable[KEI_memmove] = (kei_func_t)memmove;
    gKeiTable[KEI_memset] = (kei_func_t)memset;
}
