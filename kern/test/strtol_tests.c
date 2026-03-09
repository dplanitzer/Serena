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
    // strtol
    assert_long_eq(0xabcd, strtol("0xabcd", NULL, 0));
    assert_long_eq(0177, strtol("0177", NULL, 0));
    assert_long_eq(123, strtol("123", NULL, 0));

    assert_long_eq(0x7fffffff, strtol("7fffffff", NULL, 16));
    assert_long_eq(2147483647, strtol("2147483647", NULL, 10));

    assert_long_eq(0, strtol("0", NULL, 0));
    assert_long_eq(-123, strtol("-123", NULL, 0));
    assert_long_eq(-2147483648, strtol("-2147483648", NULL, 10));

    assert_long_eq(LONG_MAX, strtol("2147483648", NULL, 10));
    assert_long_eq(LONG_MIN, strtol("-2147483649", NULL, 10));
    assert_long_eq(LONG_MAX, strtol("77309411328", NULL, 10));
    assert_long_eq(LONG_MIN, strtol("-77309411329", NULL, 10));


    // strtoul
    assert_ulong_eq(0xabcd, strtoul("0xabcd", NULL, 0));
    assert_ulong_eq(0177, strtoul("0177", NULL, 0));
    assert_ulong_eq(0, strtoul("0877", NULL, 0));      // must detect that 8 is not a valid octal digit and thus parsing needs to stop right after 0
    assert_ulong_eq(123, strtoul("123", NULL, 0));

    assert_ulong_eq(0xffffffff, strtoul("ffffffff", NULL, 16));
    assert_ulong_eq(4294967295, strtoul("4294967295", NULL, 10));

    assert_ulong_eq(0, strtoul("0", NULL, 0));
    assert_ulong_eq(ULONG_MAX, strtoul("4294967296", NULL, 10));
    assert_ulong_eq(ULONG_MAX, strtoul("154618822620", NULL, 10));
}
