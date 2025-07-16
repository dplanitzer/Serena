//
//  dispatch_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 5/3/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <dispatch.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/timespec.h>
#include "Asserts.h"

static dispatch_t gDispatcher;
static int gCounter;


////////////////////////////////////////////////////////////////////////////////
// MARK: dispatch_async

static void OnAsync(void* _Nonnull ign)
{
    printf("%d\n", gCounter++);
    //struct timespec dur;
    // timespec_from_sec(&dur, 2);
    //clock_nanosleep(clock_uptime, 0, &dur, NULL);
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsync, NULL));
}

void dq_async_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsync, NULL));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: dispatch_sync

static int OnSync(void* _Nonnull ign)
{
    struct timespec dur;

    timespec_from_ms(&dur, 500);
    clock_nanosleep(CLOCK_MONOTONIC, 0, &dur, NULL);
    printf("%d (Dispatcher: %p)\n", gCounter++, dispatch_current());
    return 1234;
}

void dq_sync_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    while (true) {
        const int r = dispatch_sync(gDispatcher, (dispatch_sync_func_t)OnSync, NULL);

        assertEquals(1234, r);
        puts("--------");
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: dispatch_after

static struct timespec DELAY_500MS;

static void OnAfter(void* _Nonnull ign)
{
    printf("%d\n", gCounter++);
    assertOK(dispatch_after(gDispatcher, 0, &DELAY_500MS, (dispatch_async_func_t)OnAfter, NULL));
}

void dq_after_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    timespec_from_ms(&DELAY_500MS, 500);
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAfter, NULL));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: dispatch_repeating

static struct timespec DELAY_250MS;

static void OnRepeating(void* _Nonnull ign)
{
    printf("%d\n", gCounter++);
}

void dq_repeating_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    timespec_from_ms(&DELAY_250MS, 250);
    assertOK(dispatch_repeating(gDispatcher, 0, &DELAY_250MS, &DELAY_250MS, (dispatch_async_func_t)OnRepeating, NULL));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: dispatch_terminate

static void OnAsync2(intptr_t _Nonnull value)
{
    printf("async: %d\n", (int)value);
}

static void OnRepeating2(intptr_t _Nonnull value)
{
    printf("timer: %d\n", (int)value);
}

// Should print async 1 - 3 and then terminate
void dq_terminate_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    timespec_from_ms(&DELAY_500MS, 500);
    assertOK(dispatch_repeating(gDispatcher, 0, &DELAY_500MS, &DELAY_500MS, (dispatch_async_func_t)OnRepeating2, (void*)1));
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsync2, (void*)1));
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsync2, (void*)2));
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsync2, (void*)3));

    printf("Terminating...\n");
    dispatch_terminate(gDispatcher, false);
    assertOK(dispatch_await_termination(gDispatcher));
    printf("Terminated.\n");
    assertOK(dispatch_destroy(gDispatcher));
    printf("Success!\n");
}
