//
//  int32_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 12/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <stdlib.h>
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
}
