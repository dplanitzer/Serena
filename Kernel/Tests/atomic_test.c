//
//  atomic_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 1/4/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/atomic.h>
#include "Asserts.h"


void atomic_test(int argc, char *argv[])
{
    atomic_int val;

    atomic_init(&val, 0);

    assertEquals(0, atomic_int_fetch_add(&val, 1));
    assertEquals(1, atomic_int_fetch_add(&val, 1));

    assertEquals(2, atomic_int_fetch_sub(&val, 1));
    assertEquals(1, atomic_int_fetch_sub(&val, 1));
    assertEquals(0, atomic_int_load(&val));
}
