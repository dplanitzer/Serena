//
//  int32_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 12/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "Asserts.h"


void int32_test(int argc, char *argv[])
{
    // abs()
    assertEquals(0, abs(0));
    assertEquals(INT_MAX, abs(INT_MAX));
    assertEquals(INT_MAX, abs(-INT_MAX));


    // div()
    assertEquals(15, div(150, 10).quot);
    assertEquals(-15, div(-150, 10).quot);
    assertEquals(-15, div(150, -10).quot);
    assertEquals(15, div(-150, -10).quot);

    assertEquals(7, div(150, 11).rem);
    assertEquals(-7, div(-150, 11).rem);
    assertEquals(7, div(150, -11).rem);
    assertEquals(-7, div(-150, -11).rem);


    // itoa
    char buf[16];
    itoa(-78678, buf, 10);
    assertEquals(0, strcmp(buf, "-78678"));
    itoa(8789798, buf, 10);
    assertEquals(0, strcmp(buf, "8789798"));
    itoa(0, buf, 10);
    assertEquals(0, strcmp(buf, "0"));
    itoa(INT_MIN, buf, 10);
    assertEquals(0, strcmp(buf, "-2147483648"));
    itoa(INT_MAX, buf, 10);
    assertEquals(0, strcmp(buf, "2147483647"));
}
