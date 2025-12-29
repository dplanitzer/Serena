//
//  int64_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 12/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <stdlib.h>
#include "Asserts.h"


void int64_test(int argc, char *argv[])
{
    assertEquals(0, llabs(0));
    assertEquals(LLONG_MAX, llabs(LLONG_MAX));
    assertEquals(LLONG_MAX, llabs(-LLONG_MAX));
}
