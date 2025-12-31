//
//  kei.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "kei.h"
#include <__crt.h>
#include <string.h>
#include <hal/cpu.h>
#include <hal/sys_desc.h>

extern long long _rshsint64(long long x, int s);
extern unsigned long long _rshuint64(unsigned long long x, int s);
extern long long _lshint64(long long x, int s);

extern long long _mulint64_020(long long x, long long y);
extern long long _ui32_64_mul(int x, int y);


kei_func_t gKeiTable[KEI_Count];

void kei_init(void)
{
    const bool is060 = (g_sys_desc->cpu_model >= CPU_MODEL_68060);

    gKeiTable[KEI_asr64] = (kei_func_t)_rshsint64;
    gKeiTable[KEI_lsr64] = (kei_func_t)_rshuint64;
    gKeiTable[KEI_lsl64] = (kei_func_t)_lshint64;

    gKeiTable[KEI_divs64] = (kei_func_t)(is060 ? _divsint64_060 : _divsint64_020);
    gKeiTable[KEI_divu64] = (kei_func_t)(is060 ? _divuint64_060 : _divuint64_020);
    gKeiTable[KEI_mods64] = (kei_func_t)(is060 ? _modsint64_060 : _modsint64_020);
    gKeiTable[KEI_modu64] = (kei_func_t)(is060 ? _moduint64_060 : _moduint64_020);
    gKeiTable[KEI_divmods64] = (kei_func_t)_divs64;
    gKeiTable[KEI_divmodu64] = (kei_func_t)_divu64;

    gKeiTable[KEI_muls64_64] = (kei_func_t)_mulint64_020;
    gKeiTable[KEI_muls32_64] = (kei_func_t)_ui32_64_mul;

    gKeiTable[KEI_memcpy] = (kei_func_t)memcpy;
    gKeiTable[KEI_memmove] = (kei_func_t)memmove;
    gKeiTable[KEI_memset] = (kei_func_t)memset;
}
