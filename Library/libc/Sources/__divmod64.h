//
//  __divmod64.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ___DIVMOD64_H
#define ___DIVMOD64_H 1

#include <_cmndef.h>
#include <stdint.h>

__CPP_BEGIN

typedef union iu64 {
    int64_t         s64;
    uint64_t        u64;
    unsigned short  u16[4];
} iu64_t;


extern void _divu64(const iu64_t _Nonnull dividend_divisor[2], iu64_t* _Nonnull _Restrict quotient, iu64_t* _Nullable _Restrict remainder);
extern void _divs64(const iu64_t _Nonnull dividend_divisor[2], iu64_t* _Nonnull _Restrict quotient, iu64_t* _Nullable _Restrict remainder);

__CPP_END

#endif /* ___DIVMOD64_H */
