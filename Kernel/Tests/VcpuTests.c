//
//  VcpuTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/dispatch.h>
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
        vcpu_acquire_params_t params;

        params.func = (vcpu_start_t)vcpu_main;
        params.arg = gStr[i];
        params.stack_size = 0;
        params.priority = 24;
        params.groupid = 0;
        params.flags = VCPU_ACQUIRE_RESUMED;
        gId[i] = vcpu_acquire(&params);
        assertNotNULL(gId[i]);
    }
}
