//
//  strtol_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 3/5/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <ext/stdlib.h>
#include <string.h>
#include "Asserts.h"


void strtol_test(int argc, char *argv[])
{
    assertEquals(0xabcd, strtol("0xabcd", NULL, 0));
    assertEquals(0177, strtol("0177", NULL, 0));
    assertEquals(123, strtol("123", NULL, 0));

    assertEquals(0x7fffffff, strtol("7fffffff", NULL, 16));
    assertEquals(2147483647, strtol("2147483647", NULL, 10));

    assertEquals(0, strtol("0", NULL, 0));
    assertEquals(-123, strtol("-123", NULL, 0));
    assertEquals(-2147483648, strtol("-2147483648", NULL, 10));

    assertEquals(LONG_MAX, strtol("2147483648", NULL, 10));
    assertEquals(LONG_MIN, strtol("-2147483649", NULL, 10));
    assertEquals(LONG_MAX, strtol("77309411328", NULL, 10));
    assertEquals(LONG_MIN, strtol("-77309411329", NULL, 10));
}
