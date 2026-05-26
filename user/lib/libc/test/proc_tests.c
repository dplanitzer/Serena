//
//  proc_tests.c
//  C Tests
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
#include <serena/vcpu_acquire.h>
#include "asserts.h"


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
    clock_sleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &NANOTIME_INF);
}


#define CONCURRENCY 4
void proc_exit_test(int argc, char *argv[])
{
    static const char* gStr[CONCURRENCY] = {"WAIT", "SPIN"};
    static vcpu_t gId[CONCURRENCY + 1];
    vcpu_attr_t attr;

    for (size_t i = 0; i < CONCURRENCY; i++) {
        size_t r = i%2;
        vcpu_func_t func = (vcpu_func_t)((r) ? spin_loop : just_wait);
        void* arg = gStr[r];

        vcpu_attr_init(&attr);
        vcpu_attr_setqos(&attr, VCPU_QOS_INTERACTIVE, VCPU_PRI_NORMAL + i);
        gId[i] = vcpu_acquire(func, arg, &attr);
        assert_not_null(gId[i]);
    }

    vcpu_attr_init(&attr);
    vcpu_attr_setqos(&attr, VCPU_QOS_INTERACTIVE, VCPU_PRI_NORMAL);
    gId[CONCURRENCY] = vcpu_acquire((vcpu_func_t)just_suspend, "SUSPENDED", &attr);
    assert_not_null(gId[CONCURRENCY]);

    
    puts("Waiting...");
    nanotime_t delay;
    nanotime_from_sec(&delay, 2);
    clock_sleep(CLOCK_MONOTONIC, 0, &delay);

    puts("Exiting");
    exit(0);
}


////////////////////////////////////////////////////////////////////////////////
// proc_exec_test

void proc_exec_test(int argc, char *argv[])
{
    printf("About to exec...\n");

    const char* argv2[3];

    argv2[0] = "ctest";
    argv2[1] = "list";
    argv2[2] = NULL;

    assert_ok(proc_exec(argv2[0], argv2, NULL));
}
