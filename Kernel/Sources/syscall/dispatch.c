//
//  dispatch.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <process/UDispatchQueue.h>
#include <dispatcher/VirtualProcessorPool.h>
#include <dispatchqueue/DispatchQueue.h>


SYSCALL_5(dispatch_queue_create, int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nonnull pOutQueue)
{
    decl_try_err();
    ProcessRef pp = (ProcessRef)p;
    UDispatchQueueRef pQueue = NULL;

    try(UDispatchQueue_Create(pa->minConcurrency, pa->maxConcurrency, pa->qos, pa->priority, gVirtualProcessorPool, pp, &pQueue));
    try(UResourceTable_AdoptResource(&pp->uResourcesTable, (UResourceRef) pQueue, pa->pOutQueue));
    DispatchQueue_SetDescriptor(pQueue->dispatchQueue, *(pa->pOutQueue));
    pQueue = NULL;
    return EOK;

catch:
    UResource_Dispose(pQueue);
    *(pa->pOutQueue) = -1;
    return err;
}

SYSCALL_5(dispatch, int od, const VoidFunc_2 _Nonnull func, void* _Nullable ctx, uint32_t uOptions, uintptr_t tag)
{
    decl_try_err();
    ProcessRef pp = (ProcessRef)p;
    const uint32_t options = pa->uOptions & kDispatchOptionMask_User;
    UDispatchQueueRef pQueue;

    if ((options & kDispatchOption_Sync) == kDispatchOption_Sync) {
        if ((err = UResourceTable_AcquireResourceAs(&pp->uResourcesTable, pa->od, UDispatchQueue, &pQueue)) == EOK) {
            err = DispatchQueue_DispatchClosure(pQueue->dispatchQueue, (VoidFunc_2)Process_CallUser, (void*)pa->func, pa->ctx, 0, options, pa->tag);
            UResourceTable_RelinquishResource(&pp->uResourcesTable, pQueue);
        }
    }
    else {
        if ((err = UResourceTable_BeginDirectResourceAccessAs(&pp->uResourcesTable, pa->od, UDispatchQueue, &pQueue)) == EOK) {
            err = DispatchQueue_DispatchClosure(pQueue->dispatchQueue, (VoidFunc_2)Process_CallUser, (void*)pa->func, pa->ctx, 0, options, pa->tag);
            UResourceTable_EndDirectResourceAccess(&pp->uResourcesTable);
        }
    }

    return err;
}

SYSCALL_6(dispatch_timer, int od, const struct timespec* _Nonnull deadline, const struct timespec* _Nonnull interval, const VoidFunc_1 _Nonnull func, void* _Nullable ctx, uintptr_t tag)
{
    decl_try_err();
    ProcessRef pp = (ProcessRef)p;
    UDispatchQueueRef pQueue;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&pp->uResourcesTable, pa->od, UDispatchQueue, &pQueue)) == EOK) {
        err = DispatchQueue_DispatchTimer(pQueue->dispatchQueue, pa->deadline, pa->interval, (VoidFunc_2)Process_CallUser, (void*)pa->func, pa->ctx, 0, 0, pa->tag);
        UResourceTable_EndDirectResourceAccess(&pp->uResourcesTable);
    }
    return err;
}

SYSCALL_2(dispatch_remove_by_tag, int od, uintptr_t tag)
{
    decl_try_err();
    ProcessRef pp = (ProcessRef)p;
    UDispatchQueueRef pQueue;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&pp->uResourcesTable, pa->od, UDispatchQueue, &pQueue)) == EOK) {
        DispatchQueue_RemoveByTag(pQueue->dispatchQueue, pa->tag);
        UResourceTable_EndDirectResourceAccess(&pp->uResourcesTable);
    }
    return err;
}

SYSCALL_0(dispatch_queue_current)
{
    return DispatchQueue_GetDescriptor(DispatchQueue_GetCurrent());
}
