//
//  vcpu_tests.c
//  C Tests
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ext/nanotime.h>
#include <serena/clock.h>
#include <serena/signal.h>
#include <serena/vcpu.h>
#include <serena/vcpu_acquire.h>
#include "asserts.h"


////////////////////////////////////////////////////////////////////////////////
// vcpu_main_test

void vcpu_main_test(int argc, char *argv[])
{
    assert_int_eq(VCPUID_MAIN, vcpu_id(vcpu_self()));
    assert_int_eq(VCPUID_MAIN_GROUP, vcpu_groupid(vcpu_self()));
}


////////////////////////////////////////////////////////////////////////////////
// vcpu_acquire_test

static void test_acquire_loop(const char* _Nonnull str)
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
    vcpu_attr_t attr;

    for (int i = 0; i < CONCURRENCY_A; i++) {
        vcpu_attr_init(&attr);
        vcpu_attr_setqos(&attr, VCPU_QOS_INTERACTIVE, VCPU_PRI_NORMAL);

        gId[i] = vcpu_acquire((vcpu_func_t)test_acquire_loop, gStr[i], &attr);
        assert_not_null(gId[i]);
    }
}


////////////////////////////////////////////////////////////////////////////////
// vcpu_scheduling_test

static void test_scheduling_infinite_loop(void)
{
    for (;;) {
    }
}

static void test_scheduling_print_loop(void)
{
    static int counter = 0;

    for (;;) {
        printf("B: %d\n", counter++);
        vcpu_yield();
    }
}

// Two VPs:
// a) higher priority VP running an endless loop
// b) lower priority VP running a loop that prints 'B'
//
// -> (b) should not get starved to death by (a). The scheduler needs to make
//    sure that (b) receives some CPU time to do its job.
void vcpu_scheduling_test(int argc, char *argv[])
{
    vcpu_t vcpu_a, vcpu_b;
    vcpu_attr_t attr;

    vcpu_attr_init(&attr);
    vcpu_attr_setqos(&attr, VCPU_QOS_INTERACTIVE, VCPU_PRI_NORMAL);
    vcpu_a = vcpu_acquire((vcpu_func_t)test_scheduling_infinite_loop, NULL, &attr);
    assert_not_null(vcpu_a);

    vcpu_attr_init(&attr);
    vcpu_attr_setqos(&attr, VCPU_QOS_INTERACTIVE, VCPU_PRI_LOWEST);
    vcpu_b = vcpu_acquire((vcpu_func_t)test_scheduling_print_loop, NULL, &attr);
    assert_not_null(vcpu_b);
}


////////////////////////////////////////////////////////////////////////////////
// vcpu_sigkill_test

static vcpu_t test_sigkill_vcpu_a;

static void test_sigkill_print_loop(void)
{
    puts("A running\n");
    for (;;) {
        puts("A");
        vcpu_yield();
    }
}

static void test_sigkill_terminator(void)
{
    nanotime_t ts_1sec, ts_2sec;

    nanotime_from_sec(&ts_1sec, 1);
    puts("B running\n");

    clock_sleep(CLOCK_MONOTONIC, 0, &ts_1sec);
    puts("- terminating vcpu A -");
    
    assert_ok(sig_send(SIG_TARGET_VCPU, vcpu_id(test_sigkill_vcpu_a), SIG_FORCE_QUIT));
    puts("vcpu A terminated - done");
}

// Two VPs:
// a) runs continuously and prints some text
// b) sends SIG_FORCE_QUIT to (a)
// -> forces (a) to relinquish
void vcpu_sigkill_test(int argc, char *argv[])
{
    vcpu_attr_t attr;
    vcpu_t vcpu_b;

    vcpu_attr_init(&attr);
    vcpu_attr_setqos(&attr, VCPU_QOS_INTERACTIVE, VCPU_PRI_NORMAL);
    test_sigkill_vcpu_a = vcpu_acquire((vcpu_func_t)test_sigkill_print_loop, NULL, &attr);
    assert_not_null(test_sigkill_vcpu_a);

    vcpu_attr_init(&attr);
    vcpu_attr_setqos(&attr, VCPU_QOS_INTERACTIVE, VCPU_PRI_NORMAL);
    vcpu_b = vcpu_acquire((vcpu_func_t)test_sigkill_terminator, NULL, &attr);
    assert_not_null(vcpu_b);
}


////////////////////////////////////////////////////////////////////////////////
// vcpu_suspend_test

static vcpu_t test_suspend_vcpu_a;
static int suspension_count = 1;

static void test_suspend_print_loop(void)
{
    puts("A running\n");
    for (;;) {
        puts("A");
        vcpu_yield();
    }
}

static void test_suspend_A_loop(void)
{
    nanotime_t ts_1sec, ts_2sec;

    nanotime_from_sec(&ts_1sec, 1);
    nanotime_from_sec(&ts_2sec, 2);
    puts("B running\n");

    for (;;) {
        clock_sleep(CLOCK_MONOTONIC, 0, &ts_1sec);
        printf("- suspending A (%d) -\n", suspension_count++);
        
        assert_ok(vcpu_suspend(test_suspend_vcpu_a));
        clock_sleep(CLOCK_MONOTONIC, 0, &ts_2sec);
        
        puts("- resuming A -");
        vcpu_resume(test_suspend_vcpu_a);
    }
}

// Two VPs:
// a) runs continuously and prints some text
// b) suspends (a) every 1 second for 1 second and then resumes it
void vcpu_suspend_test(int argc, char *argv[])
{
    vcpu_attr_t attr;
    vcpu_t vcpu_b;

    vcpu_attr_init(&attr);
    vcpu_attr_setqos(&attr, VCPU_QOS_INTERACTIVE, VCPU_PRI_NORMAL);
    test_suspend_vcpu_a = vcpu_acquire((vcpu_func_t)test_suspend_print_loop, NULL, &attr);
    assert_not_null(test_suspend_vcpu_a);

    vcpu_attr_init(&attr);
    vcpu_attr_setqos(&attr, VCPU_QOS_INTERACTIVE, VCPU_PRI_NORMAL);
    vcpu_b = vcpu_acquire((vcpu_func_t)test_suspend_A_loop, NULL, &attr);
    assert_not_null(vcpu_b);
}
