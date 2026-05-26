//
//  syscall_tests.c
//  C Tests
//
//  Created by Dietmar Planitzer on 1/11/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <kpi/syscall.h>
#include "asserts.h"


void kern_test(int argc, char *argv[])
{
    assert_int_eq(0, _syscall(SC_nullsys));
}
