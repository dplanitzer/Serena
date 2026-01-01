//
//  int64_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 12/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <stdlib.h>
#include "Asserts.h"

extern long long _divsint64_020(long long dividend, long long divisor);
extern long long _modsint64_020(long long dividend, long long divisor);

extern long long _lshint64(long long x, int s);
extern long long _rshsint64(long long x, int s);


void int64_test(int argc, char *argv[])
{
    // llabs()
    assertEquals(0, llabs(0));
    assertEquals(LLONG_MAX, llabs(LLONG_MAX));
    assertEquals(LLONG_MAX, llabs(-LLONG_MAX));


    // lldiv()
    // 32/32
    assertEquals(15ll, _divsint64_020(150ll, 10ll));
    assertEquals(-15ll, _divsint64_020(-150ll, 10ll));
    assertEquals(-15ll, _divsint64_020(150ll, -10ll));
    assertEquals(15ll, _divsint64_020(-150ll, -10ll));

    // 64/32
    assertEquals(231412ll, _divsint64_020(78193085935ll, 337895ll));
    assertEquals(-231412ll, _divsint64_020(-78193085935ll, 337895ll));
    assertEquals(-231412ll, _divsint64_020(78193085935ll, -337895ll));
    assertEquals(231412ll, _divsint64_020(-78193085935ll, -337895ll));

    // 64/64
    assertEquals(16ll, _divsint64_020(78193085935ll, 4886718345ll));
    assertEquals(-16ll, _divsint64_020(-78193085935ll, 4886718345ll));
    assertEquals(-16ll, _divsint64_020(78193085935ll, -4886718345ll));
    assertEquals(16ll, _divsint64_020(-78193085935ll, -4886718345ll));


    // llmod()
    // 32/32
    assertEquals(0ll, _modsint64_020(150ll, 10ll));
    assertEquals(0ll, _modsint64_020(-150ll, 10ll));
    assertEquals(0ll, _modsint64_020(150ll, -10ll));
    assertEquals(0ll, _modsint64_020(-150ll, -10ll));

    // 64/32
    assertEquals(128195ll, _modsint64_020(78193085935ll, 337895ll));
    assertEquals(-128195ll, _modsint64_020(-78193085935ll, 337895ll));
    assertEquals(128195ll, _modsint64_020(78193085935ll, -337895ll));
    assertEquals(-128195ll, _modsint64_020(-78193085935ll, -337895ll));

    // 64/64
    assertEquals(5592415ll, _modsint64_020(78193085935ll, 4886718345ll));
    assertEquals(-5592415ll, _modsint64_020(-78193085935ll, 4886718345ll));
    assertEquals(5592415ll, _modsint64_020(78193085935ll, -4886718345ll));
    assertEquals(-5592415ll, _modsint64_020(-78193085935ll, -4886718345ll));


    // lsl
    assertEquals(0x12340000ull, _lshint64(0x12340000ull, 0));
    assertEquals(0x1234000000000000ull, _lshint64(0x12340000ull, 32));

    assertEquals(0x91a0ull, _lshint64(0x1234ull, 3));
    assertEquals(0x12340000ull, _lshint64(0x1234ull, 16));
    assertEquals(0x91a00000ull, _lshint64(0x1234ull, 19));
    assertEquals(0x123400000000ull, _lshint64(0x12340000ull, 16));
    assertEquals(0x91a000000000ull, _lshint64(0x12340000ull, 19));


    // lsr
    assertEquals(0x12340000ull, _rshsint64(0x12340000ull, 0));
    assertEquals(0x12340000ull, _rshsint64(0x1234000000000000ull, 32));

    assertEquals(0x1234ull, _rshsint64(0x91a0ull, 3));
    assertEquals(0x1234ull, _rshsint64(0x12340000ull, 16));
    assertEquals(0x1234ull, _rshsint64(0x91a00000ull, 19));
    assertEquals(0x12340000ull, _rshsint64(0x123400000000ull, 16));
    assertEquals(0x12340000ull, _rshsint64(0x91a000000000ull, 19));
}
