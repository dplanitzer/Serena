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
    //clock_wait(TimeInterval_MakeSeconds(2));
    assertOK(DispatchQueue_DispatchAsync(kDispatchQueue_Main, OnAsync, (void*)(val + 1)));
}

void dq_async_test(int argc, char *argv[])
{
    assertOK(DispatchQueue_DispatchAsync(kDispatchQueue_Main, OnAsync, (void*)0));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchAsyncAfter
////////////////////////////////////////////////////////////////////////////////

static void OnAsyncAfter(void* _Nonnull pValue)
{
    int val = (int)pValue;
    
    printf("%d\n", val);
    assertOK(DispatchQueue_DispatchAsyncAfter(kDispatchQueue_Main, TimeInterval_Add(clock_gettime(), TimeInterval_MakeMilliseconds(500)), OnAsyncAfter, (void*)(val + 1), 0));
}

void dq_async_after_test(int argc, char *argv[])
{
    assertOK(DispatchQueue_DispatchAsync(kDispatchQueue_Main, OnAsyncAfter, (void*)0));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchSync
////////////////////////////////////////////////////////////////////////////////

static void OnSync(void* _Nonnull pValue)
{
    int val = (int)pValue;
    
    clock_wait(TimeInterval_MakeMilliseconds(500));
    printf("%d  (Queue: %d)\n", val, DispatchQueue_GetCurrent());
}

// XXX Note: you can not call this code from the main queue because it ends up
// XXX blocking on itself. This is expected behavior.
void dq_sync_test(int argc, char *argv[])
{
    int queue, i = 0;

    assertOK(DispatchQueue_Create(0, 4, kDispatchQoS_Utility, kDispatchPriority_Normal, &queue));
    while (true) {
        DispatchQueue_DispatchSync(queue, OnSync, (void*) i);
        puts("--------");
        i++;
    }
}
