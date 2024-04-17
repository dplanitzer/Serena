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
    decl_try_err();
    int desc;

    Lock_Lock(&pProc->lock);
    // XXX optimize this: we don't need a persistent reference to the queue since
    // we're holding the process lock while doing the lookup and we only care about
    // the descriptor anyway.
    // Actually, look into storing the descriptor in the queue object itself since
    // we can get the queue object easily.
    // XXX bring this back
    //err = Process_GetDescriptorForPrivateResource_Locked(pProc, (ObjectRef) DispatchQueue_GetCurrent(), &desc);
    desc = -1;
    err = EOK;
    //XXX
    assert(err == EOK);
    Lock_Unlock(&pProc->lock);

    return desc;
}

// Dispatches the execution of the given user closure on the given dispatch queue
// with the given options. 
errno_t Process_DispatchUserClosure(ProcessRef _Nonnull pProc, int od, unsigned long options, Closure1Arg_Func _Nonnull pUserClosure, void* _Nullable pContext)
{
    decl_try_err();
    UDispatchQueueRef pQueue;

    if ((err = UResourceTable_AcquireResourceAs(&pProc->uResourcesTable, od, UDispatchQueue, &pQueue)) == EOK) {
        if ((options & kDispatchOption_Sync) == kDispatchOption_Sync) {
            err = DispatchQueue_DispatchSync(pQueue->dispatchQueue, DispatchQueueClosure_MakeUser(pUserClosure, pContext));
        } else {
            err = DispatchQueue_DispatchAsync(pQueue->dispatchQueue, DispatchQueueClosure_MakeUser(pUserClosure, pContext));
        }
        UResourceTable_RelinquishResource(&pProc->uResourcesTable, pQueue);
    }
    return err;
}

// Dispatches the execution of the given user closure on the given dispatch queue
// after the given deadline.
errno_t Process_DispatchUserClosureAsyncAfter(ProcessRef _Nonnull pProc, int od, TimeInterval deadline, Closure1Arg_Func _Nonnull pUserClosure, void* _Nullable pContext)
{
    decl_try_err();
    UDispatchQueueRef pQueue;

    if ((err = UResourceTable_AcquireResourceAs(&pProc->uResourcesTable, od, UDispatchQueue, &pQueue)) == EOK) {
        err = DispatchQueue_DispatchAsyncAfter(pQueue->dispatchQueue, deadline, DispatchQueueClosure_MakeUser(pUserClosure, pContext));
        UResourceTable_RelinquishResource(&pProc->uResourcesTable, pQueue);
    }
    return err;
}
