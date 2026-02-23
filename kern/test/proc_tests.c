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
#include <unistd.h>
#include <ext/timespec.h>
#include <sys/exception.h>
#include <sys/vcpu.h>
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
    assertOK(vcpu_suspend(vcpu_self()));
}

static void just_wait(const char* _Nonnull str)
{
    puts(str);
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &TIMESPEC_INF, NULL);
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
        attr.sched_params.type = SCHED_PARAM_QOS;
        attr.sched_params.u.qos.category = SCHED_QOS_INTERACTIVE;
        attr.sched_params.u.qos.priority = QOS_PRI_NORMAL + i;
        attr.groupid = 0;
        attr.flags = VCPU_ACQUIRE_RESUMED;
        gId[i] = vcpu_acquire(&attr);
        assertNotNULL(gId[i]);
    }

    vcpu_attr_t attr = VCPU_ATTR_INIT;

    attr.func = (vcpu_func_t)just_suspend;
    attr.arg = "SUSPENDED";
    attr.stack_size = 0;
    attr.sched_params.type = SCHED_PARAM_QOS;
    attr.sched_params.u.qos.category = SCHED_QOS_INTERACTIVE;
    attr.sched_params.u.qos.priority = QOS_PRI_NORMAL;
    attr.groupid = 0;
    attr.flags = VCPU_ACQUIRE_RESUMED;
    gId[CONCURRENCY] = vcpu_acquire(&attr);
    assertNotNULL(gId[CONCURRENCY]);

    
    puts("Waiting...");
    struct timespec delay;
    timespec_from_sec(&delay, 2);
    clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, NULL);

    puts("Exiting");
    exit(0);
}


////////////////////////////////////////////////////////////////////////////////
// proc_excpt_crash_test

int movesr(void) = "\tmove.w\tsr, d0\n";

void proc_excpt_crash_test(int argc, char *argv[])
{
    const int r = movesr();
    // -> process should have exited with an exception status
    // -> should not print
    printf("sr: %d\n", r);
}


////////////////////////////////////////////////////////////////////////////////
// proc_excpt_handler_test

static int ex_handler(void* arg, const excpt_info_t* _Nonnull ei, mcontext_t* _Nonnull mc)
{
    if (ei->code == EXCPT_PRIV_INSTRUCTION) {
        printf("arg: %s\n", arg);
        printf("code: %d\n", ei->code);
        printf("cpu_code: %d\n", ei->cpu_code);
        printf("addr: %p\n", ei->addr);
        printf("PC: %p\n", mc->pc);

        exit(EXIT_SUCCESS);
        /* NOT REACHED */
    }

    return EXCPT_ABORT_EXECUTION;
}

void proc_excpt_handler_test(int argc, char *argv[])
{
    excpt_handler_t h;

    h.func = ex_handler;
    h.arg = "exiting from handler";
    excpt_sethandler(0, &h, NULL);
    
    const int r = movesr();
    // -> process should have exited with (regular) status 0
    // -> should not print
    printf("sr: %d\n", r);
}


////////////////////////////////////////////////////////////////////////////////
// proc_excpt_return_test

static int ex_handler2(void* arg, const excpt_info_t* _Nonnull ei, mcontext_t* _Nonnull mc)
{
    if (ei->code == EXCPT_PRIV_INSTRUCTION) {
        printf("arg: %s\n", arg);
        printf("code: %d\n", ei->code);
        printf("cpu_code: %d\n", ei->cpu_code);
        printf("addr: %p\n", ei->addr);
        printf("PC: %p\n", mc->pc);

        mc->pc += 2;        // Skip the 'move sr, d0'
        mc->d[0] = 1234;    // Return a faked result

        return EXCPT_CONTINUE_EXECUTION;
    }

    return EXCPT_ABORT_EXECUTION;
}

void proc_excpt_return_test(int argc, char *argv[])
{
    excpt_handler_t h;

    h.func = ex_handler2;
    h.arg = "returning from handler";
    excpt_sethandler(0, &h, NULL);
    
    const int r = movesr();
    // -> process should have returned from ex_handler2
    // -> should skip the move SR, execute the next line and print '1234'.
    printf("\nSR: %d\nExiting.\n", r);
}

void proc_exec_test(int argc, char *argv[])
{
    printf("About to exec...\n");

    const char* argv2[3];

    argv2[0] = "test";
    argv2[1] = "list";
    argv2[2] = NULL;

    assertOK(proc_exec("test", argv2, NULL));
}
