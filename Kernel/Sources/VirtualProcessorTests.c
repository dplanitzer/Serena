//
//  VirtualProcessorTests.c
//  Apollo
//
//  Created by Dietmar Planitzer on 4/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//
#if 1

#include "Foundation.h"
#include "Console.h"
#include "Heap.h"
#include "MonotonicClock.h"
#include "Platform.h"
#include "SystemGlobals.h"
#include "VirtualProcessorPool.h"
#include "VirtualProcessorScheduler.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Acquire VP
////////////////////////////////////////////////////////////////////////////////


#if 0
static void OnHelloWorld(Byte* _Nonnull pContext)
{
    print("Hello World\n");
}

void VirtualProcessor_RunTests(void)
{
    VirtualProcessorAttributes attribs;
    
    VirtualProcessorAttributes_Init(&attribs);
    attribs.userStackSize = 0;
    VirtualProcessor* pVP = VirtualProcessorPool_AcquireVirtualProcessor(VirtualProcessorPool_GetShared(), &attribs, OnHelloWorld, NULL, false);
    VirtualProcessor_Resume(pVP, false);
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Acquire and reuse VP
////////////////////////////////////////////////////////////////////////////////


#if 0
static void OnHelloWorld(Byte* _Nonnull pContext)
{
    print("[%d]: Hello World\n", VirtualProcessor_GetCurrent()->vpid);
}

void VirtualProcessor_RunTests(void)
{
    VirtualProcessorPoolRef pPool = VirtualProcessorPool_GetShared();
    VirtualProcessorAttributes attribs;
    
    VirtualProcessorAttributes_Init(&attribs);
    attribs.userStackSize = 0;
    
    VirtualProcessor* pVP = VirtualProcessorPool_AcquireVirtualProcessor(pPool, &attribs, OnHelloWorld, NULL, false);
    VirtualProcessor_Resume(pVP, false);
    
    VirtualProcessor_Sleep(TimeInterval_MakeSeconds(1));
    print("\nreuse\n\n");

    VirtualProcessor* pVP2 = VirtualProcessorPool_AcquireVirtualProcessor(pPool, &attribs, OnHelloWorld, NULL, false);
    VirtualProcessor_Resume(pVP2, false);
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: User space call
////////////////////////////////////////////////////////////////////////////////


#if 0
extern void OnUserSpaceCallTest(Byte* _Nullable pContext);

void VirtualProcessor_RunTests(void)
{
    VirtualProcessorAttributes attribs;
    
    VirtualProcessorAttributes_Init(&attribs);
    VirtualProcessor* pVP = VirtualProcessorPool_AcquireVirtualProcessor(VirtualProcessorPool_GetShared(), &attribs, OnUserSpaceCallTest, NULL, true);
    VirtualProcessor_Resume(pVP, false);
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Async user space call
////////////////////////////////////////////////////////////////////////////////


#if 1
extern void OnAsyncUserSpaceCallTest_RunningInKernelSpace(Byte* _Nullable pContext);
extern void OnAsyncUserSpaceCallTest_RunningInUserSpace(Byte* _Nullable pContext);
extern void OnInjectedHelloWorld(Byte* _Nullable pContext);


void VirtualProcessor_RunTests(void)
{
    VirtualProcessorAttributes attribs;
    
    VirtualProcessorAttributes_Init(&attribs);
    
    VirtualProcessor* pVP = VirtualProcessorPool_AcquireVirtualProcessor(VirtualProcessorPool_GetShared(), &attribs, OnAsyncUserSpaceCallTest_RunningInKernelSpace, NULL, true);
    //VirtualProcessor* pVP = VirtualProcessorPool_AcquireVirtualProcessor(VirtualProcessorPool_GetShared(), &attribs, OnAsyncUserSpaceCallTest_RunningInUserSpace, NULL, true);
    VirtualProcessor_Resume(pVP, false);
    
    VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(100));
    VirtualProcessor_ScheduleAsyncUserClosureInvocation(pVP, OnInjectedHelloWorld, (Byte*)0x1234, false);
}
#endif

#endif
