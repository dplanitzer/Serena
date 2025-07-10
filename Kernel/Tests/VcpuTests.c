//
//  VcpuTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/proc.h>
#include <sys/vcpu.h>
#include "Asserts.h"


static void vcpu_main(const char* _Nonnull str)
{
    for (;;) {
        puts(str);
        sched_yield();
    }
}

#define CONCURRENCY 2
void acquire_vcpu_test(int argc, char *argv[])
{
    static const char* gStr[CONCURRENCY] = {"A", "B"};
    static vcpuid_t gId[CONCURRENCY];

    for (int i = 0; i < CONCURRENCY; i++) {
        vcpu_acquire_params_t params;

        params.func = (void (*)(void))vcpu_main;
        params.context = gStr[i];
        params.user_stack_size = 0;
        params.priority = 24;
        params.vpgid = 0;
        params.flags = VCPU_ACQUIRE_RESUMED;
        assertOK(proc_acquire_vcpu(&params, &gId[i]));
    }
}
