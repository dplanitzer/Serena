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
}
