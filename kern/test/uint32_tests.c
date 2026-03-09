//
//  uint32_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 1/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <ext/stdlib.h>
#include <string.h>
#include <ext/bit.h>
#include "Asserts.h"


void uint32_test(int argc, char *argv[])
{
    // udiv()
    assert_uint_eq(15, udiv(150, 10).quot);

    assert_uint_eq(7, udiv(150, 11).rem);


    // utoa
    char buf[16];
    utoa(8789798, buf, 10);
    assert_int_eq(0, strcmp(buf, "8789798"));
    utoa(0, buf, 10);
    assert_int_eq(0, strcmp(buf, "0"));
    utoa(INT_MAX, buf, 10);
    assert_int_eq(0, strcmp(buf, "2147483647"));


    // leading_zeros_uc
    assert_uint_eq(8, leading_zeros_uc(0));
    assert_uint_eq(4, leading_zeros_uc(0x0f));
    assert_uint_eq(0, leading_zeros_uc(0xff));


    // leading_zeros_us
    assert_uint_eq(16, leading_zeros_us(0));
    assert_uint_eq(8, leading_zeros_us(0x00ff));
    assert_uint_eq(4, leading_zeros_us(0x0fff));
    assert_uint_eq(0, leading_zeros_us(0xffff));


    // leading_zeros_ul
    assert_uint_eq(32, leading_zeros_ul(0));
    assert_uint_eq(16, leading_zeros_ul(0x0000ffff));
    assert_uint_eq(4, leading_zeros_ul(0x0fffffff));
    assert_uint_eq(0, leading_zeros_ul(0xffffffff));


    // leading_zeros_ull
    assert_uint_eq(64, leading_zeros_ull(0ull));
    assert_uint_eq(48, leading_zeros_ull(0x0000ffffull));
    assert_uint_eq(36, leading_zeros_ull(0x0fffffffull));
    assert_uint_eq(32, leading_zeros_ull(0xffffffffull));
    assert_uint_eq(16, leading_zeros_ull(0x0000ffffffffffffull));
}
