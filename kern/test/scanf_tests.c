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
    char ch[8];

    assertEquals(0, sscanf("hello", "hello"));
    assertEquals(0, sscanf("hello", ""));
    assertEquals(EOF, sscanf("", "hello"));

    assertEquals(0, sscanf("hello world", "hello world"));
    assertEquals(0, sscanf("hello   \t\n\r\v world", "hello world"));
    assertEquals(0, sscanf("hello   \t\n world", "hello \t\n\r\v world"));

    assertEquals(1, sscanf("\t world", " %c", &ch[0]));
    assertEquals('w', ch[0]);

    assertEquals(1, sscanf("\t world", " %2c", ch));
    assertEquals('w', ch[0]);
    assertEquals('o', ch[1]);

    assertEquals(1, sscanf("\t world", "%2s", ch));
    assertEquals('w', ch[0]);
    assertEquals('o', ch[1]);
    assertEquals('\0', ch[2]);

    assertEquals(1, sscanf("\t wo  ", "%s", ch));
    assertEquals('w', ch[0]);
    assertEquals('o', ch[1]);
    assertEquals('\0', ch[2]);
}
