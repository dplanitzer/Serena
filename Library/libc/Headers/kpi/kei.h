//
//  kpi/kei.h
//  libc
//
//  Created by Dietmar Planitzer on 2/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_KEI_H
#define _KPI_KEI_H 1

// The Kernel Express Interface
enum {
    KEI_asr64 = 0,     // long long _rshint64(long long x, int s)
    KEI_lsr64,         // long long _rshint64(long long x, int s)
    KEI_lsl64,         // long long _lshint64(long long x, int s)
    KEI_divmods64_64,  // int _divmods64(long long dividend, long long divisor, long long* quotient, long long* remainder)
    KEI_muls64_64,     // long long _mulint64(long long x, long long y)
    KEI_muls32_64,     // long long _ui32_64_mul(int x, int y)

    KEI_memcpy,        // void* memcpy(void* dst, const void* src, size_t count)
    KEI_memmove,       // void* memmove(void* dst, const void* src, size_t count)
    KEI_memset,        // void* memset(void* dst, int byte, size_t count)
    
    KEI_Count
};

typedef void (*kei_func_t)(void);

#endif /* _KPI_KEI_H */
