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


static void vcpu_main(const char* _Nonnull str)
{
    for (;;) {
        puts(str);
        vcpu_yield();
    }
}

#define CONCURRENCY 2
void acquire_vcpu_test(int argc, char *argv[])
{
    static const char* gStr[CONCURRENCY] = {"A", "B"};
    static vcpu_t gId[CONCURRENCY];

    for (int i = 0; i < CONCURRENCY; i++) {
        vcpu_attr_t attr = VCPU_ATTR_INIT;

        attr.func = (vcpu_func_t)vcpu_main;
        attr.arg = gStr[i];
        attr.stack_size = 0;
        attr.priority = 24;
        attr.groupid = 0;
        attr.flags = VCPU_ACQUIRE_RESUMED;
        gId[i] = vcpu_acquire(&attr);
        assertNotNULL(gId[i]);
    }
}
