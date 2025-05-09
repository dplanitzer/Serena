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
#include <System/System.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchAsync
////////////////////////////////////////////////////////////////////////////////

static void OnAsync(void* _Nonnull pValue)
{
    int val = (int)pValue;
    
    printf("%d\n", val);
    //TimeInterval dur = TimeInterval_MakeSeconds(2);
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
    TimeInterval ts;

    printf("%d\n", val);
    clock_gettime(CLOCK_UPTIME, &ts);
    assertOK(dispatch_after(kDispatchQueue_Main, TimeInterval_Add(ts, TimeInterval_MakeMilliseconds(500)), OnAsyncAfter, (void*)(val + 1), 0));
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
    TimeInterval dur = TimeInterval_MakeMilliseconds(500);

    clock_wait(CLOCK_UPTIME, &dur);
    printf("%d  (Queue: %d)\n", val, dispatch_getcurrent());
}

// XXX Note: you can not call this code from the main queue because it ends up
// XXX blocking on itself. This is expected behavior.
void dq_sync_test(int argc, char *argv[])
{
    int queue, i = 0;

    assertOK(dispatch_create(0, 4, kDispatchQoS_Utility, kDispatchPriority_Normal, &queue));
    while (true) {
        dispatch_sync(queue, OnSync, (void*) i);
        puts("--------");
        i++;
    }
}
