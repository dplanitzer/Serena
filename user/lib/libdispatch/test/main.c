//
//  main.c
//  libdispatch tests
//
//  Created by Dietmar Planitzer on 5/25/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ext/nanotime.h>
#include <serena/clock.h>


// dispatcher
extern void async_test(int argc, char *argv[]);
extern void after_test(int argc, char *argv[]);
extern void repeating_test(int argc, char *argv[]);
extern void signal_test(int argc, char* argv[]);
extern void sync_test(int argc, char *argv[]);
extern void terminate_test(int argc, char *argv[]);


typedef void (*test_func_t)(int argc, char *argv[]);

typedef struct test {
    const char* name;
    test_func_t func;
    bool        keepMainRunning;
} test_t;


static const test_t gTests[] = {
    {"after", after_test, true},
    {"async", async_test, true},
    {"rep", repeating_test, true},
    {"signal", signal_test, true},
    {"sync", sync_test, true},
    {"term", terminate_test, true},

    {"", NULL}
};


int main(int argc, char *argv[])
{
    if (argc < 2) {
        puts("test name?");
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
            clock_sleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &NANOTIME_INF);
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
