//
//  Urt.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYS_URT_H
#define __SYS_URT_H 1

#include <apollo/_cmndef.h>
#include <apollo/_dmdef.h>

__CPP_BEGIN

enum {
    kUrtFunc_asr64 = 0,     // long long _rshint64(long long x, int s)
    kUrtFunc_lsr64,         // long long _rshint64(long long x, int s)
    kUrtFunc_lsl64,         // long long _lshint64(long long x, int s)
    kUrtFunc_divmods64_64,  // int _divmods64(long long dividend, long long divisor, long long* quotient, long long* remainder)
    kUrtFunc_muls64_64,     // long long _mulint64(long long x, long long y)
    kUrtFunc_muls32_64,     // long long _ui32_64_mul(int x, int y)

    kUrtFunc_Count
};

typedef void (*UrtFunc)(void);

__CPP_END

#endif /* __SYS_URT_H */
