//
//  __crt.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ___CRT_H
#define ___CRT_H 1

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


// Compiler specific
extern long long _divsint64_020(long long dividend, long long divisor);
extern long long _divsint64_060(long long dividend, long long divisor);

extern unsigned long long _divuint64_020(unsigned long long dividend, unsigned long long divisor);
extern unsigned long long _divuint64_060(unsigned long long dividend, unsigned long long divisor);

extern long long _modsint64_020(long long dividend, long long divisor);
extern long long _modsint64_060(long long dividend, long long divisor);

extern unsigned long long _moduint64_020(unsigned long long dividend, unsigned long long divisor);
extern unsigned long long _moduint64_060(unsigned long long dividend, unsigned long long divisor);


extern long long _mulint64_020(long long x, long long y);


extern long long _lshint64(long long x, int s);
extern long long _rshsint64(long long x, int s);

extern unsigned long long _lshuint64(unsigned long long x, int s);
extern unsigned long long _rshuint64(unsigned long long x, int s);

__CPP_END

#endif /* ___CRT_H */
