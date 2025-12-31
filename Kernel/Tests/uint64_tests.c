//
//  uint64_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 12/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <stdlib.h>
#include "Asserts.h"

extern long long _divuint64_020(long long dividend, long long divisor);
extern long long _moduint64_020(long long dividend, long long divisor);

void uint64_test(int argc, char *argv[])
{
    // ulldiv()
    // 32/32
    assertEquals(15ull, _divuint64_020(150ull, 10ull));

    // 64/32
    assertEquals(231412ull, _divuint64_020(78193085935ull, 337895ull));

    // 64/64
    assertEquals(16ull, _divuint64_020(78193085935ull, 4886718345ull));
    assertEquals(1ull, _divuint64_020(ULLONG_MAX, ULLONG_MAX));


    // ullmod()
    // 32/32
    assertEquals(0ull, _moduint64_020(150ull, 10ull));

    // 64/32
    assertEquals(128195ull, _moduint64_020(78193085935ull, 337895ull));

    // 64/64
    assertEquals(5592415ull, _moduint64_020(78193085935ull, 4886718345ull));
    assertEquals(0ull, _moduint64_020(ULLONG_MAX, ULLONG_MAX));
}
