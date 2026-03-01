//
//  main.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ext/timespec.h>


// Atomic
extern void atomic_test(int argc, char *argv[]);

// Console
extern void interactive_console_test(int argc, char *argv[]);

// Dispatcher
extern void dq_async_test(int argc, char *argv[]);
extern void dq_after_test(int argc, char *argv[]);
extern void dq_repeating_test(int argc, char *argv[]);
extern void dq_signal_test(int argc, char* argv[]);
extern void dq_sync_test(int argc, char *argv[]);
extern void dq_terminate_test(int argc, char *argv[]);

// float, double
extern void fp_test(int argc, char *argv[]);

// File
extern void overwrite_file_test(int argc, char *argv[]);

// HID
extern void hid_test(int argc, char *argv[]);

// Int32
extern void int32_test(int argc, char *argv[]);

// Int64
extern void int64_test(int argc, char *argv[]);

// Kern
extern void kern_test(int argc, char *argv[]);

// Mem
extern void mem_test(int argc, char *argv[]);

// Mutex
extern void mtx_test(int argc, char *argv[]);

// Pipe
extern void pipe_test(int argc, char *argv[]);
extern void pipe2_test(int argc, char *argv[]);

// Proc
extern void proc_excpt_crash_test(int argc, char *argv[]);
extern void proc_excpt_handler_test(int argc, char *argv[]);
extern void proc_excpt_return_test(int argc, char *argv[]);
extern void proc_exec_test(int argc, char *argv[]);
extern void proc_exit_test(int argc, char *argv[]);

// RC
extern void rc_test(int argc, char *argv[]);

// Sema
extern void sem_test(int argc, char *argv[]);

// Stdio
extern void fopen_memory_fixed_size_test(int argc, char *argv[]);
extern void fopen_memory_variable_size_test(int argc, char *argv[]);

// String
extern void str_test(int argc, char *argv[]);

// UInt32
extern void uint32_test(int argc, char *argv[]);

// UInt64
extern void uint64_test(int argc, char *argv[]);

// Vcpu
extern void vcpu_acquire_test(int argc, char *argv[]);
extern void vcpu_scheduling_test(int argc, char *argv[]);
extern void vcpu_sigkill_test(int argc, char *argv[]);
extern void vcpu_suspend_test(int argc, char *argv[]);


typedef void (*test_func_t)(int argc, char *argv[]);

typedef struct test {
    const char* name;
    test_func_t func;
    bool        keepMainRunning;
} test_t;


static const test_t gTests[] = {
    {"atomic", atomic_test, false},

    {"console", interactive_console_test, false},

    {"dq_after", dq_after_test, true},
    {"dq_async", dq_async_test, true},
    {"dq_rep", dq_repeating_test, true},
    {"dq_signal", dq_signal_test, true},
    {"dq_sync", dq_sync_test, true},
    {"dq_term", dq_terminate_test, true},

    {"file", overwrite_file_test, false},

    {"fp", fp_test, false},

    {"hid", hid_test, false},

    {"int32", int32_test, false},

    {"int64", int64_test, false},

    {"kern", kern_test, false},

    {"mem", mem_test, false},

    {"mtx", mtx_test, true},

    {"pipe", pipe_test, false},
    {"pipe2", pipe2_test, true},

    {"proc_excpt_crash", proc_excpt_crash_test, false},
    {"proc_excpt_handler", proc_excpt_handler_test, false},
    {"proc_excpt_ret", proc_excpt_return_test, false},
    {"proc_exec", proc_exec_test, false},
    {"proc_exit", proc_exit_test, true},

    {"rc", rc_test, false},

    {"sem", sem_test, true},

    {"stdio", fopen_memory_fixed_size_test, false},
    {"stdio2", fopen_memory_variable_size_test, false},

    {"str", str_test, false},

    {"uint32", uint32_test, false},
    {"uint64", uint64_test, false},

    {"vcpu_aq", vcpu_acquire_test, true},
    {"vcpu_sched", vcpu_scheduling_test, true},
    {"vcpu_sigkill", vcpu_sigkill_test, true},
    {"vcpu_suspend", vcpu_suspend_test, true},

    {"", NULL}
};


int main(int argc, char *argv[])
{
    if (argc < 2) {
        puts("Need a test name");
        exit(1);
    }

    const char* name = argv[1];
    const test_t* test = gTests;
    const test_t* testToRun = NULL;


    if (!strcmp(name, "list")) {
        while(test->func) {
            puts(test->name);
            test++;
        }

        exit(0);
    }


    while (test->func) {
        if (!strcmp(test->name, name)) {
            testToRun = test;
            break;
        }
        test++;
    }

    if (testToRun) {
        printf("Running Test: %s\n", name);
        testToRun->func(argc, argv);

        if (testToRun->keepMainRunning) {
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &TIMESPEC_INF, NULL);
        }
        else {
            puts("ok");
            exit(0);
        }
    }
    else {
        printf("Unknown test '%s'\n", name);
        exit(1);
    }
}
