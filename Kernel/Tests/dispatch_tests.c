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
#include <sys/os_dispatch.h>
#include <sys/timespec.h>
#include "Asserts.h"

static dispatch_t gDispatcher;


////////////////////////////////////////////////////////////////////////////////
// MARK: dispatch_async

static void OnAsync(void* _Nonnull pValue)
{
    int val = (int)pValue;
    
    printf("%d\n", val);
    //struct timespec dur;
    // timespec_from_sec(&dur, 2);
    //clock_nanosleep(clock_uptime, 0, &dur, NULL);
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsync, (void*)(val + 1)));
}

void dq_async_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsync, (void*)0));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: dispatch_sync

static int OnSync(void* _Nonnull pValue)
{
    int val = (int)pValue;
    struct timespec dur;

    timespec_from_ms(&dur, 500);
    clock_nanosleep(CLOCK_MONOTONIC, 0, &dur, NULL);
    printf("%d\n", val);
//    printf("%d  (Queue: %d)\n", val, os_dispatch_getcurrent());
    return 1234;
}

// XXX Note: you can not call this code from the main queue because it ends up
// XXX blocking on itself. This is expected behavior.
void dq_sync_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    int i = 0;

    while (true) {
        const int r = dispatch_sync(gDispatcher, (dispatch_sync_func_t)OnSync, (void*) i);

        assertEquals(1234, r);
        puts("--------");
        i++;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: dispatch_after

static struct timespec DELAY_500MS;

static void OnAsyncAfter(void* _Nonnull pValue)
{
    int val = (int)pValue;

    printf("%d\n", val);
    assertOK(dispatch_after(gDispatcher, 0, &DELAY_500MS, (dispatch_async_func_t)OnAsyncAfter, (void*)(val + 1)));
}

void dq_async_after_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    timespec_from_ms(&DELAY_500MS, 500);
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsyncAfter, (void*)0));
}
