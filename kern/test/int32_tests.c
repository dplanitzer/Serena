//
//  int32_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 12/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <ext/stdlib.h>
#include <string.h>
#include "Asserts.h"


void int32_test(int argc, char *argv[])
{
    // abs()
    assert_int_eq(0, abs(0));
    assert_int_eq(INT_MAX, abs(INT_MAX));
    assert_int_eq(INT_MAX, abs(-INT_MAX));


    // div()
    assert_int_eq(15, div(150, 10).quot);
    assert_int_eq(-15, div(-150, 10).quot);
    assert_int_eq(-15, div(150, -10).quot);
    assert_int_eq(15, div(-150, -10).quot);

    assert_int_eq(7, div(150, 11).rem);
    assert_int_eq(-7, div(-150, 11).rem);
    assert_int_eq(7, div(150, -11).rem);
    assert_int_eq(-7, div(-150, -11).rem);


    // itoa
    char buf[16];
    itoa(-78678, buf, 10);
    assert_int_eq(0, strcmp(buf, "-78678"));
    itoa(8789798, buf, 10);
    assert_int_eq(0, strcmp(buf, "8789798"));
    itoa(0, buf, 10);
    assert_int_eq(0, strcmp(buf, "0"));
    itoa(INT_MIN, buf, 10);
    assert_int_eq(0, strcmp(buf, "-2147483648"));
    itoa(INT_MAX, buf, 10);
    assert_int_eq(0, strcmp(buf, "2147483647"));
}
