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
#define KEI_asr64       0   // long long _rshint64(long long x, int s)
#define KEI_lsr64       1   // long long _rshint64(long long x, int s)
#define KEI_lsl64       2   // long long _lshint64(long long x, int s)

#define KEI_divs64      3   // int64_t _divs64(int64_t dividend, int64_t divisor)
#define KEI_divu64      4   // uint64_t _divu64(uint64_t dividend, uint64_t divisor)
#define KEI_mods64      5   // int64_t _mods64(int64_t dividend, int64_t divisor)
#define KEI_modu64      6   // uint64_t _modu64(uint64_t dividend, uint64_t divisor)
#define KEI_divmods64   7   // void _divmods64(const iu64_t _Nonnull dividend_divisor[2], iu64_t* _Nonnull _Restrict quotient, iu64_t* _Nullable _Restrict remainder)
#define KEI_divmodu64   8   // void _divmodu64(const iu64_t _Nonnull dividend_divisor[2], iu64_t* _Nonnull _Restrict quotient, iu64_t* _Nullable _Restrict remainder)
    
#define KEI_muls64_64   9   // long long _mulint64(long long x, long long y)
#define KEI_mulu64_64   10  // long long _muluint64(long long x, long long y)

#define KEI_memcpy      11  // void* memcpy(void* dst, const void* src, size_t count)
#define KEI_memmove     12  // void* memmove(void* dst, const void* src, size_t count)
#define KEI_memset      13  // void* memset(void* dst, int byte, size_t count)
    
#define KEI_Count       14


typedef void (*kei_func_t)(void);

#endif /* _KPI_KEI_H */
