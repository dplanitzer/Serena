//
//  main.c
//  libflowterm Tests
//
//  Created by Dietmar Planitzer on 8/25/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <flowterm.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ext/nanotime.h>
#include <serena/clock.h>



extern void curpos_test(int argc, char *argv[]);
extern void screensize_test(int argc, char *argv[]);

extern void getevent_test(int argc, char *argv[]);


typedef void (*test_func_t)(int argc, char *argv[]);

typedef struct test {
    const char* name;
    test_func_t func;
} test_t;


static const test_t gTests[] = {
    {"curpos", curpos_test},
    {"getevents", getevent_test},
    {"screensize", screensize_test},

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

        ft_init();
        testToRun->func(argc, argv);
        ft_cleanup();

        puts("ok");
        exit(0);
    }
    else {
        printf("Unknown test '%s'\n", name);
        exit(1);
    }
}
