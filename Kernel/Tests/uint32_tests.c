//
//  uint32_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 1/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ext/bit.h>
#include "Asserts.h"


void uint32_test(int argc, char *argv[])
{
    // udiv()
    assertEquals(15, udiv(150, 10).quot);

    assertEquals(7, udiv(150, 11).rem);


    // utoa
    char buf[16];
    utoa(8789798, buf, 10);
    assertEquals(0, strcmp(buf, "8789798"));
    utoa(0, buf, 10);
    assertEquals(0, strcmp(buf, "0"));
    utoa(INT_MAX, buf, 10);
    assertEquals(0, strcmp(buf, "2147483647"));


    // leading_zeros_uc
    assertEquals(8, leading_zeros_uc(0));
    assertEquals(4, leading_zeros_uc(0x0f));
    assertEquals(0, leading_zeros_uc(0xff));


    // leading_zeros_us
    assertEquals(16, leading_zeros_us(0));
    assertEquals(8, leading_zeros_us(0x00ff));
    assertEquals(4, leading_zeros_us(0x0fff));
    assertEquals(0, leading_zeros_us(0xffff));


    // leading_zeros_ul
    assertEquals(32, leading_zeros_ul(0));
    assertEquals(16, leading_zeros_ul(0x0000ffff));
    assertEquals(4, leading_zeros_ul(0x0fffffff));
    assertEquals(0, leading_zeros_ul(0xffffffff));


    // leading_zeros_ull
    assertEquals(64, leading_zeros_ull(0ull));
    assertEquals(48, leading_zeros_ull(0x0000ffffull));
    assertEquals(36, leading_zeros_ull(0x0fffffffull));
    assertEquals(32, leading_zeros_ull(0xffffffffull));
    assertEquals(16, leading_zeros_ull(0x0000ffffffffffffull));
}
