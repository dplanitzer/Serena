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
#include <sys/exception.h>
#include <sys/timespec.h>
#include <sys/vcpu.h>
#include "Asserts.h"


static void spin_loop(const char* _Nonnull str)
{
    puts(str);

    for (;;) {
    }
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
    static vcpu_t gId[CONCURRENCY];

    for (size_t i = 0; i < CONCURRENCY; i++) {
        vcpu_attr_t attr = VCPU_ATTR_INIT;
        size_t r = i%2;

        attr.func = (vcpu_func_t)((r) ? spin_loop : just_wait);
        attr.arg = gStr[r];
        attr.stack_size = 0;
        attr.priority = 24;
        attr.groupid = 0;
        attr.flags = VCPU_ACQUIRE_RESUMED;
        gId[i] = vcpu_acquire(&attr);
        assertNotNULL(gId[i]);
    }

    puts("Waiting...");
    struct timespec delay;
    timespec_from_sec(&delay, 1);
    clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, NULL);

    puts("Exiting");
    exit(0);
}


int movesr(void) = "\tmove.w\tsr, d0\n";

void proc_excpt_crash_test(int argc, char *argv[])
{
    const int r = movesr();
    // -> process should have exited with an exception status
    // -> should not print
    printf("sr: %d\n", r);
}


static void ex_handler(void* arg, const excpt_info_t* _Nonnull ei, excpt_ctx_t* _Nonnull ctx)
{
    printf("arg: %s\n", arg);
    printf("code: %d\n", ei->code);
    printf("cpu_code: %d\n", ei->cpu_code);
    printf("addr: %p\n", ei->addr);

    exit(0);
}

void proc_excpt_handler_test(int argc, char *argv[])
{
    excpt_handler_t h;

    h.func = ex_handler;
    h.arg = "exiting fro mhandler";
    excpt_sethandler(EXCPT_SCOPE_PROC, 0, &h, NULL);
    
    const int r = movesr();
    // -> process should have exited with (regular) status 0
    // -> should not print
    printf("sr: %d\n", r);
}


static void ex_handler2(void* arg, const excpt_info_t* _Nonnull ei, excpt_ctx_t* _Nonnull ctx)
{
    printf("arg: %s\n", arg);
    printf("code: %d\n", ei->code);
    printf("cpu_code: %d\n", ei->cpu_code);
    printf("addr: %p\n", ei->addr);
}

void proc_excpt_return_test(int argc, char *argv[])
{
    excpt_handler_t h;

    h.func = ex_handler2;
    h.arg = "returning from handler";
    excpt_sethandler(EXCPT_SCOPE_PROC, 0, &h, NULL);
    
    const int r = movesr();
    // -> process should have returned from ex_handler2
    // -> should not print and instead invoke ex_handler2 again
    printf("sr: %d\n", r);
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
