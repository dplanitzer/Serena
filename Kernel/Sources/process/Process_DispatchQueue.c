//
//  Process_DispatchQueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/22/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "UDispatchQueue.h"
#include <dispatcher/VirtualProcessorPool.h>
#include <System/DispatchQueue.h>


// Creates a new dispatch queue and binds it to the process.
errno_t Process_CreateDispatchQueue(ProcessRef _Nonnull pProc, int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nullable pOutDescriptor)
{
    decl_try_err();
    UDispatchQueueRef pQueue = NULL;

    try(UDispatchQueue_Create(minConcurrency, maxConcurrency, qos, priority, gVirtualProcessorPool, pProc, &pQueue));
    try(UResourceTable_AdoptResource(&pProc->uResourcesTable, (UResourceRef) pQueue, pOutDescriptor));
    DispatchQueue_SetDescriptor(pQueue->dispatchQueue, *pOutDescriptor);
    pQueue = NULL;
    return EOK;

catch:
    UResource_Dispose(pQueue);
    *pOutDescriptor = -1;
    return err;
}

// Returns the dispatch queue associated with the virtual processor on which the
// calling code is running. Note this function assumes that it will ALWAYS be
// called from a system call context and thus the caller will necessarily run in
// the context of a (process owned) dispatch queue.
int Process_GetCurrentDispatchQueue(ProcessRef _Nonnull pProc)
{
    return DispatchQueue_GetDescriptor(DispatchQueue_GetCurrent());
}

// Dispatches the execution of the given user closure on the given dispatch queue
// with the given options. 
errno_t Process_DispatchUserClosure(ProcessRef _Nonnull pProc, int od, Closure1Arg_Func _Nonnull func, void* _Nullable ctx, uint32_t options)
{
    decl_try_err();
    UDispatchQueueRef pQueue;

    if ((options & kDispatchOption_Sync) == kDispatchOption_Sync) {
        if ((err = UResourceTable_AcquireResourceAs(&pProc->uResourcesTable, od, UDispatchQueue, &pQueue)) == EOK) {
            err = DispatchQueue_DispatchClosure(pQueue->dispatchQueue, func, ctx, options | kDispatchOption_User, 0);
            UResourceTable_RelinquishResource(&pProc->uResourcesTable, pQueue);
        }
    }
    else {
        if ((err = UResourceTable_BeginDirectResourceAccessAs(&pProc->uResourcesTable, od, UDispatchQueue, &pQueue)) == EOK) {
            err = DispatchQueue_DispatchClosure(pQueue->dispatchQueue, func, ctx, kDispatchOption_User, 0);
            UResourceTable_EndDirectResourceAccess(&pProc->uResourcesTable);
        }
    }

    return err;
}

// Dispatches the execution of the given user closure on the given dispatch queue
// after the given deadline.
errno_t Process_DispatchUserTimer(ProcessRef _Nonnull pProc, int od, TimeInterval deadline, TimeInterval interval, Closure1Arg_Func _Nonnull func, void* _Nullable ctx)
{
    decl_try_err();
    UDispatchQueueRef pQueue;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&pProc->uResourcesTable, od, UDispatchQueue, &pQueue)) == EOK) {
        err = DispatchQueue_DispatchTimer(pQueue->dispatchQueue, deadline, interval, func, ctx, kDispatchOption_User, 0);
        UResourceTable_EndDirectResourceAccess(&pProc->uResourcesTable);
    }
    return err;
}
