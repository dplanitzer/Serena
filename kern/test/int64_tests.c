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

extern unsigned long long _mulsint64_060(unsigned long long x, unsigned long long y);


void int64_test(int argc, char *argv[])
{
    // llabs()
    assert_long_long_eq(0, llabs(0));
    assert_long_long_eq(LLONG_MAX, llabs(LLONG_MAX));
    assert_long_long_eq(LLONG_MAX, llabs(-LLONG_MAX));


    // lldiv()
    // 32/32
    assert_long_long_eq(15ll, _divsint64_020(150ll, 10ll));
    assert_long_long_eq(-15ll, _divsint64_020(-150ll, 10ll));
    assert_long_long_eq(-15ll, _divsint64_020(150ll, -10ll));
    assert_long_long_eq(15ll, _divsint64_020(-150ll, -10ll));

    // 64/32
    assert_long_long_eq(231412ll, _divsint64_020(78193085935ll, 337895ll));
    assert_long_long_eq(-231412ll, _divsint64_020(-78193085935ll, 337895ll));
    assert_long_long_eq(-231412ll, _divsint64_020(78193085935ll, -337895ll));
    assert_long_long_eq(231412ll, _divsint64_020(-78193085935ll, -337895ll));

    // 64/64
    assert_long_long_eq(16ll, _divsint64_020(78193085935ll, 4886718345ll));
    assert_long_long_eq(-16ll, _divsint64_020(-78193085935ll, 4886718345ll));
    assert_long_long_eq(-16ll, _divsint64_020(78193085935ll, -4886718345ll));
    assert_long_long_eq(16ll, _divsint64_020(-78193085935ll, -4886718345ll));


    // llmod()
    // 32/32
    assert_long_long_eq(7ll, _modsint64_020(150ll, 11ll));
    assert_long_long_eq(-7ll, _modsint64_020(-150ll, 11ll));
    assert_long_long_eq(7ll, _modsint64_020(150ll, -11ll));
    assert_long_long_eq(-7ll, _modsint64_020(-150ll, -11ll));

    // 64/32
    assert_long_long_eq(128195ll, _modsint64_020(78193085935ll, 337895ll));
    assert_long_long_eq(-128195ll, _modsint64_020(-78193085935ll, 337895ll));
    assert_long_long_eq(128195ll, _modsint64_020(78193085935ll, -337895ll));
    assert_long_long_eq(-128195ll, _modsint64_020(-78193085935ll, -337895ll));

    // 64/64
    assert_long_long_eq(5592415ll, _modsint64_020(78193085935ll, 4886718345ll));
    assert_long_long_eq(-5592415ll, _modsint64_020(-78193085935ll, 4886718345ll));
    assert_long_long_eq(5592415ll, _modsint64_020(78193085935ll, -4886718345ll));
    assert_long_long_eq(-5592415ll, _modsint64_020(-78193085935ll, -4886718345ll));


    // lsl
    assert_long_long_eq(0x12340000ll, _lshint64(0x12340000ll, 0));
    assert_long_long_eq(0x1234000000000000ll, _lshint64(0x12340000ll, 32));

    assert_long_long_eq(0x91a0ll, _lshint64(0x1234ll, 3));
    assert_long_long_eq(0x12340000ll, _lshint64(0x1234ll, 16));
    assert_long_long_eq(0x91a00000ll, _lshint64(0x1234ll, 19));
    assert_long_long_eq(0x123400000000ll, _lshint64(0x12340000ll, 16));
    assert_long_long_eq(0x91a000000000ll, _lshint64(0x12340000ll, 19));


    // lsr
    assert_long_long_eq(0x12340000ll, _rshsint64(0x12340000ll, 0));
    assert_long_long_eq(0x12340000ll, _rshsint64(0x1234000000000000ll, 32));
    assert_long_long_eq(0xffffffffedcc0000ll, _rshsint64(0xedcc000000000000ll, 32));   // negative x
    assert_long_long_eq(0xfffffffff6e60000ll, _rshsint64(0xedcc000000000000ll, 33));   // negative x

    assert_long_long_eq(0x1234ll, _rshsint64(0x91a0ll, 3));
    assert_long_long_eq(0x1234ll, _rshsint64(0x12340000ll, 16));
    assert_long_long_eq(0x1234ll, _rshsint64(0x91a00000ll, 19));
    assert_long_long_eq(0x12340000ll, _rshsint64(0x123400000000ll, 16));
    assert_long_long_eq(0x12340000ll, _rshsint64(0x91a000000000ll, 19));

    assert_long_long_eq(0xfdb9800000000000ll, _rshsint64(0xedcc000000000000ll, 3));    // negative x
    assert_long_long_eq(0xffffedcc00000000ll, _rshsint64(0xedcc000000000000ll, 16));   // negative x
    assert_long_long_eq(0xfffffdb980000000ll, _rshsint64(0xedcc000000000000ll, 19));   // negative x


    // llmul()
    //32*32
    assert_long_long_eq(150ull, _mulsint64_060(15ull, 10ull));
    assert_long_long_eq(-150ull, _mulsint64_060(15ull, -10ull));

    //64*32
    assert_long_long_eq(26421052772006825ull, _mulsint64_060(78193085935ull, 337895ull));
    assert_long_long_eq(-26421052772006825ull, _mulsint64_060(-78193085935ull, 337895ull));

    //64*64
    assert_long_long_eq(0xB6CEDA547E375DE7ull, _mulsint64_060(78193085935ull, 4886718345ull));
    assert_long_long_eq(0xB6CEDA547E375DE7ull, _mulsint64_060(-78193085935ull, -4886718345ull));
}
