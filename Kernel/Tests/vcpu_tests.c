//
//  vcpu_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/timespec.h>
#include <sys/vcpu.h>
#include "Asserts.h"


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

    for (int i = 0; i < CONCURRENCY_A; i++) {
        vcpu_attr_t attr = VCPU_ATTR_INIT;

        attr.func = (vcpu_func_t)test_acquire_loop;
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
// vcpu_scheduling_test

static void test_scheduling_infinite_loop(void)
{
    for (;;) {
    }
}

static void test_scheduling_print_loop(void)
{
    for (;;) {
        puts("B");
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
    vcpu_attr_t attr = VCPU_ATTR_INIT;
    vcpu_t vcpu_a, vcpu_b;

    attr.func = (vcpu_func_t)test_scheduling_infinite_loop;
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
    attr2.func = (vcpu_func_t)test_scheduling_print_loop;
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
    struct timespec ts_1sec, ts_2sec;

    timespec_from_sec(&ts_1sec, 1);
    puts("B running\n");

    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts_1sec, NULL);
    puts("- terminating A -");
    
    assertOK(sigsend(SIG_SCOPE_VCPU, vcpu_id(test_sigkill_vcpu_a), SIGKILL));
    puts("done");
}

// Two VPs:
// a) runs continuously and prints some text
// b) sends SIGKILL to (a)
// -> forces (a) to relinquish
void vcpu_sigkill_test(int argc, char *argv[])
{
    vcpu_attr_t attr = VCPU_ATTR_INIT;
    vcpu_t vcpu_b;

    attr.func = (vcpu_func_t)test_sigkill_print_loop;
    attr.arg = NULL;
    attr.stack_size = 0;
    attr.sched_params.type = SCHED_PARAM_QOS;
    attr.sched_params.u.qos.category = SCHED_QOS_INTERACTIVE;
    attr.sched_params.u.qos.priority = QOS_PRI_NORMAL;
    attr.groupid = 0;
    attr.flags = VCPU_ACQUIRE_RESUMED;
    test_sigkill_vcpu_a = vcpu_acquire(&attr);
    assertNotNULL(test_sigkill_vcpu_a);

    vcpu_attr_t attr2 = VCPU_ATTR_INIT;
    attr2.func = (vcpu_func_t)test_sigkill_terminator;
    attr2.arg = NULL;
    attr2.stack_size = 0;
    attr2.sched_params.type = SCHED_PARAM_QOS;
    attr2.sched_params.u.qos.category = SCHED_QOS_INTERACTIVE;
    attr2.sched_params.u.qos.priority = QOS_PRI_NORMAL;
    attr2.groupid = 0;
    attr2.flags = VCPU_ACQUIRE_RESUMED;
    vcpu_b = vcpu_acquire(&attr2);
    assertNotNULL(vcpu_b);
}


////////////////////////////////////////////////////////////////////////////////
// vcpu_suspend_test

static vcpu_t test_suspend_vcpu_a;

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
    struct timespec ts_1sec, ts_2sec;

    timespec_from_sec(&ts_1sec, 1);
    timespec_from_sec(&ts_2sec, 2);
    puts("B running\n");

    for (;;) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &ts_1sec, NULL);
        puts("- suspending A -");
        
        assertOK(vcpu_suspend(test_suspend_vcpu_a));
        clock_nanosleep(CLOCK_MONOTONIC, 0, &ts_2sec, NULL);
        
        puts("- resuming A -");
        vcpu_resume(test_suspend_vcpu_a);
    }
}

// Two VPs:
// a) runs continuously and prints some text
// b) suspends (a) every 1 second for 1 second and then resumes it
void vcpu_suspend_test(int argc, char *argv[])
{
    vcpu_attr_t attr = VCPU_ATTR_INIT;
    vcpu_t vcpu_b;

    attr.func = (vcpu_func_t)test_suspend_print_loop;
    attr.arg = NULL;
    attr.stack_size = 0;
    attr.sched_params.type = SCHED_PARAM_QOS;
    attr.sched_params.u.qos.category = SCHED_QOS_INTERACTIVE;
    attr.sched_params.u.qos.priority = QOS_PRI_NORMAL;
    attr.groupid = 0;
    attr.flags = VCPU_ACQUIRE_RESUMED;
    test_suspend_vcpu_a = vcpu_acquire(&attr);
    assertNotNULL(test_suspend_vcpu_a);

    vcpu_attr_t attr2 = VCPU_ATTR_INIT;
    attr2.func = (vcpu_func_t)test_suspend_A_loop;
    attr2.arg = NULL;
    attr2.stack_size = 0;
    attr2.sched_params.type = SCHED_PARAM_QOS;
    attr2.sched_params.u.qos.category = SCHED_QOS_INTERACTIVE;
    attr2.sched_params.u.qos.priority = QOS_PRI_NORMAL;
    attr2.groupid = 0;
    attr2.flags = VCPU_ACQUIRE_RESUMED;
    vcpu_b = vcpu_acquire(&attr2);
    assertNotNULL(vcpu_b);
}
