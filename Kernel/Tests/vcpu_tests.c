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

#define CONCURRENCY_A 2
void vcpu_acquire_test(int argc, char *argv[])
{
    static const char* gStr[CONCURRENCY_A] = {"A", "B"};
    static vcpu_t gId[CONCURRENCY_A];

    for (int i = 0; i < CONCURRENCY_A; i++) {
        vcpu_attr_t attr = VCPU_ATTR_INIT;

        attr.func = (vcpu_func_t)test_loop;
        attr.arg = gStr[i];
        attr.stack_size = 0;
        attr.sched_params.type = SCHED_PARAM_QOS;
        attr.sched_params.u.qos.category = SCHED_QOS_INTERACTIVE;
        attr.sched_params.u.qos.priority = QOS_PRI_NORMAL;
        attr.groupid = 0;
        attr.flags = VCPU_ACQUIRE_RESUMED;
        gId[i] = vcpu_acquire(&attr);
        assertNotNULL(gId[i]);
    }
}


////////////////////////////////////////////////////////////////////////////////

static void test_infinite_loop(void)
{
    for (;;) {
    }
}

static void test_print_loop(void)
{
    for (;;) {
        puts("B");
        vcpu_yield();
    }
}

void vcpu_scheduling_test(int argc, char *argv[])
{
    vcpu_attr_t attr = VCPU_ATTR_INIT;
    vcpu_t vcpu_a, vcpu_b;

    attr.func = (vcpu_func_t)test_infinite_loop;
    attr.arg = NULL;
    attr.stack_size = 0;
    attr.sched_params.type = SCHED_PARAM_QOS;
    attr.sched_params.u.qos.category = SCHED_QOS_INTERACTIVE;
    attr.sched_params.u.qos.priority = QOS_PRI_NORMAL;
    attr.groupid = 0;
    attr.flags = VCPU_ACQUIRE_RESUMED;
    vcpu_a = vcpu_acquire(&attr);
    assertNotNULL(vcpu_a);

    vcpu_attr_t attr2 = VCPU_ATTR_INIT;
    attr2.func = (vcpu_func_t)test_print_loop;
    attr2.arg = NULL;
    attr2.stack_size = 0;
    attr2.sched_params.type = SCHED_PARAM_QOS;
    attr2.sched_params.u.qos.category = SCHED_QOS_INTERACTIVE;
    attr2.sched_params.u.qos.priority = QOS_PRI_LOWEST;
    attr2.groupid = 0;
    attr2.flags = VCPU_ACQUIRE_RESUMED;
    vcpu_b = vcpu_acquire(&attr2);
    assertNotNULL(vcpu_b);
}
