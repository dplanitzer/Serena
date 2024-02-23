//
//  Process_DispatchQueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/22/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "DispatchQueue.h"
#include "VirtualProcessorPool.h"
#include <System/DispatchQueue.h>


// Creates a new dispatch queue and binds it to the process.
errno_t Process_CreateDispatchQueue(ProcessRef _Nonnull pProc, int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nullable pOutDescriptor)
{
    decl_try_err();
    DispatchQueueRef pQueue = NULL;

    Lock_Lock(&pProc->lock);

    *pOutDescriptor = -1;
    try(DispatchQueue_Create(minConcurrency, maxConcurrency, qos, priority, gVirtualProcessorPool, pProc, &pQueue));
    try(Process_RegisterPrivateResource_Locked(pProc, (ObjectRef) pQueue, pOutDescriptor));

catch:
    Object_Release(pQueue);
    Lock_Unlock(&pProc->lock);
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
    err = Process_GetDescriptorForPrivateResource_Locked(pProc, (ObjectRef) DispatchQueue_GetCurrent(), &desc);
    assert(err == EOK);
    Lock_Unlock(&pProc->lock);

    return desc;
}

// Dispatches the execution of the given user closure on the given dispatch queue
// with the given options. 
errno_t Process_DispatchUserClosure(ProcessRef _Nonnull pProc, int od, unsigned long options, Closure1Arg_Func _Nonnull pUserClosure, void* _Nullable pContext)
{
    decl_try_err();
    DispatchQueueRef pQueue;

    // XXX optimize this: we can actually hold the process lock while dispatching the closure
    // asynchronously because it will be executed asynchronously anyway. Thus we won't have to retain and
    // release the queue (which is safe to do if we hold the lock until we're done with dispatching)
    // This does not apply to synchronous dispatches. Here we have to drop the process lock while we
    // are blocking and waiting for the closure to finish running. Otherwise that closure and nobody
    // else would be able to do a system call anymore.
    if ((err = Process_CopyPrivateResourceForDescriptor(pProc, od, (ObjectRef*) &pQueue)) == EOK) {
        if (Object_InstanceOf(pQueue, DispatchQueue)) {
            if ((options & kDispatchOption_Sync) == kDispatchOption_Sync) {
                err = DispatchQueue_DispatchSync(pQueue, DispatchQueueClosure_MakeUser(pUserClosure, pContext));
            } else {
                err = DispatchQueue_DispatchAsync(pQueue, DispatchQueueClosure_MakeUser(pUserClosure, pContext));
            }
        } else {
            err = EBADF;
        }
        Object_Release(pQueue);
    }
    return err;
}

// Dispatches the execution of the given user closure on the given dispatch queue
// after the given deadline.
errno_t Process_DispatchUserClosureAsyncAfter(ProcessRef _Nonnull pProc, int od, TimeInterval deadline, Closure1Arg_Func _Nonnull pUserClosure, void* _Nullable pContext)
{
    decl_try_err();
    DispatchQueueRef pQueue;

    // XXX optimize this: we can actually hold the process lock while dispatching the closure
    // asynchronously because it will be executed asynchronously anyway. Thus we won't have to retain and
    // release the queue (which is safe to do if we hold the lock until we're done with dispatching)
    if ((err = Process_CopyPrivateResourceForDescriptor(pProc, od, (ObjectRef*) &pQueue)) == EOK) {
        if (Object_InstanceOf(pQueue, DispatchQueue)) {
            err = DispatchQueue_DispatchAsyncAfter(pQueue, deadline, DispatchQueueClosure_MakeUser(pUserClosure, pContext));
        } else {
            err = EBADF;
        }
        Object_Release(pQueue);
    }
    return err;
}
