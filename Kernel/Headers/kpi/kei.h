//
//  kpi/kei.h
//  kpi
//
//  Created by Dietmar Planitzer on 2/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_KEI_H
#define _KPI_KEI_H 1

// The Kernel Express Interface
enum {
    KEI_asr64 = 0,      // long long _rshint64(long long x, int s)
    KEI_lsr64,          // long long _rshint64(long long x, int s)
    KEI_lsl64,          // long long _lshint64(long long x, int s)

    KEI_divs64,         // int64_t _divs64(int64_t dividend, int64_t divisor)
    KEI_divu64,         // uint64_t _divu64(uint64_t dividend, uint64_t divisor)
    KEI_mods64,         // int64_t _mods64(int64_t dividend, int64_t divisor)
    KEI_modu64,         // uint64_t _modu64(uint64_t dividend, uint64_t divisor)
    KEI_divmods64,      // void _divmods64(const iu64_t _Nonnull dividend_divisor[2], iu64_t* _Nonnull _Restrict quotient, iu64_t* _Nullable _Restrict remainder)
    KEI_divmodu64,      // void _divmodu64(const iu64_t _Nonnull dividend_divisor[2], iu64_t* _Nonnull _Restrict quotient, iu64_t* _Nullable _Restrict remainder)
    
    KEI_muls64_64,      // long long _mulint64(long long x, long long y)
    KEI_mulu64_64,      // long long _muluint64(long long x, long long y)

    KEI_memcpy,         // void* memcpy(void* dst, const void* src, size_t count)
    KEI_memmove,        // void* memmove(void* dst, const void* src, size_t count)
    KEI_memset,         // void* memset(void* dst, int byte, size_t count)
    
    KEI_Count
};

typedef void (*kei_func_t)(void);

#endif /* _KPI_KEI_H */
