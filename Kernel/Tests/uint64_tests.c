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

extern unsigned long long _lshuint64(unsigned long long x, int s);
extern unsigned long long _rshuint64(unsigned long long x, int s);


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


    // lsl
    assertEquals(0x12340000ull, _lshuint64(0x12340000ull, 0));
    assertEquals(0x1234000000000000ull, _lshuint64(0x12340000ull, 32));

    assertEquals(0x91a0ull, _lshuint64(0x1234ull, 3));
    assertEquals(0x12340000ull, _lshuint64(0x1234ull, 16));
    assertEquals(0x91a00000ull, _lshuint64(0x1234ull, 19));
    assertEquals(0x123400000000ull, _lshuint64(0x12340000ull, 16));
    assertEquals(0x91a000000000ull, _lshuint64(0x12340000ull, 19));


    // lsr
    assertEquals(0x12340000ull, _rshuint64(0x12340000ull, 0));
    assertEquals(0x12340000ull, _rshuint64(0x1234000000000000ull, 32));

    assertEquals(0x1234ull, _rshuint64(0x91a0ull, 3));
    assertEquals(0x1234ull, _rshuint64(0x12340000ull, 16));
    assertEquals(0x1234ull, _rshuint64(0x91a00000ull, 19));
    assertEquals(0x12340000ull, _rshuint64(0x123400000000ull, 16));
    assertEquals(0x12340000ull, _rshuint64(0x91a000000000ull, 19));
}
