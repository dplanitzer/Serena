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


// Creates a new dispatch queue and binds it to the process.
errno_t Process_CreateDispatchQueue(ProcessRef _Nonnull self, int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nullable pOutDescriptor)
{
    decl_try_err();
    UDispatchQueueRef pQueue = NULL;

    try(UDispatchQueue_Create(minConcurrency, maxConcurrency, qos, priority, gVirtualProcessorPool, self, &pQueue));
    try(UResourceTable_AdoptResource(&self->uResourcesTable, (UResourceRef) pQueue, pOutDescriptor));
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
int Process_GetCurrentDispatchQueue(ProcessRef _Nonnull self)
{
    return DispatchQueue_GetDescriptor(DispatchQueue_GetCurrent());
}

// Dispatches the execution of the given user closure on the given dispatch queue
// with the given options. 
errno_t Process_DispatchUserClosure(ProcessRef _Nonnull self, int od, VoidFunc_2 _Nonnull func, void* _Nullable ctx, uint32_t userOptions, uintptr_t tag)
{
    decl_try_err();
    const uint32_t options = userOptions & kDispatchOptionMask_User;
    UDispatchQueueRef pQueue;

    if ((options & kDispatchOption_Sync) == kDispatchOption_Sync) {
        if ((err = UResourceTable_AcquireResourceAs(&self->uResourcesTable, od, UDispatchQueue, &pQueue)) == EOK) {
            err = DispatchQueue_DispatchClosure(pQueue->dispatchQueue, (VoidFunc_2)Process_CallUser, (void*)func, ctx, 0, options, tag);
            UResourceTable_RelinquishResource(&self->uResourcesTable, pQueue);
        }
    }
    else {
        if ((err = UResourceTable_BeginDirectResourceAccessAs(&self->uResourcesTable, od, UDispatchQueue, &pQueue)) == EOK) {
            err = DispatchQueue_DispatchClosure(pQueue->dispatchQueue, (VoidFunc_2)Process_CallUser, (void*)func, ctx, 0, options, tag);
            UResourceTable_EndDirectResourceAccess(&self->uResourcesTable);
        }
    }

    return err;
}

// Dispatches the execution of the given user closure on the given dispatch queue
// after the given deadline.
errno_t Process_DispatchUserTimer(ProcessRef _Nonnull self, int od, const struct timespec* _Nonnull deadline, const struct timespec* _Nonnull interval, VoidFunc_1 _Nonnull func, void* _Nullable ctx, uintptr_t tag)
{
    decl_try_err();
    UDispatchQueueRef pQueue;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&self->uResourcesTable, od, UDispatchQueue, &pQueue)) == EOK) {
        err = DispatchQueue_DispatchTimer(pQueue->dispatchQueue, deadline, interval, (VoidFunc_2)Process_CallUser, (void*)func, ctx, 0, 0, tag);
        UResourceTable_EndDirectResourceAccess(&self->uResourcesTable);
    }
    return err;
}

errno_t Process_DispatchRemoveByTag(ProcessRef _Nonnull self, int od, uintptr_t tag)
{
    decl_try_err();
    UDispatchQueueRef pQueue;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&self->uResourcesTable, od, UDispatchQueue, &pQueue)) == EOK) {
        DispatchQueue_RemoveByTag(pQueue->dispatchQueue, tag);
        UResourceTable_EndDirectResourceAccess(&self->uResourcesTable);
    }
    return err;
}
