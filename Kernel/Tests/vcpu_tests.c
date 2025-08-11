//
//  vcpu_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/vcpu.h>
#include "Asserts.h"


static void test_loop(const char* _Nonnull str)
{
    for (;;) {
        puts(str);
        vcpu_yield();
    }
}

#define CONCURRENCY 2
void vcpu_acquire_test(int argc, char *argv[])
{
    static const char* gStr[CONCURRENCY] = {"A", "B"};
    static vcpu_t gId[CONCURRENCY];

    for (int i = 0; i < CONCURRENCY; i++) {
        vcpu_attr_t attr = VCPU_ATTR_INIT;

        attr.func = (vcpu_func_t)test_loop;
        attr.arg = gStr[i];
        attr.stack_size = 0;
        attr.sched_params.qos = VCPU_QOS_INTERACTIVE;
        attr.sched_params.priority = VCPU_PRI_NORMAL;
        attr.groupid = 0;
        attr.flags = VCPU_ACQUIRE_RESUMED;
        gId[i] = vcpu_acquire(&attr);
        assertNotNULL(gId[i]);
    }
}
