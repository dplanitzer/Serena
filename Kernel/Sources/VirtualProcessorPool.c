//
//  VirtualProcessorPool.c
//  Apollo
//
//  Created by Dietmar Planitzer on 4/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "VirtualProcessorPool.h"
#include "kalloc.h"
#include "Lock.h"
#include "VirtualProcessorScheduler.h"


#define REUSE_CACHE_CAPACITY    16
typedef struct _VirtualProcessorPool {
    Lock    lock;
    List    inuse_queue;        // VPs in use
    List    reuse_queue;        // VPs available for reuse
    Int     inuse_count;        // count of VPs that are in use
    Int     reuse_count;        // count of how many VPs are in the reuse queue
    Int     reuse_capacity;     // reuse cache will not store more than this. If a VP exits while the cache is at max capacity -> VP will exit for good and get finalized
} VirtualProcessorPool;


VirtualProcessorPoolRef gVirtualProcessorPool;


ErrorCode VirtualProcessorPool_Create(VirtualProcessorPoolRef _Nullable * _Nonnull pOutPool)
{
    decl_try_err();
    VirtualProcessorPool* pPool;
    
    try(kalloc_cleared(sizeof(VirtualProcessorPool), (Byte**) &pPool));
    List_Init(&pPool->inuse_queue);
    List_Init(&pPool->reuse_queue);
    Lock_Init(&pPool->lock);
    pPool->inuse_count = 0;
    pPool->reuse_count = 0;
    pPool->reuse_capacity = REUSE_CACHE_CAPACITY;
    *pOutPool = pPool;
    return EOK;
    
catch:
    VirtualProcessorPool_Destroy(pPool);
    *pOutPool = NULL;
    return err;
}

void VirtualProcessorPool_Destroy(VirtualProcessorPoolRef _Nullable pPool)
{
    if (pPool) {
        List_Deinit(&pPool->inuse_queue);
        List_Deinit(&pPool->reuse_queue);
        Lock_Deinit(&pPool->lock);
        kfree((Byte*)pPool);
    }
}

ErrorCode VirtualProcessorPool_AcquireVirtualProcessor(VirtualProcessorPoolRef _Nonnull pool, VirtualProcessorParameters params, VirtualProcessor* _Nonnull * _Nonnull pOutVP)
{
    decl_try_err();
    VirtualProcessor* pVP = NULL;
    Bool needsUnlock = false;

    try(Lock_Lock(&pool->lock));
    needsUnlock = true;

    // Try to reuse a cached VP
    VirtualProcessorOwner* pCurVP = (VirtualProcessorOwner*)pool->reuse_queue.first;
    while (pCurVP) {
        if (VirtualProcessor_IsSuspended(pCurVP->self)) {
            pVP = pCurVP->self;
            break;
        }
        pCurVP = (VirtualProcessorOwner*)pCurVP->queue_entry.next;
    }
        
    if (pVP) {
        List_Remove(&pool->reuse_queue, &pVP->owner.queue_entry);
        pool->reuse_count--;
        
        List_InsertBeforeFirst(&pool->inuse_queue, &pVP->owner.queue_entry);
        pool->inuse_count++;
    }
    
    Lock_Unlock(&pool->lock);
    needsUnlock = false;
    
    
    // Create a new VP if we were not able to reuse a cached one
    if (pVP == NULL) {
        try(VirtualProcessor_Create(&pVP));

        try(Lock_Lock(&pool->lock));
        needsUnlock = true;
        List_InsertBeforeFirst(&pool->inuse_queue, &pVP->owner.queue_entry);
        pool->inuse_count++;
        needsUnlock = false;
        Lock_Unlock(&pool->lock);
    }
    
    
    // Configure the VP
    VirtualProcessor_SetPriority(pVP, params.priority);
    try(VirtualProcessor_SetClosure(pVP, VirtualProcessorClosure_Make(params.func, params.context, params.kernelStackSize, params.userStackSize)));

    *pOutVP = pVP;
    return EOK;

catch:
    if (needsUnlock) {
        Lock_Unlock(&pool->lock);
    }
    *pOutVP = NULL;
    return err;
}

// Relinquishes the given VP back to the reuse pool if possible. If the reuse
// pool is full then the given VP is suspended and scheduled for finalization
// instead. Note that the VP is suspended in any case.
_Noreturn VirtualProcessorPool_RelinquishVirtualProcessor(VirtualProcessorPoolRef _Nonnull pool, VirtualProcessor* _Nonnull pVP)
{
    Bool didReuse = false;

    // Null out the dispatch queue reference in any case since the VP should no
    // longer be associated with a queue.
    VirtualProcessor_SetDispatchQueue(pVP, NULL, -1);


    // Try to cache the VP
    try_bang(Lock_Lock(&pool->lock));
    
    List_Remove(&pool->inuse_queue, &pVP->owner.queue_entry);
    pool->inuse_count--;

    if (pool->reuse_count < pool->reuse_capacity) {
        List_InsertBeforeFirst(&pool->reuse_queue, &pVP->owner.queue_entry);
        pool->reuse_count++;
        didReuse = true;
    }
    Lock_Unlock(&pool->lock);
    

    // Suspend the VP if we decided to reuse it and schedule it for finalization
    // (termination) otherwise.
    if (didReuse) {
        try_bang(VirtualProcessor_Suspend(pVP));
    } else {
        VirtualProcessor_Terminate(pVP);
    }
    /* NOT REACHED */
}
