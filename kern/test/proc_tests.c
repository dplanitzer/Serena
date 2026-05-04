//
//  proc_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 7/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ext/nanotime.h>
#include <serena/clock.h>
#include <serena/process.h>
#include <serena/vcpu.h>
#include "Asserts.h"


////////////////////////////////////////////////////////////////////////////////
// proc_exit_test

static void spin_loop(const char* _Nonnull str)
{
    puts(str);

    for (;;) {
    }
}

static void just_suspend(const char* _Nonnull str)
{
    puts(str);
    assert_ok(vcpu_suspend(vcpu_self()));
}

static void just_wait(const char* _Nonnull str)
{
    puts(str);
    clock_wait(CLOCK_MONOTONIC, TIMER_ABSTIME, &NANOTIME_INF, NULL);
}


#define CONCURRENCY 4
void proc_exit_test(int argc, char *argv[])
{
    static const char* gStr[CONCURRENCY] = {"WAIT", "SPIN"};
    static vcpu_t gId[CONCURRENCY + 1];

    for (size_t i = 0; i < CONCURRENCY; i++) {
        vcpu_attr_t attr = VCPU_ATTR_INIT;
        size_t r = i%2;

        attr.func = (vcpu_func_t)((r) ? spin_loop : just_wait);
        attr.arg = gStr[r];
        attr.stack_size = 0;
        attr.policy.version = sizeof(vcpu_policy_t);
        attr.policy.qos.grade = VCPU_QOS_INTERACTIVE;
        attr.policy.qos.priority = VCPU_PRI_NORMAL + i;
        attr.group_id = 0;
        attr.flags = VCPU_ACQUIRE_RESUMED;
        gId[i] = vcpu_acquire(&attr);
        assert_not_null(gId[i]);
    }

    vcpu_attr_t attr = VCPU_ATTR_INIT;

    attr.func = (vcpu_func_t)just_suspend;
    attr.arg = "SUSPENDED";
    attr.stack_size = 0;
    attr.policy.version = sizeof(vcpu_policy_t);
    attr.policy.qos.grade = VCPU_QOS_INTERACTIVE;
    attr.policy.qos.priority = VCPU_PRI_NORMAL;
    attr.group_id = 0;
    attr.flags = VCPU_ACQUIRE_RESUMED;
    gId[CONCURRENCY] = vcpu_acquire(&attr);
    assert_not_null(gId[CONCURRENCY]);

    
    puts("Waiting...");
    nanotime_t delay;
    nanotime_from_sec(&delay, 2);
    clock_wait(CLOCK_MONOTONIC, 0, &delay, NULL);

    puts("Exiting");
    exit(0);
}


////////////////////////////////////////////////////////////////////////////////
// proc_exec_test

void proc_exec_test(int argc, char *argv[])
{
    printf("About to exec...\n");

    const char* argv2[3];

    argv2[0] = "test";
    argv2[1] = "list";
    argv2[2] = NULL;

    assert_ok(proc_exec("test", argv2, NULL));
}
