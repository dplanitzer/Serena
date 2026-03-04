//
//  scanf_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 3/2/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <ext/stdlib.h>
#include <string.h>
#include "Asserts.h"


void scanf_test(int argc, char *argv[])
{
    assertEquals(0, sscanf("hello", "hello"));
    assertEquals(0, sscanf("hello", ""));
    assertEquals(EOF, sscanf("", "hello"));

    assertEquals(0, sscanf("hello world", "hello world"));
    assertEquals(0, sscanf("hello   \t\n\r\v world", "hello world"));
    assertEquals(0, sscanf("hello   \t\n world", "hello \t\n\r\v world"));
}
