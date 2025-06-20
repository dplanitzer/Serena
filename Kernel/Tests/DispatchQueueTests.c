//
//  DispatchQueueTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 5/3/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <Asserts.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/dispatch.h>
#include <sys/timespec.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchAsync
////////////////////////////////////////////////////////////////////////////////

static void OnAsync(void* _Nonnull pValue)
{
    int val = (int)pValue;
    
    printf("%d\n", val);
    //struct timespec dur;
    // timespec_from_sec(&dur, 2);
    //clock_wait(clock_uptime, &dur);
    assertOK(dispatch_async(kDispatchQueue_Main, OnAsync, (void*)(val + 1)));
}

void dq_async_test(int argc, char *argv[])
{
    assertOK(dispatch_async(kDispatchQueue_Main, OnAsync, (void*)0));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchAsyncAfter
////////////////////////////////////////////////////////////////////////////////

static void OnAsyncAfter(void* _Nonnull pValue)
{
    int val = (int)pValue;
    struct timespec now, dly, deadline;

    printf("%d\n", val);
    clock_gettime(CLOCK_MONOTONIC, &now);
    timespec_from_ms(&dly, 500);
    timespec_add(&now, &dly, &deadline);
    assertOK(dispatch_after(kDispatchQueue_Main, &deadline, OnAsyncAfter, (void*)(val + 1), 0));
}

void dq_async_after_test(int argc, char *argv[])
{
    assertOK(dispatch_async(kDispatchQueue_Main, OnAsyncAfter, (void*)0));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchSync
////////////////////////////////////////////////////////////////////////////////

static void OnSync(void* _Nonnull pValue)
{
    int val = (int)pValue;
    struct timespec dur;

    timespec_from_ms(&dur, 500);
    clock_wait(CLOCK_MONOTONIC, &dur);
    printf("%d  (Queue: %d)\n", val, dispatch_getcurrent());
}

// XXX Note: you can not call this code from the main queue because it ends up
// XXX blocking on itself. This is expected behavior.
void dq_sync_test(int argc, char *argv[])
{
    const int queue = dispatch_create(0, 4, kDispatchQoS_Utility, kDispatchPriority_Normal);
    int i = 0;

    assertGreaterEqual(0, queue);
    while (true) {
        dispatch_sync(queue, OnSync, (void*) i);
        puts("--------");
        i++;
    }
}
