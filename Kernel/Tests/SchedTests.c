//
//  SchedTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 6/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/dispatch.h>
#include "Asserts.h"


static void OnWriteString(const char* _Nonnull str)
{
    for (;;) {
        puts(str);
        sched_yield();
    }
}

#define CONCURRENCY 2
void sched_yield_test(int argc, char *argv[])
{
    static const char* gStr[CONCURRENCY] = {"A", "B"};
    const int dq = os_dispatch_create(CONCURRENCY, CONCURRENCY, kDispatchQoS_Utility, kDispatchPriority_Normal);
    assertGreaterEqual(0, dq);

    for (int i = 0; i < CONCURRENCY; i++) {
        assertOK(os_dispatch_async(dq, (os_dispatch_func_t)OnWriteString, (void*)gStr[i]));
    }
}
