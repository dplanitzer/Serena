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

// Console
extern void interactive_console_test(int argc, char *argv[]);

// Dispatch Queue
extern void dq_async_test(int argc, char *argv[]);
extern void dq_async_after_test(int argc, char *argv[]);
extern void dq_sync_test(int argc, char *argv[]);

// Pipe
extern void pipe_test(int argc, char *argv[]);
extern void pipe2_test(int argc, char *argv[]);

// Stdio
extern void fopen_memory_fixed_size_test(int argc, char *argv[]);
extern void fopen_memory_variable_size_test(int argc, char *argv[]);


typedef void (*test_func_t)(int argc, char *argv[]);

typedef struct test {
    const char* name;
    test_func_t func;
    bool        keepMainRunning;
} test_t;


static const test_t gTests[] = {
    {"console", interactive_console_test, false},

    {"dq_async", dq_async_test, true},
    {"dq_async_after", dq_async_after_test, true},
    {"dq_sync", dq_sync_test, true},

    {"pipe", pipe_test, false},
    {"pipe2", pipe2_test, true},

    {"stdio", fopen_memory_fixed_size_test, false},
    {"stdio2", fopen_memory_variable_size_test, false},
    {"", NULL}
};


void main_closure(int argc, char *argv[])
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
        fputs("Test: ", stdout);
        puts(name);
        testToRun->func(argc, argv);

        if (!testToRun->keepMainRunning) {
            exit(0);
        }
    }
    else {
        fputs("Unknown test '", stdout);
        fputs(name, stdout);
        fputs("'\n", stdout);

        exit(1);
    }
}
